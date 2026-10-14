// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's own exponential, logarithm and trigonometry against the
// C library's
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

// Each sweep holds a bound a little above the worst error measured on
// it, under the 1 ulp the functions promise, so that a change which
// loses accuracy fails here first; one more ulp is allowed against a
// double reference.
#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
#define WIDE_REFERENCE 1
static const double s_referenceSlack = 0.0;
#else
#define WIDE_REFERENCE 0
static const double s_referenceSlack = 1.0;
#endif

static double Bound(double measured)
{
    return measured + s_referenceSlack;
}

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

// a times b = product + *errorOut exactly (Veltkamp's split, Dekker's
// product), for operands well inside the range.
static double ExactProduct(double a, double b, double* errorOut)
{
    double product = a * b;
    double aSplit = a * 134217729.0;
    double aHigh = aSplit - (aSplit - a);
    double aLow = a - aHigh;
    double bSplit = b * 134217729.0;
    double bHigh = bSplit - (bSplit - b);
    double bLow = b - bHigh;
    *errorOut = ((aHigh * bHigh - product) + aHigh * bLow + aLow * bHigh) + aLow * bLow;
    return product;
}

// The error of a cube root y of x in ulps of y, with no reference
// function: (x - y^3) / (3 y^2) is the distance to the true root to
// first order, and x - y^3 is computed exactly after scaling x by 2^(3k)
// and y by 2^k into [1, 2). The C library's cbrt is not accurate enough
// to judge half an ulp on every platform (Windows' is not).
static double CbrtUlpError(double x, double y)
{
    int exponent = 0;
    (void)frexp(y, &exponent);
    double ys = fabs(ldexp(y, 1 - exponent));
    double xs = fabs(ldexp(x, 3 * (1 - exponent)));
    double squareLow = 0.0;
    double square = ExactProduct(ys, ys, &squareLow);
    double cubeLow = 0.0;
    double cube = ExactProduct(square, ys, &cubeLow);
    double cubeLowMore = 0.0;
    double cubeTail = ExactProduct(squareLow, ys, &cubeLowMore);
    long double residual = ((long double)xs - (long double)cube) - (long double)cubeLow -
                           (long double)cubeTail - (long double)cubeLowMore;
    double distance = (double)(residual / (3.0L * (long double)square));
    double ulp = nextafter(ys, HUGE_VAL) - ys;
    return fabs(distance) / ulp;
}

static long double ReferencePow(double x, double y)
{
#if WIDE_REFERENCE
    return powl((long double)x, (long double)y);
#else
    return (long double)pow(x, y);
#endif
}

static long double ReferenceSin(double x)
{
#if WIDE_REFERENCE
    return sinl((long double)x);
#else
    return (long double)sin(x);
#endif
}

static long double ReferenceCos(double x)
{
#if WIDE_REFERENCE
    return cosl((long double)x);
#else
    return (long double)cos(x);
#endif
}

static long double ReferenceAtan2(double y, double x)
{
#if WIDE_REFERENCE
    return atan2l((long double)y, (long double)x);
#else
    return (long double)atan2(y, x);
#endif
}

// The larger of the sine's and the cosine's errors at x.
static double SinCosError(double x)
{
    double sine = 0.0;
    double cosine = 0.0;
    mtwnSinCos(x, &sine, &cosine);
    double sineError = UlpError(sine, ReferenceSin(x));
    double cosineError = UlpError(cosine, ReferenceCos(x));
    return sineError > cosineError ? sineError : cosineError;
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
    CHECK(worst <= Bound(0.65), "e^x within 0.65 ulp");
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
    CHECK(worst <= Bound(0.75), "e^x within 0.75 ulp near 0");
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
    CHECK(worst <= Bound(0.65), "2^x within 0.65 ulp");
}

