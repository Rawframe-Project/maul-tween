// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Decay and the rubber band
// against their formulas in long double where the platform has one wider
// than double: positions keep their relative digits down to tiny times,
// iOS's deceleration rate means what it does there, and the stop and
// crossing times land where the motion does.

#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/spring.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

// The bounds hold with a double reference too, where long double is no
// wider (macOS on arm64, Windows under clang-cl, the web): its worst error
// there is 0.57 of a bound.
#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_EXP   expl
#define WIDE_EXPM1 expm1l
#define WIDE_LOG1P log1pl
#else
typedef double Wide;
#define WIDE_EXP   exp
#define WIDE_EXPM1 expm1
#define WIDE_LOG1P log1p
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
                SampleDecay(&decay, 5.0, -300.0, seconds, &position, &velocity);
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
    if (!(worstTravel <= 1e-15) || !(worstVelocity <= 1e-15))
    {
        printf("  travel %.3g, velocity %.3g\n", worstTravel, worstVelocity);
    }
    CHECK(worstTravel <= 1e-15, "the position within 1e-15 at every time");
    CHECK(worstVelocity <= 1e-15, "the velocity too");
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
        SampleDecay(&decay, 0.0, 1.0, seconds, &position, NULL);
        Wide y = 2 * (Wide)seconds;
        double error = Relative(position, -WIDE_EXPM1(-y) / 2);
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 1e-15, "travel from 0 within 1e-15 relative, subnormal times too");
}

static void TestDecayFromIos(void)
{
    mtwnDecay normal;
    CHECK(mtwnMakeDecayFromDeceleration(0.998, &normal) == mtwn_success, "0.998");
    // e^(-k t) is 0.998 to the t in milliseconds.
    double velocity = 0.0;
    SampleDecay(&normal, 0.0, 1.0, 0.5, NULL, &velocity);
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
    SampleDecay(&decay, 10.0, -200.0, HUGE_VAL, &position, &velocity);
    CHECK(position == 10.0 + -200.0 / 4.0 && velocity == 0.0, "at rest at infinity");
    double stop = ComputeDecayStopTime(&decay, -200.0, 0.5);
    SampleDecay(&decay, 10.0, -200.0, stop, NULL, &velocity);
    CHECK(fabs(fabs(velocity) - 0.5) <= 1e-14, "at its stop time it has the stop speed");
    CHECK(ComputeDecayStopTime(&decay, 0.3, 0.5) == 0.0, "a start already slow");
    CHECK(ComputeDecayStopTime(&decay, -0.5, 0.5) == 0.0, "a start at the stop speed");
    double never = 0.0;
    CHECK(mtwnComputeDecayStopTime(&decay, -HUGE_VAL, HUGE_VAL, &never) == mtwn_errorInvalid &&
              mtwnComputeDecayStopTime(&decay, 3.0, 0.0, &never) == mtwn_errorInvalid &&
              mtwnComputeDecayStopTime(&decay, 3.0, (double)NAN, &never) == mtwn_errorInvalid &&
              never == 0.0,
          "an endless start, a stop speed of 0 or a NaN one refused");
    SampleDecay(&decay, 1.0, 1.0, 1.0, NULL, NULL);
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
            double t = ComputeDecayCrossingTime(&decay, 2.0, sign * 6.0, target);
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
    if (!(worst <= 2e-15))
    {
        printf("  crossing %.3g\n", worst);
    }
    CHECK(worst <= 2e-15, "crossing times within 2e-15 of their conditioning, near the start too");
    // From 0 a tiny target makes a share that is no multiple of 2^-53, so
    // 1 - f rounds: the series keeps its digits.
    double tiny = ComputeDecayCrossingTime(&decay, 0.0, 7.0, 1e-12);
    Wide tinyShare = (Wide)1e-12 * 3 / 7;
    CHECK(Relative(tiny, -WIDE_LOG1P(-tinyShare) / 3) <= 2e-15, "a target next to the start");
    // Half the travel is the series' last share: its bits, the same on
    // every platform (1 ulp below ln 2 correctly rounded, within the bound).
    mtwnDecay unit;
    CHECK(mtwnMakeDecay(1.0, &unit) == mtwn_success &&
              ComputeDecayCrossingTime(&unit, 0.0, 2.0, 1.0) == 0x1.62e42fefa39eep-1,
          "half the travel by the series");
    CHECK(ComputeDecayCrossingTime(&decay, 2.0, 6.0, 2.0) == 0.0, "at the start");
    CHECK(isinf(ComputeDecayCrossingTime(&decay, 2.0, 6.0, 1.0)), "behind it");
    CHECK(isinf(ComputeDecayCrossingTime(&decay, 2.0, 6.0, 4.0)), "exactly at rest");
    CHECK(isinf(ComputeDecayCrossingTime(&decay, 2.0, 6.0, 5.0)), "past the rest");
    CHECK(isinf(ComputeDecayCrossingTime(&decay, 2.0, 0.0, 5.0)), "with no velocity");
    double t = ComputeDecayCrossingTime(&decay, 2.0, 6.0, 3.9);
    double position = 0.0;
    SampleDecay(&decay, 2.0, 6.0, t, &position, NULL);
    CHECK(fabs(position - 3.9) <= 1e-14, "the motion is at the target then");
}

