// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CSS Easing Level 2's curves:
// cubic-bezier against bisection in long double where the platform has
// one wider than double, its end tangents in every case the
// specification lists, steps and linear() on cases worked by hand from
// the specification's algorithms, and the modes and before flag on them.

#include "test_harness.h"

#include "maul-tween/easing.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
// The bisection's own error is far below the bounds.
static const double s_scale = 1.0;
static const double s_slack = 0.0;
#else
typedef double Wide;
// A double bisection carries x's rounding into y, magnified by the slope
// and, where the slope of x vanishes, by the conditioning that sets the
// bounds. The library's results are the same bits everywhere, so the
// tight bounds, held where long double is wider, hold here too.
static const double s_scale = 16.0;
static const double s_slack = 1e-13;
#endif

// y at x on cubic-bezier(x1, y1, x2, y2), by bisecting x(t) = x.
static double BezierWide(const double c[4], double xd)
{
    Wide x1 = (Wide)c[0];
    Wide y1 = (Wide)c[1];
    Wide x2 = (Wide)c[2];
    Wide y2 = (Wide)c[3];
    Wide x = (Wide)xd;
    Wide low = 0;
    Wide high = 1;
    for (int i = 0; i < 90; ++i)
    {
        Wide t = (low + high) / 2;
        Wide u = 1 - t;
        Wide at = 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t;
        if (at < x)
        {
            low = t;
        }
        else
        {
            high = t;
        }
    }
    Wide t = (low + high) / 2;
    Wide u = 1 - t;
    return (double)(3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t);
}

// A linear() input not given, and a missing value, in double.
#define NONE ((double)NAN)

typedef struct BezierCase
{
    double control[4];
    double bound;
} BezierCase;

// The eight curves the solver was measured on: the
// four keywords, a back-like overshoot, and three whose slope vanishes or
// nearly does, where conditioning sets the floor; then two whose x slope
// vanishes at an end, where a Newton step from t = x leaves the bracket.
static const BezierCase s_beziers[] = {
    {{0.25, 0.1, 0.25, 1.0}, 3e-14},     {{0.42, 0.0, 1.0, 1.0}, 2.5e-14},
    {{0.0, 0.0, 0.58, 1.0}, 2.5e-14},    {{0.42, 0.0, 0.58, 1.0}, 2.5e-14},
    {{0.68, -0.55, 0.265, 1.55}, 7e-14}, {{0.0, 1.0, 1.0, 0.0}, 4e-13},
    {{1.0, 0.0, 0.0, 1.0}, 3.5e-7},      {{0.05, 0.7, 0.1, 1.0}, 2e-13},
    {{1.0, 0.0, 1.0, 1.0}, 2.5e-13},     {{0.0, 0.0, 0.0, 1.0}, 2.5e-13},
};

static void TestBezierAgainstBisection(void)
{
    for (size_t c = 0; c < sizeof s_beziers / sizeof s_beziers[0]; ++c)
    {
        const double* q = s_beziers[c].control;
        mtwnEasing easing;
        CHECK(mtwnMakeCubicBezier(q[0], q[1], q[2], q[3], &easing) == mtwn_success, "made");
        double worst = 0.0;
        for (int i = 1; i < 20000; ++i)
        {
            double x = (double)i / 20000.0;
            double error = fabs(mtwnEase(&easing, x, false) - BezierWide(q, x));
            worst = error > worst ? error : worst;
        }
        double allowed = s_beziers[c].bound * s_scale + s_slack;
        if (!(worst <= allowed))
        {
            printf("  bezier %zu: %.3g\n", c, worst);
        }
        CHECK(worst <= allowed, "a bezier within its bound of bisection");
        CHECK(mtwnEase(&easing, 0.0, false) == 0.0 && mtwnEase(&easing, 1.0, false) == 1.0,
              "a bezier is exactly 0 and 1 at its ends");
    }
}

