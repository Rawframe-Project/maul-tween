// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Decay and the rubber band (records mtwn-0010, mtwn-0020, mtwn-0023)
// against their formulas in long double where the platform has one wider
// than double: positions keep their relative digits down to tiny times,
// iOS's deceleration rate means what it does there, and the stop and
// crossing times land where the motion does.

#include "test_harness.h"

#include "maul-tween/spring.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_EXP   expl
#define WIDE_EXPM1 expm1l
#define WIDE_LOG1P log1pl
static const double s_slack = 0.0;
#else
typedef double Wide;
#define WIDE_EXP   exp
#define WIDE_EXPM1 expm1
#define WIDE_LOG1P log1p
// The C library's double functions add an ulp or two of their own.
static const double s_slack = 4.0 * DBL_EPSILON;
#endif

static double Relative(double value, Wide reference)
{
    double r = (double)reference;
    return r == 0.0 ? fabs(value) : fabs(value - r) / fabs(r);
}

static void TestDecayAgainstTheFormula(void)
{
    static const double rates[] = {0.3, 2.002002, 20.0, 1000.0};
    double worstTravel = 0.0;
    double worstVelocity = 0.0;
    for (int r = 0; r < 4; ++r)
    {
        mtwnDecay decay;
        CHECK(mtwnMakeDecay(rates[r], &decay) == mtwn_success, "made");
        for (int e = -40; e <= 12; ++e)
        {
            for (int m = 1; m <= 9; m += 2)
            {
                double seconds = ldexp((double)m / 8.0, e);
                double position = 0.0;
                double velocity = 0.0;
                mtwnSampleDecay(&decay, 5.0, -300.0, seconds, &position, &velocity);
                Wide y = (Wide)rates[r] * (Wide)seconds;
                Wide travel = (Wide)-300.0 * -WIDE_EXPM1(-y) / (Wide)rates[r];
                // The position within an ulp or so of its size, start
                // included.
                double t = fabs(position - (double)(5 + travel)) / (5.0 + fabs((double)travel));
                worstTravel = t > worstTravel ? t : worstTravel;
                // The velocity relative to itself while it is normal.
                Wide kept = (Wide)-300.0 * WIDE_EXP(-y);
                if (fabs((double)kept) > 1e-290)
                {
                    // Rounding y = k t moves e^(-y) by y ulps: the error is
                    // judged per unit of 1 + y.
                    double v = Relative(velocity, kept) / (1.0 + (double)y);
                    worstVelocity = v > worstVelocity ? v : worstVelocity;
                }
            }
        }
    }
    if (!(worstTravel <= 1e-15 + s_slack) || !(worstVelocity <= 1e-15 + s_slack))
    {
        printf("  travel %.3g, velocity %.3g\n", worstTravel, worstVelocity);
    }
    CHECK(worstTravel <= 1e-15 + s_slack, "the position within 1e-15 at every time");
    CHECK(worstVelocity <= 1e-15 + s_slack, "the velocity too");
}

static void TestDecayTravelAlone(void)
{
    // From 0 the travel is the position, so its relative digits show at
    // the tiniest times.
    mtwnDecay decay;
    CHECK(mtwnMakeDecay(2.0, &decay) == mtwn_success, "made");
    double worst = 0.0;
    for (int e = -1060; e <= 6; e += 3)
    {
        double seconds = ldexp(1.0, e);
        double position = 0.0;
        mtwnSampleDecay(&decay, 0.0, 1.0, seconds, &position, NULL);
        Wide y = 2 * (Wide)seconds;
        double error = Relative(position, -WIDE_EXPM1(-y) / 2);
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 1e-15 + s_slack, "travel from 0 within 1e-15 relative, subnormal times too");
}

