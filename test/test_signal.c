// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Integers, discrete values, punch and shake (records mtwn-0016,
// mtwn-0017, mtwn-0020, mtwn-0026): integers pass through every value
// between their ends, punch is its formula in long double, and shake is
// bounded, continuous, 0 on its lattice and at its ends, and the same
// bits on every platform for a seed.

#include "test_harness.h"

#include "maul-tween/interpolate.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_SIN sinl
#define WIDE_POW powl
static const double s_slack = 0.0;
#else
typedef double Wide;
#define WIDE_SIN sin
#define WIDE_POW pow
static const double s_slack = 4.0 * DBL_EPSILON;
#endif

static void TestIntegers(void)
{
    CHECK(mtwnMixInteger(3, 9, 0.0) == 3 && mtwnMixInteger(3, 9, 1.0) == 9, "the ends");
    CHECK(mtwnMixInteger(0, 10, 0.25) == 3 && mtwnMixInteger(0, -10, 0.25) == -3,
          "ties away from zero");
    CHECK(mtwnMixInteger(0, 10, 0.24) == 2, "to nearest");
    int seen[101] = {0};
    for (int k = 0; k <= 100000; ++k)
    {
        int64_t v = mtwnMixInteger(0, 100, (double)k / 100000.0);
        if (v >= 0 && v <= 100)
        {
            seen[v] = 1;
        }
    }
    int missing = 0;
    for (int v = 0; v <= 100; ++v)
    {
        missing += !seen[v];
    }
    CHECK(missing == 0, "every integer between the ends is passed");
    int64_t big = (int64_t)1 << 52;
    CHECK(mtwnMixInteger(big, big + 10, 0.5) == big + 5, "exact within 2^53");
    CHECK(mtwnMixInteger(INT64_MAX, INT64_MIN, 0.0) == INT64_MAX &&
              mtwnMixInteger(INT64_MAX, INT64_MIN, 1.0) == INT64_MIN,
          "the type's ends exactly");
    CHECK(mtwnMixInteger(0, INT64_MAX, 2.0) == INT64_MAX &&
              mtwnMixInteger(0, INT64_MIN, 2.0) == INT64_MIN,
          "clamped past the type's range");
    CHECK(mtwnMixInteger(10, 20, -0.5) == 5, "extended below 0");
    int64_t odd = ((int64_t)1 << 60) + 1;
    CHECK(mtwnMixInteger(odd, 5, 0.0) == odd && mtwnMixInteger(5, odd, 1.0) == odd,
          "ends past 2^53, which double cannot hold, exactly");
    CHECK(mtwnMixInteger(0, INT64_MIN, 0.75) == -3 * ((int64_t)1 << 61),
          "large values short of the range are not clamped");
    CHECK(mtwnMixInteger(1, 2, (double)NAN) == 1, "NaN progress keeps the start");
}

static void TestDiscrete(void)
{
    CHECK(mtwnMixDiscrete(7, 9, 0.0) == 7 && mtwnMixDiscrete(7, 9, 0.4999) == 7, "before 1/2");
    CHECK(mtwnMixDiscrete(7, 9, 0.5) == 9 && mtwnMixDiscrete(7, 9, 1.0) == 9, "from 1/2");
    CHECK(mtwnMixDiscrete(7, 9, -3.0) == 7 && mtwnMixDiscrete(7, 9, 3.0) == 9, "outside");
}