static double Ease(double x1, double y1, double x2, double y2, double x)
{
    mtwnEasing easing;
    if (mtwnMakeCubicBezier(x1, y1, x2, y2, &easing) != mtwn_success)
    {
        return NONE;
    }
    return mtwnEase(&easing, x, false);
}

static void TestBezierTangents(void)
{
    // Below 0: through P1 when x1 > 0, else P2 when x2 > 0, else 0.
    CHECK(Ease(0.5, 0.25, 0.75, 1.0, -0.5) == -0.25, "below 0 through P1");
    CHECK(Ease(0.0, 0.3, 0.5, 1.0, -0.5) == -1.0, "below 0 through P2 when x1 is 0");
    CHECK(Ease(0.0, 0.3, 0.0, 0.7, -0.5) == 0.0, "flat at 0 when both x are 0");
    // Above 1: through P2 when x2 < 1, else P1 when x1 < 1, else 1.
    CHECK(Ease(0.25, 0.0, 0.5, 0.75, 1.5) == 1.25, "above 1 through P2");
    CHECK(Ease(0.5, 0.0, 1.0, 0.3, 1.5) == 2.0, "above 1 through P1 when x2 is 1");
    CHECK(Ease(1.0, 0.3, 1.0, 0.7, 1.5) == 1.0, "flat at 1 when both x are 1");
    // An overshooting curve keeps its slope beyond the ends.
    double slope = -0.55 / 0.68;
    CHECK(fabs(Ease(0.68, -0.55, 0.265, 1.55, -0.25) - slope * -0.25) <= 1e-16,
          "an overshoot's start tangent");
}

static void TestBezierMonotonicAndRefusals(void)
{
    mtwnEasing easing;
    int wrong = 0;
    for (size_t c = 0; c < sizeof s_beziers / sizeof s_beziers[0]; ++c)
    {
        const double* q = s_beziers[c].control;
        CHECK(mtwnMakeCubicBezier(q[0], q[1], q[2], q[3], &easing) == mtwn_success, "made");
        bool expected = q[1] >= 0.0 && q[1] <= 1.0 && q[3] >= 0.0 && q[3] <= 1.0;
        wrong += mtwnIsEasingMonotonic(&easing) != expected;
        if (!expected)
        {
            continue;
        }
        double previous = mtwnEase(&easing, -0.5, false);
        for (int i = -50000; i <= 150000; ++i)
        {
            double value = mtwnEase(&easing, (double)i / 100000.0, false);
            wrong += value < previous;
            previous = value;
        }
    }
    CHECK(wrong == 0, "a bezier with both y in [0, 1] never decreases, tangents included");
    // The rule is y in [0, 1] for both points, either end of the range.
    CHECK(mtwnMakeCubicBezier(0.3, 1.2, 0.7, 0.9, &easing) == mtwn_success &&
              !mtwnIsEasingMonotonic(&easing),
          "y1 above 1");
    CHECK(mtwnMakeCubicBezier(0.3, 0.2, 0.7, -0.1, &easing) == mtwn_success &&
              !mtwnIsEasingMonotonic(&easing),
          "y2 below 0");
    easing = mtwnLinearEasing();
    CHECK(mtwnMakeCubicBezier(-0.1, 0.0, 0.5, 1.0, &easing) == mtwn_errorInvalid, "x1 below 0");
    CHECK(mtwnMakeCubicBezier(0.1, 0.0, 1.1, 1.0, &easing) == mtwn_errorInvalid, "x2 above 1");
    CHECK(mtwnMakeCubicBezier(0.1, 0.0, -0.1, 1.0, &easing) == mtwn_errorInvalid, "x2 below 0");
    CHECK(mtwnMakeCubicBezier(1.1, 0.0, 0.5, 1.0, &easing) == mtwn_errorInvalid, "x1 above 1");
    CHECK(mtwnMakeCubicBezier(NONE, 0.0, 0.5, 1.0, &easing) == mtwn_errorInvalid, "NaN x");
    CHECK(mtwnMakeCubicBezier(0.1, HUGE_VAL, 0.5, 1.0, &easing) == mtwn_errorInvalid,
          "infinite y1");
    CHECK(mtwnMakeCubicBezier(0.1, 0.0, 0.5, NONE, &easing) == mtwn_errorInvalid, "NaN y2");
    CHECK(mtwnMakeCubicBezier(0.1, 0.0, 0.5, 1.0, NULL) == mtwn_errorInvalid, "null out");
    CHECK(easing.kind == mtwn_easingLinear, "refusals leave the out-parameter as it was");
}

