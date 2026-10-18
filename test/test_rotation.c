// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Angles and rotations (records mtwn-0015, mtwn-0020, mtwn-0025): angles
// on CSS Color 4's hue examples in radians and with turns; rotations
// against slerp written again in long double, at constant angular speed,
// the shortest way and the longest.

#include "test_harness.h"

#include "maul-tween/interpolate.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_ACOS acosl
#define WIDE_SIN  sinl
#define WIDE_SQRT sqrtl
#else
typedef double Wide;
#define WIDE_ACOS acos
#define WIDE_SIN  sin
#define WIDE_SQRT sqrt
#endif

static const double s_pi = 3.141592653589793;

static double Degrees(double degrees)
{
    return degrees * s_pi / 180.0;
}

// The angle a - b brought into [0, 2 pi).
static double Wrapped(double angle)
{
    double turn = 2.0 * s_pi;
    return angle - turn * floor(angle / turn);
}

static void TestAngles(void)
{
    CHECK(mtwnMixAngle(0.0, 4.0 * s_pi, mtwn_angleRaw, 0.5) == 2.0 * s_pi, "raw keeps turns");
    CHECK(mtwnMixAngle(0.3, 2.5, mtwn_angleRaw, 1.0) == 2.5 &&
              mtwnMixAngle(0.3, 2.5, mtwn_angleRaw, 0.0) == 0.3,
          "the ends exactly");
    // CSS Color 4's examples, in radians.
    double tolerance = 1e-12;
    CHECK(fabs(mtwnMixAngle(Degrees(30), Degrees(90), mtwn_angleShorter, 0.5) - Degrees(60)) <=
              tolerance,
          "shorter: 30 to 90 through 60");
    CHECK(fabs(Wrapped(mtwnMixAngle(Degrees(30), Degrees(90), mtwn_angleLonger, 0.5)) -
               Degrees(240)) <= tolerance,
          "longer: through 240");
    CHECK(fabs(mtwnMixAngle(Degrees(30), Degrees(190), mtwn_angleIncreasing, 0.5) - Degrees(110)) <=
              tolerance,
          "increasing: through 110");
    CHECK(fabs(Wrapped(mtwnMixAngle(Degrees(30), Degrees(190), mtwn_angleDecreasing, 0.5)) -
               Degrees(290)) <= tolerance,
          "decreasing: through 290");
    CHECK(fabs(Wrapped(mtwnMixAngle(Degrees(350), Degrees(10), mtwn_angleShorter, 0.25)) -
               Degrees(355)) <= tolerance,
          "shorter across 0");
    CHECK(fabs(mtwnMixAngle(1.0, 1.0, mtwn_angleLonger, 0.5) - (1.0 + s_pi)) <= tolerance,
          "longer between equal angles turns all the way");
    CHECK(mtwnMixAngle(1.0, 1.0, mtwn_angleIncreasing, 0.5) == 1.0 &&
              mtwnMixAngle(1.0, 1.0, mtwn_angleDecreasing, 0.5) == 1.0,
          "increasing and decreasing hold equal angles");
    // Angles many turns out keep their turn count at 0 and move the end.
    CHECK(fabs(mtwnMixAngle(100.0, 100.5 - 6.0 * s_pi, mtwn_angleShorter, 0.5) - 100.25) <= 1e-12,
          "shorter many turns out");
    CHECK(mtwnMixAngle(100.0, 100.5, mtwn_angleShorter, 1.0) == 100.5,
          "an end moved by no turn stays exact");
    CHECK(fabs(mtwnMixAngle(0.0, Degrees(-90), mtwn_angleIncreasing, 1.0) - Degrees(270)) <=
              tolerance,
          "increasing moves the end by a turn");
    CHECK(fabs(mtwnMixAngle(0.0, 1.0, mtwn_angleShorter, 2.0) - 2.0) <= tolerance,
          "past the end it extends");
    CHECK(mtwnMixAngle(0.0, 3.0, 9, 0.5) == 1.5, "an unknown method is raw");
    CHECK(fabs(mtwnMixAngle(0.0, 4.0 + 4.0 * s_pi, mtwn_angleShorter, 0.5) -
               (4.0 - 2.0 * s_pi) / 2.0) <= tolerance,
          "shorter takes the nearest turn, not the one below");
    CHECK(fabs(mtwnMixAngle(0.0, s_pi, mtwn_angleLonger, 0.5) - s_pi / 2.0) <= tolerance,
          "longer at exactly half a turn keeps its direction, as CSS's does");
    CHECK(fabs(mtwnMixAngle(Degrees(30), Degrees(90), mtwn_angleLonger, 0.5) - Degrees(-120)) <=
              tolerance,
          "longer from 30 to 90 goes down through -120");
    CHECK(mtwnMixAngle(1e16, 1.0, mtwn_angleRaw, 1.0) == 1.0,
          "the end exactly where from + t (to - from) would miss it");
}

