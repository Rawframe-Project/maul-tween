// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Springs (records mtwn-0010, mtwn-0020, mtwn-0023) against an
// independent reference: the oscillator x'' = -2 a x' - w0^2 x
// integrated by fourth-order Runge-Kutta in small fixed steps, in long
// double where the platform has one wider than double. Then the makers'
// mappings, the ends of time, and settling times checked by sampling
// past them.

#include "test_harness.h"

#include "maul-tween/easing.h"
#include "maul-tween/sample.h"
#include "maul-tween/spring.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
static const double s_slack = 0.0;
#else
typedef double Wide;
// A double integration's own rounding, over many steps.
static const double s_slack = 2e-11;
#endif

static const double s_twoPi = 6.283185307179586;

typedef struct Case
{
    double hertz;
    double ratio;
    // The largest error allowed, each entry of the motion scaled to 1.
    double bound;
} Case;

// An undamped spring turns the double rounding of 2 pi f into phase,
// 56 radians of it by 3 seconds at 3 Hz.
static const Case s_cases[] = {
    {1.0, 0.5, 3e-16},      {2.0, 0.1, 8e-16},      {1.5, 1.0, 3e-16},
    {1.0, 0.999999, 3e-16}, {1.0, 1.000001, 4e-16}, {1.0, 3.0, 4e-16},
    {0.5, 50.0, 4e-16},     {3.0, 0.0, 1.2e-14},    {10.0, 0.05, 1.5e-15},
};

// The motion's four entries at times k / 100 for k from 0 to 300, by
// integrating from (1, 0) and from (0, 1).
static void Integrate(double hertz, double ratio, double motion[301][4])
{
    Wide w0 = (Wide)s_twoPi * (Wide)hertz;
    Wide a = (Wide)ratio * w0;
    Wide fastest = a + w0 + (Wide)ratio * w0;
    int perSample = (int)ceil((double)(fastest * (Wide)(0.01L / 2.5e-4L)));
    perSample = perSample < 1 ? 1 : perSample;
    Wide h = (Wide)0.01L / (Wide)perSample;
    for (int column = 0; column < 2; ++column)
    {
        Wide x = column == 0 ? 1 : 0;
        Wide v = column == 0 ? 0 : 1;
        for (int k = 0; k <= 300; ++k)
        {
            motion[k][column] = (double)x;
            motion[k][2 + column] = (double)v;
            for (int s = 0; s < perSample; ++s)
            {
                Wide k1x = v;
                Wide k1v = -2 * a * v - w0 * w0 * x;
                Wide k2x = v + h / 2 * k1v;
                Wide k2v = -2 * a * k2x - w0 * w0 * (x + h / 2 * k1x);
                Wide k3x = v + h / 2 * k2v;
                Wide k3v = -2 * a * k3x - w0 * w0 * (x + h / 2 * k2x);
                Wide k4x = v + h * k3v;
                Wide k4v = -2 * a * k4x - w0 * w0 * (x + h * k3x);
                x += h / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
                v += h / 6 * (k1v + 2 * k2v + 2 * k3v + k4v);
            }
        }
    }
}

static double s_reference[301][4];

static void TestAgainstIntegration(void)
{
    for (size_t c = 0; c < sizeof s_cases / sizeof s_cases[0]; ++c)
    {
        mtwnSpring spring;
        CHECK(mtwnMakeSpringFromFrequency(s_cases[c].hertz, s_cases[c].ratio, &spring) ==
                  mtwn_success,
              "made");
        Integrate(s_cases[c].hertz, s_cases[c].ratio, s_reference);
        double w0 = s_twoPi * s_cases[c].hertz;
        double worst = 0.0;
        for (int k = 0; k <= 300; ++k)
        {
            mtwnSpringMotion m = mtwnSpringMotionAt(&spring, (double)k / 100.0);
            double errors[4] = {
                fabs(m.offsetFromOffset - s_reference[k][0]),
                fabs(m.offsetFromVelocity - s_reference[k][1]) * w0,
                fabs(m.velocityFromOffset - s_reference[k][2]) / w0,
                fabs(m.velocityFromVelocity - s_reference[k][3]),
            };
            for (int e = 0; e < 4; ++e)
            {
                worst = errors[e] > worst ? errors[e] : worst;
            }
        }
        if (!(worst <= s_cases[c].bound + s_slack))
        {
            printf("  %g Hz, ratio %g: %.3g\n", s_cases[c].hertz, s_cases[c].ratio, worst);
        }
        CHECK(worst <= s_cases[c].bound + s_slack, "a spring within its bound of integration");
    }
}

