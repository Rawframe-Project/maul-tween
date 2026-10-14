// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's binary32 elementary functions against the C library's
// binary64 ones, whose own error is a billionth of a float ulp, so the
// bounds hold on every platform alike. Each sweep holds a bound a little
// above the worst error measured on it, under the 1 ulp the functions
// promise.

#include "elementary_float.h"
#include "test_harness.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

// SplitMix64: a fixed sequence, so every run checks the same inputs.
static uint64_t Next(uint64_t* state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// A uniform float in [lo, hi).
static float Uniform(uint64_t* state, float lo, float hi)
{
    return lo + (hi - lo) * ((float)(Next(state) >> 40) * 0x1.0p-24f);
}

// The error of a result in units in the last place of the float nearest
// the reference.
static double UlpError(float value, double reference)
{
    float nearest = (float)reference;
    float magnitude = fabsf(nearest);
    float ulp = nextafterf(magnitude, HUGE_VALF) - magnitude;
    return fabs((double)value - reference) / (double)ulp;
}

static void TestExpSpecialValues(void)
{
    CHECK(mtwnExpF(0.0f) == 1.0f, "e^0 is 1");
    CHECK(mtwnExpF(HUGE_VALF) == HUGE_VALF && mtwnExpF(-HUGE_VALF) == 0.0f, "infinities");
    CHECK(isnan(mtwnExpF(NAN)), "NaN");
    CHECK(mtwnExpF(0x1.62e42ep+6f) < HUGE_VALF, "the largest finite argument");
    CHECK(mtwnExpF(0x1.62e430p+6f) == HUGE_VALF, "the next overflows");
    CHECK(mtwnExpF(-0x1.9fe368p+6f) == 0x1.0p-149f, "the smallest subnormal");
    CHECK(mtwnExpF(-0x1.9fe36ap+6f) == 0.0f, "the next rounds to 0");
}

static void TestExpWithinOneUlp(void)
{
    uint64_t state = 1;
    double worst = 0.0;
    for (int i = 0; i < 400000; ++i)
    {
        float x = (i % 4 == 0) ? Uniform(&state, -87.0f, 88.7f) : Uniform(&state, -20.0f, 20.0f);
        double error = UlpError(mtwnExpF(x), exp((double)x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0.75, "e^x within 0.75 ulp");
}

static void TestExp2IntegersExact(void)
{
    int wrong = 0;
    for (int k = -149; k <= 127; ++k)
    {
        wrong += mtwnExp2F((float)k) != ldexpf(1.0f, k);
    }
    CHECK(wrong == 0, "2^k is exact for an integer k");
    CHECK(mtwnExp2F(128.0f) == HUGE_VALF && mtwnExp2F(-151.0f) == 0.0f, "past the ends");
    CHECK(isnan(mtwnExp2F(NAN)), "NaN");
}

static void TestExp2WithinOneUlp(void)
{
    uint64_t state = 3;
    double worst = 0.0;
    for (int i = 0; i < 400000; ++i)
    {
        float x = (i % 4 == 0) ? Uniform(&state, -125.0f, 127.0f) : Uniform(&state, -12.0f, 12.0f);
        double error = UlpError(mtwnExp2F(x), exp2((double)x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0.7, "2^x within 0.7 ulp");
}

static void TestLogSpecialValues(void)
{
    CHECK(mtwnLogF(1.0f) == 0.0f, "ln 1 is 0");
    CHECK(mtwnLogF(0.0f) == -HUGE_VALF && mtwnLogF(-0.0f) == -HUGE_VALF, "ln 0 is -inf");
    CHECK(isnan(mtwnLogF(-1.0f)) && isnan(mtwnLogF(NAN)), "NaN below 0 and for NaN");
    CHECK(mtwnLogF(HUGE_VALF) == HUGE_VALF, "ln inf is inf");
    CHECK(UlpError(mtwnLogF(0x1.0p-149f), log(0x1.0p-149)) <= 1.0, "ln of the smallest subnormal");
    CHECK(UlpError(mtwnLogF(FLT_MAX), log((double)FLT_MAX)) <= 1.0, "ln FLT_MAX");
}

static void TestLogWithinOneUlp(void)
{
    uint64_t state = 4;
    double worst = 0.0;
    for (int i = 0; i < 400000; ++i)
    {
        float x = ldexpf(Uniform(&state, 1.0f, 2.0f), (int)(Next(&state) % 277) - 149);
        double error = UlpError(mtwnLogF(x), log((double)x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0.55, "ln x within 0.55 ulp");
}

static void TestLogNearOne(void)
{
    uint64_t state = 5;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        float x = 1.0f + ldexpf(Uniform(&state, -1.0f, 1.0f), -(int)(Next(&state) % 23));
        if (x == 1.0f)
        {
            continue;
        }
        double error = UlpError(mtwnLogF(x), log((double)x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0.55, "ln x within 0.55 ulp near 1");
}

int main(void)
{
    TestExpSpecialValues();
    TestExpWithinOneUlp();
    TestExp2IntegersExact();
    TestExp2WithinOneUlp();
    TestLogSpecialValues();
    TestLogWithinOneUlp();
    TestLogNearOne();
    return s_failures == 0 ? 0 : 1;
}
