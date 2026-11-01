// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A timeline's easing (src/timeline_easing.c): a cubic bezier
// mtwnIsEasingMonotonic calls monotonic never decreases to the bit in
// any mode, where the keyframes' solver steps down; the bisections skip
// only the steps the full ones take; and the curve stays within its bound
// of the definition. White-box: no public call samples a timeline's easing
// but through a clip's child times.

#include "test_harness.h"
#include "timeline_easing.h"
#include "work.h"

#include "maul-tween/easing.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const double s_beziers[][4] = {
    {0.25, 0.1, 0.25, 1.0}, // ease
    {0.42, 0.0, 1.0, 1.0},  // ease-in
    {0.0, 0.0, 0.58, 1.0},  // ease-out
    {0.42, 0.0, 0.58, 1.0}, // ease-in-out
    {1.0, 0.0, 0.0, 1.0},   // a vertical tangent at 1/2, where the solver missed by 2.6e-5
    {0.0, 1.0, 1.0, 0.0},   // a flat one at 1/2
    {0.1, 0.9, 0.9, 0.1},
};
enum
{
    BEZIERS = sizeof s_beziers / sizeof s_beziers[0]
};

static bool s_dense = false;

static mtwnEasing Bezier(int b, mtwnEaseMode mode)
{
    mtwnEasing easing;
    const double* c = s_beziers[b];
    CHECK(mtwnMakeCubicBezierEasing(c[0], c[1], c[2], c[3], &easing) == mtwn_success, "a bezier");
    CHECK(mtwnSetEaseMode(&easing, mode) == mtwn_success, "its mode");
    return easing;
}

// Counts the steps down over progresses in order from a start, each the
// next double or a step of a fixed size.
static int StepsDown(const mtwnEasing* easing, double from, int count, double step)
{
    int down = 0;
    double p = from;
    double previous = mtwnSampleTimelineEasingWork(easing, p, false, NULL);
    for (int i = 0; i < count; ++i)
    {
        p = step > 0.0 ? p + step : nextafter(p, 2.0);
        double value = mtwnSampleTimelineEasingWork(easing, p, false, NULL);
        down += value < previous;
        previous = value;
    }
    return down;
}