static double Distance(const mtwnSpring* a, const mtwnSpring* b, double seconds)
{
    mtwnSpringMotion x = mtwnSpringMotionAt(a, seconds);
    mtwnSpringMotion y = mtwnSpringMotionAt(b, seconds);
    return fabs(x.offsetFromOffset - y.offsetFromOffset) +
           fabs(x.offsetFromVelocity - y.offsetFromVelocity) +
           fabs(x.velocityFromOffset - y.velocityFromOffset) * 1e-3 +
           fabs(x.velocityFromVelocity - y.velocityFromVelocity);
}

static void TestMakersAgree(void)
{
    mtwnSpring byFrequency;
    mtwnSpring physical;
    mtwnSpring byDuration;
    double w0 = s_twoPi * 2.0;
    CHECK(mtwnMakeSpringFromFrequency(2.0, 0.7, &byFrequency) == mtwn_success, "by frequency");
    CHECK(mtwnMakeSpring(w0 * w0 * 3.0, 2.0 * 0.7 * w0 * 3.0, 3.0, &physical) == mtwn_success,
          "physical, mass 3");
    CHECK(mtwnMakeSpringFromDuration(0.5, 0.3, &byDuration) == mtwn_success, "by duration");
    double worst = 0.0;
    for (int k = 0; k <= 100; ++k)
    {
        double t = (double)k / 50.0;
        double d = Distance(&byFrequency, &physical, t) + Distance(&byFrequency, &byDuration, t);
        worst = d > worst ? d : worst;
    }
    CHECK(worst <= 1e-13, "stiffness, damping and mass; hertz and ratio; duration and bounce");
    // k m overflows here, and its root does not.
    mtwnSpring large;
    mtwnSpring unit;
    CHECK(mtwnMakeSpring(1e200, 2e200, 1e200, &large) == mtwn_success &&
              mtwnMakeSpringFromFrequency(1.0 / s_twoPi, 1.0, &unit) == mtwn_success &&
              Distance(&large, &unit, 1.5) <= 1e-15,
          "a stiffness and a mass whose product overflows");
    mtwnSpring overdamped;
    mtwnSpring expected;
    CHECK(mtwnMakeSpringFromDuration(1.0, -0.5, &overdamped) == mtwn_success &&
              mtwnMakeSpringFromFrequency(1.0, 2.0, &expected) == mtwn_success &&
              Distance(&overdamped, &expected, 0.8) <= 1e-15,
          "a negative bounce b is the ratio 1 / (1 + b)");
    CHECK(mtwnMakeSpringFromDuration(1.0, 0.0, &overdamped) == mtwn_success &&
              mtwnMakeSpringFromFrequency(1.0, 1.0, &expected) == mtwn_success &&
              Distance(&overdamped, &expected, 0.8) == 0.0,
          "no bounce is critical");
}

static void TestRegimesMeet(void)
{
    // Across critical damping and across the series' edge the motion is
    // continuous: neighbours differ by what their parameters do.
    mtwnSpring below;
    mtwnSpring at;
    mtwnSpring above;
    CHECK(mtwnMakeSpringFromFrequency(1.0, 1.0 - 1e-9, &below) == mtwn_success &&
              mtwnMakeSpringFromFrequency(1.0, 1.0, &at) == mtwn_success &&
              mtwnMakeSpringFromFrequency(1.0, 1.0 + 1e-9, &above) == mtwn_success,
          "made");
    double worst = 0.0;
    for (int k = 1; k <= 300; ++k)
    {
        double t = (double)k / 100.0;
        double d = Distance(&below, &at, t) + Distance(&at, &above, t);
        worst = d > worst ? d : worst;
    }
    CHECK(worst <= 1e-7, "the three regimes meet at critical damping");
    mtwnSpring heavy;
    CHECK(mtwnMakeSpringFromFrequency(1.0, 2.0, &heavy) == mtwn_success, "made");
    // w t = 1/2 at t = 1 / (4 pi sqrt(3)).
    double edge = 0.5 / (s_twoPi * sqrt(3.0));
    double jump = Distance(&heavy, &heavy, edge) +
                  fabs(mtwnSpringMotionAt(&heavy, nextafter(edge, 0.0)).offsetFromOffset -
                       mtwnSpringMotionAt(&heavy, edge).offsetFromOffset);
    CHECK(jump <= 1e-15, "the series and the closed form meet");
}