static void TestKeywords(void)
{
    static const double expected[4][4] = {{0.25, 0.1, 0.25, 1.0},
                                          {0.42, 0.0, 1.0, 1.0},
                                          {0.0, 0.0, 0.58, 1.0},
                                          {0.42, 0.0, 0.58, 1.0}};
    mtwnEasing easing;
    int wrong = 0;
    for (int k = 0; k < 4; ++k)
    {
        wrong += mtwnMakeCssEasing((mtwnCssKeyword)(mtwn_cssEase + k), &easing) != mtwn_success;
        for (int i = 0; i <= 100; ++i)
        {
            double x = (double)i / 100.0;
            wrong += mtwnEase(&easing, x, false) !=
                     Ease(expected[k][0], expected[k][1], expected[k][2], expected[k][3], x);
        }
    }
    CHECK(wrong == 0, "ease, ease-in, ease-out and ease-in-out are their beziers");
    CHECK(mtwnMakeCssEasing(mtwn_cssLinear, &easing) == mtwn_success &&
              easing.kind == mtwn_easingLinear && mtwnEase(&easing, 1.5, false) == 1.5,
          "linear");
    CHECK(mtwnMakeCssEasing(mtwn_cssStepStart, &easing) == mtwn_success &&
              mtwnEase(&easing, 0.5, false) == 1.0 && mtwnEase(&easing, 0.0, true) == 0.0,
          "step-start is steps(1, jump-start)");
    CHECK(mtwnMakeCssEasing(mtwn_cssStepEnd, &easing) == mtwn_success &&
              mtwnEase(&easing, 0.5, false) == 0.0 && mtwnEase(&easing, 1.0, false) == 1.0,
          "step-end is steps(1, jump-end)");
    CHECK(mtwnMakeCssEasing(7, &easing) == mtwn_errorInvalid, "an unknown keyword");
    CHECK(mtwnMakeCssEasing(mtwn_cssEase, NULL) == mtwn_errorInvalid, "null out");
}

typedef struct StepCase
{
    uint32_t count;
    mtwnStepPosition position;
    double progress;
    bool before;
    double expected;
} StepCase;

// Worked by hand from CSS Easing Level 2's seven steps.
static const StepCase s_steps[] = {
    {4, mtwn_jumpEnd, 0.0, false, 0.0},        {4, mtwn_jumpEnd, 0.24, false, 0.0},
    {4, mtwn_jumpEnd, 0.25, false, 0.25},      {4, mtwn_jumpEnd, 0.25, true, 0.0},
    {4, mtwn_jumpEnd, 0.999, false, 0.75},     {4, mtwn_jumpEnd, 1.0, false, 1.0},
    {4, mtwn_jumpEnd, -0.1, false, -0.25},     {4, mtwn_jumpEnd, 1.3, false, 1.25},
    {4, mtwn_jumpStart, 0.0, false, 0.25},     {4, mtwn_jumpStart, 0.0, true, 0.0},
    {4, mtwn_jumpStart, -0.1, true, 0.0},      {4, mtwn_jumpStart, -0.25, true, -0.25},
    {4, mtwn_jumpStart, 0.5, false, 0.75},     {4, mtwn_jumpStart, 1.0, false, 1.0},
    {5, mtwn_jumpNone, 0.0, false, 0.0},       {5, mtwn_jumpNone, 0.2, false, 0.25},
    {5, mtwn_jumpNone, 0.99, false, 1.0},      {5, mtwn_jumpNone, 1.0, false, 1.0},
    {3, mtwn_jumpBoth, 0.0, false, 0.25},      {3, mtwn_jumpBoth, 0.0, true, 0.0},
    {3, mtwn_jumpBoth, 1.0 / 3.0, false, 0.5}, {3, mtwn_jumpBoth, 1.0, false, 1.0},
    {3, mtwn_jumpBoth, 1.5, false, 1.25},      {1, mtwn_jumpEnd, 0.5, false, 0.0},
};

