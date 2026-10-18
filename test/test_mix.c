// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The mix's error (record mtwn-0070): float ends, equal, one ulp apart,
// thirty orders apart or random, at progress within and far past 0 to 1,
// against the exact a + t (b - a) kept as an unevaluated sum. Its error is
// at most u |r| + 2u |t (b - a)|, it is exact at t = 0, at t = 1 and for
// equal ends, and monotone in t.
#include "mix_internal.h"
#include "test_harness.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static const double s_u = 0x1p-53;
static uint64_t s_state = 0x9E3779B97F4A7C15u;

static uint64_t Next(void)
{
    s_state ^= s_state << 13;
    s_state ^= s_state >> 7;
    s_state ^= s_state << 17;
    return s_state;
}

// A double from [0, 1).
static double Unit(void)
{
    return (double)(Next() >> 11) * 0x1p-53;
}

// A float of either sign, 2^-40 to 2^41 in magnitude.
static float RandomFloat(void)
{
    int exponent = -40 + (int)(Next() % 81u);
    float v = ldexpf((float)(1.0 + Unit()), exponent);
    return (Next() & 1u) != 0 ? -v : v;
}

// x + y as hi + lo exactly (Knuth's TwoSum).
static void TwoSum(double x, double y, double* hi, double* lo)
{
    double s = x + y;
    double b = s - x;
    *hi = s;
    *lo = (x - (s - b)) + (y - b);
}

// m minus the exact a + t (b - a), to within u of itself and u^2 of the
// terms; the result r and |t (b - a)| for the bound.
static double ErrorOf(double m, double a, double b, double t, double* r, double* change)
{
    double dHi = 0.0;
    double dLo = 0.0;
    TwoSum(b, -a, &dHi, &dLo);
    double pHi = t * dHi;
    double pLo = fma(t, dHi, -pHi) + t * dLo;
    double sHi = 0.0;
    double sLo = 0.0;
    TwoSum(a, pHi, &sHi, &sLo);
    double eHi = 0.0;
    double eLo = 0.0;
    TwoSum(m, -sHi, &eHi, &eLo);
    *r = sHi;
    *change = fabs(pHi);
    return eHi + (eLo - sLo - pLo);
}

static double RandomProgress(uint32_t n)
{
    switch (n % 8u)
    {
    case 0:
    case 1:
    case 2:
        return Unit();
    case 3:
        return 1.0 - Unit() * 0x1p-20;
    case 4:
        return (double)(Next() % 2u);
    default:
    {
        double t = ldexp(0.5 + Unit(), (int)(Next() % 71u));
        return (Next() & 1u) != 0 ? -t : t;
    }
    }
}

static void TestBound(void)
{
    uint32_t over = 0;
    uint32_t moved = 0;
    double worst = 0.0;
    for (uint32_t n = 0; n < 400000u; ++n)
    {
        float a = RandomFloat();
        float b = RandomFloat();
        switch (n % 5u)
        {
        case 0:
            b = a;
            break;
        case 1:
            b = nextafterf(a, (Next() & 1u) != 0 ? INFINITY : -INFINITY);
            break;
        case 2:
            b = fabsf(a) < 1.0f ? a * 1e30f : a * 1e-30f;
            break;
        default:
            break;
        }
        double t = RandomProgress(n);
        double m = mtwnMix((double)a, (double)b, t);
        double r = 0.0;
        double change = 0.0;
        double error = fabs(ErrorOf(m, (double)a, (double)b, t, &r, &change));
        double bound = s_u * fabs(r) + 2.0 * s_u * change;
        over += error > bound * (1.0 + 0x1p-40) + 0x1p-1074;
        worst = bound > 0.0 && error / bound > worst ? error / bound : worst;
        bool exact = t == 0.0 || t == 1.0 || a == b;
        double end = t == 1.0 ? (double)b : (double)a;
        moved += exact && m != end;
    }
    CHECK(over == 0, "every mix within u |r| + 2u |t (b - a)|");
    CHECK(worst > 0.5, "the bound is near the worst error, not loose by orders");
    CHECK(moved == 0, "exact at 0, at 1 and between equal ends");
}

static void TestMonotone(void)
{
    uint32_t backwards = 0;
    for (uint32_t n = 0; n < 200000u; ++n)
    {
        float a = RandomFloat();
        float b = n % 2u == 0 ? nextafterf(a, INFINITY) : RandomFloat();
        double t = n % 3u == 0 ? 1.0 - Unit() * 0x1p-30 : Unit();
        double next = nextafter(t, 2.0);
        double step = mtwnMix((double)a, (double)b, next) - mtwnMix((double)a, (double)b, t);
        backwards += (b > a && step < 0.0) || (b < a && step > 0.0);
    }
    CHECK(backwards == 0, "a step forward in t never moves back from b");
}

// A float of either sign, a power of two from 2^-40 to 2^40.
static float RandomPower(void)
{
    float v = ldexpf(1.0f, -40 + (int)(Next() % 81u));
    return (Next() & 1u) != 0 ? -v : v;
}

static void TestWithinEnds(void)
{
    // Float ends held in doubles: b - a is exact within 2^29 of each
    // other, and further apart t (b - a) stays a spacing short of the
    // rounded difference, so for t in [0, 1] the mix never leaves its
    // ends, as P0811 clamps for doubles in general.
    uint32_t outside = 0;
    for (uint32_t n = 0; n < 400000u; ++n)
    {
        float a = n % 7u == 0 ? RandomPower() : RandomFloat();
        float b = RandomFloat();
        switch (n % 4u)
        {
        case 0:
            b = nextafterf(a, (Next() & 1u) != 0 ? INFINITY : -INFINITY);
            break;
        case 1:
            b = fabsf(a) < 1.0f ? a * 1e30f : a * 1e-30f;
            break;
        case 2:
            b = n % 3u == 0 ? RandomPower() : b;
            break;
        default:
            break;
        }
        double t = 0.0;
        switch (n % 5u)
        {
        case 0:
            t = 1.0 - (double)(1u + Next() % 64u) * 0x1p-53;
            break;
        case 1:
            t = 1.0 - (double)(1u + Next() % 4u) * 0x1p-24;
            break;
        case 2:
            t = 1.0 - Unit() * 0x1p-30;
            break;
        default:
            t = Unit();
            break;
        }
        double m = mtwnMix((double)a, (double)b, t);
        double low = a < b ? (double)a : (double)b;
        double high = a < b ? (double)b : (double)a;
        float held = (float)m;
        outside += m < low || m > high || held < fminf(a, b) || held > fmaxf(a, b);
    }
    CHECK(outside == 0, "for t in [0, 1] within its ends, in double and as a float");
}

static void TestNearEndsFarOut(void)
{
    // One ulp apart, eased 8 million times past them, where the line
    // crosses 0: the weighted sum (1 - t) a + t b erred by about 2^-18 of
    // the result.
    float a = -0x1.4d004ep-4f;
    float b = -0x1.4d005p-4f;
    double t = -0x1.4d24bd1d49c0fp+23;
    double r = 0.0;
    double change = 0.0;
    double error =
        fabs(ErrorOf(mtwnMix((double)a, (double)b, t), (double)a, (double)b, t, &r, &change));
    CHECK(error <= s_u * (fabs(r) + change) && error <= 0x1p-40 * fabs(r),
          "two roundings, b - a exact: within 2^-40 of the result");
}

int main(void)
{
    TestBound();
    TestMonotone();
    TestWithinEnds();
    TestNearEndsFarOut();
    return s_failures == 0 ? 0 : 1;
}