static void TestDecayFromIos(void)
{
    mtwnDecay normal;
    CHECK(mtwnMakeDecayFromDeceleration(0.998, &normal) == mtwn_success, "0.998");
    // e^(-k t) is 0.998 to the t in milliseconds.
    double velocity = 0.0;
    mtwnSampleDecay(&normal, 0.0, 1.0, 0.5, NULL, &velocity);
    CHECK(Relative(velocity, (Wide)pow(0.998, 500.0)) <= 2e-15, "0.998 kept each millisecond");
    CHECK(fabs(normal.rate - 2.0020026706730793) <= 1e-15, "a rate of -1000 ln 0.998 a second");
    mtwnDecay fast;
    CHECK(mtwnMakeDecayFromDeceleration(0.99, &fast) == mtwn_success &&
              fabs(fast.rate - 10.050335853501451) <= 1e-14,
          "0.99");
    mtwnDecay decay = {7.0};
    CHECK(mtwnMakeDecayFromDeceleration(1.0, &decay) == mtwn_errorInvalid, "1 never decays");
    CHECK(mtwnMakeDecayFromDeceleration(1.5, &decay) == mtwn_errorInvalid, "above 1 grows");
    CHECK(mtwnMakeDecayFromDeceleration(-0.5, &decay) == mtwn_errorInvalid, "below 0");
    CHECK(mtwnMakeDecayFromDeceleration(0.0, &decay) == mtwn_errorInvalid, "0");
    CHECK(mtwnMakeDecayFromDeceleration((double)NAN, &decay) == mtwn_errorInvalid, "NaN");
    CHECK(mtwnMakeDecayFromDeceleration(1.0 - 0x1.0p-53, &decay) == mtwn_success &&
              decay.rate > 0.0,
          "the share nearest 1 still decays");
    CHECK(mtwnMakeDecay(0.0, &decay) == mtwn_errorInvalid, "no rate");
    CHECK(mtwnMakeDecay(HUGE_VAL, &decay) == mtwn_errorInvalid, "an infinite rate");
    CHECK(mtwnMakeDecay(1.0, NULL) == mtwn_errorInvalid, "null out");
}

static void TestDecayRestAndStop(void)
{
    mtwnDecay decay;
    CHECK(mtwnMakeDecay(4.0, &decay) == mtwn_success, "made");
    double position = 0.0;
    double velocity = 1.0;
    mtwnSampleDecay(&decay, 10.0, -200.0, HUGE_VAL, &position, &velocity);
    CHECK(position == 10.0 + -200.0 / 4.0 && velocity == 0.0, "at rest at infinity");
    double stop = mtwnDecayStopTime(&decay, -200.0, 0.5);
    mtwnSampleDecay(&decay, 10.0, -200.0, stop, NULL, &velocity);
    CHECK(fabs(fabs(velocity) - 0.5) <= 1e-14, "at its stop time it has the stop speed");
    CHECK(mtwnDecayStopTime(&decay, 0.3, 0.5) == 0.0, "a start already slow");
    CHECK(mtwnDecayStopTime(&decay, -0.5, 0.5) == 0.0, "a start at the stop speed");
    CHECK(isinf(mtwnDecayStopTime(&decay, 3.0, 0.0)), "a stop speed of 0 is never reached");
    CHECK(isinf(mtwnDecayStopTime(&decay, 3.0, (double)NAN)), "nor a NaN one");
    mtwnSampleDecay(&decay, 1.0, 1.0, 1.0, NULL, NULL);
}

