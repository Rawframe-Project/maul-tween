// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Inertia: a decay landing on a chosen end keeps its
// start velocity; a bounded decay is the decay until the exact time it
// reaches a bound, then a spring from the bound with the velocity it had
// there, continuous in both; a start past a bound is the spring's.

#include "test_harness.h"
#include "test_values.h"

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
    SampleDecay(&natural, 40.0, 1500.0, HUGE_VAL, &rest, NULL);
    double page = 320.0 * round(rest / 320.0);
    CHECK(fabs(rest - 789.25) < 1.0 && page == 640.0, "lands near 789, snapped to 640");
    mtwnDecay landing;
    CHECK(mtwnMakeDecayToRest(40.0, 1500.0, page, &landing) == mtwn_success, "a landing");
    double end = 0.0;
    double velocity = 0.0;
    SampleDecay(&landing, 40.0, 1500.0, HUGE_VAL, &end, NULL);
    SampleDecay(&landing, 40.0, 1500.0, 0.0, NULL, &velocity);
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
            SampleBoundedDecay(&bounded, starts[s][0], starts[s][1], t, &a, &va);
            SampleDecay(&decay, starts[s][0], starts[s][1], t, &b, &vb);
            differ += a != b || va != vb;
        }
    }
    CHECK(differ == 0, "within the bounds, the decay bit for bit");
    // A rest on the bound where the crossing's share rounds below 1, so its
    // time is finite: still the decay, with no hand-off at the bound.
    mtwnDecay near;
    CHECK(mtwnMakeDecay(0x1.d51e3f4p+2, &near) == mtwn_success, "a decay");
    const double from = 0x1.1376edp+2;
    const double flung = 0x1.e444263333333p+3;
    mtwnBoundedDecay onRest;
    mtwnSpring bounce = Bounce();
    CHECK(mtwnMakeBoundedDecay(&near, &bounce, -1e9, from + flung / near.rate, &onRest) ==
              mtwn_success,
          "its rest the upper bound");
    for (int k = 0; k <= 40; ++k)
    {
        double a = 0.0;
        double b = 0.0;
        double va = 0.0;
        double vb = 0.0;
        SampleBoundedDecay(&onRest, from, flung, k * 0.25, &a, &va);
        SampleDecay(&near, from, flung, k * 0.25, &b, &vb);
        differ += a != b || va != vb;
    }
    CHECK(differ == 0, "a rest on the bound, the decay bit for bit");
    // A rest past the bound by rounding alone, an ulp: the crossing never
    // comes, so it stops as the decay does.
    mtwnDecay ulp;
    CHECK(mtwnMakeDecay(0x1.2f2497p+3, &ulp) == mtwn_success, "a decay");
    mtwnBoundedDecay past;
    CHECK(mtwnMakeBoundedDecay(&ulp, &bounce, -1e9, -0x1.a2c5572e58c5dp-2, &past) == mtwn_success,
          "its rest an ulp past the upper bound");
    CHECK(BoundedDecayStopTime(&past, -0x1.1e67f6p+1, 0x1.15298e199999ap+4, 0.5, 1.0) ==
              DecayStopTime(&ulp, 0x1.15298e199999ap+4, 1.0),
          "the decay's stop");
    CHECK(BoundedDecayStopTime(&bounded, 40.0, 1500.0, 0.5, 1.0) ==
              DecayStopTime(&decay, 1500.0, 1.0),
          "and its stop");
    double stop = 0.0;
    CHECK(mtwnBoundedDecayStopTime(&bounded, 40.0, 1500.0, 0.0, 1.0, &stop) == mtwn_errorInvalid &&
              mtwnBoundedDecayStopTime(&bounded, 40.0, 1500.0, -0.5, 1.0, &stop) ==
                  mtwn_errorInvalid &&
              mtwnBoundedDecayStopTime(&bounded, 40.0, 1500.0, 0.5, 0.0, &stop) ==
                  mtwn_errorInvalid &&
              stop == 0.0,
          "a tolerance not above 0 refused, though this one never needs it");
    mtwnBoundedDecay open = Bounded(-HUGE_VAL, HUGE_VAL);
    double far = 0.0;
    SampleBoundedDecay(&open, 0.0, 1e6, 10.0, &far, NULL);
    double plain = 0.0;
    SampleDecay(&decay, 0.0, 1e6, 10.0, &plain, NULL);
    CHECK(far == plain, "no bounds, no spring");
    // A start at an infinite bound is not finite, refused with the outputs
    // as they were.
    mtwnBoundedDecay below = Bounded(-HUGE_VAL, 600.0);
    double at = 0.0;
    double speed = 0.0;
    CHECK(mtwnSampleBoundedDecay(&below, -HUGE_VAL, 300.0, 0.5, &at, &speed) == mtwn_errorInvalid &&
              mtwnBoundedDecayStopTime(&below, -HUGE_VAL, 300.0, 0.5, 1.0, &stop) ==
                  mtwn_errorInvalid &&
              at == 0.0 && speed == 0.0 && stop == 0.0,
          "a start at an infinite bound refused");
}