static void TestEndsOfTime(void)
{
    mtwnSpring spring;
    CHECK(mtwnMakeSpringFromFrequency(1.5, 0.4, &spring) == mtwn_success, "made");
    mtwnSpringMotion start = mtwnSpringMotionAt(&spring, 0.0);
    CHECK(start.offsetFromOffset == 1.0 && start.offsetFromVelocity == 0.0 &&
              start.velocityFromOffset == 0.0 && start.velocityFromVelocity == 1.0,
          "the identity at 0");
    mtwnSpringMotion rest = mtwnSpringMotionAt(&spring, HUGE_VAL);
    CHECK(rest.offsetFromOffset == 0.0 && rest.offsetFromVelocity == 0.0 &&
              rest.velocityFromOffset == 0.0 && rest.velocityFromVelocity == 0.0,
          "at rest at infinity");
    // Far past where its envelope underflows, at rest too, though the
    // phase there is beyond the sine's range (found by the values' fuzz).
    mtwnSpringMotion far = mtwnSpringMotionAt(&spring, 1e300);
    CHECK(far.offsetFromOffset == 0.0 && far.offsetFromVelocity == 0.0 &&
              far.velocityFromOffset == 0.0 && far.velocityFromVelocity == 0.0,
          "at rest at 1e300 s");
    // A spring stiff past where any time has a phase: its motion stays
    // bounded, the sine of a phase that holds no angle (found by the
    // values' fuzz, -inf and NaN before).
    mtwnSpring stiff;
    CHECK(mtwnMakeSpringFromFrequency(1e148, 1e-149, &stiff) == mtwn_success, "a stiff spring");
    bool bounded = true;
    for (double t = 0.016; t < 100.0; t *= 4.0)
    {
        mtwnSpringMotion m = mtwnSpringMotionAt(&stiff, t);
        bounded = bounded && fabs(m.offsetFromOffset) <= 1.0 &&
                  fabs(m.velocityFromVelocity) <= 1.0 && isfinite(m.offsetFromVelocity) &&
                  isfinite(m.velocityFromOffset);
    }
    // Between 2^53 and 2^60 too, where the sine is already out of
    // [-1, 1] (-3.2e22 at 1e18; a mutant moving the bound found it).
    CHECK(mtwnMakeSpringFromFrequency(1e18 / s_twoPi, 0.0, &stiff) == mtwn_success, "1e18 rad/s");
    mtwnSpringMotion mid = mtwnSpringMotionAt(&stiff, 1.0);
    bounded = bounded && fabs(mid.offsetFromOffset) <= 1.0 && fabs(mid.velocityFromVelocity) <= 1.0;
    CHECK(bounded, "a phase past 2^52 radians bounded");
    // Undamped, its phase overflowing: no angle either, and no NaN.
    CHECK(mtwnMakeSpringFromFrequency(1e150, 0.0, &stiff) == mtwn_success, "an undamped one");
    mtwnSpringMotion endless = mtwnSpringMotionAt(&stiff, 1e160);
    CHECK(fabs(endless.offsetFromOffset) <= 1.0 && isfinite(endless.offsetFromVelocity) &&
              isfinite(endless.velocityFromOffset) && fabs(endless.velocityFromVelocity) <= 1.0,
          "an infinite phase bounded");
    // Both bounds fall from the start here, the speed's from just under
    // its tolerance: settled at once, not at a time before the start.
    mtwnSpring firm;
    double falling = -0.9 * s_twoPi * 1e-4 * 0.999;
    CHECK(mtwnMakeSpringFromFrequency(1.0, 0.9, &firm) == mtwn_success &&
              mtwnSpringSettlingTime(&firm, 1e-4, falling, 1e-3, 1.01 * fabs(falling)) == 0.0,
          "a start falling inside both is settled at once");
    mtwnSpring undamped;
    CHECK(mtwnMakeSpringFromFrequency(1.0, 0.0, &undamped) == mtwn_success, "undamped");
    CHECK(isnan(mtwnSpringMotionAt(&undamped, HUGE_VAL).offsetFromOffset),
          "an undamped spring has no rest");
    double offset = 0.0;
    double velocity = 0.0;
    mtwnSampleSpring(&spring, 2.0, -3.0, 0.4, &offset, &velocity);
    mtwnSpringMotion m = mtwnSpringMotionAt(&spring, 0.4);
    CHECK(offset == m.offsetFromOffset * 2.0 + m.offsetFromVelocity * -3.0 &&
              velocity == m.velocityFromOffset * 2.0 + m.velocityFromVelocity * -3.0,
          "a sample is the motion applied");
    mtwnSampleSpring(&spring, 2.0, -3.0, 0.4, NULL, NULL);
}