static void TestSteps(void)
{
    for (size_t i = 0; i < sizeof s_steps / sizeof s_steps[0]; ++i)
    {
        const StepCase* c = &s_steps[i];
        mtwnEasing easing;
        bool made = mtwnMakeSteps(c->count, c->position, &easing) == mtwn_success;
        double value = made ? mtwnEase(&easing, c->progress, c->before) : NONE;
        if (value != c->expected)
        {
            printf("  steps case %zu: %.17g\n", i, value);
        }
        CHECK(value == c->expected, "steps as CSS defines them");
    }
    mtwnEasing easing = mtwnLinearEasing();
    CHECK(mtwnMakeSteps(0, mtwn_jumpEnd, &easing) == mtwn_errorInvalid, "no steps");
    CHECK(mtwnMakeSteps(1, mtwn_jumpNone, &easing) == mtwn_errorInvalid, "one step, jump-none");
    CHECK(mtwnMakeSteps(2, 4, &easing) == mtwn_errorInvalid, "an unknown position");
    CHECK(mtwnMakeSteps(2, mtwn_jumpEnd, NULL) == mtwn_errorInvalid, "null out");
    CHECK(easing.kind == mtwn_easingLinear, "refusals leave the out-parameter as it was");
    CHECK(mtwnMakeSteps(2, mtwn_jumpNone, &easing) == mtwn_success &&
              mtwnIsEasingMonotonic(&easing),
          "steps are monotonic");
}

static bool SameInputs(const mtwnLinearPoint* points, const double* inputs, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (points[i].input != inputs[i])
        {
            return false;
        }
    }
    return true;
}

