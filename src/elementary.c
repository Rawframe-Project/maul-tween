// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The exponential reduces x to r = x - k ln 2 with |r| <= ln 2 / 2 by
// Cody and Waite's split of ln 2; e^r is its Taylor series to degree
// 13, whose remainder is under 4e-18 relative there. The logarithm
// writes x = 2^k m with m in [sqrt(1/2), sqrt(2)), f = m - 1 exactly,
// s = f / (2 + f), and ln m = f - f^2/2 + s (f^2/2 + R(s^2)), R the
// atanh series to s^22, whose remainder is under 1e-17 relative for
// |s| <= 0.172. In each, the leading terms are added exactly as pairs
// of doubles (Knuth's two-sum, Dekker's product) and the result is
// rounded once, so the error stays near half an ulp.

#include "elementary.h"

#include "generated/math_constants.h"

#include <math.h>

// The largest x whose exponential is finite, the double just below
// ln(DBL_MAX), and the smallest whose exponential rounds above 0.
static const double s_expOverflow = 0x1.62e42fefa39efp+9;
static const double s_expUnderflow = -0x1.74910d52d3051p+9;
// sqrt(1/2), where the logarithm moves its mantissa into the upper half.
static const double s_sqrtHalf = 0x1.6a09e667f3bcdp-1;

// Evaluates c[0] + c[1] x + ... + c[count - 1] x^(count - 1) by Horner's
// rule.
static double Polynomial(const double* c, int count, double x)
{
    double sum = c[count - 1];
    for (int i = count - 2; i >= 0; --i)
    {
        sum = sum * x + c[i];
    }
    return sum;
}

// a + b = sum + *errorOut exactly, whatever their magnitudes (Knuth).
static double TwoSum(double a, double b, double* errorOut)
{
    double sum = a + b;
    double bVirtual = sum - a;
    double aVirtual = sum - bVirtual;
    *errorOut = (a - aVirtual) + (b - bVirtual);
    return sum;
}

// a + b = sum + *errorOut exactly when |a| >= |b| (Dekker).
static double FastTwoSum(double a, double b, double* errorOut)
{
    double sum = a + b;
    *errorOut = b - (sum - a);
    return sum;
}

// a times b = product + *errorOut exactly, for operands whose product and
// halves stay in range (Veltkamp's split, Dekker's product).
static double TwoProduct(double a, double b, double* errorOut)
{
    double product = a * b;
    double aSplit = a * 134217729.0;
    double aHigh = aSplit - (aSplit - a);
    double aLow = a - aHigh;
    double bSplit = b * 134217729.0;
    double bHigh = bSplit - (bSplit - b);
    double bLow = b - bHigh;
    *errorOut = ((aHigh * bHigh - product) + aHigh * bLow + aLow * bHigh) + aLow * bLow;
    return product;
}

double mtwnExp(double x)
{
    if (isnan(x))
    {
        return x;
    }
    if (x > s_expOverflow)
    {
        return HUGE_VAL;
    }
    if (x < s_expUnderflow)
    {
        return 0.0;
    }
    double k = floor(x * MTWN_INV_LN2 + 0.5);
    // x - k times the high part is exact; r + rLow is the reduced
    // argument to well beyond double precision.
    double reduced = x - k * MTWN_LN2_HI;
    double rLow = 0.0;
    double r = TwoSum(reduced, -k * MTWN_LN2_LO, &rLow);
    // e^r = 1 + r + r^2 (1/2! + r / 3! + ... + r^11 / 13!), the 1 + r
    // added exactly; e^(r + rLow) adds rLow to first order.
    double tail = r * r * Polynomial(s_expTaylor, 12, r);
    double sumLow = 0.0;
    double sum = FastTwoSum(1.0, r, &sumLow);
    return ldexp(sum + (sumLow + (rLow + tail)), (int)k);
}

double mtwnExp2(double x)
{
    if (isnan(x))
    {
        return x;
    }
    if (x >= 1024.0)
    {
        return HUGE_VAL;
    }
    if (x < -1075.0)
    {
        return 0.0;
    }
    double k = floor(x + 0.5);
    // Exact: x and k are within one half of each other.
    double f = x - k;
    // 2^f = 1 + f ln 2 + f^2 ((ln 2)^2 / 2! + ... + f^11 (ln 2)^13 / 13!),
    // the 1 + f ln 2 added exactly, ln 2 carried in two parts.
    double productLow = 0.0;
    double product = TwoProduct(f, MTWN_LN2, &productLow);
    productLow += f * MTWN_LN2_TAIL;
    double tail = f * f * Polynomial(s_exp2Taylor + 1, 12, f);
    double sumLow = 0.0;
    double sum = FastTwoSum(1.0, product, &sumLow);
    return ldexp(sum + (sumLow + (productLow + tail)), (int)k);
}

double mtwnLog(double x)
{
    if (isnan(x) || x < 0.0)
    {
        return (double)NAN;
    }
    if (x == 0.0)
    {
        return -HUGE_VAL;
    }
    if (isinf(x))
    {
        return x;
    }
    int e = 0;
    double m = frexp(x, &e);
    if (m < s_sqrtHalf)
    {
        m *= 2.0;
        e -= 1;
    }
    // Exact by Sterbenz's lemma, m being within a factor of two of 1.
    double f = m - 1.0;
    double s = f / (2.0 + f);
    double z = s * s;
    double r = z * Polynomial(s_logSeries, 11, z);
    // f^2 / 2 exactly as a pair, then f - f^2/2 exactly as a pair.
    double squareLow = 0.0;
    double halfSquare = 0.5 * TwoProduct(f, f, &squareLow);
    squareLow *= 0.5;
    double headLow = 0.0;
    double head = FastTwoSum(f, -halfSquare, &headLow);
    double small = s * (halfSquare + r) - squareLow;
    // k times MTWN_LN2_HI is exact for every exponent a double has; adding
    // it to the head exactly keeps the cancellation near x = 1/2 exact.
    double k = (double)e;
    double totalLow = 0.0;
    double total = TwoSum(k * MTWN_LN2_HI, head, &totalLow);
    return total + (totalLow + (headLow + small + k * MTWN_LN2_LO));
}
