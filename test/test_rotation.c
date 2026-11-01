// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Angles and rotations: angles
// on CSS Color 4's hue examples in radians and with turns; rotations
// against slerp written again in long double, at constant angular speed,
// the shortest way and the longest.

#include "test_harness.h"
#include "test_values.h"

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
    CHECK(MixAngle(0.0, 4.0 * s_pi, mtwn_angleRaw, 0.5) == 2.0 * s_pi, "raw keeps turns");
    CHECK(MixAngle(0.3, 2.5, mtwn_angleRaw, 1.0) == 2.5 &&
              MixAngle(0.3, 2.5, mtwn_angleRaw, 0.0) == 0.3,
          "the ends exactly");
    // CSS Color 4's examples, in radians.
    double tolerance = 1e-12;
    CHECK(fabs(MixAngle(Degrees(30), Degrees(90), mtwn_angleShorter, 0.5) - Degrees(60)) <=
              tolerance,
          "shorter: 30 to 90 through 60");
    CHECK(fabs(Wrapped(MixAngle(Degrees(30), Degrees(90), mtwn_angleLonger, 0.5)) - Degrees(240)) <=
              tolerance,
          "longer: through 240");
    CHECK(fabs(MixAngle(Degrees(30), Degrees(190), mtwn_angleIncreasing, 0.5) - Degrees(110)) <=
              tolerance,
          "increasing: through 110");
    CHECK(fabs(Wrapped(MixAngle(Degrees(30), Degrees(190), mtwn_angleDecreasing, 0.5)) -
               Degrees(290)) <= tolerance,
          "decreasing: through 290");
    CHECK(fabs(Wrapped(MixAngle(Degrees(350), Degrees(10), mtwn_angleShorter, 0.25)) -
               Degrees(355)) <= tolerance,
          "shorter across 0");
    CHECK(fabs(MixAngle(1.0, 1.0, mtwn_angleLonger, 0.5) - (1.0 + s_pi)) <= tolerance,
          "longer between equal angles turns all the way");
    CHECK(MixAngle(1.0, 1.0, mtwn_angleIncreasing, 0.5) == 1.0 &&
              MixAngle(1.0, 1.0, mtwn_angleDecreasing, 0.5) == 1.0,
          "increasing and decreasing hold equal angles");
    // Angles many turns out keep their turn count at 0 and move the end.
    CHECK(fabs(MixAngle(100.0, 100.5 - 6.0 * s_pi, mtwn_angleShorter, 0.5) - 100.25) <= 1e-12,
          "shorter many turns out");
    CHECK(MixAngle(100.0, 100.5, mtwn_angleShorter, 1.0) == 100.5,
          "an end moved by no turn stays exact");
    CHECK(fabs(MixAngle(0.0, Degrees(-90), mtwn_angleIncreasing, 1.0) - Degrees(270)) <= tolerance,
          "increasing moves the end by a turn");
    CHECK(fabs(MixAngle(0.0, 1.0, mtwn_angleShorter, 2.0) - 2.0) <= tolerance,
          "past the end it extends");
    double kept = 7.0;
    CHECK(mtwnMixAngle(0.0, 3.0, mtwn_angleDecreasing + 1, 0.5, &kept) == mtwn_errorInvalid &&
              mtwnMixAngle((double)NAN, 3.0, mtwn_angleRaw, 0.5, &kept) == mtwn_errorInvalid &&
              mtwnMixAngle(0.0, HUGE_VAL, mtwn_angleShorter, 0.5, &kept) == mtwn_errorInvalid &&
              mtwnMixAngle(0.0, 3.0, mtwn_angleRaw, -HUGE_VAL, &kept) == mtwn_errorInvalid &&
              mtwnMixAngle(0.0, 3.0, mtwn_angleRaw, (double)NAN, &kept) == mtwn_errorInvalid &&
              mtwnMixAngle(0.0, 3.0, mtwn_angleRaw, 0.5, NULL) == mtwn_errorInvalid && kept == 7.0,
          "an unknown method, an angle or progress not finite and a null out refused, the "
          "angle as it was");
    CHECK(fabs(MixAngle(0.0, 4.0 + 4.0 * s_pi, mtwn_angleShorter, 0.5) -
               (4.0 - 2.0 * s_pi) / 2.0) <= tolerance,
          "shorter takes the nearest turn, not the one below");
    CHECK(fabs(MixAngle(0.0, s_pi, mtwn_angleLonger, 0.5) - s_pi / 2.0) <= tolerance,
          "longer at exactly half a turn keeps its direction, as CSS's does");
    CHECK(fabs(MixAngle(Degrees(30), Degrees(90), mtwn_angleLonger, 0.5) - Degrees(-120)) <=
              tolerance,
          "longer from 30 to 90 goes down through -120");
    CHECK(MixAngle(1e16, 1.0, mtwn_angleRaw, 1.0) == 1.0,
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
            mtwnQuat q = MixRotation(&mix, t);
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
    mtwnQuat middle = MixRotation(&mix, 0.5);
    // Three quarters the other way, half of it at the middle: 135 degrees
    // about -z.
    double expected = 3.0 * s_pi / 4.0;
    CHECK(fabs((double)middle.z + sin(expected / 2.0)) <= 1e-6 &&
              fabs((double)middle.w - cos(expected / 2.0)) <= 1e-6,
          "the long way through 135 degrees about -z");
    mtwnQuat negated = {-quarter.x, -quarter.y, -quarter.z, -quarter.w};
    CHECK(mtwnMakeRotationMix(identity, negated, mtwn_rotationShortest, &mix) == mtwn_success,
          "the same quarter, given as -q");
    middle = MixRotation(&mix, 0.5);
    CHECK(fabs((double)middle.z - sin(s_pi / 8.0)) <= 1e-6, "still the short way");
    mtwnQuat end = MixRotation(&mix, 1.0);
    CHECK(end.x == quarter.x && end.y == quarter.y && end.z == quarter.z && end.w == quarter.w,
          "the end negated back where the path needed it");
    mtwnQuat start = MixRotation(&mix, 0.0);
    CHECK(start.w == 1.0f && start.z == 0.0f, "the start exactly");
    CHECK(mtwnMakeRotationMix((mtwnQuat){0.0f, 0.0f, 0.0f, 3.0f}, quarter, mtwn_rotationShortest,
                              &mix) == mtwn_success &&
              MixRotation(&mix, 0.0).w == 1.0f,
          "an end given unnormalized comes out normalized");
    CHECK(mtwnMakeRotationMix(identity, identity, mtwn_rotationShortest, &mix) == mtwn_success &&
              MixRotation(&mix, 0.3).w == 1.0f,
          "the same rotation holds still");
    CHECK(mtwnMakeRotationMix(identity, identity, mtwn_rotationLongest, &mix) == mtwn_errorInvalid,
          "the long way between equal rotations has no axis");
    mtwnQuat scaled = {0.0f, 0.0f, 0.0f, 3.0f};
    CHECK(mtwnMakeRotationMix(scaled, quarter, mtwn_rotationShortest, &mix) == mtwn_success &&
              fabsf(MixRotation(&mix, 0.5).w - (float)cos(s_pi / 8.0)) <= 1e-6f,
          "ends are normalized");
    CHECK(mtwnMakeRotationMix(identity, quarter, mtwn_rotationShortest, &mix) == mtwn_success,
          "made");
    mtwnQuat past = MixRotation(&mix, 2.0);
    CHECK(fabs((double)past.z - sin(s_pi / 2.0)) <= 1e-6 && fabs((double)past.w) <= 1e-6,
          "past the end it keeps turning: half a turn at 2");
    double norm =
        sqrt((double)(past.x * past.x + past.y * past.y + past.z * past.z + past.w * past.w));
    CHECK(fabs(norm - 1.0) <= 1e-6, "of unit length");
}