// Starts whose products and quotients on the way leave the doubles while
// the result does not; each reference in a form that stays within them.
static void TestDecayPastTheRange(void)
{
    // A share of 2^-1074: s = share / (2 - share) rounds to 0, and the
    // time, d / v (1 + share / 2 + ...), is 1.
    mtwnDecay faint;
    CHECK(mtwnMakeDecay(0x1p-1074, &faint) == mtwn_success, "the faintest decay");
    CHECK(ComputeDecayCrossingTime(&faint, 0.0, 1.0, 1.0) == 1.0,
          "a subnormal share's time is d / v");
    // A share of 2^-1076 rounds to 0, but the target is ahead.
    CHECK(ComputeDecayCrossingTime(&faint, 0.0, 4.0, 1.0) == 0.25,
          "a share below the least subnormal");
    // d k = 1e-400 underflows to 0 though the share is 1e-200.
    mtwnDecay slow;
    CHECK(mtwnMakeDecay(1e-200, &slow) == mtwn_success, "a slow decay");
    CHECK(Relative(ComputeDecayCrossingTime(&slow, 0.0, 1e-200, 1e-200), (Wide)1.0) <= 2e-16,
          "a share whose d k underflows");
    // target - position overflows; the share is 0.02.
    mtwnDecay gentle;
    CHECK(mtwnMakeDecay(1e-10, &gentle) == mtwn_success, "a gentle decay");
    double share = 2.0 * (1e308 * 1e-10) / 1e300;
    CHECK(Relative(ComputeDecayCrossingTime(&gentle, -1e308, 1e300, 1e308),
                   -WIDE_LOG1P(-(Wide)share) / (Wide)1e-10) <= 4e-16,
          "a distance past the doubles");
    // The speed ratio 1e600 overflows: ln 1e300 - ln 1e-300.
    mtwnDecay unit;
    CHECK(mtwnMakeDecay(1.0, &unit) == mtwn_success, "a unit decay");
    CHECK(Relative(ComputeDecayStopTime(&unit, 1e300, 1e-300),
                   (Wide)log(1e300) - (Wide)log(1e-300)) <= 2e-16,
          "a stop speed ratio past the doubles");
    // v t = 1e310 overflows; the travel is v (1 - e^(-1e10)) = 1e300.
    double position = 0.0;
    double velocity = 0.0;
    SampleDecay(&unit, 0.0, 1e300, 1e10, &position, NULL);
    CHECK(position == 1e300, "a v t past the doubles");
    // v t = 1.1 DBL_MAX overflows at y = 0.44, below the series' 1/2, yet
    // the travel is 0.89 DBL_MAX.
    mtwnDecay mild;
    CHECK(mtwnMakeDecay(0.4, &mild) == mtwn_success, "a mild decay");
    SampleDecay(&mild, 0.0, DBL_MAX, 1.1, &position, NULL);
    CHECK(Relative(position, (Wide)DBL_MAX * -WIDE_EXPM1((Wide)-0.4 * (Wide)1.1) / (Wide)0.4) <=
              4e-16,
          "a v t past the doubles at a small k t");
    // From -DBL_MAX a travel of 1.97 DBL_MAX at k t = 0.22: half the
    // velocity's v t still overflows, and its own half does not.
    mtwnDecay tenth;
    CHECK(mtwnMakeDecay(0.1, &tenth) == mtwn_success, "a tenth decay");
    SampleDecay(&tenth, -DBL_MAX, DBL_MAX, 2.2, &position, NULL);
    CHECK(Relative(position,
                   (Wide)DBL_MAX * (-WIDE_EXPM1((Wide)-0.1 * (Wide)2.2) / (Wide)0.1 - 1)) <= 4e-16,
          "a v t past twice the doubles at a small k t, from beyond its end");
    // k t = 1e310 overflows; the travel is v / k.
    mtwnDecay sharp;
    CHECK(mtwnMakeDecay(1e300, &sharp) == mtwn_success, "a sharp decay");
    SampleDecay(&sharp, 0.0, 1.0, 1e10, &position, NULL);
    CHECK(Relative(position, (Wide)1e-300) <= 2e-16, "a k t past the doubles");
    // From -DBL_MAX a travel of 1.55 DBL_MAX lands within the doubles.
    mtwnDecay half;
    CHECK(mtwnMakeDecay(0.5, &half) == mtwn_success, "a half decay");
    SampleDecay(&half, -DBL_MAX, DBL_MAX, 3.0, &position, NULL);
    CHECK(Relative(position, (Wide)DBL_MAX * (2 * -WIDE_EXPM1((Wide)-1.5) - 1)) <= 4e-16,
          "a travel past the doubles from beyond its end");
    // e^(-1000) is below the least subnormal; 1e300 times it is not.
    SampleDecay(&unit, 0.0, 1e300, 1000.0, NULL, &velocity);
    CHECK(Relative(velocity, (Wide)1e300 * WIDE_EXP((Wide)-500) * WIDE_EXP((Wide)-500)) <= 4e-16,
          "a velocity whose e^(-k t) underflows");
}