static void TestDecayCrossings(void)
{
    mtwnDecay decay;
    CHECK(mtwnMakeDecay(3.0, &decay) == mtwn_success, "made");
    // Rest at 2 + 6 / 3 = 4; targets along the way, near the start and
    // near the rest, both ways.
    static const double targets[] = {2.0 + 1e-12, 2.0 + 1e-6, 2.5, 3.0, 3.5, 3.999};
    double worst = 0.0;
    for (int i = 0; i < 6; ++i)
    {
        for (int sign = -1; sign <= 1; sign += 2)
        {
            double target = 2.0 + sign * (targets[i] - 2.0);
            double t = mtwnDecayCrossingTime(&decay, 2.0, sign * 6.0, target);
            Wide share = ((Wide)target - 2) * 3 / (sign * 6);
            Wide expected = -WIDE_LOG1P(-share) / 3;
            // Near the rest the time is ill-conditioned in the share f, by
            // f / ((1 - f) |ln(1 - f)|): the error is judged per unit of it.
            double f = (double)share;
            double condition = f / ((1.0 - f) * fabs(log1p(-f)));
            double error = Relative(t, expected) / (condition > 1.0 ? condition : 1.0);
            worst = error > worst ? error : worst;
        }
    }
    if (!(worst <= 2e-15 + s_slack))
    {
        printf("  crossing %.3g\n", worst);
    }
    CHECK(worst <= 2e-15 + s_slack,
          "crossing times within 2e-15 of their conditioning, near the start too");
    // From 0 a tiny target makes a share that is no multiple of 2^-53, so
    // 1 - f rounds: the series keeps its digits.
    double tiny = mtwnDecayCrossingTime(&decay, 0.0, 7.0, 1e-12);
    Wide tinyShare = (Wide)1e-12 * 3 / 7;
    CHECK(Relative(tiny, -WIDE_LOG1P(-tinyShare) / 3) <= 2e-15 + s_slack,
          "a target next to the start");
    // Half the travel is the series' last share: its bits, the same on
    // every platform (1 ulp below ln 2 correctly rounded, within the bound).
    mtwnDecay unit;
    CHECK(mtwnMakeDecay(1.0, &unit) == mtwn_success &&
              mtwnDecayCrossingTime(&unit, 0.0, 2.0, 1.0) == 0x1.62e42fefa39eep-1,
          "half the travel by the series");
    CHECK(mtwnDecayCrossingTime(&decay, 2.0, 6.0, 2.0) == 0.0, "at the start");
    CHECK(isinf(mtwnDecayCrossingTime(&decay, 2.0, 6.0, 1.0)), "behind it");
    CHECK(isinf(mtwnDecayCrossingTime(&decay, 2.0, 6.0, 4.0)), "exactly at rest");
    CHECK(isinf(mtwnDecayCrossingTime(&decay, 2.0, 6.0, 5.0)), "past the rest");
    CHECK(isinf(mtwnDecayCrossingTime(&decay, 2.0, 0.0, 5.0)), "with no velocity");
    double t = mtwnDecayCrossingTime(&decay, 2.0, 6.0, 3.9);
    double position = 0.0;
    mtwnSampleDecay(&decay, 2.0, 6.0, t, &position, NULL);
    CHECK(fabs(position - 3.9) <= 1e-14, "the motion is at the target then");
}