// The methods at exactly half a turn apart, and at none, as CSS Color 4
// adjusts hues: shorter leaves a half turn either way as it is; longer
// adjusts only (0, half) and (-half, 0], so a half turn is left too, and
// no difference becomes a whole turn.
static void TestHalfTurns(void)
{
    double half = s_pi;
    CHECK(MixAngle(0.0, half, mtwn_angleShorter, 0.5) == 0.5 * half &&
              MixAngle(half, 0.0, mtwn_angleShorter, 0.5) == 0.5 * half,
          "shorter: a half turn either way, straight");
    CHECK(MixAngle(0.0, half, mtwn_angleLonger, 0.5) == 0.5 * half &&
              MixAngle(half, 0.0, mtwn_angleLonger, 0.5) == 0.5 * half,
          "longer: a half turn either way, straight");
    CHECK(fabs(MixAngle(0.5, 0.5, mtwn_angleLonger, 0.5) - (0.5 + half)) < 1e-12,
          "longer: no difference, a whole turn");
}

// A slerp extrapolated so far that its phase holds no angle, or
// overflows: still a unit rotation, not NaN.
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
        mtwnQuat q = MixRotation(&mix, far[i]);
        double norm = sqrt((double)q.x * (double)q.x + (double)q.y * (double)q.y +
                           (double)q.z * (double)q.z + (double)q.w * (double)q.w);
        bounded = bounded && fabs(norm - 1.0) < 1e-6;
    }
    CHECK(bounded, "far past its ends, a unit rotation");
    mtwnQuat lost = {2.0f, 2.0f, 2.0f, 2.0f};
    CHECK(mtwnMixRotation(&mix, (double)NAN, &lost) == mtwn_errorInvalid && lost.w == 2.0f,
          "a NaN progress refused, not reduced");
}

