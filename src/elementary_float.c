// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The methods of src/elementary.c in binary32: the exponential reduces
// x to r = x - k ln 2 with |r| <= ln 2 / 2 by a split of ln 2 whose
// high part has 16 bits, and sums its Taylor series to degree 7, whose
// remainder is under 6e-9 relative there; the logarithm writes
// x = 2^k m with m in [sqrt(1/2), sqrt(2)) and sums the atanh series of
// s = (m - 1) / (m + 1) to s^11. The leading terms are added exactly as
// pairs of floats, so the result is rounded once.

#include "elementary_float.h"

#include "generated/math_constants_float.h"

#include <math.h>

// sqrt(1/2), where the logarithm moves its mantissa into the upper half.
static const float s_sqrtHalfF = 0x1.6a09e6p-1f;

// Evaluates c[0] + c[1] x + ... + c[count - 1] x^(count - 1) by Horner's
// rule.
static float PolynomialF(const float* c, int count, float x)
{
    float sum = c[count - 1];
    for (int i = count - 2; i >= 0; --i)
    {
        sum = sum * x + c[i];
    }
    return sum;
}

// a + b = sum + *errorOut exactly, whatever their magnitudes (Knuth).
static float TwoSumF(float a, float b, float* errorOut)
{
    float sum = a + b;
    float bVirtual = sum - a;
    float aVirtual = sum - bVirtual;
    *errorOut = (a - aVirtual) + (b - bVirtual);
    return sum;
}

// a + b = sum + *errorOut exactly when |a| >= |b| (Dekker).
static float FastTwoSumF(float a, float b, float* errorOut)
{
    float sum = a + b;
    *errorOut = b - (sum - a);
    return sum;
}

// a times b = product + *errorOut exactly, for operands whose product and
// halves stay in range (Veltkamp's split at 2^12 + 1, Dekker's product).
static float TwoProductF(float a, float b, float* errorOut)
{
    float product = a * b;
    float aSplit = a * 4097.0f;
    float aHigh = aSplit - (aSplit - a);
    float aLow = a - aHigh;
    float bSplit = b * 4097.0f;
    float bHigh = bSplit - (bSplit - b);
    float bLow = b - bHigh;
    *errorOut = ((aHigh * bHigh - product) + aHigh * bLow + aLow * bHigh) + aLow * bLow;
    return product;
}

float mtwnExpF(float x)
{
    if (isnan(x))
    {
        return x;
    }
    if (x > MTWN_EXP_OVERFLOW_F)
    {
        return HUGE_VALF;
    }
    if (x < MTWN_EXP_UNDERFLOW_F)
    {
        return 0.0f;
    }
    float k = floorf(x * MTWN_INV_LN2_F + 0.5f);
    // x - k times the high part is exact for |k| < 2^8; r + rLow is the
    // reduced argument to beyond float precision.
    float reduced = x - k * MTWN_LN2_HI_F;
    float rLow = 0.0f;
    float r = TwoSumF(reduced, -k * MTWN_LN2_LO_F, &rLow);
    // e^r = 1 + r + r^2 (1/2! + ... + r^5 / 7!), the 1 + r added exactly.
    float tail = r * r * PolynomialF(s_expTaylorF, 6, r);
    float sumLow = 0.0f;
    float sum = FastTwoSumF(1.0f, r, &sumLow);
    return ldexpf(sum + (sumLow + (rLow + tail)), (int)k);
}

float mtwnExp2F(float x)
{
    if (isnan(x))
    {
        return x;
    }
    if (x >= 128.0f)
    {
        return HUGE_VALF;
    }
    if (x < -150.0f)
    {
        return 0.0f;
    }
    float k = floorf(x + 0.5f);
    // Exact: x and k are within one half of each other.
    float f = x - k;
    // 2^f = 1 + f ln 2 + f^2 ((ln 2)^2 / 2! + ... + f^5 (ln 2)^7 / 7!), the
    // 1 + f ln 2 added exactly, ln 2 carried in two parts.
    float productLow = 0.0f;
    float product = TwoProductF(f, MTWN_LN2_F, &productLow);
    productLow += f * MTWN_LN2_TAIL_F;
    float tail = f * f * PolynomialF(s_exp2TaylorF + 1, 6, f);
    float sumLow = 0.0f;
    float sum = FastTwoSumF(1.0f, product, &sumLow);
    return ldexpf(sum + (sumLow + (productLow + tail)), (int)k);
}

float mtwnLogF(float x)
{
    if (isnan(x) || x < 0.0f)
    {
        return NAN;
    }
    if (x == 0.0f)
    {
        return -HUGE_VALF;
    }
    if (isinf(x))
    {
        return x;
    }
    int e = 0;
    float m = frexpf(x, &e);
    if (m < s_sqrtHalfF)
    {
        m *= 2.0f;
        e -= 1;
    }
    // Exact by Sterbenz's lemma, m being within a factor of two of 1.
    float f = m - 1.0f;
    // s = f / (2 + f) as a pair, from 2 + f kept exactly as a pair.
    float denominatorLow = 0.0f;
    float denominator = FastTwoSumF(2.0f, f, &denominatorLow);
    float s = f / denominator;
    float backLow = 0.0f;
    float back = TwoProductF(s, denominator, &backLow);
    float sLow = ((f - back) - backLow - s * denominatorLow) / denominator;
    float z = s * s;
    float r = z * PolynomialF(s_logSeriesF, 5, z);
    // f - f^2/2 + s (f^2/2 + R), the leading terms as exact pairs.
    float squareLow = 0.0f;
    float halfSquare = 0.5f * TwoProductF(f, f, &squareLow);
    squareLow *= 0.5f;
    float headLow = 0.0f;
    float head = FastTwoSumF(f, -halfSquare, &headLow);
    float innerLow = 0.0f;
    float inner = TwoSumF(halfSquare, r, &innerLow);
    innerLow += squareLow;
    float termLow = 0.0f;
    float term = TwoProductF(s, inner, &termLow);
    termLow += s * innerLow + sLow * inner;
    // k times MTWN_LN2_HI_F is exact for every exponent a float has.
    float k = (float)e;
    float totalLow = 0.0f;
    float total = TwoSumF(k * MTWN_LN2_HI_F, head, &totalLow);
    float withTermLow = 0.0f;
    float withTerm = TwoSumF(total, term, &withTermLow);
    return withTerm +
           ((totalLow + withTermLow) + (headLow + (termLow - squareLow) + k * MTWN_LN2_LO_F));
}
