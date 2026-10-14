// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The methods of src/elementary.c in binary32: the exponential reduces
// x to r = x - k ln 2 with |r| <= ln 2 / 2 by a split of ln 2 whose
// high part has 16 bits, and sums its Taylor series to degree 7, whose
// remainder is under 6e-9 relative there; the logarithm writes
// x = 2^k m with m in [sqrt(1/2), sqrt(2)) and sums the atanh series of
// s = (m - 1) / (m + 1) to s^9, whose remainder is under 0.05 ulp.
// Sine and cosine reduce by pi / 2 held in seven parts, six of 8 bits
// whose products with k are exact for |k| < 2^16, about 72 bits in all,
// as floats within 2^-27.8 of a multiple of pi / 2 need; they sum their
// Taylor series to degrees 9 and 10; the arctangent reduces around atan(k / 8) to |u| <= 1/16 and
// sums its series to u^7. The leading terms are added exactly as pairs
// of floats, so the result is rounded once.

#include "elementary_float.h"

#include "generated/math_constants_float.h"

#include <math.h>
#include <stdbool.h>

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
    float r = z * PolynomialF(s_logSeriesF, 4, z);
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

// Below this, sine is x and cosine is 1 to within half an ulp.
static const float s_tinyAngleF = 0x1.0p-12f;

// The sine and cosine of r + rLow for |r| <= pi / 4 and rLow far below
// an ulp of r.
static void SinCosKernelF(float r, float rLow, float* sineOut, float* cosineOut)
{
    float zLow = 0.0f;
    float z = TwoProductF(r, r, &zLow);
    float sineTail = r * z * PolynomialF(s_sinTaylorF, 4, z);
    *sineOut = r + (sineTail + rLow * (1.0f - 0.5f * z));
    float halfLow = 0.5f * zLow;
    float headLow = 0.0f;
    float head = FastTwoSumF(1.0f, -0.5f * z, &headLow);
    float cosineTail = z * z * PolynomialF(s_cosTaylorF, 4, z);
    *cosineOut = head + (headLow - halfLow + (cosineTail - r * rLow));
}

void mtwnSinCosF(float x, float* sineOut, float* cosineOut)
{
    if (!isfinite(x))
    {
        *sineOut = NAN;
        *cosineOut = NAN;
        return;
    }
    if (fabsf(x) < s_tinyAngleF)
    {
        *sineOut = x;
        *cosineOut = 1.0f;
        return;
    }
    float k = floorf(x * MTWN_INV_PIO2_F + 0.5f);
    // x - k p1 is exact (Sterbenz) and each k p an exact product for
    // |k| < 2^16; the sums carry their errors in rLow.
    float r = x - k * s_pio2PartsF[0];
    float rLow = 0.0f;
    for (int i = 1; i < 6; ++i)
    {
        float error = 0.0f;
        r = TwoSumF(r, -k * s_pio2PartsF[i], &error);
        rLow += error;
    }
    // rLow is a few ulps of r at most, which the kernel takes to first
    // order.
    rLow -= k * s_pio2PartsF[6];
    float sine = 0.0f;
    float cosine = 0.0f;
    SinCosKernelF(r, rLow, &sine, &cosine);
    // The quadrant, from k modulo 4 computed exactly for any k.
    float quadrant = fmodf(k, 4.0f);
    if (quadrant < 0.0f)
    {
        quadrant += 4.0f;
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
// returned float cannot.
static float ArctanPairF(float z, float zLow, float* lowOut)
{
    float index = floorf(z * 8.0f + 0.5f);
    float c = index * 0.125f;
    // z - c is exact (Sterbenz, or c = 0); 1 + z c is kept as a pair.
    float numerator = z - c;
    float productLow = 0.0f;
    float product = TwoProductF(z, c, &productLow);
    float denominatorLow = 0.0f;
    float denominator = FastTwoSumF(1.0f, product, &denominatorLow);
    denominatorLow += productLow;
    float u = numerator / denominator;
    float backLow = 0.0f;
    float back = TwoProductF(u, denominator, &backLow);
    float uLow = ((numerator - back) - backLow - u * denominatorLow) / denominator;
    // zLow moves atan by zLow / (1 + z^2) to first order.
    uLow += zLow / (1.0f + z * z);
    float v = u * u;
    float tail = u * v * PolynomialF(s_atanSeriesF, 3, v);
    int i = (int)index;
    float sumLow = 0.0f;
    float sum = TwoSumF(s_atanEighthsF[i], u, &sumLow);
    *lowOut = sumLow + (s_atanEighthsLowF[i] + uLow + tail);
    return sum;
}

// The C standard's atan2 where x or y is zero or infinite: true, with the
// angle in *angleOut; false for every other pair.
static bool Atan2SpecialF(float y, float x, float* angleOut)
{
    float ax = fabsf(x);
    float ay = fabsf(y);
    float angle = 0.0f;
    if (isinf(ax) || isinf(ay))
    {
        // Infinities of both, of y alone, or of x alone.
        float quarter = MTWN_PIO2_HI_F * 0.5f;
        angle = isinf(ay) ? (isinf(ax) ? quarter : MTWN_PIO2_HI_F) : 0.0f;
        if (signbit(x))
        {
            angle = isinf(ay) ? (isinf(ax) ? 3.0f * quarter : MTWN_PIO2_HI_F) : MTWN_PI_HI_F;
        }
    }
    else if (ay == 0.0f)
    {
        angle = signbit(x) ? MTWN_PI_HI_F : 0.0f;
    }
    else if (ax == 0.0f)
    {
        angle = MTWN_PIO2_HI_F;
    }
    else
    {
        return false;
    }
    *angleOut = copysignf(angle, y);
    return true;
}

float mtwnAtan2F(float y, float x)
{
    if (isnan(x) || isnan(y))
    {
        return NAN;
    }
    float special = 0.0f;
    if (Atan2SpecialF(y, x, &special))
    {
        return special;
    }
    float ax = fabsf(x);
    float ay = fabsf(y);
    // The ratio of the smaller to the larger, as a pair, then its angle.
    // Both are first scaled by one power of two, which leaves the angle as
    // it is and keeps Dekker's product in range for any finite pair.
    float small = ay <= ax ? ay : ax;
    float large = ay <= ax ? ax : ay;
    int exponent = 0;
    large = frexpf(large, &exponent);
    small = ldexpf(small, -exponent);
    float z = small / large;
    float backLow = 0.0f;
    float back = TwoProductF(z, large, &backLow);
    float zLow = ((small - back) - backLow) / large;
    float angleLow = 0.0f;
    float angle = ArctanPairF(z, zLow, &angleLow);
    if (ay > ax)
    {
        float low = 0.0f;
        angle = TwoSumF(MTWN_PIO2_HI_F, -angle, &low);
        angleLow = low + (MTWN_PIO2_LO_F - angleLow);
    }
    if (signbit(x))
    {
        float low = 0.0f;
        angle = TwoSumF(MTWN_PI_HI_F, -angle, &low);
        angleLow = low + (MTWN_PI_LO_F - angleLow);
    }
    return copysignf(angle + angleLow, y);
}
