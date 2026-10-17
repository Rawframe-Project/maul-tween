// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rough easings (record mtwn-0061): a seed's points the same bits on
// every platform (against SplitMix64 worked in Python), the ends exact,
// the inputs one to each range, the offsets within the strength by the
// taper, the flags, and the refusals.

#include "test_harness.h"

#include "maul-tween/easing.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static mtwnRoughDef Def(double strength, uint64_t seed, mtwnRoughTaper taper, uint8_t flags)
{
    mtwnRoughDef def;
    memset(&def, 0, sizeof def);
    def.shape = mtwnLinearEasing();
    def.strength = strength;
    def.seed = seed;
    def.taper = taper;
    def.flags = flags;
    return def;
}

static void TestSeedBits(void)
{
    // Seed 0x5EED, 6 points, strength 1/4, by the record's draws.
    static const mtwnLinearPoint want[6] = {
        {0x0.0p+0, 0x0.0p+0},
        {0x1.3e3fb3a07e158p-7, -0x1.2ea468db12413p-4},
        {0x1.5d5bca4696b34p-2, 0x1.3f01011f90f7ep-2},
        {0x1.05f95b8b7cc8bp-1, 0x1.c962a96e24026p-2},
        {0x1.94b66879610ecp-1, 0x1.27400f1166cfep-1},
        {0x1.0000000000000p+0, 0x1.0000000000000p+0},
    };
    mtwnRoughDef def = Def(0.25, 0x5EED, mtwn_roughTaperNone, 0);
    mtwnLinearPoint points[6];
    mtwnEasing easing;
    CHECK(mtwnMakeRoughEasing(&def, points, 6, &easing) == mtwn_success, "a rough easing");
    CHECK(memcmp(points, want, sizeof want) == 0, "the seed's points, bit for bit");
    CHECK(mtwnEase(&easing, 0.0, false) == 0.0 && mtwnEase(&easing, 1.0, false) == 1.0,
          "its ends exact");
    double a = points[2].output;
    double b = points[3].output;
    double mid = 0.5 * (points[2].input + points[3].input);
    CHECK(fabs(mtwnEase(&easing, mid, false) - 0.5 * (a + b)) <= 1e-15, "straight between points");
    mtwnLinearPoint again[6];
    CHECK(mtwnMakeRoughEasing(&def, again, 6, &easing) == mtwn_success &&
              memcmp(points, again, sizeof again) == 0,
          "a seed repeats");
    def.seed = 0x5EEE;
    CHECK(mtwnMakeRoughEasing(&def, again, 6, &easing) == mtwn_success &&
              memcmp(points, again, sizeof again) != 0,
          "another seed differs");
}

// The taper's share at x, written as the header writes it.
static double Share(mtwnRoughTaper taper, double x)
{
    double t[4] = {1.0, x, 1.0 - x, 1.0 - fabs(2.0 * x - 1.0)};
    return t[taper] * t[taper];
}

static void TestBounds(void)
{
    enum
    {
        count = 34
    };
    mtwnLinearPoint points[count];
    mtwnEasing easing;
    int outside = 0;
    int unordered = 0;
    int clamped = 0;
    int falling = 0;
    for (uint64_t seed = 0; seed < 64; ++seed)
    {
        for (mtwnRoughTaper taper = 0; taper <= mtwn_roughTaperBoth; ++taper)
        {
            mtwnRoughDef def = Def(0.3, seed, taper, 0);
            CHECK(mtwnMakeBackEasing(mtwn_easeInOut, 1.70158, &def.shape) == mtwn_success,
                  "a back shape");
            if (mtwnMakeRoughEasing(&def, points, count, &easing) != mtwn_success)
            {
                ++outside;
                continue;
            }
            for (uint32_t k = 1; k + 1 < count; ++k)
            {
                double x = points[k].input;
                double low = (double)(k - 1) / (count - 2);
                double high = (double)k / (count - 2);
                unordered += !(x > low && x < high);
                double offset = fabs(points[k].output - mtwnEase(&def.shape, x, false));
                outside += offset > 0.3 * Share(taper, x) * (1.0 + 1e-15) + 1e-16;
            }
            def.flags = mtwn_roughClamp;
            CHECK(mtwnMakeRoughEasing(&def, points, count, &easing) == mtwn_success, "clamped");
            for (uint32_t k = 0; k < count; ++k)
            {
                clamped += points[k].output < 0.0 || points[k].output > 1.0;
            }
            def.flags = mtwn_roughRising;
            CHECK(mtwnMakeRoughEasing(&def, points, count, &easing) == mtwn_success, "rising");
            for (uint32_t k = 1; k < count; ++k)
            {
                falling += points[k].output < points[k - 1].output || points[k].output > 1.0;
            }
            falling += !mtwnIsEasingMonotonic(&easing);
        }
    }
    CHECK(outside == 0, "every offset within the strength by the taper");
    CHECK(unordered == 0, "each inner point inside its own range");
    CHECK(clamped == 0, "clamped outputs in [0, 1]");
    CHECK(falling == 0, "rising outputs never fall, and the easing is monotonic");
}

