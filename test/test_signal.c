// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Integers, discrete values, punch and shake:
// integers pass through every value
// between their ends, punch is its formula in long double, and shake is
// bounded, continuous, 0 on its lattice and at its ends, and the same
// bits on every platform for a seed.

#include "test_harness.h"
#include "test_values.h"

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
// A double reference's sine and power add 0.7 ulps of 1 to the worst
// error on macOS, Windows and the web; one is allowed.
static const double s_slack = DBL_EPSILON;
#endif

static void TestIntegers(void)
{
    CHECK(MixInteger(3, 9, 0.0) == 3 && MixInteger(3, 9, 1.0) == 9, "the ends");
    CHECK(MixInteger(0, 10, 0.25) == 3 && MixInteger(0, -10, 0.25) == -3, "ties away from zero");
    CHECK(MixInteger(0, 10, 0.24) == 2, "to nearest");
    int seen[101] = {0};
    for (int k = 0; k <= 100000; ++k)
    {
        int64_t v = MixInteger(0, 100, (double)k / 100000.0);
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
    CHECK(MixInteger(big, big + 10, 0.5) == big + 5, "exact within 2^53");
    CHECK(MixInteger(INT64_MAX, INT64_MIN, 0.0) == INT64_MAX &&
              MixInteger(INT64_MAX, INT64_MIN, 1.0) == INT64_MIN,
          "the type's ends exactly");
    CHECK(MixInteger(0, INT64_MAX, 2.0) == INT64_MAX && MixInteger(0, INT64_MIN, 2.0) == INT64_MIN,
          "clamped past the type's range");
    CHECK(MixInteger(10, 20, -0.5) == 5, "extended below 0");
    int64_t odd = ((int64_t)1 << 60) + 1;
    CHECK(MixInteger(odd, 5, 0.0) == odd && MixInteger(5, odd, 1.0) == odd,
          "ends past 2^53, which double cannot hold, exactly");
    CHECK(MixInteger(0, INT64_MIN, 0.75) == -3 * ((int64_t)1 << 61),
          "large values short of the range are not clamped");
    int64_t kept = 5;
    CHECK(mtwnMixInteger(1, 2, (double)NAN, &kept) == mtwn_errorInvalid &&
              mtwnMixInteger(1, 2, HUGE_VAL, &kept) == mtwn_errorInvalid &&
              mtwnMixInteger(1, 2, 0.5, NULL) == mtwn_errorInvalid && kept == 5,
          "a progress not finite and a null out refused, the value as it was");
}

static void TestDiscrete(void)
{
    CHECK(MixDiscrete(7, 9, 0.0) == 7 && MixDiscrete(7, 9, 0.4999) == 7, "before 1/2");
    CHECK(MixDiscrete(7, 9, 0.5) == 9 && MixDiscrete(7, 9, 1.0) == 9, "from 1/2");
    CHECK(MixDiscrete(7, 9, -3.0) == 7 && MixDiscrete(7, 9, 3.0) == 9, "outside");
    uint32_t kept = 5;
    CHECK(mtwnMixDiscrete(7, 9, (double)NAN, &kept) == mtwn_errorInvalid &&
              mtwnMixDiscrete(7, 9, -HUGE_VAL, &kept) == mtwn_errorInvalid &&
              mtwnMixDiscrete(7, 9, 0.5, NULL) == mtwn_errorInvalid && kept == 5,
          "a progress not finite and a null out refused, the value as it was");
}

static void TestPunch(void)
{
    mtwnPunch punch;
    CHECK(mtwnMakePunch(6.0, 0.8, 1.5, &punch) == mtwn_success, "made");
    CHECK(SamplePunch(&punch, 0.0) == 0.0 && SamplePunch(&punch, 0.8) == 0.0 &&
              SamplePunch(&punch, -1.0) == 0.0 && SamplePunch(&punch, 5.0) == 0.0,
          "0 at its ends and outside");
    double factor = 7.0;
    CHECK(SamplePunch(&punch, HUGE_VAL) == 0.0 &&
              mtwnSamplePunch(&punch, (double)NAN, &factor) == mtwn_errorInvalid && factor == 7.0,
          "0 at an infinite time, not the sine's NaN, and a NaN time refused");
    double worst = 0.0;
    double largest = 0.0;
    for (int k = 1; k < 8000; ++k)
    {
        double t = 0.8 * (double)k / 8000.0;
        Wide reference = WIDE_SIN(2 * (Wide)3.14159265358979323846264L * 6 * (Wide)t) *
                         WIDE_POW(1 - (Wide)t / (Wide)0.8, (Wide)1.5);
        double value = SamplePunch(&punch, t);
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
    // in range however far in.
    CHECK(mtwnMakePunch(8.0, 1e300, 1.5, &punch) == mtwn_success, "a punch over 1e300 s");
    double deep = SamplePunch(&punch, 2.5e299 + 0.03125);
    CHECK(fabs(deep) <= 1.0, "a factor deep into it");
    // Its cycles past the double range: a whole count, the sine 0.
    CHECK(mtwnMakePunch(1e300, 1e300, 1e-300, &punch) == mtwn_success, "a fast punch");
    CHECK(SamplePunch(&punch, 2.5e299) == 0.0, "cycles overflowed");
}

static void TestShakeShape(void)
{
    mtwnShake shake;
    CHECK(mtwnMakeShake(4.0, 3.0, 1.0, 42, &shake) == mtwn_success, "made");
    CHECK(SampleShake(&shake, 0, 0.0) == 0.0 && SampleShake(&shake, 0, 3.0) == 0.0 &&
              SampleShake(&shake, 0, -1.0) == 0.0 && SampleShake(&shake, 0, 9.0) == 0.0,
          "0 at its ends and outside");
    int negative = 0;
    for (uint64_t seed = 0; seed < 8; ++seed)
    {
        CHECK(mtwnMakeShake(7.0, 1.0, 2.0, seed, &shake) == mtwn_success, "made");
        for (uint32_t channel = 0; channel < 4; ++channel)
        {
            negative += signbit(SampleShake(&shake, channel, 0.0)) ? 1 : 0;
        }
    }
    CHECK(negative == 0, "its start an unsigned 0 on every channel");
    CHECK(mtwnMakeShake(4.0, 3.0, 1.0, 42, &shake) == mtwn_success, "made again");
    int lattice = 0;
    for (int k = 1; k < 12; ++k)
    {
        lattice += SampleShake(&shake, 1, (double)k * 0.25) != 0.0;
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
        double value = SampleShake(&steady, 3, t);
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
        same += SampleShake(&a, 2, t) == SampleShake(&b, 2, t);
        // On the lattice every channel is 0; elsewhere they differ.
        double value = SampleShake(&a, 2, t);
        otherChannel += value != 0.0 && value == SampleShake(&a, 3, t);
    }
    CHECK(same == 999, "the same seed shakes the same way");
    CHECK(otherChannel == 0, "each channel its own noise off the lattice");
    mtwnShake c;
    CHECK(mtwnMakeShake(10.0, 2.0, 2.0, 1235, &c) == mtwn_success &&
              SampleShake(&c, 2, 0.123) != SampleShake(&a, 2, 0.123),
          "another seed another way");
    CHECK(mtwnMakeShake(1.0, 1.0, 0.0, 0, &c) == mtwn_errorInvalid, "a zero exponent");
    CHECK(mtwnMakeShake(-1.0, 1.0, 1.0, 0, &c) == mtwn_errorInvalid, "a negative frequency");
    CHECK(mtwnMakeShake(1.0, 1.0, 1.0, 0, NULL) == mtwn_errorInvalid, "null out");
}

// The noise alone: the shake without its envelope, on
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
        apart += SampleShake(&shake, 4, t) != ShakeNoise(&shake, 4, t) * envelope;
    }
    CHECK(apart == 0, "the shake is its noise times its envelope, bit for bit");
    CHECK(ShakeNoise(&shake, 4, 3.07) != 0.0 && SampleShake(&shake, 4, 3.07) == 0.0,
          "the noise runs on past the duration");
    double factor = 7.0;
    CHECK(ShakeNoise(&shake, 4, 0.3) == 0.0 && ShakeNoise(&shake, 4, -0.07) == 0.0 &&
              ShakeNoise(&shake, 4, 0x1p53 / 10.0) == 0.0 &&
              mtwnShakeNoise(&shake, 4, (double)NAN, &factor) == mtwn_errorInvalid && factor == 7.0,
          "0 on the lattice, before 0 and past 2^53 points, and a NaN clock refused");
    double largest = 0.0;
    for (int k = 0; k < 100000; ++k)
    {
        largest = fmax(largest, fabs(ShakeNoise(&shake, 9, (double)k * 1e-3)));
    }
    CHECK(largest <= 1.0 && largest > 0.7, "within [-1, 1], filling most of it");
}