// Ends of one rotation stay at any progress: far out the weights 1 - p
// and p cancel to 0, which gave a zero quaternion for 1e20.
static void TestEqualEndsFarOut(void)
{
    mtwnQuat q = {0.0f, 0.0f, 0.6f, 0.8f};
    mtwnRotationMix mix;
    CHECK(mtwnMakeRotationMix(q, q, mtwn_rotationShortest, &mix) == mtwn_success, "made");
    static const double s_progress[6] = {
        0.25, -3.0, 1e20, -1e20, 0x1.fffffffffffffp1023, -0x1.fffffffffffffp1023};
    int moved = 0;
    for (int i = 0; i < 6; ++i)
    {
        mtwnQuat r = MixRotation(&mix, s_progress[i]);
        moved += r.x != q.x || r.y != q.y || r.z != q.z || r.w != q.w;
    }
    CHECK(moved == 0, "equal ends stay at any progress");
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

// Equal angles mix to that angle at any progress.
static void TestAngleFarProgress(void)
{
    CHECK(MixAngle(1.25, 1.25, mtwn_angleRaw, 1e20) == 1.25 &&
              MixAngle(1.25, 1.25, mtwn_angleShorter, -1e20) == 1.25,
          "equal ends, far out: the end");
}

// An end's squared length summed as the mix's check sums it.
static double Squared(const double e[4])
{
    return e[0] * e[0] + e[1] * e[1] + e[2] * e[2] + e[3] * e[3];
}

static bool RotationRefused(const mtwnRotationMix* mix, double progress)
{
    mtwnQuat q = {2.0f, 2.0f, 2.0f, 2.0f};
    return mtwnMixRotation(mix, progress, &q) == mtwn_errorInvalid && q.x == 2.0f && q.w == 2.0f;
}

static bool RotationTaken(const mtwnRotationMix* mix, double progress)
{
    mtwnQuat q = {0};
    return mtwnMixRotation(mix, progress, &q) == mtwn_success && isfinite(q.x) && isfinite(q.y) &&
           isfinite(q.z) && isfinite(q.w);
}

// An end forged as e, stored as its floats.
static void ForgeFrom(mtwnRotationMix* mix, double w, double z)
{
    double e[4] = {0.0, 0.0, z, w};
    for (int i = 0; i < 4; ++i)
    {
        mix->from[i] = e[i];
    }
    mix->fromRotation = (mtwnQuat){0.0f, 0.0f, (float)z, (float)w};
}

// The check's unit length on both sides of its tolerance, 12u (u = 2^-53)
// in the squared length: the worst a maker's rounding gave in 2e8 random
// rotations, 6u either way, taken; 12u either way taken; past it, 14u
// above (the next double) and 13u below, refused.
static void TestMixUnitLength(void)
{
    const double u = 0x1p-53;
    mtwnQuat identity = {0.0f, 0.0f, 0.0f, 1.0f};
    const mtwnQuat worst[2] = {{0x1.307dccp+14f, 0x1.ea25a6p-1f, 0x1.f8588cp-1f, 0x1.1d21eap-2f},
                               {0x1.103718p+11f, -0x1.9af9fep-1f, 0x1.447cd8p-2f, 0x1.c5cffap-3f}};
    mtwnRotationMix mix;
    for (int i = 0; i < 2; ++i)
    {
        CHECK(mtwnMakeRotationMix(worst[i], identity, mtwn_rotationShortest, &mix) == mtwn_success,
              "a rotation at a maker's worst rounding");
        CHECK(Squared(mix.from) == 1.0 + (i == 0 ? 6.0 : -6.0) * u, "its squared length 6u off");
        CHECK(RotationTaken(&mix, 0.5), "taken");
    }
    mtwnRotationMix made;
    CHECK(mtwnMakeRotationMix(identity, (mtwnQuat){0.0f, 0.6f, 0.0f, 0.8f}, mtwn_rotationShortest,
                              &made) == mtwn_success,
          "a rotation mix");
    mix = made;
    ForgeFrom(&mix, 1.0 + 6.0 * u, 0.0);
    CHECK(Squared(mix.from) == 1.0 + 12.0 * u && RotationTaken(&mix, 0.5), "12u above taken");
    ForgeFrom(&mix, 1.0 + 6.0 * u, 0x1p-26);
    CHECK(Squared(mix.from) == 1.0 + 14.0 * u && RotationRefused(&mix, 0.5), "14u above refused");
    ForgeFrom(&mix, 1.0 - 6.0 * u, 0.0);
    CHECK(Squared(mix.from) == 1.0 - 12.0 * u && RotationTaken(&mix, 0.5), "12u below taken");
    ForgeFrom(&mix, 1.0 - 7.0 * u, 0x1.6a09e667f3bcdp-27);
    CHECK(Squared(mix.from) == 1.0 - 13.0 * u && RotationRefused(&mix, 0.5), "13u below refused");
    mix = made;
    mix.to[1] = 0.6 * (1.0 + 0x1p-40);
    mix.toRotation.y = (float)mix.to[1];
    CHECK(RotationRefused(&mix, 0.5), "the other end likewise");
}

static void TestMixChecks(void)
{
    mtwnQuat identity = {0.0f, 0.0f, 0.0f, 1.0f};
    mtwnRotationMix made;
    CHECK(mtwnMakeRotationMix(identity, (mtwnQuat){1.0f, 0.0f, 0.0f, 0.0f}, mtwn_rotationShortest,
                              &made) == mtwn_success,
          "a quarter turn's mix, pi / 2 apart as quaternions");
    mtwnQuat out = {2.0f, 2.0f, 2.0f, 2.0f};
    int refused = 0;
    refused += mtwnMixRotation(NULL, 0.5, &out) == mtwn_errorInvalid && out.w == 2.0f;
    refused += mtwnMixRotation(&made, 0.5, NULL) == mtwn_errorInvalid;
    refused += RotationRefused(&made, (double)INFINITY);
    refused += RotationRefused(&made, -(double)INFINITY);
    CHECK(refused == 4, "a null mix or out and an infinite progress refused");

    refused = 0;
    for (int i = 0; i < 4; ++i)
    {
        mtwnRotationMix m = made;
        float* from[4] = {&m.fromRotation.x, &m.fromRotation.y, &m.fromRotation.z,
                          &m.fromRotation.w};
        // One ulp toward 0, or off 0 by 2^-20.
        *from[i] = *from[i] == 0.0f ? 0x1p-20f : nextafterf(*from[i], 0.0f);
        refused += RotationRefused(&m, 0.5);
        m = made;
        float* to[4] = {&m.toRotation.x, &m.toRotation.y, &m.toRotation.z, &m.toRotation.w};
        *to[i] = *to[i] == 0.0f ? -0x1p-20f : nextafterf(*to[i], 0.0f);
        refused += RotationRefused(&m, 0.5);
    }
    mtwnRotationMix m = made;
    m.from[0] = (double)NAN;
    refused += RotationRefused(&m, 0.5);
    CHECK(refused == 9, "a rotation not its end's floats, each, and a NaN end refused");

    // The angle from 0 to pi.
    m = made;
    refused = 0;
    m.angle = -0x1p-1074;
    refused += RotationRefused(&m, 0.5);
    m.angle = nextafter(3.141592653589793, 4.0);
    refused += RotationRefused(&m, 0.5);
    m.angle = (double)NAN;
    refused += RotationRefused(&m, 0.5);
    CHECK(refused == 3, "an angle below 0, past pi or NaN refused");
    m.angle = 0.0;
    bool taken = RotationTaken(&m, 0.5);
    m.angle = 3.141592653589793;
    CHECK(taken && RotationTaken(&m, 0.5) && RotationTaken(&m, 1e300), "angles of 0 and pi taken");

    // An angle not its ends': 1e-300 for ends pi / 2 apart weighs them by
    // about 1e300 at progress 1e300, held within the floats.
    m = made;
    m.angle = 1e-300;
    mtwnQuat held = {0};
    CHECK(mtwnMixRotation(&m, 1e300, &held) == mtwn_success &&
              (fabsf(held.x) == FLT_MAX || fabsf(held.w) == FLT_MAX) && isfinite(held.x) &&
              isfinite(held.w),
          "a forged angle's weights held within the floats");
}

int main(void)
{
    TestAngleFarProgress();
    TestHalfTurns();
    TestFarExtrapolation();
    TestEqualEndsFarOut();
    TestAngles();
    TestRotationsAgainstSlerp();
    TestPaths();
    TestRefusals();
    TestMixUnitLength();
    TestMixChecks();
    return s_failures == 0 ? 0 : 1;
}