static void TestCrossing(void)
{
    // From 40 at 1,500 a second the decay would rest near 789; the upper
    // bound at 600 is reached first.
    mtwnBoundedDecay bounded = Bounded(0.0, 600.0);
    mtwnDecay decay = Ios();
    double crossing = DecayCrossingTime(&decay, 40.0, 1500.0, 600.0);
    CHECK(crossing > 0.0 && isfinite(crossing), "a crossing");
    // Up to the crossing the decay; at it the bound exactly.
    double before = 0.0;
    double vBefore = 0.0;
    double at = 0.0;
    double vAt = 0.0;
    double t = nextafter(crossing, 0.0);
    SampleBoundedDecay(&bounded, 40.0, 1500.0, t, &before, &vBefore);
    SampleBoundedDecay(&bounded, 40.0, 1500.0, crossing, &at, &vAt);
    double plain = 0.0;
    SampleDecay(&decay, 40.0, 1500.0, t, &plain, NULL);
    CHECK(before == plain && before < 600.0, "the decay up to it");
    CHECK(at == 600.0, "the bound at it");
    // The velocity alone, past the handoff, as with the position.
    double vOnly = 0.0;
    double vWith = 0.0;
    double unused = 0.0;
    SampleBoundedDecay(&bounded, 40.0, 1500.0, crossing + 0.1, NULL, &vOnly);
    SampleBoundedDecay(&bounded, 40.0, 1500.0, crossing + 0.1, &unused, &vWith);
    CHECK(vOnly == vWith, "the velocity without the position");
    CHECK(fabs(vAt - vBefore) <= 1e-9 * vAt, "the velocity it had there, continuous");
    // The decay's own path reaches the bound there, not a frame later.
    double reached = 0.0;
    SampleDecay(&decay, 40.0, 1500.0, crossing, &reached, NULL);
    CHECK(fabs(reached - 600.0) <= 1e-12 * 600.0, "the crossing exact");
    // Past it, the spring carries it over and back to the bound.
    double furthest = 0.0;
    for (int k = 1; k <= 200; ++k)
    {
        double p = 0.0;
        SampleBoundedDecay(&bounded, 40.0, 1500.0, crossing + k / 1000.0, &p, NULL);
        furthest = fmax(furthest, p);
    }
    CHECK(furthest > 600.0 && furthest < 789.0, "over the bound, short of the natural rest");
    double stop = BoundedDecayStopTime(&bounded, 40.0, 1500.0, 0.5, 1.0);
    int restless = 0;
    for (int k = 0; k <= 100; ++k)
    {
        double p = 0.0;
        double v = 0.0;
        SampleBoundedDecay(&bounded, 40.0, 1500.0, stop + k / 50.0, &p, &v);
        restless += fabs(p - 600.0) > 0.5 || fabs(v) > 1.0;
    }
    CHECK(stop > crossing && isfinite(stop) && restless == 0, "at rest on the bound from its stop");
    // The lower bound, flung down.
    bounded = Bounded(-100.0, 600.0);
    double low = DecayCrossingTime(&decay, 40.0, -1500.0, -100.0);
    SampleBoundedDecay(&bounded, 40.0, -1500.0, low, &at, NULL);
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
        SampleBoundedDecay(&bounded, 650.0, 300.0, t, &p, NULL);
        SampleSpring(&spring, 50.0, 300.0, t, &offset, NULL);
        differ += p != 600.0 + offset;
    }
    double start = 0.0;
    SampleBoundedDecay(&bounded, -20.0, -300.0, 0.0, &start, NULL);
    CHECK(differ == 0 && start == -20.0, "the spring from the start, to the nearer bound");
    CHECK(BoundedDecayStopTime(&bounded, 650.0, 300.0, 0.5, 1.0) ==
              SpringSettlingTime(&spring, 50.0, 300.0, 0.5, 1.0),
          "its stop the spring's settling");
    double stop = 0.0;
    CHECK(mtwnBoundedDecayStopTime(&bounded, 650.0, 300.0, 0.0, 1.0, &stop) == mtwn_errorInvalid &&
              mtwnBoundedDecayStopTime(&bounded, 650.0, 300.0, 0.5, (double)NAN, &stop) ==
                  mtwn_errorInvalid &&
              stop == 0.0,
          "no tolerance, refused");
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
    refused += mtwnMakeBoundedDecay(&decay, &spring, (double)INFINITY, (double)INFINITY,
                                    &bounded) == mtwn_errorInvalid;
    refused += mtwnMakeBoundedDecay(&decay, &spring, -(double)INFINITY, -(double)INFINITY,
                                    &bounded) == mtwn_errorInvalid;
    CHECK(refused == 8, "no decay, spring or out, bounds the wrong way round or NaN, a lower "
                        "bound of infinity or an upper of -infinity refused");
    CHECK(mtwnMakeBoundedDecay(&decay, &spring, -(double)INFINITY, (double)INFINITY, &bounded) ==
              mtwn_success,
          "no bounds at all, a plain decay");
    CHECK(mtwnMakeBoundedDecay(&decay, &spring, 5.0, 5.0, &bounded) == mtwn_success,
          "equal bounds, a single resting place");
    mtwnDecay still = {0.0};
    mtwnSpring forged = spring;
    forged.parameters[2] = nextafter(forged.parameters[2], 0.0);
    CHECK(mtwnMakeBoundedDecay(&still, &spring, 0.0, 1.0, &bounded) == mtwn_errorInvalid &&
              mtwnMakeBoundedDecay(&decay, &forged, 0.0, 1.0, &bounded) == mtwn_errorInvalid,
          "a decay or a spring no maker gave refused");
}

