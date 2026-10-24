// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Sampling: even tables are the easing rounded to
// float, and a fitted linear() stays within its tolerance of the easing
// on a grid of 100,001 progresses the fit never probed, read through the
// library's own linear().

#include "test_harness.h"

#include "maul-tween/easing.h"
#include "maul-tween/sample.h"

#include <math.h>
#include <stdint.h>

enum
{
    s_capacity = 4096,
    s_grid = 100000,
};

static mtwnLinearPoint s_points[s_capacity];

static void TestEvenTables(void)
{
    mtwnEasing quad;
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &quad) == mtwn_success, "quad");
    float values[6];
    values[5] = -7.0f;
    CHECK(mtwnSampleEasing(&quad, 5, values) == mtwn_success, "sampled");
    CHECK(values[0] == 0.0f && values[1] == 0.0625f && values[2] == 0.25f && values[3] == 0.5625f &&
              values[4] == 1.0f,
          "p^2 at quarters");
    CHECK(values[5] == -7.0f, "nothing written past the count");
    // Outside the before phase: a jump at the start is taken at 0.
    mtwnEasing steps;
    CHECK(mtwnMakeSteps(4, mtwn_jumpStart, &steps) == mtwn_success, "steps");
    CHECK(mtwnSampleEasing(&steps, 5, values) == mtwn_success && values[0] == 0.25f &&
              values[1] == 0.5f && values[2] == 0.75f && values[3] == 1.0f && values[4] == 1.0f,
          "jump-start steps from their first step");
    mtwnEasing elastic;
    CHECK(mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &elastic) == mtwn_success, "elastic");
    float table[257];
    CHECK(mtwnSampleEasing(&elastic, 257, table) == mtwn_success, "sampled");
    int wrong = 0;
    for (int i = 0; i <= 256; ++i)
    {
        wrong += table[i] != (float)mtwnEase(&elastic, (double)i / 256.0, false);
    }
    CHECK(wrong == 0 && table[0] == 0.0f && table[256] == 1.0f, "the easing, rounded to float");
    CHECK(mtwnSampleEasing(&quad, 2, values) == mtwn_success && values[0] == 0.0f &&
              values[1] == 1.0f,
          "two samples, the ends");
    CHECK(mtwnSampleEasing(&quad, 1, values) == mtwn_errorInvalid, "one sample");
    CHECK(mtwnSampleEasing(NULL, 5, values) == mtwn_errorInvalid, "no easing");
    CHECK(mtwnSampleEasing(&quad, 5, NULL) == mtwn_errorInvalid, "no buffer");
}

// The largest error of the fitted points against the easing on the grid,
// and whether the points are in order, on the curve, from 0 to 1.
static double FitError(const mtwnEasing* easing, uint32_t count, bool* shapeOut)
{
    bool shape = count >= 2 && s_points[0].input == 0.0 && s_points[count - 1].input == 1.0;
    for (uint32_t i = 0; i < count; ++i)
    {
        shape = shape && s_points[i].output == mtwnEase(easing, s_points[i].input, false);
        shape = shape && (i == 0 || s_points[i].input > s_points[i - 1].input);
    }
    *shapeOut = shape;
    mtwnEasing fitted;
    if (mtwnMakeLinearEasing(s_points, count, &fitted) != mtwn_success)
    {
        *shapeOut = false;
        return HUGE_VAL;
    }
    double worst = 0.0;
    for (int i = 0; i <= s_grid; ++i)
    {
        double p = (double)i / s_grid;
        double error = fabs(mtwnEase(&fitted, p, false) - mtwnEase(easing, p, false));
        worst = error > worst ? error : worst;
    }
    return worst;
}

static void TestFitsWithinTolerance(void)
{
    mtwnEasing easings[7];
    int n = 0;
    bool made = mtwnMakePowerEasing(mtwn_easeIn, 2.0, &easings[n++]) == mtwn_success;
    made = made && mtwnMakeCssEasing(mtwn_cssEase, &easings[n++]) == mtwn_success;
    made = made && mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &easings[n++]) == mtwn_success;
    made =
        made && mtwnMakeBackEasing(mtwn_easeInOut, 1.70158 * 1.525, &easings[n++]) == mtwn_success;
    made = made &&
           mtwnMakePennerEasing(mtwn_easingBounce, mtwn_easeOut, &easings[n++]) == mtwn_success;
    made =
        made && mtwnMakePennerEasing(mtwn_easingCirc, mtwn_easeIn, &easings[n++]) == mtwn_success;
    made = made && mtwnMakeCubicBezier(0.0, 1.0, 1.0, 0.0, &easings[n++]) == mtwn_success;
    CHECK(made, "the easings");
    static const double tolerances[] = {1e-2, 1e-3, 1e-4};
    for (int e = 0; e < n; ++e)
    {
        for (int t = 0; t < 3; ++t)
        {
            uint32_t count = 0;
            mtwnResult fit =
                mtwnFitLinearEasing(&easings[e], tolerances[t], s_points, s_capacity, &count);
            bool shape = false;
            double error = fit == mtwn_success ? FitError(&easings[e], count, &shape) : HUGE_VAL;
            if (!(error <= tolerances[t]) || !shape)
            {
                printf("  easing %d at %g: %u points, error %.3g\n", e, tolerances[t], count,
                       error);
            }
            CHECK(fit == mtwn_success && shape, "points in order on the curve, from 0 to 1");
            CHECK(error <= tolerances[t], "a fit within its tolerance between its probes too");
        }
    }
}