// Equal ends mix to that end at any progress, past 2^53 too, where the
// weighted sum's terms cancelled.
static void TestIntegerFarProgress(void)
{
    CHECK(MixInteger(7, 7, 1e20) == 7 && MixInteger(-3, -3, -1e20) == -3,
          "equal ends, far out: the end");
}

// An integer mix that rounds to 2^63 exactly is the type's largest, not
// past it.
static void TestIntegerTop(void)
{
    CHECK(MixInteger(INT64_MAX, INT64_MAX, 0.5) == INT64_MAX, "a mix of 2^63 exactly: the largest");
}

// Whether a punch's sample refuses it, its out as it was.
static bool PunchRefused(const mtwnPunch* punch)
{
    double factor = 7.0;
    return mtwnSamplePunch(punch, 0.25, &factor) == mtwn_errorInvalid && factor == 7.0;
}

// Whether a shake's sample and noise both refuse it, their outs as they
// were.
static bool ShakeRefused(const mtwnShake* shake)
{
    double factor = 7.0;
    double noise = 7.0;
    return mtwnSampleShake(shake, 1, 0.25, &factor) == mtwn_errorInvalid &&
           mtwnShakeNoise(shake, 1, 0.25, &noise) == mtwn_errorInvalid && factor == 7.0 &&
           noise == 7.0;
}

