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
    {1.0, 0.5, 1.0, 0.0},   {2.0, 0.1, -100.0, 30.0},  {1.5, 1.0, 1.0, 0.0},
    {1.5, 1.0, 1.0, -40.0}, {1.0, 0.999999, 1.0, 0.0}, {1.0, 3.0, 1.0, 0.0},
    {0.5, 50.0, 1.0, 5.0},  {4.0, 0.7, 0.0, 300.0},    {1.0, 1.5, 1e-6, 0.0},
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
    CHECK(isinf(mtwnSpringSettlingTime(&spring, 1.0, 0.0, 1e-3, (double)NAN)), "a NaN one");
}

int main(void)
{
    TestAgainstIntegration();
    TestMakersAgree();
    TestRegimesMeet();
    TestEndsOfTime();
    TestRefusals();
    TestSettling();
    return s_failures == 0 ? 0 : 1;
}