// The rotation by angle radians about a unit axis.
static mtwnQuat AxisAngle(double x, double y, double z, double angle)
{
    double s = sin(angle / 2.0);
    return (mtwnQuat){(float)(x * s), (float)(y * s), (float)(z * s), (float)cos(angle / 2.0)};
}

// The angle between two rotations as 4D unit vectors, in long double.
static Wide Between(mtwnQuat a, mtwnQuat b)
{
    Wide dot = (Wide)a.x * (Wide)b.x + (Wide)a.y * (Wide)b.y + (Wide)a.z * (Wide)b.z +
               (Wide)a.w * (Wide)b.w;
    Wide na = WIDE_SQRT((Wide)a.x * (Wide)a.x + (Wide)a.y * (Wide)a.y + (Wide)a.z * (Wide)a.z +
                        (Wide)a.w * (Wide)a.w);
    Wide nb = WIDE_SQRT((Wide)b.x * (Wide)b.x + (Wide)b.y * (Wide)b.y + (Wide)b.z * (Wide)b.z +
                        (Wide)b.w * (Wide)b.w);
    Wide c = dot / (na * nb);
    c = c > 1 ? 1 : (c < -1 ? -1 : c);
    return WIDE_ACOS(c);
}

// Shoemake's slerp in long double, from the ends' own floats.
static void SlerpWide(mtwnQuat a, mtwnQuat b, Wide t, Wide out[4])
{
    Wide p[4] = {(Wide)a.x, (Wide)a.y, (Wide)a.z, (Wide)a.w};
    Wide q[4] = {(Wide)b.x, (Wide)b.y, (Wide)b.z, (Wide)b.w};
    Wide np = WIDE_SQRT(p[0] * p[0] + p[1] * p[1] + p[2] * p[2] + p[3] * p[3]);
    Wide nq = WIDE_SQRT(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    Wide dot = 0;
    for (int i = 0; i < 4; ++i)
    {
        p[i] /= np;
        q[i] /= nq;
        dot += p[i] * q[i];
    }
    if (dot < 0)
    {
        dot = -dot;
        for (int i = 0; i < 4; ++i)
        {
            q[i] = -q[i];
        }
    }
    dot = dot > 1 ? 1 : dot;
    Wide theta = WIDE_ACOS(dot);
    for (int i = 0; i < 4; ++i)
    {
        out[i] = theta == 0 ? p[i]
                            : (WIDE_SIN((1 - t) * theta) * p[i] + WIDE_SIN(t * theta) * q[i]) /
                                  WIDE_SIN(theta);
    }
}

static void TestRotationsAgainstSlerp(void)
{
    double worst = 0.0;
    double worstSpeed = 0.0;
    static const double angles[] = {1e-6, 1e-3, 0.1, 1.0, 2.0, 3.0, 3.14};
    for (int a = 0; a < 7; ++a)
    {
        mtwnQuat from = AxisAngle(0.0, 0.6, 0.8, 0.4);
        mtwnQuat to = AxisAngle(0.48, 0.6, 0.64, 0.4 + angles[a]);
        mtwnRotationMix mix;
        CHECK(mtwnMakeRotationMix(from, to, mtwn_rotationShortest, &mix) == mtwn_success, "made");
        Wide total = Between(from, to);
        total = total > (Wide)(s_pi / 2) ? (Wide)s_pi - total : total;
        for (int k = 1; k < 64; ++k)
        {
            double t = (double)k / 64.0;
            mtwnQuat q = mtwnMixRotation(&mix, t);
            Wide reference[4];
            SlerpWide(from, to, (Wide)t, reference);
            double e = fabs((double)q.x - (double)reference[0]) +
                       fabs((double)q.y - (double)reference[1]) +
                       fabs((double)q.z - (double)reference[2]) +
                       fabs((double)q.w - (double)reference[3]);
            worst = e > worst ? e : worst;
            if (angles[a] >= 0.1)
            {
                double speed = fabs((double)(Between(from, q) - (Wide)t * total)) / angles[a];
                worstSpeed = speed > worstSpeed ? speed : worstSpeed;
            }
        }
    }
    if (!(worst <= 1.5e-7) || !(worstSpeed <= 1.5e-7))
    {
        printf("  rotation %.3g, speed %.3g\n", worst, worstSpeed);
    }
    CHECK(worst <= 1.5e-7, "slerp within float rounding of long double, tiny angles included");
    CHECK(worstSpeed <= 1.5e-7, "at constant angular speed");
}

static void TestPaths(void)
{
    mtwnQuat identity = {0.0f, 0.0f, 0.0f, 1.0f};
    mtwnQuat quarter = AxisAngle(0.0, 0.0, 1.0, s_pi / 2.0);
    mtwnRotationMix mix;
    CHECK(mtwnMakeRotationMix(identity, quarter, mtwn_rotationLongest, &mix) == mtwn_success,
          "a quarter turn the long way");
    mtwnQuat middle = mtwnMixRotation(&mix, 0.5);
    // Three quarters the other way, half of it at the middle: 135 degrees
    // about -z.
    double expected = 3.0 * s_pi / 4.0;
    CHECK(fabs((double)middle.z + sin(expected / 2.0)) <= 1e-6 &&
              fabs((double)middle.w - cos(expected / 2.0)) <= 1e-6,
          "the long way through 135 degrees about -z");
    mtwnQuat negated = {-quarter.x, -quarter.y, -quarter.z, -quarter.w};
    CHECK(mtwnMakeRotationMix(identity, negated, mtwn_rotationShortest, &mix) == mtwn_success,
          "the same quarter, given as -q");
    middle = mtwnMixRotation(&mix, 0.5);
    CHECK(fabs((double)middle.z - sin(s_pi / 8.0)) <= 1e-6, "still the short way");
    mtwnQuat end = mtwnMixRotation(&mix, 1.0);
    CHECK(end.x == quarter.x && end.y == quarter.y && end.z == quarter.z && end.w == quarter.w,
          "the end negated back where the path needed it");
    mtwnQuat start = mtwnMixRotation(&mix, 0.0);
    CHECK(start.w == 1.0f && start.z == 0.0f, "the start exactly");
    CHECK(mtwnMakeRotationMix((mtwnQuat){0.0f, 0.0f, 0.0f, 3.0f}, quarter, mtwn_rotationShortest,
                              &mix) == mtwn_success &&
              mtwnMixRotation(&mix, 0.0).w == 1.0f,
          "an end given unnormalized comes out normalized");
    CHECK(mtwnMakeRotationMix(identity, identity, mtwn_rotationShortest, &mix) == mtwn_success &&
              mtwnMixRotation(&mix, 0.3).w == 1.0f,
          "the same rotation holds still");
    CHECK(mtwnMakeRotationMix(identity, identity, mtwn_rotationLongest, &mix) == mtwn_errorInvalid,
          "the long way between equal rotations has no axis");
    mtwnQuat scaled = {0.0f, 0.0f, 0.0f, 3.0f};
    CHECK(mtwnMakeRotationMix(scaled, quarter, mtwn_rotationShortest, &mix) == mtwn_success &&
              fabsf(mtwnMixRotation(&mix, 0.5).w - (float)cos(s_pi / 8.0)) <= 1e-6f,
          "ends are normalized");
    CHECK(mtwnMakeRotationMix(identity, quarter, mtwn_rotationShortest, &mix) == mtwn_success,
          "made");
    mtwnQuat past = mtwnMixRotation(&mix, 2.0);
    CHECK(fabs((double)past.z - sin(s_pi / 2.0)) <= 1e-6 && fabs((double)past.w) <= 1e-6,
          "past the end it keeps turning: half a turn at 2");
    double norm =
        sqrt((double)(past.x * past.x + past.y * past.y + past.z * past.z + past.w * past.w));
    CHECK(fabs(norm - 1.0) <= 1e-6, "of unit length");
}

// The methods at exactly half a turn apart, and at none, as CSS Color 4
// adjusts hues: shorter leaves a half turn either way as it is; longer
// adjusts only (0, half) and (-half, 0], so a half turn is left too, and
// no difference becomes a whole turn (found by mutants).
static void TestHalfTurns(void)
{
    double half = s_pi;
    CHECK(mtwnMixAngle(0.0, half, mtwn_angleShorter, 0.5) == 0.5 * half &&
              mtwnMixAngle(half, 0.0, mtwn_angleShorter, 0.5) == 0.5 * half,
          "shorter: a half turn either way, straight");
    CHECK(mtwnMixAngle(0.0, half, mtwn_angleLonger, 0.5) == 0.5 * half &&
              mtwnMixAngle(half, 0.0, mtwn_angleLonger, 0.5) == 0.5 * half,
          "longer: a half turn either way, straight");
    CHECK(fabs(mtwnMixAngle(0.5, 0.5, mtwn_angleLonger, 0.5) - (0.5 + half)) < 1e-12,
          "longer: no difference, a whole turn");
}

// A slerp extrapolated so far that its phase holds no angle, or
// overflows: still a unit rotation (found by the values' fuzz, NaN
// before).
static void TestFarExtrapolation(void)
{
    mtwnRotationMix mix;
    CHECK(mtwnMakeRotationMix((mtwnQuat){0.0f, 0.0f, 0.0f, 1.0f},
                              (mtwnQuat){0.0f, 1.0f, 0.0f, 0.0f}, mtwn_rotationShortest,
                              &mix) == mtwn_success,
          "a half turn, pi / 2 apart as quaternions");
    bool bounded = true;
    // A phase of 1e18 too, below where the guard once stood untested.
    const double far[] = {-1.5e300, 1.5e300, 1.7e308, 1e18 / (0.5 * s_pi)};
    for (int i = 0; i < 4; ++i)
    {
        mtwnQuat q = mtwnMixRotation(&mix, far[i]);
        double norm = sqrt((double)q.x * (double)q.x + (double)q.y * (double)q.y +
                           (double)q.z * (double)q.z + (double)q.w * (double)q.w);
        bounded = bounded && fabs(norm - 1.0) < 1e-6;
    }
    CHECK(bounded, "far past its ends, a unit rotation");
    mtwnQuat lost = mtwnMixRotation(&mix, (double)NAN);
    CHECK(isnan(lost.w), "a NaN progress is NaN, not a reduction");
}

static void TestRefusals(void)
{
    mtwnRotationMix mix;
    mtwnQuat identity = {0.0f, 0.0f, 0.0f, 1.0f};
    CHECK(mtwnMakeRotationMix((mtwnQuat){0.0f, 0.0f, 0.0f, 0.0f}, identity, mtwn_rotationShortest,
                              &mix) == mtwn_errorInvalid,
          "zero");
    CHECK(mtwnMakeRotationMix(identity, (mtwnQuat){NAN, 0.0f, 0.0f, 1.0f}, mtwn_rotationShortest,
                              &mix) == mtwn_errorInvalid,
          "NaN");
    CHECK(mtwnMakeRotationMix(identity, (mtwnQuat){INFINITY, 0.0f, 0.0f, 1.0f},
                              mtwn_rotationShortest, &mix) == mtwn_errorInvalid,
          "infinite");
    CHECK(mtwnMakeRotationMix(identity, (mtwnQuat){0.0f, 0.0f, 0.6f, 0.8f}, 2, &mix) ==
              mtwn_errorInvalid,
          "an unknown path");
    CHECK(mtwnMakeRotationMix(identity, identity, mtwn_rotationShortest, NULL) == mtwn_errorInvalid,
          "null out");
}

// Equal angles mix to that angle at any progress (record mtwn-0070).
static void TestAngleFarProgress(void)
{
    CHECK(mtwnMixAngle(1.25, 1.25, mtwn_angleRaw, 1e20) == 1.25 &&
              mtwnMixAngle(1.25, 1.25, mtwn_angleShorter, -1e20) == 1.25,
          "equal ends, far out: the end");
}

int main(void)
{
    TestAngleFarProgress();
    TestHalfTurns();
    TestFarExtrapolation();
    TestAngles();
    TestRotationsAgainstSlerp();
    TestPaths();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