static void TestRubberBand(void)
{
    double worst = 0.0;
    double worstInverse = 0.0;
    for (int i = 1; i <= 2000; ++i)
    {
        double x = ldexp((double)i / 2000.0, i % 40 - 20);
        double band = mtwnRubberBand(x, 600.0, 0.55);
        // iOS's d (1 - 1 / (u + 1)) with u = c x / d is c x / (1 + u),
        // which loses no digits in either width: the band over c x is
        // checked against 1 / (1 + u).
        Wide u = (Wide)0.55 * (Wide)x / 600;
        double error = Relative(band / (0.55 * x), 1 / (1 + u));
        worst = error > worst ? error : worst;
        double back = mtwnRubberBandInverse(band, 600.0, 0.55);
        double round = fabs(back - x) / x;
        worstInverse = round > worstInverse ? round : worstInverse;
    }
    if (!(worst <= 6e-16 + s_slack) || !(worstInverse <= 1e-13))
    {
        printf("  band %.3g, inverse %.3g\n", worst, worstInverse);
    }
    CHECK(worst <= 6e-16 + s_slack, "iOS's rubber band, small drags keeping their digits");
    CHECK(worstInverse <= 1e-13, "its inverse takes the band back to the drag");
    CHECK(mtwnRubberBand(-30.0, 600.0, 0.55) == -mtwnRubberBand(30.0, 600.0, 0.55), "signed");
    CHECK(mtwnRubberBand(0.0, 600.0, 0.55) == 0.0, "nothing past the limit");
    CHECK(fabs(mtwnRubberBand(1e-9, 600.0, 0.55) / 1e-9 - 0.55) <= 1e-12, "slope c at the limit");
    CHECK(mtwnRubberBand(HUGE_VAL, 600.0, 0.55) == 600.0 &&
              mtwnRubberBand(-HUGE_VAL, 600.0, 0.55) == -600.0,
          "an infinite drag reaches the dimension");
    CHECK(mtwnRubberBand(30.0, HUGE_VAL, 0.55) == 0.55 * 30.0 &&
              mtwnRubberBand(-30.0, HUGE_VAL, HUGE_VAL) == -HUGE_VAL,
          "an endless view resists nothing; with an endless coefficient, not NaN");
    CHECK(mtwnRubberBand(0.0, 600.0, HUGE_VAL) == 0.0 &&
              signbit(mtwnRubberBand(-0.0, 600.0, HUGE_VAL)) &&
              mtwnRubberBand(30.0, 600.0, HUGE_VAL) == 600.0 &&
              mtwnRubberBandInverse(0.0, 600.0, HUGE_VAL) == 0.0,
          "an endless coefficient: nothing past the limit moves nothing, any drag the view");
    CHECK(mtwnRubberBandInverse(-16.5, HUGE_VAL, 0.55) == -16.5 / 0.55,
          "and its inverse takes it back");
    CHECK(mtwnRubberBand(1e10, 600.0, 0.55) < 600.0, "a finite one does not");
    CHECK(mtwnRubberBand(1e308, 600.0, 4.0) == 600.0 &&
              mtwnRubberBand(1e-310, 600.0, 0.55) == 0.55 * 1e-310,
          "nothing overflows at huge drags or vanishes at tiny ones");
    CHECK(mtwnRubberBand(30.0, 0.0, 0.55) == 0.0 && mtwnRubberBand(30.0, 600.0, 0.0) == 0.0 &&
              mtwnRubberBand((double)NAN, 600.0, 0.55) == 0.0 &&
              mtwnRubberBand(30.0, -1.0, 0.55) == 0.0,
          "no band without a dimension and a coefficient");
    CHECK(mtwnRubberBandInverse(-30.0, 600.0, 0.55) == -mtwnRubberBandInverse(30.0, 600.0, 0.55) &&
              mtwnRubberBandInverse(-30.0, 600.0, 0.55) < 0.0,
          "the inverse is signed");
    CHECK(mtwnRubberBandInverse(600.0, 600.0, 0.55) == HUGE_VAL &&
              mtwnRubberBandInverse(-700.0, 600.0, 0.55) == -HUGE_VAL,
          "no drag moves it the dimension");
    CHECK(mtwnRubberBandInverse(30.0, 0.0, 0.55) == 0.0 &&
              mtwnRubberBandInverse((double)NAN, 600.0, 0.55) == 0.0,
          "nor without a dimension");
}

// The documented ends (found by mutants): a stop speed not above 0 is
// never reached, even from rest; a dimension of 0 bands nothing, a drag
// of 0 included.
static void TestEnds(void)
{
    mtwnDecay decay;
    CHECK(mtwnMakeDecay(4.0, &decay) == mtwn_success, "a decay");
    CHECK(mtwnDecayStopTime(&decay, 0.0, 0.0) == HUGE_VAL &&
              mtwnDecayStopTime(&decay, 5.0, -1.0) == HUGE_VAL,
          "a stop speed not above 0 never reached");
    CHECK(mtwnRubberBand(0.0, 0.0, 0.55) == 0.0 && mtwnRubberBand(3.0, 0.0, 0.55) == 0.0 &&
              mtwnRubberBand(3.0, 100.0, 0.0) == 0.0,
          "no dimension or coefficient, no band");
}

int main(void)
{
    TestEnds();
    TestDecayAgainstTheFormula();
    TestDecayTravelAlone();
    TestDecayFromIos();
    TestDecayRestAndStop();
    TestDecayCrossings();
    TestRubberBand();
    return s_failures == 0 ? 0 : 1;
}
