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

// The larger of the sine's and the cosine's errors at x.
static double SinCosError(float x)
{
    float sine = 0.0f;
    float cosine = 0.0f;
    mtwnSinCosF(x, &sine, &cosine);
    double sineError = UlpError(sine, sin((double)x));
    double cosineError = UlpError(cosine, cos((double)x));
    return sineError > cosineError ? sineError : cosineError;
}

static void TestSinCosSpecialValues(void)
{
    float sine = 1.0f;
    float cosine = 0.0f;
    mtwnSinCosF(0.0f, &sine, &cosine);
    CHECK(sine == 0.0f && !signbit(sine) && cosine == 1.0f, "exact at 0");
    mtwnSinCosF(-0.0f, &sine, &cosine);
    CHECK(sine == 0.0f && signbit(sine) && cosine == 1.0f, "the sign of -0 is kept");
    mtwnSinCosF(0x1.0p-14f, &sine, &cosine);
    CHECK(sine == 0x1.0p-14f && cosine == 1.0f, "a tiny angle");
    mtwnSinCosF(HUGE_VALF, &sine, &cosine);
    CHECK(isnan(sine) && isnan(cosine), "infinity gives NaN");
    mtwnSinCosF(NAN, &sine, &cosine);
    CHECK(isnan(sine) && isnan(cosine), "NaN gives NaN");
}

static void TestSinCosWithinOneUlp(void)
{
    static const float ranges[] = {1.0f, 10.0f, 2000.0f, 1.0e5f};
    for (size_t r = 0; r < sizeof ranges / sizeof ranges[0]; ++r)
    {
        uint64_t state = 10 + r;
        double worst = 0.0;
        for (int i = 0; i < 200000; ++i)
        {
            double error = SinCosError(Uniform(&state, -ranges[r], ranges[r]));
            worst = error > worst ? error : worst;
        }
        CHECK(worst <= 0.8, "sine and cosine within 0.8 ulp up to 1e5");
    }
}

static void TestSinCosNearQuadrantBoundaries(void)
{
    // The floats nearest k pi / 2 and their neighbors, where the reduced
    // argument is small and its leading bits cancel.
    double worst = 0.0;
    for (int k = 1; k < 65536; ++k)
    {
        float x = (float)((double)k * 0x1.921fb54442d18p+0);
        for (int step = 0; step < 3; ++step)
        {
            double error = SinCosError(x);
            worst = error > worst ? error : worst;
            x = nextafterf(x, HUGE_VALF);
        }
    }
    CHECK(worst <= 0.55, "sine and cosine within 0.55 ulp next to k pi / 2");
}

static void TestSinCosOnTheUnitCircle(void)
{
    uint64_t state = 15;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        float sine = 0.0f;
        float cosine = 0.0f;
        mtwnSinCosF(Uniform(&state, -2000.0f, 2000.0f), &sine, &cosine);
        double unit = fabs((double)sine * (double)sine + (double)cosine * (double)cosine - 1.0);
        worst = unit > worst ? unit : worst;
    }
    CHECK(worst <= 2.0 * (double)FLT_EPSILON, "on the unit circle");
}

static void TestAtan2SpecialValues(void)
{
    const float pi = 0x1.921fb6p+1f;
    const float half = 0x1.921fb6p+0f;
    CHECK(mtwnAtan2F(0.0f, 1.0f) == 0.0f && !signbit(mtwnAtan2F(0.0f, 1.0f)), "+0 at +x");
    CHECK(mtwnAtan2F(-0.0f, 1.0f) == 0.0f && signbit(mtwnAtan2F(-0.0f, 1.0f)), "-0 at +x");
    CHECK(mtwnAtan2F(0.0f, -1.0f) == pi && mtwnAtan2F(-0.0f, -1.0f) == -pi, "+-pi at -x");
    CHECK(mtwnAtan2F(0.0f, -0.0f) == pi && mtwnAtan2F(-0.0f, -0.0f) == -pi, "zero over -0");
    CHECK(mtwnAtan2F(1.0f, 0.0f) == half && mtwnAtan2F(-1.0f, -0.0f) == -half, "the y axis");
    CHECK(mtwnAtan2F(HUGE_VALF, HUGE_VALF) == 0.5f * half, "pi / 4 at two infinities");
    CHECK(UlpError(mtwnAtan2F(HUGE_VALF, -HUGE_VALF), atan2(HUGE_VAL, -HUGE_VAL)) <= 1.0,
          "3 pi / 4 at two infinities");
    CHECK(mtwnAtan2F(5.0f, HUGE_VALF) == 0.0f && mtwnAtan2F(-5.0f, -HUGE_VALF) == -pi,
          "an infinite x");
    CHECK(isnan(mtwnAtan2F(NAN, 1.0f)) && isnan(mtwnAtan2F(1.0f, NAN)), "NaN");
    CHECK(mtwnAtan2F(FLT_MAX, FLT_MAX) == 0.5f * half, "pi / 4 at the largest floats");
    CHECK(UlpError(mtwnAtan2F(0x1.0p-149f, 3.0f), atan2(0x1.0p-149, 3.0)) <= 1.0,
          "a subnormal angle");
}

static void TestAtan2WithinOneUlp(void)
{
    uint64_t state = 30;
    double worst = 0.0;
    for (int i = 0; i < 400000; ++i)
    {
        float y = ldexpf(Uniform(&state, -1.0f, 1.0f), (int)(Next(&state) % 80) - 40);
        float x = ldexpf(Uniform(&state, -1.0f, 1.0f), (int)(Next(&state) % 80) - 40);
        double error = UlpError(mtwnAtan2F(y, x), atan2((double)y, (double)x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0.55, "atan2 within 0.55 ulp");
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
    TestSinCosSpecialValues();
    TestSinCosWithinOneUlp();
    TestSinCosNearQuadrantBoundaries();
    TestSinCosOnTheUnitCircle();
    TestAtan2SpecialValues();
    TestAtan2WithinOneUlp();
    return s_failures == 0 ? 0 : 1;
}