// A punch or a shake with a field written as its maker never writes it:
// each field refused on its own, wherever it is taken. A shake of 2^53
// lattice points or more over its duration is refused: a sample inside it
// would hold an index past what the noise keeps exact, and from 2^64 one
// a cast could not.
static void TestForgedSignals(void)
{
    mtwnPunch madePunch;
    mtwnShake madeShake;
    CHECK(mtwnMakePunch(6.0, 0.8, 1.5, &madePunch) == mtwn_success &&
              mtwnMakeShake(12.0, 0.9, 1.5, 7u, &madeShake) == mtwn_success,
          "a punch and a shake");
    double factor = 7.0;
    CHECK(mtwnSamplePunch(NULL, 0.25, &factor) == mtwn_errorInvalid &&
              mtwnSamplePunch(&madePunch, 0.25, NULL) == mtwn_errorInvalid &&
              mtwnSampleShake(NULL, 0, 0.25, &factor) == mtwn_errorInvalid &&
              mtwnSampleShake(&madeShake, 0, 0.25, NULL) == mtwn_errorInvalid &&
              mtwnShakeNoise(NULL, 0, 0.25, &factor) == mtwn_errorInvalid &&
              mtwnShakeNoise(&madeShake, 0, 0.25, NULL) == mtwn_errorInvalid && factor == 7.0,
          "a null signal or out");
    double shake = 7.0;
    CHECK(mtwnSampleShake(&madeShake, 0, (double)NAN, &shake) == mtwn_errorInvalid && shake == 7.0,
          "a NaN time");
    const double wrong[] = {0.0, -1.0, (double)NAN, (double)INFINITY};
    int refused = 0;
    for (int i = 0; i < 4; ++i)
    {
        for (int field = 0; field < 3; ++field)
        {
            mtwnPunch punch = madePunch;
            mtwnShake forged = madeShake;
            double* punchField = field == 0   ? &punch.frequency
                                 : field == 1 ? &punch.duration
                                              : &punch.exponent;
            double* shakeField = field == 0   ? &forged.frequency
                                 : field == 1 ? &forged.duration
                                              : &forged.exponent;
            *punchField = wrong[i];
            *shakeField = wrong[i];
            refused += PunchRefused(&punch) && ShakeRefused(&forged);
        }
    }
    CHECK(refused == 12, "a frequency, duration or exponent of 0, below 0, NaN or infinite");
    mtwnShake forged = madeShake;
    forged.frequency = 1.0;
    forged.duration = 0x1.fffffffffffffp52;
    double inside = 7.0;
    CHECK(mtwnSampleShake(&forged, 0, 0x1.8p52, &inside) == mtwn_success && isfinite(inside),
          "just under 2^53 lattice points taken");
    forged.duration = 0x1p53;
    CHECK(ShakeRefused(&forged), "2^53 refused");
    forged.frequency = 0x1p40;
    forged.duration = 0x1p30;
    CHECK(ShakeRefused(&forged),
          "past 2^64 refused, where a sample's index cast would be undefined");
}

int main(void)
{
    TestIntegerFarProgress();
    TestIntegers();
    TestDiscrete();
    TestPunch();
    TestShakeShape();
    TestShakeRepeats();
    TestShakeNoise();
    TestIntegerTop();
    TestForgedSignals();
    return s_failures == 0 ? 0 : 1;
}