static void TestLinearCanonicalization(void)
{
    mtwnEasing easing;
    mtwnLinearPoint even[] = {{NONE, 0.0}, {NONE, 0.25}, {NONE, 1.0}};
    CHECK(mtwnMakeLinearEasing(even, 3, &easing) == mtwn_success, "linear(0, 0.25, 1)");
    CHECK(SameInputs(even, (const double[]){0.0, 0.5, 1.0}, 3), "inputs 0, 1/2 and 1");
    CHECK(mtwnEase(&easing, 0.25, false) == 0.125 && mtwnEase(&easing, 0.75, false) == 0.625,
          "interpolated between its points");
    mtwnLinearPoint raised[] = {{0.0, 0.0}, {0.6, 0.5}, {0.4, 0.8}, {1.0, 1.0}};
    CHECK(mtwnMakeLinearEasing(raised, 4, &easing) == mtwn_success, "a decreasing input");
    CHECK(SameInputs(raised, (const double[]){0.0, 0.6, 0.6, 1.0}, 4), "raised to the largest");
    CHECK(mtwnEase(&easing, 0.6, false) == 0.8 && mtwnEase(&easing, 0.3, false) == 0.25,
          "the jump takes the later point");
    // Outputs of opposite signs near the double range: their difference
    // overflows, the value between them does not: no -inf at the second
    // point.
    mtwnLinearPoint wide[] = {{0.0, 1e308}, {1.0, -1e308}};
    CHECK(mtwnMakeLinearEasing(wide, 2, &easing) == mtwn_success, "a wide linear()");
    CHECK(mtwnEase(&easing, 1.0, false) == -1e308 && mtwnEase(&easing, 0.0, false) == 1e308 &&
              mtwnEase(&easing, 0.5, false) == 0.0,
          "each point at its input, and finite between");
    mtwnLinearPoint run[] = {{0.0, 0.0}, {NONE, 0.2}, {NONE, 0.4}, {0.9, 1.0}};
    CHECK(mtwnMakeLinearEasing(run, 4, &easing) == mtwn_success, "a run without inputs");
    CHECK(fabs(run[1].input - 0.3) <= 1e-16 && fabs(run[2].input - 0.6) <= 1e-16,
          "spread evenly between its neighbors");
    // A run after a point past 0: spread over the span from it, a third
    // and two thirds of the way from 0.5 to 1.
    mtwnLinearPoint later[] = {{0.0, 0.0}, {0.5, 0.5}, {NONE, 0.6}, {NONE, 0.7}, {1.0, 1.0}};
    CHECK(mtwnMakeLinearEasing(later, 5, &easing) == mtwn_success, "a run after 0.5");
    CHECK(fabs(later[2].input - 2.0 / 3.0) <= 1e-15 && fabs(later[3].input - 5.0 / 6.0) <= 1e-15,
          "spread from its neighbor past 0");
    mtwnLinearPoint late[] = {{NONE, 0.0}, {1.5, 1.0}, {NONE, 0.5}};
    CHECK(mtwnMakeLinearEasing(late, 3, &easing) == mtwn_success, "an input past 1");
    CHECK(SameInputs(late, (const double[]){0.0, 1.5, 1.5}, 3), "the last is at least it");
    CHECK(mtwnEase(&easing, 2.0, false) == 0.5, "past equal last inputs, the last output");
    mtwnLinearPoint early[] = {{-0.5, -1.0}, {NONE, 1.0}};
    CHECK(mtwnMakeLinearEasing(early, 2, &easing) == mtwn_success, "a first input below 0");
    CHECK(SameInputs(early, (const double[]){-0.5, 1.0}, 2), "kept, and the last is 1");
    CHECK(fabs(mtwnEase(&easing, -1.0, false) - (-1.0 - 0.5 * 2.0 / 1.5)) <= 1e-15,
          "extended along the first segment");
    mtwnLinearPoint ends[] = {{NONE, 0.0}, {NONE, 1.0}};
    CHECK(mtwnMakeLinearEasing(ends, 2, &easing) == mtwn_success &&
              mtwnEase(&easing, 2.0, false) == 2.0 && mtwnEase(&easing, -1.0, true) == -1.0,
          "linear(0, 1) extends both ways");
    mtwnLinearPoint flat[] = {{0.0, 0.0}, {1.0, 0.5}, {1.0, 1.0}};
    CHECK(mtwnMakeLinearEasing(flat, 3, &easing) == mtwn_success &&
              mtwnEase(&easing, 1.0, false) == 1.0 && mtwnEase(&easing, 3.0, false) == 1.0,
          "equal inputs at the end give the later output");
}