static void TestRefusals(void)
{
    mtwnSpring spring = {{7.0}, 9, 0};
    CHECK(mtwnMakeSpring(0.0, 1.0, 1.0, &spring) == mtwn_errorInvalid, "no stiffness");
    CHECK(mtwnMakeSpring(1.0, -1.0, 1.0, &spring) == mtwn_errorInvalid, "negative damping");
    CHECK(mtwnMakeSpring(1.0, 1.0, 0.0, &spring) == mtwn_errorInvalid, "no mass");
    CHECK(mtwnMakeSpring(HUGE_VAL, 1.0, 1.0, &spring) == mtwn_errorInvalid, "infinite stiffness");
    CHECK(mtwnMakeSpring(1e300, 1.0, 1e-300, &spring) == mtwn_errorInvalid,
          "a frequency that overflows");
    CHECK(mtwnMakeSpringFromFrequency(0.0, 1.0, &spring) == mtwn_errorInvalid, "no frequency");
    CHECK(mtwnMakeSpringFromFrequency(1.0, -0.1, &spring) == mtwn_errorInvalid, "negative ratio");
    CHECK(mtwnMakeSpringFromFrequency(1.0, (double)NAN, &spring) == mtwn_errorInvalid, "NaN ratio");
    CHECK(mtwnMakeSpringFromFrequency(1e200, 1.0, &spring) == mtwn_errorInvalid,
          "a squared frequency that overflows");
    CHECK(mtwnMakeSpringFromFrequency(1e-170, 1.0, &spring) == mtwn_errorInvalid,
          "a squared frequency that underflows");
    CHECK(mtwnMakeSpringFromFrequency(1.0, 1e308, &spring) == mtwn_errorInvalid,
          "a decay rate that overflows");
    CHECK(mtwnMakeSpringFromDuration(0.0, 0.0, &spring) == mtwn_errorInvalid, "no duration");
    CHECK(mtwnMakeSpringFromDuration(1.0, -1.0, &spring) == mtwn_errorInvalid, "bounce -1");
    CHECK(mtwnMakeSpringFromDuration(1.0, 1.01, &spring) == mtwn_errorInvalid, "bounce above 1");
    CHECK(mtwnMakeSpringFromDuration(1.0, (double)NAN, &spring) == mtwn_errorInvalid, "NaN bounce");
    CHECK(mtwnMakeSpringFromFrequency(1.0, 1.0, NULL) == mtwn_errorInvalid, "null out");
    CHECK(spring.parameters[0] == 7.0 && spring.regime == 9,
          "refusals leave the out-parameter as it was");
    CHECK(mtwnMakeSpringFromDuration(1.0, 1.0, &spring) == mtwn_success &&
              isinf(mtwnSpringSettlingTime(&spring, 1.0, 0.0, 1e-3, 1e-3)),
          "bounce 1 never settles");
}

typedef struct SettleCase
{
    double hertz;
    double ratio;
    double offset;
    double velocity;
} SettleCase;

static const SettleCase s_settles[] = {
    {1.0, 0.5, 1.0, 0.0},
    {2.0, 0.1, -100.0, 30.0},
    {1.5, 1.0, 1.0, 0.0},
    {1.5, 1.0, 1.0, -40.0},
    {1.0, 0.999999, 1.0, 0.0},
    {1.0, 3.0, 1.0, 0.0},
    {0.5, 50.0, 1.0, 5.0},
    {4.0, 0.7, 0.0, 300.0},
    {1.0, 1.5, 1e-6, 0.0},
    // Inside both and falling from the start: the offset bound peaks
    // before 0, and above a ratio of sqrt(1/2) the velocity bound too.
    {1.0, 0.5, 1e-4, -0.5 * 6.283185307179586 * 1e-4 * 0.999},
    {1.0, 0.9, 1e-4, -0.9 * 6.283185307179586 * 1e-4 * 0.999},
};

