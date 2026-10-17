// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Inertia (record mtwn-0063): a decay landing on a chosen end keeps its
// start velocity; a bounded decay is the decay until the exact time it
// reaches a bound, then a spring from the bound with the velocity it had
// there, continuous in both; a start past a bound is the spring's.

#include "test_harness.h"

#include "maul-tween/spring.h"

#include <math.h>
#include <stdint.h>

static mtwnDecay Ios(void)
{
    mtwnDecay decay;
    CHECK(mtwnMakeDecayFromDeceleration(0.998, &decay) == mtwn_success, "iOS's decay");
    return decay;
}

static mtwnSpring Bounce(void)
{
    // Motion's bounceStiffness and bounceDamping.
    mtwnSpring spring;
    CHECK(mtwnMakeSpring(500.0, 10.0, 1.0, &spring) == mtwn_success, "a bounce");
    return spring;
}

static void TestLanding(void)
{
    // A fling of 1,500 a second from 40: its natural rest, snapped to the
    // nearest page of 320.
    mtwnDecay natural = Ios();
    double rest = 0.0;
    mtwnSampleDecay(&natural, 40.0, 1500.0, HUGE_VAL, &rest, NULL);
    double page = 320.0 * round(rest / 320.0);
    CHECK(fabs(rest - 789.25) < 1.0 && page == 640.0, "lands near 789, snapped to 640");
    mtwnDecay landing;
    CHECK(mtwnMakeDecayToRest(40.0, 1500.0, page, &landing) == mtwn_success, "a landing");
    double end = 0.0;
    double velocity = 0.0;
    mtwnSampleDecay(&landing, 40.0, 1500.0, HUGE_VAL, &end, NULL);
    mtwnSampleDecay(&landing, 40.0, 1500.0, 0.0, NULL, &velocity);
    CHECK(fabs(end - page) <= 4.0 * 0x1p-52 * page, "comes to rest on the page");
    CHECK(velocity == 1500.0, "with the start's velocity kept");
    // Backwards too.
    CHECK(mtwnMakeDecayToRest(40.0, -900.0, 0.0, &landing) == mtwn_success &&
              landing.rate == 900.0 / 40.0,
          "back to 0");
    int refused = 0;
    refused += mtwnMakeDecayToRest(40.0, 1500.0, 0.0, &landing) == mtwn_errorInvalid;
    refused += mtwnMakeDecayToRest(40.0, 1500.0, 40.0, &landing) == mtwn_errorInvalid;
    refused += mtwnMakeDecayToRest(40.0, 0.0, 640.0, &landing) == mtwn_errorInvalid;
    refused += mtwnMakeDecayToRest((double)NAN, 1500.0, 640.0, &landing) == mtwn_errorInvalid;
    refused += mtwnMakeDecayToRest(40.0, HUGE_VAL, 640.0, &landing) == mtwn_errorInvalid;
    refused += mtwnMakeDecayToRest(40.0, 1500.0, HUGE_VAL, &landing) == mtwn_errorInvalid;
    refused += mtwnMakeDecayToRest(-1e308, 1.0, 1e308, &landing) == mtwn_errorInvalid;
    refused += mtwnMakeDecayToRest(0.0, 1e300, 1e-300, &landing) == mtwn_errorInvalid;
    refused += mtwnMakeDecayToRest(40.0, 1500.0, 640.0, NULL) == mtwn_errorInvalid;
    CHECK(refused == 9, "an end behind, at the start, no velocity, a value not finite, a distance "
                        "or rate overflowing and no decay refused");
}

static mtwnBoundedDecay Bounded(double lower, double upper)
{
    mtwnDecay decay = Ios();
    mtwnSpring spring = Bounce();
    mtwnBoundedDecay bounded;
    CHECK(mtwnMakeBoundedDecay(&decay, &spring, lower, upper, &bounded) == mtwn_success,
          "a bounded decay");
    return bounded;
}

static void TestWithin(void)
{
    // Coming to rest within the bounds, or exactly on one: the decay.
    mtwnBoundedDecay bounded = Bounded(0.0, 1000.0);
    mtwnDecay decay = Ios();
    int differ = 0;
    const double starts[3][2] = {{40.0, 1500.0}, {900.0, -1200.0}, {500.0, 1000.0}};
    for (int s = 0; s < 3; ++s)
    {
        for (int k = 0; k <= 400; ++k)
        {
            double t = k / 100.0;
            double a = 0.0;
            double b = 0.0;
            double va = 0.0;
            double vb = 0.0;
            mtwnSampleBoundedDecay(&bounded, starts[s][0], starts[s][1], t, &a, &va);
            mtwnSampleDecay(&decay, starts[s][0], starts[s][1], t, &b, &vb);
            differ += a != b || va != vb;
        }
    }
    CHECK(differ == 0, "within the bounds, the decay bit for bit");
    CHECK(mtwnBoundedDecayStopTime(&bounded, 40.0, 1500.0, 0.5, 1.0) ==
              mtwnDecayStopTime(&decay, 1500.0, 1.0),
          "and its stop");
    mtwnBoundedDecay open = Bounded(-HUGE_VAL, HUGE_VAL);
    double far = 0.0;
    mtwnSampleBoundedDecay(&open, 0.0, 1e6, 10.0, &far, NULL);
    double plain = 0.0;
    mtwnSampleDecay(&decay, 0.0, 1e6, 10.0, &plain, NULL);
    CHECK(far == plain, "no bounds, no spring");
}