// Whether the rubber band and its inverse both refuse their arguments,
// leaving their outs as they were.
static bool BandRefused(double distance, double dimension, double coefficient)
{
    double offset = 7.0;
    double drag = 7.0;
    return mtwnApplyRubberBand(distance, dimension, coefficient, &offset) == mtwn_errorInvalid &&
           mtwnInvertRubberBand(distance, dimension, coefficient, &drag) == mtwn_errorInvalid &&
           offset == 7.0 && drag == 7.0;
}

static void TestRubberBand(void)
{
    double worst = 0.0;
    double worstInverse = 0.0;
    for (int i = 1; i <= 2000; ++i)
    {
        double x = ldexp((double)i / 2000.0, i % 40 - 20);
        double band = ApplyRubberBand(x, 600.0, 0.55);
        // iOS's d (1 - 1 / (u + 1)) with u = c x / d is c x / (1 + u),
        // which loses no digits in either width: the band over c x is
        // checked against 1 / (1 + u).
        Wide u = (Wide)0.55 * (Wide)x / 600;
        double error = Relative(band / (0.55 * x), 1 / (1 + u));
        worst = error > worst ? error : worst;
        double back = InvertRubberBand(band, 600.0, 0.55);
        double round = fabs(back - x) / x;
        worstInverse = round > worstInverse ? round : worstInverse;
    }
    if (!(worst <= 6e-16) || !(worstInverse <= 1e-13))
    {
        printf("  band %.3g, inverse %.3g\n", worst, worstInverse);
    }
    CHECK(worst <= 6e-16, "iOS's rubber band, small drags keeping their digits");
    CHECK(worstInverse <= 1e-13, "its inverse takes the band back to the drag");
    CHECK(ApplyRubberBand(-30.0, 600.0, 0.55) == -ApplyRubberBand(30.0, 600.0, 0.55), "signed");
    CHECK(ApplyRubberBand(0.0, 600.0, 0.55) == 0.0, "nothing past the limit");
    CHECK(fabs(ApplyRubberBand(1e-9, 600.0, 0.55) / 1e-9 - 0.55) <= 1e-12, "slope c at the limit");
    CHECK(ApplyRubberBand(HUGE_VAL, 600.0, 0.55) == 600.0 &&
              ApplyRubberBand(-HUGE_VAL, 600.0, 0.55) == -600.0,
          "an infinite drag reaches the dimension");
    CHECK(ApplyRubberBand(30.0, HUGE_VAL, 0.55) == 0.55 * 30.0 &&
              ApplyRubberBand(-30.0, HUGE_VAL, HUGE_VAL) == -HUGE_VAL,
          "an endless view resists nothing; with an endless coefficient, not NaN");
    CHECK(ApplyRubberBand(0.0, 600.0, HUGE_VAL) == 0.0 &&
              signbit(ApplyRubberBand(-0.0, 600.0, HUGE_VAL)) &&
              ApplyRubberBand(30.0, 600.0, HUGE_VAL) == 600.0 &&
              InvertRubberBand(0.0, 600.0, HUGE_VAL) == 0.0,
          "an endless coefficient: nothing past the limit moves nothing, any drag the view");
    CHECK(InvertRubberBand(-16.5, HUGE_VAL, 0.55) == -16.5 / 0.55, "and its inverse takes it back");
    CHECK(ApplyRubberBand(1e10, 600.0, 0.55) < 600.0, "a finite one does not");
    CHECK(ApplyRubberBand(1e308, 600.0, 4.0) == 600.0 &&
              ApplyRubberBand(1e-310, 600.0, 0.55) == 0.55 * 1e-310,
          "nothing overflows at huge drags or vanishes at tiny ones");
    CHECK(BandRefused(30.0, 0.0, 0.55) && BandRefused(30.0, 600.0, 0.0) &&
              BandRefused((double)NAN, 600.0, 0.55) && BandRefused(30.0, -1.0, 0.55) &&
              BandRefused(30.0, (double)NAN, 0.55) && BandRefused(30.0, 600.0, (double)NAN),
          "no band without a dimension and a coefficient above 0, nor for a NaN drag");
    double small = ApplyRubberBand(1.0, 0.5, 0.55);
    CHECK(fabs(small - 0.5 * 0.55 / 1.05) <= 1e-15 &&
              fabs(InvertRubberBand(small, 0.5, 0.55) - 1.0) <= 1e-12,
          "a band under a unit wide");
    CHECK(InvertRubberBand(-30.0, 600.0, 0.55) == -InvertRubberBand(30.0, 600.0, 0.55) &&
              InvertRubberBand(-30.0, 600.0, 0.55) < 0.0,
          "the inverse is signed");
    CHECK(InvertRubberBand(600.0, 600.0, 0.55) == HUGE_VAL &&
              InvertRubberBand(-700.0, 600.0, 0.55) == -HUGE_VAL,
          "no drag moves it the dimension");
    CHECK(BandRefused(30.0, 0.0, 0.55) && BandRefused((double)NAN, 600.0, 0.55) &&
              BandRefused(30.0, 600.0, -0.55),
          "the inverse refuses the same, and a NaN offset");
    CHECK(mtwnApplyRubberBand(30.0, 600.0, 0.55, NULL) == mtwn_errorInvalid &&
              mtwnInvertRubberBand(30.0, 600.0, 0.55, NULL) == mtwn_errorInvalid,
          "a null out refused");
}