static void TestSettling(void)
{
    for (size_t c = 0; c < sizeof s_settles / sizeof s_settles[0]; ++c)
    {
        const SettleCase* sc = &s_settles[c];
        mtwnSpring spring;
        CHECK(mtwnMakeSpringFromFrequency(sc->hertz, sc->ratio, &spring) == mtwn_success, "made");
        const double tolerance = 1e-3;
        const double speed = 1e-2;
        double settle = mtwnSpringSettlingTime(&spring, sc->offset, sc->velocity, tolerance, speed);
        // Sampled past it, the spring stays within both; just before the
        // last time it is out of them, it is not settled yet.
        bool within = isfinite(settle) && settle >= 0.0;
        double lastOut = 0.0;
        double horizon = (isfinite(settle) ? settle : 0.0) * 3.0 + 1.0;
        for (int k = 0; k <= 200000; ++k)
        {
            double t = horizon * (double)k / 200000.0;
            double offset = 0.0;
            double velocity = 0.0;
            mtwnSampleSpring(&spring, sc->offset, sc->velocity, t, &offset, &velocity);
            bool out = fabs(offset) > tolerance || fabs(velocity) > speed;
            lastOut = out ? t : lastOut;
            within = within && !(out && t >= settle);
        }
        if (!within || settle > 1.2 * lastOut + 0.01)
        {
            printf("  settle case %zu: %.6g, last out %.6g\n", c, settle, lastOut);
        }
        CHECK(within, "a spring stays settled from its settling time");
        CHECK(settle <= 1.2 * lastOut + 0.01, "and its settling time is not far past the last");
    }
    mtwnSpring spring;
    CHECK(mtwnMakeSpringFromFrequency(1.0, 0.5, &spring) == mtwn_success, "made");
    CHECK(mtwnSpringSettlingTime(&spring, 1e-4, 1e-4, 1e-3, 1e-2) == 0.0,
          "a start within both is settled");
    CHECK(mtwnSpringSettlingTime(&spring, 0.0, 0.0, 1e-3, 1e-2) == 0.0, "at rest");
    CHECK(isinf(mtwnSpringSettlingTime(&spring, 1.0, 0.0, 0.0, 1e-2)), "a zero tolerance");
    mtwnSpring undamped;
    CHECK(mtwnMakeSpringFromFrequency(1.0, 0.0, &undamped) == mtwn_success, "undamped");
    CHECK(isinf(mtwnSpringSettlingTime(&undamped, 0.0, 0.009, 1e-3, 1e-2)),
          "an undamped start inside both that swings out of them never settles");
    CHECK(mtwnSpringSettlingTime(&undamped, 0.0, 0.001, 1e-3, 1e-2) == 0.0,
          "one whose swing stays inside both is settled at once");
    CHECK(isinf(mtwnSpringSettlingTime(&spring, 1.0, 0.0, 1e-3, (double)NAN)), "a NaN one");
}

// The settling bounds of record mtwn-0023, written again: |x| <= e^(-r t)
// (a + b min(t, cap)) for the offset, with a = |x0|, b = |v0 + alpha x0|,
// and for the velocity with a = |v0|, b = |alpha v0 + w0^2 x0|; rate
// alpha and cap 1 / w below critical damping, rate w0^2 / (alpha + w) and
// cap 1 / (2 w) from it.
typedef struct WideBound
{
    Wide a;
    Wide b;
    Wide rate;
    Wide cap;
} WideBound;

static Wide BoundAt(const WideBound* bound, Wide t)
{
    Wide g = t < bound->cap ? t : bound->cap;
    return expl(-(long double)(bound->rate * t)) * (long double)(bound->a + bound->b * g);
}

// Where a bound is largest, by ternary search: it is log-concave, so
// unimodal.
static Wide PeakTime(const WideBound* bound)
{
    Wide low = 0;
    Wide high = 64 / bound->rate;
    for (int i = 0; i < 300; ++i)
    {
        Wide a = low + (high - low) / 3;
        Wide b = high - (high - low) / 3;
        if (BoundAt(bound, a) < BoundAt(bound, b))
        {
            low = a;
        }
        else
        {
            high = b;
        }
    }
    return (low + high) / 2;
}

static Wide BoundPeak(const WideBound* bound)
{
    return BoundAt(bound, PeakTime(bound));
}

