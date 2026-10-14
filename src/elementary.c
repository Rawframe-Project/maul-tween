// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The exponential reduces x to r = x - k ln 2 with |r| <= ln 2 / 2 by
// Cody and Waite's split of ln 2; e^r is its Taylor series to degree
// 13, whose remainder is under 4e-18 relative there. The logarithm
// writes x = 2^k m with m in [sqrt(1/2), sqrt(2)), f = m - 1 exactly,
// s = f / (2 + f), and ln m = f - f^2/2 + s (f^2/2 + R(s^2)), R the
// atanh series to s^22, whose remainder is under 1e-17 relative for
// |s| <= 0.172. Sine and cosine reduce x to r = x - k pi / 2 with
// |r| <= pi / 4 by a three-part split of pi / 2, keeping r as a pair of
// doubles, and sum their Taylor series to degrees 17 and 18. The
// arctangent reduces to z in [0, 1], then to u = (z - c) / (1 + z c)
// with c the nearest multiple of 1/8, |u| <= 1/16, and sums its series
// to u^13, whose remainder is under 1e-18 relative, onto atan(c). In
// each, the leading terms are added exactly as pairs of doubles (Knuth's
// two-sum, Dekker's product) and the result is rounded once, so the
// error stays near half an ulp.

#include "elementary.h"

#include "generated/math_constants.h"

#include <math.h>
#include <stdbool.h>

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

// Below this, sine is x and cosine is 1 to within half an ulp.
static const double s_tinyAngle = 0x1.0p-27;

// The sine and cosine of r + rLow for |r| <= pi / 4 and rLow far below
// an ulp of r.
static void SinCosKernel(double r, double rLow, double* sineOut, double* cosineOut)
{
    double zLow = 0.0;
    double z = TwoProduct(r, r, &zLow);
    // sin = r + r^3 (-1/3! + z / 5! - ...), with rLow's first-order term.
    double sineTail = r * z * Polynomial(s_sinTaylor, 8, z);
    *sineOut = r + (sineTail + rLow * (1.0 - 0.5 * z));
    // cos = 1 - z / 2 + z^2 (1/4! - z / 6! + ...), 1 - z / 2 exactly as a
    // pair, minus rLow's first-order term r rLow.
    double halfLow = 0.5 * zLow;
    double headLow = 0.0;
    double head = FastTwoSum(1.0, -0.5 * z, &headLow);
    double cosineTail = z * z * Polynomial(s_cosTaylor, 8, z);
    *cosineOut = head + (headLow - halfLow + (cosineTail - r * rLow));
}

void mtwnSinCos(double x, double* sineOut, double* cosineOut)
{
    if (!isfinite(x))
    {
        *sineOut = (double)NAN;
        *cosineOut = (double)NAN;
        return;
    }
    if (fabs(x) < s_tinyAngle)
    {
        *sineOut = x;
        *cosineOut = 1.0;
        return;
    }
    double k = floor(x * MTWN_INV_PIO2 + 0.5);
    // x - k p1 is exact (Sterbenz), k p2 and k p3 are exact products for
    // |k| < 2^20, and the sums carry their errors.
    double first = x - k * MTWN_PIO2_1;
    double secondLow = 0.0;
    double second = TwoSum(first, -k * MTWN_PIO2_2, &secondLow);
    double rLow = 0.0;
    double r = FastTwoSum(second, secondLow - k * MTWN_PIO2_3, &rLow);
    double sine = 0.0;
    double cosine = 0.0;
    SinCosKernel(r, rLow, &sine, &cosine);
    // The quadrant, from k modulo 4 computed exactly for any k.
    double quadrant = fmod(k, 4.0);
    if (quadrant < 0.0)
    {
        quadrant += 4.0;
    }
    switch ((int)quadrant)
    {
    case 0:
        *sineOut = sine;
        *cosineOut = cosine;
        break;
    case 1:
        *sineOut = cosine;
        *cosineOut = -sine;
        break;
    case 2:
        *sineOut = -sine;
        *cosineOut = -cosine;
        break;
    default:
        *sineOut = -cosine;
        *cosineOut = sine;
        break;
    }
}