// Every bezier in every mode: the doubles next to 0, 1/4, 1/2, 3/4 and 1, a
// sweep from the subnormals to 1/2, and a grid, -1/4 to 5/4.
static void TestNeverDecreases(void)
{
    int down = 0;
    for (int b = 0; b < BEZIERS; ++b)
    {
        for (int mode = mtwn_easeIn; mode <= mtwn_easeOutIn; ++mode)
        {
            mtwnEasing easing = Bezier(b, mode);
            static const double s_centers[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
            for (int c = 0; c < 5; ++c)
            {
                double from = s_centers[c];
                for (int i = 0; i < 300; ++i)
                {
                    from = nextafter(from, -1.0);
                }
                down += StepsDown(&easing, from, 600, 0.0);
            }
            double previous = mtwnSampleTimelineEasingWork(&easing, 0.0, false, NULL);
            for (int e = -1074; e <= -2; ++e)
            {
                for (int j = 0; j < 4; ++j)
                {
                    double value =
                        mtwnSampleTimelineEasingWork(&easing, ldexp(1.0 + j / 4.0, e), false, NULL);
                    down += value < previous;
                    previous = value;
                }
            }
            down += StepsDown(&easing, -0.25, 1500, 1.0 / 1000.0);
        }
    }
    CHECK(down == 0, "a timeline's bezier never decreases");
}

// The skipped steps are the full bisections': the same bits with every
// test evaluated, near the ends, the middle and anywhere.
static void TestSkipsOnlyDecidedSteps(void)
{
    int differ = 0;
    for (int b = 0; b < BEZIERS; ++b)
    {
        const double* c = s_beziers[b];
        for (int i = 1; i < 120; ++i)
        {
            double at[4] = {i / 120.0, ldexp(1.0, -i / 3), 1.0 - ldexp(1.0, -i / 3),
                            0.5 + ldexp(1.0, -i / 2) * (i % 2 ? 1.0 : -1.0)};
            for (int k = 0; k < 4; ++k)
            {
                double skipped = mtwnSampleMonotoneCubicBezier(c, at[k], true);
                double full = mtwnSampleMonotoneCubicBezier(c, at[k], false);
                differ += !(skipped == full);
            }
        }
    }
    CHECK(differ == 0, "the skipping bisections give the full ones' bits");
}

// The bits of a double.
static uint64_t Bits(double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

// The skipping bisections against the full ones at scale, for every curve,
// the flat tangents inside among them, where the bound decides the fewest
// steps: a quarter of the progresses on a
// grid, a quarter by an exponent from each end, the doubles about 1/2,
// and the rest at random (a fixed generator), 300,000 a curve dense and
// 6,000 otherwise. Skipping rests on the error bound; a bound too small
// would skip a step the full bisection takes the other way.
static void TestSkipsMatchFullAtScale(void)
{
    const int count = s_dense ? 300000 : 6000;
    uint64_t state = 0x9E3779B97F4A7C15u;
    int differ = 0;
    for (int b = 0; b < BEZIERS; ++b)
    {
        const double* c = s_beziers[b];
        double half = 0.5;
        for (int i = 0; i < count; ++i)
        {
            double x = 0.0;
            switch (i % 4)
            {
            case 0:
                x = (double)(i / 4 + 1) / (double)(count / 4 + 2);
                break;
            case 1:
            {
                double tiny = ldexp(1.0 + (double)(i % 997) / 997.0, -1 - (i / 4) % 1070);
                x = (i / 4) % 2 ? tiny : 1.0 - tiny;
                break;
            }
            case 2:
                half = (i / 4) % 2 ? nextafter(half, 1.0) : nextafter(0.5 - (half - 0.5), 0.0);
                x = half;
                break;
            default:
                state ^= state << 13;
                state ^= state >> 7;
                state ^= state << 17;
                x = (double)(state >> 11) * 0x1.0p-53;
                break;
            }
            if (!(x > 0.0 && x < 1.0))
            {
                continue;
            }
            differ += Bits(mtwnSampleMonotoneCubicBezier(c, x, true)) !=
                      Bits(mtwnSampleMonotoneCubicBezier(c, x, false));
        }
    }
    if (differ != 0)
    {
        printf("  %d progresses differ\n", differ);
    }
    CHECK(differ == 0, "the skipping bisections give the full ones' bits at scale");
}

// The exact curve at x, for a reference: t by bisection in long double, then
// y(t); in double where long double is no wider, far inside the bound still.
static long double ExactBezier(const double c[4], double x)
{
    long double x1 = (long double)c[0];
    long double y1 = (long double)c[1];
    long double x2 = (long double)c[2];
    long double y2 = (long double)c[3];
    long double low = 0.0L;
    long double high = 1.0L;
    for (int i = 0; i < 200; ++i)
    {
        long double t = 0.5L * (low + high);
        long double u = 1.0L - t;
        long double xt = 3.0L * x1 * u * u * t + 3.0L * x2 * u * t * t + t * t * t;
        if (xt <= (long double)x)
        {
            low = t;
        }
        else
        {
            high = t;
        }
    }
    long double t = 0.5L * (low + high);
    long double u = 1.0L - t;
    return 3.0L * y1 * u * u * t + 3.0L * y2 * u * t * t + t * t * t;
}

// ease's sample within its bound of the curve. The grid's t is within
// 2^-52 + E / x'min of x's, E < 2^-45 the evaluation's error, and the
// value within E and an ulp of y there: for ease x' >= 9/16 (at t = 1/4)
// and |y'| <= 2.7 (3 times its largest control step, 0.9), so within
// 2^-45 + 2^-53 + 2.7 (2^-52 + 2^-45 16 / 9) < 1.7e-13.
static void TestNearTheCurve(void)
{
    const double* ease = s_beziers[0];
    double worst = 0.0;
    for (int i = 1; i < 4000; ++i)
    {
        double x = i / 4000.0;
        double error = fabs((double)((long double)mtwnSampleMonotoneCubicBezier(ease, x, true) -
                                     ExactBezier(ease, x)));
        worst = error > worst ? error : worst;
    }
    CHECK(worst < 1.7e-13, "a timeline's ease within its bound of the curve");
}

int main(int argc, char** argv)
{
    s_dense = argc == 2 && strcmp(argv[1], "dense") == 0;
    if (argc > 2 || (argc == 2 && !s_dense))
    {
        printf("usage: test_timeline_easing [dense]\n");
        return 2;
    }
    TestSkipsOnlyDecidedSteps();
    TestSkipsMatchFullAtScale();
    TestNearTheCurve();
    TestNeverDecreases();
    return s_failures == 0 ? 0 : 1;
}
