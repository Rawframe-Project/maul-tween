// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's own exponential and logarithm against the C library's
// long double functions where long double is wider than double and the
// C library computes them in it (glibc on x86 and arm64), and against
// its double functions elsewhere, with one more ulp allowed there for
// the reference's own error. Emscripten's musl answers expl and logl
// with the double functions, so WebAssembly takes the double reference.

#include "elementary.h"
#include "test_harness.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

// The bound the sweeps hold: under the 1 ulp the functions promise, so
// that a change which loses accuracy fails here first.
#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
#define WIDE_REFERENCE 1
static const double s_bound = 0.75;
#else
#define WIDE_REFERENCE 0
static const double s_bound = 1.75;
#endif

// SplitMix64: a fixed sequence, so every run checks the same inputs.
static uint64_t Next(uint64_t* state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// A uniform double in [lo, hi).
static double Uniform(uint64_t* state, double lo, double hi)
{
    return lo + (hi - lo) * ((double)(Next(state) >> 11) * 0x1.0p-53);
}

// The error of a result in units in the last place of the reference.
static double UlpError(double value, long double reference)
{
    double nearest = (double)reference;
    double ulp = nextafter(fabs(nearest), HUGE_VAL) - fabs(nearest);
    return (double)(fabsl((long double)value - reference) / (long double)ulp);
}

static long double ReferenceExp(double x)
{
#if WIDE_REFERENCE
    return expl((long double)x);
#else
    return (long double)exp(x);
#endif
}

static long double ReferenceExp2(double x)
{
#if WIDE_REFERENCE
    return exp2l((long double)x);
#else
    return (long double)exp2(x);
#endif
}

static long double ReferenceLog(double x)
{
#if WIDE_REFERENCE
    return logl((long double)x);
#else
    return (long double)log(x);
#endif
}

static void TestExpSpecialValues(void)
{
    CHECK(mtwnExp(0.0) == 1.0, "e^0 is 1");
    CHECK(mtwnExp(-0.0) == 1.0, "e^-0 is 1");
    CHECK(mtwnExp(HUGE_VAL) == HUGE_VAL, "e^inf is inf");
    CHECK(mtwnExp(-HUGE_VAL) == 0.0, "e^-inf is 0");
    CHECK(isnan(mtwnExp((double)NAN)), "e^NaN is NaN");
    CHECK(mtwnExp(0x1.62e42fefa39efp+9) < HUGE_VAL, "the largest finite argument");
    CHECK(mtwnExp(0x1.62e42fefa39f0p+9) == HUGE_VAL, "the next overflows");
    CHECK(mtwnExp(-0x1.74910d52d3051p+9) == 0x1.0p-1074, "the smallest subnormal");
    CHECK(mtwnExp(-0x1.74910d52d3052p+9) == 0.0, "the next rounds to 0");
}

static void TestExpWithinOneUlp(void)
{
    uint64_t state = 1;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        // Mostly the range a spring or a decay uses, some the whole range.
        double x = (i % 4 == 0) ? Uniform(&state, -708.0, 709.0) : Uniform(&state, -40.0, 40.0);
        double error = UlpError(mtwnExp(x), ReferenceExp(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= s_bound, "e^x within 1 ulp");
}

static void TestExpSmallArguments(void)
{
    uint64_t state = 2;
    double worst = 0.0;
    for (int i = 0; i < 100000; ++i)
    {
        double x = ldexp(Uniform(&state, -1.0, 1.0), -(int)(Next(&state) % 60));
        double error = UlpError(mtwnExp(x), ReferenceExp(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= s_bound, "e^x within 1 ulp near 0");
}

static void TestExp2IntegersExact(void)
{
    for (int k = -1074; k <= 1023; ++k)
    {
        if (mtwnExp2((double)k) != ldexp(1.0, k))
        {
            CHECK(0, "2^k is exact for an integer k");
            return;
        }
    }
    CHECK(mtwnExp2(1024.0) == HUGE_VAL, "2^1024 overflows");
    CHECK(mtwnExp2(-1076.0) == 0.0, "2^-1076 is 0");
    CHECK(isnan(mtwnExp2((double)NAN)), "2^NaN is NaN");
}

static void TestExp2WithinOneUlp(void)
{
    uint64_t state = 3;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        double x = (i % 4 == 0) ? Uniform(&state, -1021.0, 1023.0) : Uniform(&state, -12.0, 12.0);
        double error = UlpError(mtwnExp2(x), ReferenceExp2(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= s_bound, "2^x within 1 ulp");
}

static void TestLogSpecialValues(void)
{
    CHECK(mtwnLog(1.0) == 0.0, "ln 1 is 0");
    CHECK(mtwnLog(0.0) == -HUGE_VAL, "ln 0 is -inf");
    CHECK(mtwnLog(-0.0) == -HUGE_VAL, "ln -0 is -inf");
    CHECK(isnan(mtwnLog(-1.0)), "ln of a negative is NaN");
    CHECK(isnan(mtwnLog((double)NAN)), "ln NaN is NaN");
    CHECK(mtwnLog(HUGE_VAL) == HUGE_VAL, "ln inf is inf");
    CHECK(UlpError(mtwnLog(0x1.0p-1074), ReferenceLog(0x1.0p-1074)) <= s_bound,
          "ln of the smallest subnormal");
    CHECK(UlpError(mtwnLog(DBL_MAX), ReferenceLog(DBL_MAX)) <= s_bound, "ln DBL_MAX");
}

static void TestLogWithinOneUlp(void)
{
    uint64_t state = 4;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        // Any positive double: a random exponent and mantissa.
        double x = ldexp(Uniform(&state, 1.0, 2.0), (int)(Next(&state) % 2098) - 1074);
        double error = UlpError(mtwnLog(x), ReferenceLog(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= s_bound, "ln x within 1 ulp");
}

static void TestLogNearOne(void)
{
    uint64_t state = 5;
    double worst = 0.0;
    for (int i = 0; i < 100000; ++i)
    {
        double x = 1.0 + ldexp(Uniform(&state, -1.0, 1.0), -(int)(Next(&state) % 50));
        double error = UlpError(mtwnLog(x), ReferenceLog(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= s_bound, "ln x within 1 ulp near 1");
}

int main(void)
{
    TestExpSpecialValues();
    TestExpWithinOneUlp();
    TestExpSmallArguments();
    TestExp2IntegersExact();
    TestExp2WithinOneUlp();
    TestLogSpecialValues();
    TestLogWithinOneUlp();
    TestLogNearOne();
    return s_failures == 0 ? 0 : 1;
}