// A bump only the last probe of the first segment sees: on the chord
// everywhere but from 0.96 to 0.98, where 32/33 = 0.9697 lies.
static void TestFitLastProbe(void)
{
    static mtwnLinearPoint bump[5] = {
        {0.0, 0.0}, {0.96, 0.96}, {0.97, 1.5}, {0.98, 0.98}, {1.0, 1.0}};
    mtwnEasing easing;
    CHECK(mtwnMakeLinearEasing(bump, 5, &easing) == mtwn_success, "a bump");
    uint32_t count = 0;
    CHECK(mtwnFitLinearEasing(&easing, 1e-3, s_points, s_capacity, &count) == mtwn_success,
          "fitted");
    bool shape = false;
    double error = FitError(&easing, count, &shape);
    CHECK(count > 2 && shape && error <= 1e-3, "the bump kept, within the tolerance");
}

static void TestFitCounts(void)
{
    uint32_t count = 0;
    mtwnEasing linear = mtwnLinearEasing();
    CHECK(mtwnFitLinearEasing(&linear, 1e-6, s_points, s_capacity, &count) == mtwn_success &&
              count == 2,
          "linear is two points");
    mtwnEasing ease;
    CHECK(mtwnMakeCssEasing(mtwn_cssEase, &ease) == mtwn_success, "ease");
    CHECK(mtwnFitLinearEasing(&ease, 1e-3, NULL, 0, &count) == mtwn_errorCapacity && count > 2,
          "no buffer reports the count");
    uint32_t needed = count;
    mtwnLinearPoint guarded[65];
    guarded[needed - 1] = (mtwnLinearPoint){-7.0, -7.0};
    CHECK(needed <= 64 &&
              mtwnFitLinearEasing(&ease, 1e-3, guarded, needed - 1, &count) == mtwn_errorCapacity &&
              count == needed,
          "one short reports the same count");
    CHECK(guarded[needed - 1].input == -7.0 && guarded[needed - 1].output == -7.0,
          "and writes nothing past the capacity");
    mtwnLinearPoint exact[64];
    CHECK(needed <= 64 && mtwnFitLinearEasing(&ease, 1e-3, exact, needed, &count) == mtwn_success,
          "an exact buffer fits");
    CHECK(mtwnFitLinearEasing(&ease, 1e-3, s_points, s_capacity, &count) == mtwn_success,
          "a large one too");
    int wrong = 0;
    for (uint32_t i = 0; i < needed; ++i)
    {
        wrong += exact[i].input != s_points[i].input || exact[i].output != s_points[i].output;
    }
    CHECK(wrong == 0, "the same points whatever the capacity");
    // Fewer points for a looser tolerance.
    uint32_t loose = 0;
    uint32_t tight = 0;
    CHECK(mtwnFitLinearEasing(&ease, 1e-2, NULL, 0, &loose) == mtwn_errorCapacity &&
              mtwnFitLinearEasing(&ease, 1e-4, NULL, 0, &tight) == mtwn_errorCapacity &&
              loose < needed && needed < tight,
          "the count grows as the tolerance shrinks");
    // A tolerance below the curve's rounding is never met: every segment
    // would go down to 2^-24, about 10^8 points; the fit stops at its
    // limit (found by the values' fuzz, minutes of work before).
    count = 7;
    CHECK(mtwnFitLinearEasing(&ease, 1e-300, NULL, 0, &count) == mtwn_errorLimit && count == 0,
          "an unmeetable tolerance stops at the limit");
    CHECK(mtwnFitLinearEasing(&ease, 1e-300, s_points, s_capacity, &count) == mtwn_errorLimit,
          "whatever the capacity");
}