static void TestPunch(void)
{
    mtwnPunch punch;
    CHECK(mtwnMakePunch(6.0, 0.8, 1.5, &punch) == mtwn_success, "made");
    CHECK(mtwnSamplePunch(&punch, 0.0) == 0.0 && mtwnSamplePunch(&punch, 0.8) == 0.0 &&
              mtwnSamplePunch(&punch, -1.0) == 0.0 && mtwnSamplePunch(&punch, 5.0) == 0.0,
          "0 at its ends and outside");
    CHECK(mtwnSamplePunch(&punch, HUGE_VAL) == 0.0 && mtwnSamplePunch(&punch, (double)NAN) == 0.0,
          "0 at an infinite or NaN time, not the sine's NaN");
    double worst = 0.0;
    double largest = 0.0;
    for (int k = 1; k < 8000; ++k)
    {
        double t = 0.8 * (double)k / 8000.0;
        Wide reference = WIDE_SIN(2 * (Wide)3.14159265358979323846264L * 6 * (Wide)t) *
                         WIDE_POW(1 - (Wide)t / (Wide)0.8, (Wide)1.5);
        double value = mtwnSamplePunch(&punch, t);
        double e = fabs(value - (double)reference);
        worst = e > worst ? e : worst;
        largest = fabs(value) > largest ? fabs(value) : largest;
    }
    CHECK(worst <= 1e-15 + s_slack, "sin(2 pi f t) (1 - p)^e within 1e-15");
    CHECK(largest <= 1.0 && largest > 0.8, "a factor within [-1, 1] that nearly reaches it");
    CHECK(mtwnMakePunch(0.0, 1.0, 1.0, &punch) == mtwn_errorInvalid, "no frequency");
    CHECK(mtwnMakePunch(1.0, HUGE_VAL, 1.0, &punch) == mtwn_errorInvalid, "infinite duration");
    CHECK(mtwnMakePunch(1.0, 1.0, (double)NAN, &punch) == mtwn_errorInvalid, "NaN exponent");
    CHECK(mtwnMakePunch(1.0, 1.0, 1.0, NULL) == mtwn_errorInvalid, "null out");
    // A punch over a very long span: its sine is of the cycles' fraction,
    // in range however far in (found by the values' fuzz).
    CHECK(mtwnMakePunch(8.0, 1e300, 1.5, &punch) == mtwn_success, "a punch over 1e300 s");
    double deep = mtwnSamplePunch(&punch, 2.5e299 + 0.03125);
    CHECK(fabs(deep) <= 1.0, "a factor deep into it");
    // Its cycles past the double range: a whole count, the sine 0.
    CHECK(mtwnMakePunch(1e300, 1e300, 1e-300, &punch) == mtwn_success, "a fast punch");
    CHECK(mtwnSamplePunch(&punch, 2.5e299) == 0.0, "cycles overflowed");
}

static void TestShakeShape(void)
{
    mtwnShake shake;
    CHECK(mtwnMakeShake(4.0, 3.0, 1.0, 42, &shake) == mtwn_success, "made");
    CHECK(mtwnSampleShake(&shake, 0, 0.0) == 0.0 && mtwnSampleShake(&shake, 0, 3.0) == 0.0 &&
              mtwnSampleShake(&shake, 0, -1.0) == 0.0 && mtwnSampleShake(&shake, 0, 9.0) == 0.0,
          "0 at its ends and outside");
    int lattice = 0;
    for (int k = 1; k < 12; ++k)
    {
        lattice += mtwnSampleShake(&shake, 1, (double)k * 0.25) != 0.0;
    }
    CHECK(lattice == 0, "0 on its lattice, as gradient noise is");
    mtwnShake steady;
    CHECK(mtwnMakeShake(4.0, 1e9, 1.0, 7, &steady) == mtwn_success, "a long one");
    // Its lattice keeps a fraction between points and an index below 2^64.
    CHECK(mtwnMakeShake(4.0, 0x1p51, 1.0, 7, &shake) == mtwn_errorInvalid &&
              mtwnMakeShake(4.0, 0x1p51 - 1.0, 1.0, 7, &shake) == mtwn_success,
          "fewer than 2^53 lattice points");
    double largest = 0.0;
    double jump = 0.0;
    double previous = 0.0;
    for (int k = 0; k <= 2000000; ++k)
    {
        double t = (double)k * 1e-5;
        double value = mtwnSampleShake(&steady, 3, t);
        largest = fabs(value) > largest ? fabs(value) : largest;
        double step = fabs(value - previous);
        jump = k > 0 && step > jump ? step : jump;
        previous = value;
    }
    CHECK(largest <= 1.0 && largest > 0.7, "a factor within [-1, 1] that fills most of it");
    // The noise's slope is at most a few times the frequency.
    CHECK(jump <= 1e-5 * 4.0 * 4.0, "continuous: no jumps between samples");
}