static void TestLogSpecialValues(void)
{
    CHECK(mtwnLog(1.0) == 0.0, "ln 1 is 0");
    CHECK(mtwnLog(0.0) == -HUGE_VAL, "ln 0 is -inf");
    CHECK(mtwnLog(-0.0) == -HUGE_VAL, "ln -0 is -inf");
    CHECK(isnan(mtwnLog(-1.0)), "ln of a negative is NaN");
    CHECK(isnan(mtwnLog((double)NAN)), "ln NaN is NaN");
    CHECK(mtwnLog(HUGE_VAL) == HUGE_VAL, "ln inf is inf");
    CHECK(UlpError(mtwnLog(0x1.0p-1074), ReferenceLog(0x1.0p-1074)) <= Bound(1.0),
          "ln of the smallest subnormal");
    CHECK(UlpError(mtwnLog(DBL_MAX), ReferenceLog(DBL_MAX)) <= Bound(1.0), "ln DBL_MAX");
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
    CHECK(worst <= Bound(0.55), "ln x within 0.55 ulp");
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
    CHECK(worst <= Bound(0.55), "ln x within 0.55 ulp near 1");
}

static void TestLogAtTheReductionEdges(void)
{
    // Mantissas just below sqrt(1/2) and sqrt(2), where |s| is largest
    // and the series' last terms count.
    uint64_t state = 7;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        double m = (i % 2 == 0) ? Uniform(&state, 0.70, 0.7072) : Uniform(&state, 1.40, 1.4143);
        double x = ldexp(m, (int)(Next(&state) % 40) - 20);
        double error = UlpError(mtwnLog(x), ReferenceLog(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= Bound(0.7), "ln x within 0.7 ulp at the reduction's edges");
}

static void TestExpOnMaulUiInputs(void)
{
    // The grid Maul UI's own exponential is tested on.
    double worst = 0.0;
    for (int i = 0; i <= 140000; ++i)
    {
        double x = -700.0 + (double)i * 0.01;
        double error = UlpError(mtwnExp(x), ReferenceExp(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= Bound(0.7), "e^x within 0.7 ulp on Maul UI's grid");
}

static void TestLogOnMaulUiInputs(void)
{
    // Maul UI's inputs: powers of 1.37, nudged.
    double worst = 0.0;
    for (int i = -400; i <= 400; ++i)
    {
        double x = pow(1.37, (double)i) * (1.0 + 1e-3 * (double)(i % 7));
        double error = UlpError(mtwnLog(x), ReferenceLog(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= Bound(0.6), "ln x within 0.6 ulp on Maul UI's inputs");
}

static void TestSinCosSpecialValues(void)
{
    double sine = 1.0;
    double cosine = 0.0;
    mtwnSinCos(0.0, &sine, &cosine);
    CHECK(sine == 0.0 && !signbit(sine) && cosine == 1.0, "exact at 0");
    mtwnSinCos(-0.0, &sine, &cosine);
    CHECK(sine == 0.0 && signbit(sine) && cosine == 1.0, "the sign of -0 is kept");
    mtwnSinCos(0x1.0p-30, &sine, &cosine);
    CHECK(sine == 0x1.0p-30 && cosine == 1.0, "a tiny angle");
    mtwnSinCos(HUGE_VAL, &sine, &cosine);
    CHECK(isnan(sine) && isnan(cosine), "infinity gives NaN");
    mtwnSinCos((double)NAN, &sine, &cosine);
    CHECK(isnan(sine) && isnan(cosine), "NaN gives NaN");
    // Each quadrant, and negative arguments, as Maul UI checks them.
    static const double points[] = {0.5, 2.0, 3.5, 5.0, -0.5, -2.0, -3.5, -5.0};
    double worst = 0.0;
    for (size_t i = 0; i < sizeof points / sizeof points[0]; ++i)
    {
        double error = SinCosError(points[i]);
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= Bound(0.85), "every quadrant");
}

static void TestSinCosWithinOneUlp(void)
{
    static const double ranges[] = {1.0, 10.0, 2000.0, 1.6e6};
    for (size_t r = 0; r < sizeof ranges / sizeof ranges[0]; ++r)
    {
        uint64_t state = 10 + r;
        double worst = 0.0;
        for (int i = 0; i < 100000; ++i)
        {
            double error = SinCosError(Uniform(&state, -ranges[r], ranges[r]));
            worst = error > worst ? error : worst;
        }
        CHECK(worst <= Bound(0.85), "sine and cosine within 0.85 ulp up to 1.6e6");
    }
}

static void TestSinCosNearQuadrantBoundaries(void)
{
    // Just off multiples of pi / 2, where the reduced argument is small and
    // its leading bits cancel.
    uint64_t state = 20;
    double worst = 0.0;
    for (int i = 0; i < 100000; ++i)
    {
        double k = (double)(Next(&state) % 1000000);
        double x =
            k * 0x1.921fb54442d18p+0 + ldexp(Uniform(&state, -1.0, 1.0), -(int)(Next(&state) % 30));
        double error = SinCosError(x);
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= Bound(0.85), "sine and cosine within 0.85 ulp next to k pi / 2");
}

static void TestSinCosOnMaulUiInputs(void)
{
    // Maul UI's grid, with its check that the point stays on the circle.
    double worst = 0.0;
    double worstUnit = 0.0;
    for (int i = 0; i <= 200000; ++i)
    {
        double x = -2000.0 + (double)i * 0.02;
        double sine = 0.0;
        double cosine = 0.0;
        mtwnSinCos(x, &sine, &cosine);
        double error = SinCosError(x);
        worst = error > worst ? error : worst;
        double unit = fabs(sine * sine + cosine * cosine - 1.0);
        worstUnit = unit > worstUnit ? unit : worstUnit;
    }
    CHECK(worst <= Bound(0.85), "sine and cosine within 0.85 ulp on Maul UI's grid");
    CHECK(worstUnit <= 2.0 * DBL_EPSILON, "on the unit circle");
}

static void TestAtan2SpecialValues(void)
{
    const double pi = 0x1.921fb54442d18p+1;
    const double half = 0x1.921fb54442d18p+0;
    CHECK(mtwnAtan2(0.0, 1.0) == 0.0 && !signbit(mtwnAtan2(0.0, 1.0)), "+0 at +x");
    CHECK(mtwnAtan2(-0.0, 1.0) == 0.0 && signbit(mtwnAtan2(-0.0, 1.0)), "-0 at +x");
    CHECK(mtwnAtan2(0.0, -1.0) == pi && mtwnAtan2(-0.0, -1.0) == -pi, "+-pi at -x");
    CHECK(mtwnAtan2(0.0, 0.0) == 0.0 && mtwnAtan2(0.0, -0.0) == pi, "zero over zero");
    CHECK(mtwnAtan2(-0.0, -0.0) == -pi, "-0 over -0");
    CHECK(mtwnAtan2(1.0, 0.0) == half && mtwnAtan2(-1.0, -0.0) == -half, "+-pi / 2 on the y axis");
    CHECK(mtwnAtan2(HUGE_VAL, HUGE_VAL) == 0.5 * half, "pi / 4 at two infinities");
    CHECK(UlpError(mtwnAtan2(HUGE_VAL, -HUGE_VAL), ReferenceAtan2(HUGE_VAL, -HUGE_VAL)) <=
              1.0 + s_referenceSlack,
          "3 pi / 4 at two infinities");
    CHECK(mtwnAtan2(-HUGE_VAL, 5.0) == -half, "-pi / 2 at -infinity");
    CHECK(mtwnAtan2(5.0, HUGE_VAL) == 0.0 && mtwnAtan2(-5.0, -HUGE_VAL) == -pi, "an infinite x");
    CHECK(isnan(mtwnAtan2((double)NAN, 1.0)) && isnan(mtwnAtan2(1.0, (double)NAN)), "NaN");
    CHECK(mtwnAtan2(1.0, 1.0) == 0.5 * half, "pi / 4 on the diagonal");
    CHECK(mtwnAtan2(DBL_MAX, DBL_MAX) == 0.5 * half, "pi / 4 at the largest doubles");
    CHECK(UlpError(mtwnAtan2(DBL_MAX, 1e300), ReferenceAtan2(DBL_MAX, 1e300)) <= Bound(0.55),
          "huge operands");
    CHECK(UlpError(mtwnAtan2(0x1.0p-1074, 3.0), ReferenceAtan2(0x1.0p-1074, 3.0)) <= Bound(1.0),
          "a subnormal angle");
}

static void TestAtan2WithinOneUlp(void)
{
    uint64_t state = 30;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        double y = ldexp(Uniform(&state, -1.0, 1.0), (int)(Next(&state) % 80) - 40);
        double x = ldexp(Uniform(&state, -1.0, 1.0), (int)(Next(&state) % 80) - 40);
        double error = UlpError(mtwnAtan2(y, x), ReferenceAtan2(y, x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= Bound(0.55), "atan2 within 0.55 ulp");
}

static void TestAtanOfRatiosNearTheTable(void)
{
    // Ratios in [0, 1] over every interval the table splits.
    uint64_t state = 31;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        double y = Uniform(&state, 0.0, 1.0);
        double error = UlpError(mtwnAtan2(y, 1.0), ReferenceAtan2(y, 1.0));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= Bound(0.55), "atan within 0.55 ulp on [0, 1]");
}

static void TestCbrtSpecialValues(void)
{
    CHECK(mtwnCbrt(0.0) == 0.0 && !signbit(mtwnCbrt(0.0)), "+0");
    CHECK(mtwnCbrt(-0.0) == 0.0 && signbit(mtwnCbrt(-0.0)), "-0");
    CHECK(mtwnCbrt(HUGE_VAL) == HUGE_VAL && mtwnCbrt(-HUGE_VAL) == -HUGE_VAL, "infinities");
    CHECK(isnan(mtwnCbrt((double)NAN)), "NaN");
    CHECK(mtwnCbrt(-27.0) == -3.0 && mtwnCbrt(8.0) == 2.0, "Maul UI's exact cubes");
    int wrong = 0;
    for (int k = -100000; k <= 100000; ++k)
    {
        double cube = (double)k * (double)k * (double)k;
        wrong += mtwnCbrt(cube) != (double)k;
    }
    CHECK(wrong == 0, "the cube root of an integer's cube is the integer");
    CHECK(CbrtUlpError(0x1.0p-1074, mtwnCbrt(0x1.0p-1074)) <= 0.55, "the smallest subnormal");
}

static void TestCbrtWithinHalfAnUlp(void)
{
    uint64_t state = 40;
    double worst = 0.0;
    for (int i = 0; i < 200000; ++i)
    {
        double x = ldexp(Uniform(&state, -2.0, 2.0), (int)(Next(&state) % 2098) - 1074);
        double error = CbrtUlpError(x, mtwnCbrt(x));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0.55, "cube root within 0.55 ulp");
}

static void TestCbrtOnMaulUiInputs(void)
{
    double worst = 0.0;
    for (int i = -400; i <= 400; ++i)
    {
        double x = pow(1.37, (double)i) * (1.0 + 1e-3 * (double)(i % 7));
        double error = CbrtUlpError(x, mtwnCbrt(x));
        double negative = CbrtUlpError(-x, mtwnCbrt(-x));
        worst = error > worst ? error : worst;
        worst = negative > worst ? negative : worst;
    }
    CHECK(worst <= 0.55, "cube root within 0.55 ulp on Maul UI's inputs");
}

static void TestPowSpecialValues(void)
{
    CHECK(mtwnPow(0.0, 0.0) == 1.0 && mtwnPow((double)NAN, 0.0) == 1.0, "x^0 is 1");
    CHECK(mtwnPow(1.0, (double)NAN) == 1.0 && mtwnPow(1.0, HUGE_VAL) == 1.0, "1^y is 1");
    CHECK(isnan(mtwnPow(-2.0, 2.0)), "a negative base is NaN");
    CHECK(isnan(mtwnPow((double)NAN, 2.0)) && isnan(mtwnPow(2.0, (double)NAN)), "NaN");
    CHECK(mtwnPow(0.0, 3.0) == 0.0 && mtwnPow(0.0, -3.0) == HUGE_VAL, "powers of 0");
    CHECK(mtwnPow(HUGE_VAL, 0.5) == HUGE_VAL && mtwnPow(HUGE_VAL, -0.5) == 0.0,
          "powers of infinity");
    CHECK(mtwnPow(2.0, HUGE_VAL) == HUGE_VAL && mtwnPow(0.5, HUGE_VAL) == 0.0, "to +infinity");
    CHECK(mtwnPow(2.0, -HUGE_VAL) == 0.0 && mtwnPow(0.5, -HUGE_VAL) == HUGE_VAL, "to -infinity");
    CHECK(mtwnPow(10.0, 400.0) == HUGE_VAL && mtwnPow(10.0, -400.0) == 0.0, "past the range");
    int wrong = 0;
    for (int k = -1074; k <= 1023; ++k)
    {
        wrong += mtwnPow(2.0, (double)k) != ldexp(1.0, k);
    }
    double ten = 1.0;
    for (int k = 0; k <= 22; ++k)
    {
        wrong += mtwnPow(10.0, (double)k) != ten;
        ten *= 10.0;
    }
    CHECK(wrong == 0, "2^k and 10^k up to 10^22 are exact");
}

static void TestPowWithinOneUlp(void)
{
    // |y ln x| up to 8, the range the library raises in, and up to 32.
    static const double limits[] = {8.0, 32.0};
    static const double bounds[] = {0.7, 0.95};
    for (int l = 0; l < 2; ++l)
    {
        uint64_t state = 41;
        double worst = 0.0;
        for (int i = 0; i < 100000; ++i)
        {
            double x = mtwnExp(Uniform(&state, -30.0, 30.0));
            double logX = fabs(mtwnLog(x));
            double y = Uniform(&state, -limits[l], limits[l]) / (logX > 1e-3 ? logX : 1.0);
            double error = UlpError(mtwnPow(x, y), ReferencePow(x, y));
            worst = error > worst ? error : worst;
        }
        CHECK(worst <= Bound(bounds[l]), "x^y within its bound");
    }
}

static void TestPowOnMaulUiInputs(void)
{
    // The ranges the sRGB transfer function raises, as Maul UI tests them.
    double worst = 0.0;
    for (int i = 0; i <= 1000; ++i)
    {
        double toLinear = 0.0899 + 0.9101 * (double)i / 1000.0;
        double fromLinear = 0.0031308 + 0.9968692 * (double)i / 1000.0;
        double a = UlpError(mtwnPow(toLinear, 2.4), ReferencePow(toLinear, 2.4));
        double b = UlpError(mtwnPow(fromLinear, 1.0 / 2.4), ReferencePow(fromLinear, 1.0 / 2.4));
        worst = a > worst ? a : worst;
        worst = b > worst ? b : worst;
    }
    CHECK(worst <= Bound(0.65), "the sRGB powers within 0.65 ulp");
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
    TestLogAtTheReductionEdges();
    TestExpOnMaulUiInputs();
    TestLogOnMaulUiInputs();
    TestSinCosSpecialValues();
    TestSinCosWithinOneUlp();
    TestSinCosNearQuadrantBoundaries();
    TestSinCosOnMaulUiInputs();
    TestAtan2SpecialValues();
    TestAtan2WithinOneUlp();
    TestAtanOfRatiosNearTheTable();
    TestCbrtSpecialValues();
    TestCbrtWithinHalfAnUlp();
    TestCbrtOnMaulUiInputs();
    TestPowSpecialValues();
    TestPowWithinOneUlp();
    TestPowOnMaulUiInputs();
    return s_failures == 0 ? 0 : 1;
}