// atan(z + zLow) for z in [0, 1], as a pair: *lowOut carries what the
// returned double cannot.
static double ArctanPair(double z, double zLow, double* lowOut)
{
    double index = floor(z * 8.0 + 0.5);
    double c = index * 0.125;
    // z - c is exact (Sterbenz, or c = 0); 1 + z c is kept as a pair.
    double numerator = z - c;
    double productLow = 0.0;
    double product = TwoProduct(z, c, &productLow);
    double denominatorLow = 0.0;
    double denominator = FastTwoSum(1.0, product, &denominatorLow);
    denominatorLow += productLow;
    double u = numerator / denominator;
    double backLow = 0.0;
    double back = TwoProduct(u, denominator, &backLow);
    double uLow = ((numerator - back) - backLow - u * denominatorLow) / denominator;
    // zLow moves atan by zLow / (1 + z^2) to first order.
    uLow += zLow / (1.0 + z * z);
    double v = u * u;
    double tail = u * v * Polynomial(s_atanSeries, 6, v);
    int i = (int)index;
    double sumLow = 0.0;
    double sum = TwoSum(s_atanEighths[i], u, &sumLow);
    *lowOut = sumLow + (s_atanEighthsLow[i] + uLow + tail);
    return sum;
}

// The C standard's atan2 where x or y is zero or infinite: true, with the
// angle in *angleOut; false for every other pair.
static bool Atan2Special(double y, double x, double* angleOut)
{
    double ax = fabs(x);
    double ay = fabs(y);
    double angle = 0.0;
    if (isinf(ax) || isinf(ay))
    {
        // Infinities of both, of y alone, or of x alone.
        double quarter = MTWN_PIO2_HI * 0.5;
        angle = isinf(ay) ? (isinf(ax) ? quarter : MTWN_PIO2_HI) : 0.0;
        if (signbit(x))
        {
            angle = isinf(ay) ? (isinf(ax) ? 3.0 * quarter : MTWN_PIO2_HI) : MTWN_PI_HI;
        }
    }
    else if (ay == 0.0)
    {
        angle = signbit(x) ? MTWN_PI_HI : 0.0;
    }
    else if (ax == 0.0)
    {
        angle = MTWN_PIO2_HI;
    }
    else
    {
        return false;
    }
    *angleOut = copysign(angle, y);
    return true;
}

double mtwnAtan2(double y, double x)
{
    if (isnan(x) || isnan(y))
    {
        return (double)NAN;
    }
    double special = 0.0;
    if (Atan2Special(y, x, &special))
    {
        return special;
    }
    double ax = fabs(x);
    double ay = fabs(y);
    // The ratio of the smaller to the larger, as a pair, then its angle.
    // Both are first scaled by one power of two, which leaves the angle as
    // it is and keeps Dekker's product in range for any finite pair.
    double small = ay <= ax ? ay : ax;
    double large = ay <= ax ? ax : ay;
    int exponent = 0;
    large = frexp(large, &exponent);
    small = ldexp(small, -exponent);
    double z = small / large;
    double backLow = 0.0;
    double back = TwoProduct(z, large, &backLow);
    double zLow = ((small - back) - backLow) / large;
    double angleLow = 0.0;
    double angle = ArctanPair(z, zLow, &angleLow);
    if (ay > ax)
    {
        double low = 0.0;
        angle = TwoSum(MTWN_PIO2_HI, -angle, &low);
        angleLow = low + (MTWN_PIO2_LO - angleLow);
    }
    if (signbit(x))
    {
        double low = 0.0;
        angle = TwoSum(MTWN_PI_HI, -angle, &low);
        angleLow = low + (MTWN_PI_LO - angleLow);
    }
    return copysign(angle + angleLow, y);
}