static void TestZigZag(void)
{
    mtwnRoughDef def = Def(0.1, 99, mtwn_roughTaperNone, mtwn_roughZigZag);
    mtwnLinearPoint points[6];
    mtwnEasing easing;
    CHECK(mtwnMakeRoughEasing(&def, points, 6, &easing) == mtwn_success, "a zig-zag");
    int wrong = 0;
    for (uint32_t k = 1; k < 5; ++k)
    {
        double x = ((double)(k - 1) + 0.5) / 4.0;
        double y = x + ((k & 1u) != 0 ? 0.1 : -0.1);
        wrong += points[k].input != x || points[k].output != y;
    }
    CHECK(wrong == 0, "at the ranges' middles, up then down by the strength");
    def.seed = 7;
    mtwnLinearPoint again[6];
    CHECK(mtwnMakeRoughEasing(&def, again, 6, &easing) == mtwn_success &&
              memcmp(points, again, sizeof again) == 0,
          "no seed in it");
    // Two points are the shape's ends: linear.
    CHECK(mtwnMakeRoughEasing(&def, points, 2, &easing) == mtwn_success &&
              mtwnEase(&easing, 0.25, false) == 0.25,
          "two points, a straight line");
}

static double NotANumber(void* context, double progress)
{
    (void)context;
    (void)progress;
    return (double)NAN;
}

static double Infinite(void* context, double progress)
{
    (void)context;
    (void)progress;
    return HUGE_VAL;
}

static void TestRefusals(void)
{
    mtwnRoughDef def = Def(0.2, 1, mtwn_roughTaperNone, 0);
    mtwnLinearPoint points[8];
    mtwnEasing easing;
    int refused = 0;
    refused += mtwnMakeRoughEasing(NULL, points, 8, &easing) == mtwn_errorInvalid;
    refused += mtwnMakeRoughEasing(&def, NULL, 8, &easing) == mtwn_errorInvalid;
    refused += mtwnMakeRoughEasing(&def, points, 8, NULL) == mtwn_errorInvalid;
    refused += mtwnMakeRoughEasing(&def, points, 1, &easing) == mtwn_errorInvalid;
    const double strengths[] = {-0.1, HUGE_VAL, (double)NAN};
    for (int i = 0; i < 3; ++i)
    {
        mtwnRoughDef bad = def;
        bad.strength = strengths[i];
        refused += mtwnMakeRoughEasing(&bad, points, 8, &easing) == mtwn_errorInvalid;
    }
    mtwnRoughDef bad = def;
    bad.taper = 4;
    refused += mtwnMakeRoughEasing(&bad, points, 8, &easing) == mtwn_errorInvalid;
    bad = def;
    bad.flags = 8;
    refused += mtwnMakeRoughEasing(&bad, points, 8, &easing) == mtwn_errorInvalid;
    bad = def;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, NotANumber, NULL, &bad.shape) == mtwn_success,
          "a host shape");
    refused += mtwnMakeRoughEasing(&bad, points, 8, &easing) == mtwn_errorInvalid;
    // Clamping would bring an infinity to 1; the shape is refused first.
    bad.flags = mtwn_roughClamp;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, Infinite, NULL, &bad.shape) == mtwn_success,
          "a host shape");
    refused += mtwnMakeRoughEasing(&bad, points, 8, &easing) == mtwn_errorInvalid;
    CHECK(refused == 11, "no def, points or easing, one point, a strength below 0, infinite or "
                         "NaN, an unknown taper or flag, and a shape giving NaN or an infinity "
                         "refused");
}

int main(void)
{
    TestSeedBits();
    TestBounds();
    TestZigZag();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