static void TestShakeRepeats(void)
{
    mtwnShake a;
    mtwnShake b;
    CHECK(mtwnMakeShake(10.0, 2.0, 2.0, 1234, &a) == mtwn_success &&
              mtwnMakeShake(10.0, 2.0, 2.0, 1234, &b) == mtwn_success,
          "two of the same seed");
    int same = 0;
    int otherChannel = 0;
    for (int k = 1; k < 1000; ++k)
    {
        double t = (double)k * 0.002;
        same += mtwnSampleShake(&a, 2, t) == mtwnSampleShake(&b, 2, t);
        // On the lattice every channel is 0; elsewhere they differ.
        double value = mtwnSampleShake(&a, 2, t);
        otherChannel += value != 0.0 && value == mtwnSampleShake(&a, 3, t);
    }
    CHECK(same == 999, "the same seed shakes the same way");
    CHECK(otherChannel == 0, "each channel its own noise off the lattice");
    mtwnShake c;
    CHECK(mtwnMakeShake(10.0, 2.0, 2.0, 1235, &c) == mtwn_success &&
              mtwnSampleShake(&c, 2, 0.123) != mtwnSampleShake(&a, 2, 0.123),
          "another seed another way");
    // The same bits on every platform: values computed once and kept.
    CHECK(mtwnSampleShake(&a, 0, 0.123) == 0x1.178aae82e3cd7p-2 &&
              mtwnSampleShake(&a, 5, 1.37) == 0x1.3908d98b58cbap-5,
          "the kept values");
    CHECK(mtwnMakeShake(1.0, 1.0, 0.0, 0, &c) == mtwn_errorInvalid, "a zero exponent");
    CHECK(mtwnMakeShake(-1.0, 1.0, 1.0, 0, &c) == mtwn_errorInvalid, "a negative frequency");
    CHECK(mtwnMakeShake(1.0, 1.0, 1.0, 0, NULL) == mtwn_errorInvalid, "null out");
}

// The noise alone (record mtwn-0065): the shake without its envelope, on
// a clock past its duration, 0 on the lattice and outside its range.
static void TestShakeNoise(void)
{
    mtwnShake shake;
    CHECK(mtwnMakeShake(10.0, 2.0, 1.0, 1234, &shake) == mtwn_success, "a shake");
    int apart = 0;
    for (int k = 1; k < 200; ++k)
    {
        double t = (double)k * 0.0093;
        double envelope = 1.0 - t / 2.0;
        apart += mtwnSampleShake(&shake, 4, t) != mtwnShakeNoise(&shake, 4, t) * envelope;
    }
    CHECK(apart == 0, "the shake is its noise times its envelope, bit for bit");
    CHECK(mtwnShakeNoise(&shake, 4, 3.07) != 0.0 && mtwnSampleShake(&shake, 4, 3.07) == 0.0,
          "the noise runs on past the duration");
    CHECK(mtwnShakeNoise(&shake, 4, 0.3) == 0.0 && mtwnShakeNoise(&shake, 4, -0.07) == 0.0 &&
              mtwnShakeNoise(&shake, 4, 0x1p53 / 10.0) == 0.0 &&
              mtwnShakeNoise(&shake, 4, (double)NAN) == 0.0,
          "0 on the lattice, before 0, past 2^53 points and for NaN");
    double largest = 0.0;
    for (int k = 0; k < 100000; ++k)
    {
        largest = fmax(largest, fabs(mtwnShakeNoise(&shake, 9, (double)k * 1e-3)));
    }
    CHECK(largest <= 1.0 && largest > 0.7, "within [-1, 1], filling most of it");
}

int main(void)
{
    TestIntegers();
    TestDiscrete();
    TestPunch();
    TestShakeShape();
    TestShakeRepeats();
    TestShakeNoise();
    return s_failures == 0 ? 0 : 1;
}