// The documented ends: a stop speed not above 0 is
// never reached, even from rest; a dimension of 0 bands nothing, a drag
// of 0 included.
static void TestEnds(void)
{
    mtwnDecay decay;
    CHECK(mtwnMakeDecay(4.0, &decay) == mtwn_success, "a decay");
    double stop = 0.0;
    CHECK(mtwnComputeDecayStopTime(&decay, 0.0, 0.0, &stop) == mtwn_errorInvalid &&
              mtwnComputeDecayStopTime(&decay, 5.0, -1.0, &stop) == mtwn_errorInvalid &&
              stop == 0.0,
          "a stop speed not above 0 refused");
    CHECK(BandRefused(0.0, 0.0, 0.55) && BandRefused(3.0, 0.0, 0.55) &&
              BandRefused(3.0, 100.0, 0.0),
          "no dimension or coefficient refused");
}

// A decay's rate is the library's: one no maker gives, not finite and
// above 0, is refused by every call taking it, with its outputs as they
// were.
static void TestForgedDecays(void)
{
    static const double rates[] = {0.0, -0.0, -1.0, HUGE_VAL, (double)NAN};
    int refused = 0;
    double values[4] = {7.0, 7.0, 7.0, 7.0};
    for (size_t i = 0; i < sizeof rates / sizeof rates[0]; i++)
    {
        mtwnDecay decay = {rates[i]};
        refused +=
            mtwnSampleDecay(&decay, 0.0, 1.0, 0.5, &values[0], &values[1]) == mtwn_errorInvalid;
        refused += mtwnComputeDecayStopTime(&decay, 1.0, 0.5, &values[2]) == mtwn_errorInvalid;
        refused +=
            mtwnComputeDecayCrossingTime(&decay, 0.0, 1.0, 0.1, &values[3]) == mtwn_errorInvalid;
    }
    refused += mtwnSampleDecay(NULL, 0.0, 1.0, 0.5, &values[0], &values[1]) == mtwn_errorInvalid;
    refused += mtwnComputeDecayStopTime(NULL, 1.0, 0.5, &values[2]) == mtwn_errorInvalid;
    refused += mtwnComputeDecayCrossingTime(NULL, 0.0, 1.0, 0.1, &values[3]) == mtwn_errorInvalid;
    CHECK(refused == 18 && values[0] == 7.0 && values[1] == 7.0 && values[2] == 7.0 &&
              values[3] == 7.0,
          "a rate not finite and above 0, or no decay, refused");
    mtwnDecay least = {DBL_TRUE_MIN};
    CHECK(mtwnComputeDecayStopTime(&least, 1.0, 0.5, &values[2]) == mtwn_success,
          "the least rate above 0 taken");
}

int main(void)
{
    TestEnds();
    TestForgedDecays();
    TestDecayAgainstTheFormula();
    TestDecayTravelAlone();
    TestDecayFromIos();
    TestDecayRestAndStop();
    TestDecayCrossings();
    TestDecayPastTheRange();
    TestRubberBand();
    return s_failures == 0 ? 0 : 1;
}