static void TestCrossing(void)
{
    // From 40 at 1,500 a second the decay would rest near 789; the upper
    // bound at 600 is reached first.
    mtwnBoundedDecay bounded = Bounded(0.0, 600.0);
    mtwnDecay decay = Ios();
    double crossing = mtwnDecayCrossingTime(&decay, 40.0, 1500.0, 600.0);
    CHECK(crossing > 0.0 && isfinite(crossing), "a crossing");
    // Up to the crossing the decay; at it the bound exactly.
    double before = 0.0;
    double vBefore = 0.0;
    double at = 0.0;
    double vAt = 0.0;
    double t = nextafter(crossing, 0.0);
    mtwnSampleBoundedDecay(&bounded, 40.0, 1500.0, t, &before, &vBefore);
    mtwnSampleBoundedDecay(&bounded, 40.0, 1500.0, crossing, &at, &vAt);
    double plain = 0.0;
    mtwnSampleDecay(&decay, 40.0, 1500.0, t, &plain, NULL);
    CHECK(before == plain && before < 600.0, "the decay up to it");
    CHECK(at == 600.0, "the bound at it");
    CHECK(fabs(vAt - vBefore) <= 1e-9 * vAt, "the velocity it had there, continuous");
    // The decay's own path reaches the bound there, not a frame later.
    double reached = 0.0;
    mtwnSampleDecay(&decay, 40.0, 1500.0, crossing, &reached, NULL);
    CHECK(fabs(reached - 600.0) <= 1e-12 * 600.0, "the crossing exact");
    // Past it, the spring carries it over and back to the bound.
    double furthest = 0.0;
    for (int k = 1; k <= 200; ++k)
    {
        double p = 0.0;
        mtwnSampleBoundedDecay(&bounded, 40.0, 1500.0, crossing + k / 1000.0, &p, NULL);
        furthest = fmax(furthest, p);
    }
    CHECK(furthest > 600.0 && furthest < 789.0, "over the bound, short of the natural rest");
    double stop = mtwnBoundedDecayStopTime(&bounded, 40.0, 1500.0, 0.5, 1.0);
    int restless = 0;
    for (int k = 0; k <= 100; ++k)
    {
        double p = 0.0;
        double v = 0.0;
        mtwnSampleBoundedDecay(&bounded, 40.0, 1500.0, stop + k / 50.0, &p, &v);
        restless += fabs(p - 600.0) > 0.5 || fabs(v) > 1.0;
    }
    CHECK(stop > crossing && isfinite(stop) && restless == 0, "at rest on the bound from its stop");
    // The lower bound, flung down.
    bounded = Bounded(-100.0, 600.0);
    double low = mtwnDecayCrossingTime(&decay, 40.0, -1500.0, -100.0);
    mtwnSampleBoundedDecay(&bounded, 40.0, -1500.0, low, &at, NULL);
    CHECK(isfinite(low) && at == -100.0, "the lower bound alike");
}

static void TestStartPast(void)
{
    // Dragged past the upper bound and let go: the spring from there,
    // whichever way it moves.
    mtwnBoundedDecay bounded = Bounded(0.0, 600.0);
    mtwnSpring spring = Bounce();
    int differ = 0;
    for (int k = 0; k <= 100; ++k)
    {
        double t = k / 100.0;
        double p = 0.0;
        double offset = 0.0;
        mtwnSampleBoundedDecay(&bounded, 650.0, 300.0, t, &p, NULL);
        mtwnSampleSpring(&spring, 50.0, 300.0, t, &offset, NULL);
        differ += p != 600.0 + offset;
    }
    double start = 0.0;
    mtwnSampleBoundedDecay(&bounded, -20.0, -300.0, 0.0, &start, NULL);
    CHECK(differ == 0 && start == -20.0, "the spring from the start, to the nearer bound");
    CHECK(mtwnBoundedDecayStopTime(&bounded, 650.0, 300.0, 0.5, 1.0) ==
              mtwnSpringSettlingTime(&spring, 50.0, 300.0, 0.5, 1.0),
          "its stop the spring's settling");
    CHECK(isinf(mtwnBoundedDecayStopTime(&bounded, 650.0, 300.0, 0.0, 1.0)) &&
              isinf(mtwnBoundedDecayStopTime(&bounded, 650.0, 300.0, 0.5, (double)NAN)),
          "no stop without tolerances");
}

static void TestRefusals(void)
{
    mtwnDecay decay = Ios();
    mtwnSpring spring = Bounce();
    mtwnBoundedDecay bounded;
    int refused = 0;
    refused += mtwnMakeBoundedDecay(NULL, &spring, 0.0, 1.0, &bounded) == mtwn_errorInvalid;
    refused += mtwnMakeBoundedDecay(&decay, NULL, 0.0, 1.0, &bounded) == mtwn_errorInvalid;
    refused += mtwnMakeBoundedDecay(&decay, &spring, 0.0, 1.0, NULL) == mtwn_errorInvalid;
    refused += mtwnMakeBoundedDecay(&decay, &spring, 2.0, 1.0, &bounded) == mtwn_errorInvalid;
    refused +=
        mtwnMakeBoundedDecay(&decay, &spring, (double)NAN, 1.0, &bounded) == mtwn_errorInvalid;
    refused +=
        mtwnMakeBoundedDecay(&decay, &spring, 0.0, (double)NAN, &bounded) == mtwn_errorInvalid;
    CHECK(refused == 6, "no decay, spring or out, bounds the wrong way round or NaN refused");
    CHECK(mtwnMakeBoundedDecay(&decay, &spring, 5.0, 5.0, &bounded) == mtwn_success,
          "equal bounds, a single resting place");
}

int main(void)
{
    TestLanding();
    TestWithin();
    TestCrossing();
    TestStartPast();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