static void TestLinearRefusalsAndMonotonic(void)
{
    mtwnEasing easing = mtwnLinearEasing();
    mtwnLinearPoint one[] = {{NONE, 0.0}};
    CHECK(mtwnMakeLinearEasing(one, 1, &easing) == mtwn_errorInvalid, "one point");
    CHECK(mtwnMakeLinearEasing(NULL, 2, &easing) == mtwn_errorInvalid, "no points");
    mtwnLinearPoint bad[] = {{NONE, 0.0}, {NONE, 0.5}, {NONE, NONE}};
    CHECK(mtwnMakeLinearEasing(bad, 3, &easing) == mtwn_errorInvalid, "a NaN output");
    CHECK(isnan(bad[0].input) && isnan(bad[1].input), "a refusal leaves the points as they were");
    mtwnLinearPoint infinite[] = {{0.0, 0.0}, {HUGE_VAL, 1.0}};
    CHECK(mtwnMakeLinearEasing(infinite, 2, &easing) == mtwn_errorInvalid, "an infinite input");
    mtwnLinearPoint fine[] = {{NONE, 0.0}, {NONE, 1.0}};
    CHECK(mtwnMakeLinearEasing(fine, 2, NULL) == mtwn_errorInvalid, "null out");
    CHECK(easing.kind == mtwn_easingLinear, "refusals leave the out-parameter as it was");
    mtwnLinearPoint rising[] = {{NONE, 0.0}, {NONE, 0.5}, {NONE, 0.5}, {NONE, 1.0}};
    CHECK(mtwnMakeLinearEasing(rising, 4, &easing) == mtwn_success &&
              mtwnIsEasingMonotonic(&easing),
          "outputs that never decrease are monotonic");
    mtwnLinearPoint dipping[] = {{NONE, 0.0}, {NONE, 0.6}, {NONE, 0.5}, {NONE, 1.0}};
    CHECK(mtwnMakeLinearEasing(dipping, 4, &easing) == mtwn_success &&
              !mtwnIsEasingMonotonic(&easing),
          "a dip is not");
}

static void TestModesAndTheBeforeFlag(void)
{
    mtwnEasing bezier;
    CHECK(mtwnMakeCssEasing(mtwn_cssEaseIn, &bezier) == mtwn_success, "ease-in");
    mtwnEasing out = bezier;
    CHECK(mtwnSetEaseMode(&out, mtwn_easeOut) == mtwn_success, "out");
    int wrong = 0;
    for (int i = -256; i <= 1280; ++i)
    {
        double p = (double)i / 1024.0;
        wrong += mtwnEase(&out, p, false) != 1.0 - mtwnEase(&bezier, 1.0 - p, false);
    }
    CHECK(wrong == 0, "out is 1 - in(1 - p) for CSS curves too, outside 0 to 1 included");
    CHECK(mtwnEase(&bezier, 1.5, false) == 1.0 + 0.5 * (0.0 - 1.0) / (0.42 - 1.0),
          "CSS curves are not clamped");
    mtwnEasing steps;
    CHECK(mtwnMakeSteps(4, mtwn_jumpStart, &steps) == mtwn_success, "steps");
    CHECK(mtwnEase(&steps, 0.0, true) == 0.0 && mtwnEase(&steps, 0.0, false) == 0.25,
          "in: the before flag reaches the curve");
    CHECK(mtwnSetEaseMode(&steps, mtwn_easeInOut) == mtwn_success, "in-out");
    CHECK(mtwnEase(&steps, 0.0, true) == 0.0 && mtwnEase(&steps, 0.0, false) == 0.125,
          "in-out: the first half samples progress unreflected");
    CHECK(mtwnSetEaseMode(&steps, mtwn_easeOut) == mtwn_success, "out");
    CHECK(mtwnEase(&steps, 1.0, true) == 0.75 && mtwnEase(&steps, 1.0, false) == 0.75,
          "out: the reflected curve never sees it, and starts at 1/4 as jump-start does");
    CHECK(mtwnSetEaseMode(&steps, mtwn_easeOutIn) == mtwn_success, "out-in");
    CHECK(mtwnEase(&steps, 0.5, true) == 0.5 && mtwnEase(&steps, 0.5, false) == 0.625,
          "out-in: the second half samples progress unreflected");
}

int main(void)
{
    TestBezierAgainstBisection();
    TestBezierTangents();
    TestBezierMonotonicAndRefusals();
    TestKeywords();
    TestSteps();
    TestLinearCanonicalization();
    TestLinearRefusalsAndMonotonic();
    TestModesAndTheBeforeFlag();
    return s_failures == 0 ? 0 : 1;
}