// The last time the bound exceeds the tolerance: none if its peak does
// not, else by bisection where it falls, past the peak.
static double LastAbove(const WideBound* bound, Wide tolerance)
{
    Wide low = PeakTime(bound);
    if (!(BoundAt(bound, low) > tolerance))
    {
        return 0.0;
    }
    Wide high = low + 1;
    while (BoundAt(bound, high) > tolerance)
    {
        high = low + 2 * (high - low);
    }
    for (int i = 0; i < 200; ++i)
    {
        Wide middle = (low + high) / 2;
        if (BoundAt(bound, middle) > tolerance)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return (double)high;
}

// The bounds of a start on a spring.
static void MakeBounds(const SettleCase* sc, WideBound* offsetOut, WideBound* velocityOut)
{
    Wide w0 = (Wide)s_twoPi * (Wide)sc->hertz;
    Wide ratio = (Wide)sc->ratio;
    Wide alpha = ratio * w0;
    Wide w = w0 * (Wide)sqrt((double)fabsl((long double)((1 - ratio) * (1 + ratio))));
    Wide x0 = (Wide)sc->offset;
    Wide v0 = (Wide)sc->velocity;
    WideBound offset = {0, 0, alpha, (Wide)HUGE_VAL};
    if (ratio < 1)
    {
        offset.cap = 1 / w;
    }
    else
    {
        offset.rate = w0 * w0 / (alpha + w);
        offset.cap = w > 0 ? 1 / (2 * w) : (Wide)HUGE_VAL;
    }
    WideBound velocity = offset;
    offset.a = x0 < 0 ? -x0 : x0;
    offset.b = v0 + alpha * x0;
    offset.b = offset.b < 0 ? -offset.b : offset.b;
    velocity.a = v0 < 0 ? -v0 : v0;
    velocity.b = alpha * v0 + w0 * w0 * x0;
    velocity.b = velocity.b < 0 ? -velocity.b : velocity.b;
    *offsetOut = offset;
    *velocityOut = velocity;
}

static bool SettlesAsBounded(const SettleCase* sc, Wide offsetTolerance, Wide velocityTolerance)
{
    mtwnSpring spring;
    if (mtwnMakeSpringFromFrequency(sc->hertz, sc->ratio, &spring) != mtwn_success)
    {
        return false;
    }
    WideBound offset;
    WideBound velocity;
    MakeBounds(sc, &offset, &velocity);
    double expected = LastAbove(&offset, offsetTolerance);
    double speed = LastAbove(&velocity, velocityTolerance);
    expected = speed > expected ? speed : expected;
    double settle = mtwnSpringSettlingTime(&spring, sc->offset, sc->velocity,
                                           (double)offsetTolerance, (double)velocityTolerance);
    bool close = fabs(settle - expected) <= 1e-12 * expected + 1e-13;
    if (!close)
    {
        printf("  bound: %.17g, expected %.17g\n", settle, expected);
    }
    return close;
}

static void TestSettlingTimesAreTheBounds(void)
{
    int wrong = 0;
    for (size_t c = 0; c < sizeof s_settles / sizeof s_settles[0]; ++c)
    {
        wrong += !SettlesAsBounded(&s_settles[c], (Wide)1e-3, (Wide)1e-2);
    }
    CHECK(wrong == 0, "a settling time is where its bounds cross the tolerances last");
    // Starts inside both tolerances whose offset bound then rises: with
    // the tolerance just above the bound's peak they are settled at once,
    // just below it they are not.
    static const SettleCase rising[] = {
        {0.2, 1.0, 0.0, 0.005},
        {0.2, 1.0, 2e-4, 0.004},
        {0.3, 0.1, 0.0, 0.004},
        {0.2, 2.0, 1e-4, 0.006},
    };
    wrong = 0;
    for (size_t c = 0; c < sizeof rising / sizeof rising[0]; ++c)
    {
        WideBound offset;
        WideBound velocity;
        MakeBounds(&rising[c], &offset, &velocity);
        Wide peak = BoundPeak(&offset);
        Wide fast = BoundPeak(&velocity) * 2;
        wrong += !SettlesAsBounded(&rising[c], peak * (Wide)(1.0 + 1e-9), fast);
        wrong += !SettlesAsBounded(&rising[c], peak * (Wide)(1.0 - 1e-6), fast);
        mtwnSpring spring;
        wrong +=
            mtwnMakeSpringFromFrequency(rising[c].hertz, rising[c].ratio, &spring) != mtwn_success;
        wrong += mtwnSpringSettlingTime(&spring, rising[c].offset, rising[c].velocity,
                                        (double)(peak * (Wide)(1.0 + 1e-9)), (double)fast) != 0.0;
        wrong += !(mtwnSpringSettlingTime(&spring, rising[c].offset, rising[c].velocity,
                                          (double)(peak * (Wide)(1.0 - 1e-6)), (double)fast) > 0.0);
    }
    CHECK(wrong == 0, "a start within both but rising is settled only if its peak is");
}

// Starts whose bounds the search met at their edges (found by fuzz_values
// with record mtwn-0063's bounded decay): a critical spring whose
// velocity bound peaks at a time that rounds to nearly 0, where Newton
// once started on a slope of 0; and products overflowing with opposite
// signs, once a NaN sum.
static void TestSettlingAtTheEdges(void)
{
    static const SettleCase flat = {1.0 / 3.0, 1.0, 0x1.56e1fc2f8f359p-997, 0x1.443418p+9};
    mtwnSpring spring;
    CHECK(mtwnMakeSpringFromFrequency(flat.hertz, flat.ratio, &spring) == mtwn_success,
          "a critical spring");
    double settle =
        mtwnSpringSettlingTime(&spring, flat.offset, flat.velocity, flat.offset, 0x1.4bfcp-1);
    CHECK(isfinite(settle) && settle > 0.0, "a peak at nearly 0 settles");
    CHECK(SettlesAsBounded(&flat, (Wide)flat.offset, (Wide)0x1.4bfcp-1),
          "where its bounds cross the tolerances last");
    mtwnSpring stiff;
    CHECK(mtwnMakeSpring(1e300, 1e300, 1.0, &stiff) == mtwn_success, "a stiff spring");
    settle = mtwnSpringSettlingTime(&stiff, 210.8154296875 - 1e300, 1e300, 0.5, 1.0);
    CHECK(settle == HUGE_VAL, "overflowing bounds never end it early");
}

typedef struct EasingCase
{
    double hertz;
    double ratio;
    double duration;
    double velocity;
} EasingCase;

static const EasingCase s_easings[] = {
    {1.0, 0.5, 1.0, 0.0},  {2.0, 0.1, 1.5, 0.0}, {1.5, 1.0, 0.8, 2.0},
    {1.0, 3.0, 2.0, -1.0}, {3.0, 0.0, 0.4, 0.0}, {1.0, 1.0 - 1e-9, 1.0, 0.0},
};

static void TestSpringEasings(void)
{
    static const mtwnEaseMode modes[] = {mtwn_easeIn, mtwn_easeOut, mtwn_easeInOut, mtwn_easeOutIn};
    int wrong = 0;
    double worst = 0.0;
    for (size_t c = 0; c < sizeof s_easings / sizeof s_easings[0]; ++c)
    {
        const EasingCase* ec = &s_easings[c];
        mtwnSpring spring;
        mtwnEasing easing;
        wrong += mtwnMakeSpringFromFrequency(ec->hertz, ec->ratio, &spring) != mtwn_success;
        wrong += mtwnMakeSpringEasing(&spring, ec->duration, ec->velocity, mtwn_easeIn, &easing) !=
                 mtwn_success;
        // The spring from -1 sampled through its own functions, divided by
        // its value at the duration.
        double end = 0.0;
        mtwnSampleSpring(&spring, -1.0, ec->velocity, ec->duration, &end, NULL);
        for (int k = 0; k <= 1000; ++k)
        {
            double p = (double)k / 1000.0;
            double offset = 0.0;
            mtwnSampleSpring(&spring, -1.0, ec->velocity, p * ec->duration, &offset, NULL);
            double error = fabs(mtwnEase(&easing, p, false) - (1.0 + offset) / (1.0 + end));
            worst = error > worst ? error : worst;
        }
        for (int m = 0; m < 4; ++m)
        {
            wrong += mtwnSetEaseMode(&easing, modes[m]) != mtwn_success;
            wrong += mtwnEase(&easing, 0.0, false) != 0.0 || mtwnEase(&easing, 1.0, false) != 1.0;
            wrong += m >= 2 && mtwnEase(&easing, 0.5, false) != 0.5;
            wrong += mtwnEase(&easing, -0.5, true) != 0.0 || mtwnEase(&easing, 2.0, false) != 1.0;
        }
        wrong += mtwnIsEasingMonotonic(&easing);
    }
    CHECK(wrong == 0, "a spring easing lands exactly on 0 and 1 in every mode, clamped outside");
    CHECK(worst <= 1e-14, "and is the spring from -1 over its duration, normalized");
}

static void TestSpringEasingFitsLinear(void)
{
    mtwnSpring spring;
    mtwnEasing easing;
    CHECK(mtwnMakeSpringFromDuration(0.5, 0.3, &spring) == mtwn_success &&
              mtwnMakeSpringEasing(&spring, 1.2, 0.0, mtwn_easeIn, &easing) == mtwn_success,
          "SwiftUI's bouncy spring over 1.2 s");
    static mtwnLinearPoint points[512];
    uint32_t count = 0;
    CHECK(mtwnFitLinearEasing(&easing, 1e-3, points, 512, &count) == mtwn_success, "fitted");
    mtwnEasing fitted;
    CHECK(mtwnMakeLinearEasing(points, count, &fitted) == mtwn_success, "as linear()");
    double worst = 0.0;
    for (int k = 0; k <= 100000; ++k)
    {
        double p = (double)k / 100000.0;
        double error = fabs(mtwnEase(&fitted, p, false) - mtwnEase(&easing, p, false));
        worst = error > worst ? error : worst;
    }
    if (!(worst <= 1e-3))
    {
        printf("  spring linear(): %u points, %.3g\n", count, worst);
    }
    CHECK(worst <= 1e-3, "a CSS linear() of a spring within its tolerance");
}

static void TestSpringEasingRefusals(void)
{
    mtwnSpring spring;
    CHECK(mtwnMakeSpringFromFrequency(1.0, 0.5, &spring) == mtwn_success, "made");
    mtwnEasing easing = mtwnLinearEasing();
    CHECK(mtwnMakeSpringEasing(&spring, 0.0, 0.0, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "no duration");
    CHECK(mtwnMakeSpringEasing(&spring, HUGE_VAL, 0.0, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "an infinite duration");
    CHECK(mtwnMakeSpringEasing(&spring, 1.0, (double)NAN, mtwn_easeIn, &easing) ==
              mtwn_errorInvalid,
          "a NaN velocity");
    CHECK(mtwnMakeSpringEasing(&spring, 1.0, 0.0, 4, &easing) == mtwn_errorInvalid,
          "an unknown mode");
    CHECK(mtwnMakeSpringEasing(NULL, 1.0, 0.0, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "no spring");
    CHECK(mtwnMakeSpringEasing(&spring, 1.0, 0.0, mtwn_easeIn, NULL) == mtwn_errorInvalid,
          "null out");
    CHECK(mtwnMakeSpringEasing(&spring, 1e300, 1e300, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "a scaled velocity that overflows");
    CHECK(mtwnMakeSpringEasing(&spring, 1e200, 0.0, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "a scaled frequency whose square overflows");
    CHECK(mtwnMakeSpringEasing(&spring, 1e-170, 0.0, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "a duration too short for the spring to move");
    CHECK(mtwnMakeSpringEasing(&spring, (double)NAN, 0.0, mtwn_easeIn, &easing) ==
              mtwn_errorInvalid,
          "a NaN duration");
    CHECK(mtwnMakeSpringEasing(&spring, 1.0, HUGE_VAL, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "an infinite velocity");
    CHECK(easing.kind == mtwn_easingLinear, "refusals leave the out-parameter as it was");
}

// The makers' ranges at their ends (found by mutants): a stiffness of 0
// is refused, a bounce of 1 (no damping) taken, a spring easing in every
// mode taken.
static void TestRangeEnds(void)
{
    mtwnSpring spring;
    CHECK(mtwnMakeSpring(0.0, 1.0, 1.0, &spring) == mtwn_errorInvalid, "no stiffness refused");
    CHECK(mtwnMakeSpringFromDuration(0.5, 1.0, &spring) == mtwn_success, "a bounce of 1 taken");
    CHECK(mtwnMakeSpringFromDuration(0.5, 0.2, &spring) == mtwn_success, "a spring");
    for (uint32_t mode = mtwn_easeIn; mode <= mtwn_easeOutIn; ++mode)
    {
        mtwnEasing easing;
        CHECK(mtwnMakeSpringEasing(&spring, 1.0, 0.0, (mtwnEaseMode)mode, &easing) == mtwn_success,
              "a spring easing in each mode");
    }
}

int main(void)
{
    TestRangeEnds();
    TestAgainstIntegration();
    TestMakersAgree();
    TestRegimesMeet();
    TestEndsOfTime();
    TestRefusals();
    TestSettling();
    TestSettlingTimesAreTheBounds();
    TestSettlingAtTheEdges();
    TestSpringEasings();
    TestSpringEasingFitsLinear();
    TestSpringEasingRefusals();
    return s_failures == 0 ? 0 : 1;
}