static void TestToleranceMetExactly(void)
{
    // The first segment's chord is y = x and its probes at k / 33, so the
    // worst miss is computable here exactly as the fit computes it; a
    // tolerance equal to it is met.
    mtwnEasing quad;
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &quad) == mtwn_success, "quad");
    double worst = 0.0;
    for (int k = 1; k <= 32; ++k)
    {
        double share = (double)k / 33.0;
        double miss = fabs(mtwnEase(&quad, share, false) - share);
        worst = miss > worst ? miss : worst;
    }
    uint32_t count = 0;
    CHECK(mtwnFitLinearEasing(&quad, worst, s_points, s_capacity, &count) == mtwn_success &&
              count == 2,
          "a miss equal to the tolerance is within it");
}

static double Jump(void* context, double progress)
{
    (void)context;
    return progress < 0.5 ? 0.0 : 1.0;
}

static void TestJumpsEnd(void)
{
    mtwnEasing jump;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, Jump, NULL, &jump) == mtwn_success, "jump");
    uint32_t count = 0;
    CHECK(mtwnFitLinearEasing(&jump, 1e-3, s_points, s_capacity, &count) == mtwn_success,
          "a jump is fitted");
    mtwnEasing fitted;
    CHECK(count < 300 && mtwnMakeLinearEasing(s_points, count, &fitted) == mtwn_success,
          "in a bounded count");
    double worst = 0.0;
    for (int i = 0; i <= s_grid; ++i)
    {
        double p = (double)i / s_grid;
        if (fabs(p - 0.5) > 0x1.0p-24)
        {
            double error = fabs(mtwnEase(&fitted, p, false) - Jump(NULL, p));
            worst = error > worst ? error : worst;
        }
    }
    CHECK(worst <= 1e-3, "within the tolerance away from the jump");
}

// Infinite in the middle, where its miss makes the fit split, NaN at the
// start, infinite at the end: kept points no linear() holds. (A NaN
// inside never misses most, so it is never kept.)
static double Endless(void* context, double progress)
{
    (void)context;
    return progress > 0.4 && progress < 0.6 ? HUGE_VAL : progress;
}

static double Unbounded(void* context, double progress)
{
    (void)context;
    return progress == 1.0 ? HUGE_VAL : progress;
}

static double Hole(void* context, double progress)
{
    (void)context;
    return progress == 0.0 ? (double)NAN : progress;
}

static void TestFitRefusals(void)
{
    uint32_t count = 7;
    mtwnEasing steps;
    CHECK(mtwnMakeSteps(4, mtwn_jumpEnd, &steps) == mtwn_success, "steps");
    CHECK(mtwnFitLinearEasing(&steps, 1e-3, s_points, s_capacity, &count) == mtwn_errorInvalid,
          "steps are refused");
    mtwnEasing quad;
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &quad) == mtwn_success, "quad");
    CHECK(mtwnFitLinearEasing(&quad, 0.0, s_points, s_capacity, &count) == mtwn_errorInvalid,
          "a zero tolerance");
    CHECK(mtwnFitLinearEasing(&quad, (double)NAN, s_points, s_capacity, &count) ==
              mtwn_errorInvalid,
          "a NaN tolerance");
    CHECK(mtwnFitLinearEasing(&quad, HUGE_VAL, s_points, s_capacity, &count) == mtwn_errorInvalid,
          "an infinite tolerance");
    CHECK(mtwnFitLinearEasing(&quad, 1e-3, NULL, 4, &count) == mtwn_errorInvalid,
          "a capacity without a buffer");
    CHECK(mtwnFitLinearEasing(NULL, 1e-3, s_points, s_capacity, &count) == mtwn_errorInvalid,
          "no easing");
    CHECK(mtwnFitLinearEasing(&quad, 1e-3, s_points, s_capacity, NULL) == mtwn_errorInvalid,
          "no count");
    mtwnEasing endless;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, Endless, NULL, &endless) == mtwn_success, "endless");
    CHECK(mtwnFitLinearEasing(&endless, 1e-3, s_points, s_capacity, &count) == mtwn_errorInvalid,
          "a curve infinite at a kept point");
    mtwnEasing hole;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, Hole, NULL, &hole) == mtwn_success, "hole");
    CHECK(mtwnFitLinearEasing(&hole, 1e-3, s_points, s_capacity, &count) == mtwn_errorInvalid,
          "a curve NaN at a kept point");
    mtwnEasing unbounded;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, Unbounded, NULL, &unbounded) == mtwn_success,
          "unbounded");
    CHECK(mtwnFitLinearEasing(&unbounded, 1e-3, s_points, s_capacity, &count) == mtwn_errorInvalid,
          "a curve infinite at its end");
    CHECK(count == 7, "refusals leave the count as it was");
}

int main(void)
{
    TestEvenTables();
    TestFitsWithinTolerance();
    TestFitLastProbe();
    TestFitCounts();
    TestToleranceMetExactly();
    TestJumpsEnd();
    TestFitRefusals();
    return s_failures == 0 ? 0 : 1;
}