// How many of the two calls taking a bounded decay refuse this one, or -1
// if one changed an output while refusing.
static int Refusals(const mtwnBoundedDecay* bounded)
{
    double values[3] = {7.0, 7.0, 7.0};
    int refused = 0;
    refused +=
        mtwnSampleBoundedDecay(bounded, 0.5, 1.0, 0.5, &values[0], &values[1]) == mtwn_errorInvalid;
    refused +=
        mtwnBoundedDecayStopTime(bounded, 0.5, 1.0, 0.5, 1.0, &values[2]) == mtwn_errorInvalid;
    if (refused == 2 && !(values[0] == 7.0 && values[1] == 7.0 && values[2] == 7.0))
    {
        return -1;
    }
    return refused;
}

// A bounded decay is the library's: one mtwnMakeBoundedDecay could not
// have made is refused, each field on its own.
static void TestForgedBounds(void)
{
    mtwnBoundedDecay made = Bounded(0.0, 1.0);
    CHECK(Refusals(&made) == 0, "as made, taken");
    CHECK(Refusals(NULL) == 2, "none");
    mtwnBoundedDecay forged = made;
    forged.decay.rate = -1.0;
    CHECK(Refusals(&forged) == 2, "its decay's rate below 0");
    forged = made;
    forged.spring.regime ^= 1U;
    CHECK(Refusals(&forged) == 2, "its spring in another regime");
    forged = made;
    forged.lower = 2.0;
    CHECK(Refusals(&forged) == 2, "its bounds crossed");
    forged = made;
    forged.upper = (double)NAN;
    CHECK(Refusals(&forged) == 2, "a NaN bound");
    forged = made;
    forged.lower = HUGE_VAL;
    forged.upper = HUGE_VAL;
    CHECK(Refusals(&forged) == 2, "a lower bound of infinity");
    forged = made;
    forged.lower = -HUGE_VAL;
    forged.upper = -HUGE_VAL;
    CHECK(Refusals(&forged) == 2, "an upper bound of -infinity");
}

int main(void)
{
    TestLanding();
    TestWithin();
    TestCrossing();
    TestStartPast();
    TestRefusals();
    TestForgedBounds();
    return s_failures == 0 ? 0 : 1;
}
