// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Colors (records mtwn-0014, mtwn-0020, mtwn-0024): the conversions
// against conversions.js written again in long double, with its matrices
// applied one after another rather than combined; the hue methods on CSS
// Color 4's own worked examples; premultiplication, powerless hues and
// the ends of a mix.

#include "test_harness.h"

#include "maul-tween/interpolate.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_POW   powl
#define WIDE_CBRT  cbrtl
#define WIDE_ATAN2 atan2l
#define WIDE_SQRT  sqrtl
static const double s_slack = 0.0;
#else
typedef double Wide;
#define WIDE_POW   pow
#define WIDE_CBRT  cbrt
#define WIDE_ATAN2 atan2
#define WIDE_SQRT  sqrt
static const double s_slack = 1e-14;
#endif

static const Wide s_srgbToXyz[3][3] = {
    {(Wide)506752 / 1228815, (Wide)87881 / 245763, (Wide)12673 / 70218},
    {(Wide)87098 / 409605, (Wide)175762 / 245763, (Wide)12673 / 175545},
    {(Wide)7918 / 409605, (Wide)87881 / 737289, (Wide)1001167 / 1053270},
};
#define W(x) ((Wide)(x##L))
static const Wide s_xyzToLms[3][3] = {
    {W(0.8190224379967030), W(0.3619062600528904), W(-0.1288737815209879)},
    {W(0.0329836539323885), W(0.9292868615863434), W(0.0361446663506424)},
    {W(0.0481771893596242), W(0.2642395317527308), W(0.6335478284694309)},
};
static const Wide s_lmsToOklab[3][3] = {
    {W(0.2104542683093140), W(0.7936177747023054), W(-0.0040720430116193)},
    {W(1.9779985324311684), W(-2.4285922420485799), W(0.4505937096174110)},
    {W(0.0259040424655478), W(0.7827717124575296), W(-0.8086757549230774)},
};

static Wide LinearWide(Wide v)
{
    Wide magnitude = v < 0 ? -v : v;
    if (magnitude <= W(0.04045))
    {
        return v / W(12.92);
    }
    Wide r = WIDE_POW((magnitude + W(0.055)) / W(1.055), W(2.4));
    return v < 0 ? -r : r;
}

static void Apply(const Wide m[3][3], const Wide in[3], Wide out[3])
{
    for (int i = 0; i < 3; ++i)
    {
        out[i] = m[i][0] * in[0] + m[i][1] * in[1] + m[i][2] * in[2];
    }
}

// conversions.js's chain: lin_sRGB, lin_sRGB_to_XYZ, XYZ_to_OKLab; the
// smallest LMS component's magnitude in *smallestOut, where the cube root
// is steepest.
static void OklabWide(mtwnColor c, Wide out[3], Wide* smallestOut)
{
    Wide linear[3] = {LinearWide((Wide)c.red), LinearWide((Wide)c.green), LinearWide((Wide)c.blue)};
    Wide xyz[3];
    Wide lms[3];
    Apply(s_srgbToXyz, linear, xyz);
    Apply(s_xyzToLms, xyz, lms);
    Wide roots[3] = {WIDE_CBRT(lms[0]), WIDE_CBRT(lms[1]), WIDE_CBRT(lms[2])};
    Apply(s_lmsToOklab, roots, out);
    Wide smallest = lms[0] < 0 ? -lms[0] : lms[0];
    for (int k = 1; k < 3; ++k)
    {
        Wide m = lms[k] < 0 ? -lms[k] : lms[k];
        smallest = m < smallest ? m : smallest;
    }
    *smallestOut = smallest;
}

static uint64_t s_state = 99;

static float Random(float lo, float hi)
{
    uint64_t z = (s_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return lo + (hi - lo) * ((float)(z >> 40) * 0x1.0p-24f);
}

static void TestConversionsAgainstConversionsJs(void)
{
    double worstLab = 0.0;
    double worstLch = 0.0;
    for (int i = 0; i < 100000; ++i)
    {
        float lo = i % 4 == 0 ? -0.2f : 0.0f;
        float hi = i % 4 == 0 ? 1.2f : 1.0f;
        mtwnColor c = {Random(lo, hi), Random(lo, hi), Random(lo, hi), Random(0.0f, 1.0f)};
        double lab[4];
        double lch[4];
        Wide reference[3];
        Wide smallest = 0;
        OklabWide(c, reference, &smallest);
        // An LMS rounded by an ulp or two moves its cube root by that over
        // 3 x^(2/3): the error is judged per unit of it, at least 1.
        double steep = 1.0 / (100.0 * pow((double)smallest, 2.0 / 3.0));
        steep = steep > 1.0 ? steep : 1.0;
        CHECK(mtwnColorToSpace(c, mtwn_colorOklab, lab) == mtwn_success, "to Oklab");
        CHECK(mtwnColorToSpace(c, mtwn_colorOklch, lch) == mtwn_success, "to OkLCh");
        for (int k = 0; k < 3; ++k)
        {
            double e = fabs(lab[k] - (double)reference[k]) / steep;
            worstLab = e > worstLab ? e : worstLab;
        }
        Wide chroma = WIDE_SQRT(reference[1] * reference[1] + reference[2] * reference[2]);
        double e = fabs(lch[1] - (double)chroma) / steep;
        if (chroma > W(1e-3))
        {
            Wide hue = WIDE_ATAN2(reference[2], reference[1]);
            hue = hue < 0 ? hue + 2 * W(3.14159265358979323846264) : hue;
            double h = fabs(lch[2] - (double)hue) * (double)chroma / steep;
            e = h > e ? h : e;
        }
        worstLch = e > worstLch ? e : worstLch;
        CHECK(lab[3] == (double)c.alpha && lch[3] == (double)c.alpha, "alpha passes through");
    }
    if (!(worstLab <= 2e-15 + s_slack) || !(worstLch <= 2e-15 + s_slack))
    {
        printf("  Oklab %.3g, OkLCh %.3g\n", worstLab, worstLch);
    }
    CHECK(worstLab <= 2e-15 + s_slack, "Oklab within 2e-15 of conversions.js in long double");
    CHECK(worstLch <= 2e-15 + s_slack, "OkLCh too, the hue as arc length");
}

static void TestKnownValues(void)
{
    double lab[4];
    CHECK(mtwnColorToSpace((mtwnColor){1.0f, 0.0f, 0.0f, 1.0f}, mtwn_colorOklab, lab) ==
              mtwn_success,
          "red");
    // conversions.js under Node 24 gives 0.6279553639214313,
    // 0.22486306842627424, 0.12584627733058495.
    CHECK(fabs(lab[0] - 0.6279553639214313) <= 1e-15 &&
              fabs(lab[1] - 0.22486306842627424) <= 1e-15 &&
              fabs(lab[2] - 0.12584627733058495) <= 1e-15,
          "sRGB red as conversions.js gives it");
    CHECK(mtwnColorToSpace((mtwnColor){1.0f, 1.0f, 1.0f, 1.0f}, mtwn_colorOklab, lab) ==
                  mtwn_success &&
              fabs(lab[0] - 1.0) <= 1e-15 && fabs(lab[1]) <= 1e-15 && fabs(lab[2]) <= 1e-15,
          "white is lightness 1");
    double lch[4];
    CHECK(mtwnColorToSpace((mtwnColor){0.5f, 0.5f, 0.5f, 1.0f}, mtwn_colorOklch, lch) ==
                  mtwn_success &&
              isnan(lch[2]),
          "a gray's hue is powerless");
    double linear[4];
    CHECK(mtwnColorToSpace((mtwnColor){0.5f, -0.5f, 0.02f, 1.0f}, mtwn_colorLinearSrgb, linear) ==
                  mtwn_success &&
              linear[1] == -linear[0] && linear[2] == (double)0.02f / 12.92,
          "sRGB's transfer, reflected for negative values, linear near 0");
}

static void TestRoundTrips(void)
{
    int wrong = 0;
    static const mtwnColorSpace spaces[] = {mtwn_colorOklab, mtwn_colorOklch, mtwn_colorLinearSrgb,
                                            mtwn_colorSrgb};
    for (int i = 0; i < 40000; ++i)
    {
        mtwnColor c = {Random(-0.1f, 1.1f), Random(0.0f, 1.0f), Random(0.0f, 1.0f),
                       Random(0.0f, 1.0f)};
        for (int s = 0; s < 4; ++s)
        {
            double coordinates[4];
            mtwnColor back;
            wrong += mtwnColorToSpace(c, spaces[s], coordinates) != mtwn_success;
            wrong += mtwnColorFromSpace(coordinates, spaces[s], &back) != mtwn_success;
            wrong += back.red != c.red || back.green != c.green || back.blue != c.blue ||
                     back.alpha != c.alpha;
        }
    }
    CHECK(wrong == 0, "every space gives the same floats back");
}

static const double s_pi = 3.141592653589793;

// The OkLCh coordinates at the middle of a mix of two OkLCh colors, hues
// in degrees as CSS writes them, converted at this edge.
static void Middle(const double first[3], const double second[3], mtwnHueMethod method,
                   double out[4])
{
    double a[4] = {first[0], first[1], first[2] * s_pi / 180.0, 1.0};
    double b[4] = {second[0], second[1], second[2] * s_pi / 180.0, 1.0};
    mtwnColor from;
    mtwnColor to;
    mtwnColorMix mix;
    if (mtwnColorFromSpace(a, mtwn_colorOklch, &from) != mtwn_success ||
        mtwnColorFromSpace(b, mtwn_colorOklch, &to) != mtwn_success ||
        mtwnMakeColorMix(from, to, mtwn_colorOklch, method, &mix) != mtwn_success ||
        mtwnColorToSpace(mtwnMixColor(&mix, 0.5), mtwn_colorOklch, out) != mtwn_success)
    {
        out[0] = out[1] = out[2] = (double)NAN;
    }
}

static bool Near(const double got[4], double l, double c, double h)
{
    // Float colors carry about 1e-7; hues are compared in degrees, mod 360.
    double dh = fmod(fabs(got[2] * 180.0 / s_pi - h), 360.0);
    dh = dh > 180.0 ? 360.0 - dh : dh;
    return fabs(got[0] - l) <= 2e-6 && fabs(got[1] - c) <= 2e-6 && dh <= 2e-4;
}

static void TestHueMethodsOnCssExamples(void)
{
    // CSS Color 4, section 13.5.
    static const double red[3] = {0.6, 0.24, 30.0};
    static const double yellow[3] = {0.8, 0.15, 90.0};
    static const double brown[3] = {0.5, 0.1, 30.0};
    static const double turquoise[3] = {0.7, 0.1, 190.0};
    double got[4];
    Middle(red, yellow, mtwn_hueShorter, got);
    CHECK(Near(got, 0.7, 0.195, 60.0), "shorter: oklch(0.7 0.195 60)");
    Middle(red, yellow, mtwn_hueLonger, got);
    CHECK(Near(got, 0.7, 0.195, 240.0), "longer: oklch(0.7 0.195 240)");
    Middle(brown, turquoise, mtwn_hueIncreasing, got);
    CHECK(Near(got, 0.6, 0.1, 110.0), "increasing: 30 to 190 through 110");
    Middle(brown, turquoise, mtwn_hueDecreasing, got);
    CHECK(Near(got, 0.6, 0.1, 290.0), "decreasing: oklch(0.6 0.1 290)");
    Middle(yellow, red, mtwn_hueIncreasing, got);
    CHECK(Near(got, 0.7, 0.195, 240.0), "increasing from 90 to 30 goes the long way");
}

static void TestPremultiplication(void)
{
    mtwnColorMix mix;
    mtwnColor clear = {1.0f, 0.0f, 0.0f, 0.0f};
    mtwnColor blue = {0.0f, 0.0f, 1.0f, 1.0f};
    CHECK(mtwnMakeColorMix(clear, blue, mtwn_colorOklab, mtwn_hueShorter, &mix) == mtwn_success,
          "transparent red to blue");
    mtwnColor middle = mtwnMixColor(&mix, 0.5);
    CHECK(fabsf(middle.red) <= 1e-6f && fabsf(middle.green) <= 1e-6f &&
              fabsf(middle.blue - 1.0f) <= 1e-6f && middle.alpha == 0.5f,
          "a transparent end lends no color: half-transparent blue");
    mtwnColor invisible = {0.0f, 1.0f, 0.0f, 0.0f};
    CHECK(mtwnMakeColorMix(clear, invisible, mtwn_colorOklab, mtwn_hueShorter, &mix) ==
              mtwn_success,
          "two transparent ends");
    middle = mtwnMixColor(&mix, 0.5);
    CHECK(middle.red == 0.0f && middle.green == 0.0f && middle.blue == 0.0f && middle.alpha == 0.0f,
          "at alpha 0 the premultiplied value, as CSS says");
}

static void TestMixes(void)
{
    mtwnColorMix mix;
    mtwnColor black = {0.0f, 0.0f, 0.0f, 1.0f};
    mtwnColor white = {1.0f, 1.0f, 1.0f, 1.0f};
    CHECK(mtwnMakeColorMix(black, white, mtwn_colorSrgb, mtwn_hueShorter, &mix) == mtwn_success,
          "gamma sRGB");
    mtwnColor middle = mtwnMixColor(&mix, 0.5);
    CHECK(middle.red == 0.5f && middle.green == 0.5f && middle.blue == 0.5f, "gray 0.5");
    CHECK(mtwnMakeColorMix(black, white, mtwn_colorLinearSrgb, mtwn_hueShorter, &mix) ==
              mtwn_success,
          "linear sRGB");
    middle = mtwnMixColor(&mix, 0.5);
    CHECK(fabsf(middle.red - 0.7353569830524495f) <= 1e-7f, "half the light, gamma 0.735");
    mtwnColor from = {0.2f, 0.4f, 0.6f, 0.3f};
    mtwnColor to = {0.9f, 0.1f, 0.3f, 0.8f};
    CHECK(mtwnMakeColorMix(from, to, mtwn_colorOklch, mtwn_hueLonger, &mix) == mtwn_success,
          "OkLCh, longer");
    mtwnColor start = mtwnMixColor(&mix, 0.0);
    mtwnColor end = mtwnMixColor(&mix, 1.0);
    CHECK(start.red == from.red && start.green == from.green && start.blue == from.blue &&
              start.alpha == from.alpha && end.red == to.red && end.alpha == to.alpha,
          "the ends exactly");
    CHECK(mtwnMixColor(&mix, 1.5).alpha == 1.0f && mtwnMixColor(&mix, -1.0).alpha == 0.0f,
          "alpha clamped past the ends");
    mtwnColor near = mtwnMixColor(&mix, 1e-9);
    CHECK(fabsf(near.red - from.red) <= 1e-6f && fabsf(near.alpha - from.alpha) <= 1e-6f,
          "continuous at the ends");
    // White's hue is powerless: blue's hue all the way, no turn.
    CHECK(mtwnMakeColorMix(white, (mtwnColor){0.0f, 0.0f, 1.0f, 1.0f}, mtwn_colorOklch,
                           mtwn_hueShorter, &mix) == mtwn_success,
          "white to blue");
    double blueLch[4];
    double midLch[4];
    CHECK(mtwnColorToSpace((mtwnColor){0.0f, 0.0f, 1.0f, 1.0f}, mtwn_colorOklch, blueLch) ==
                  mtwn_success &&
              mtwnColorToSpace(mtwnMixColor(&mix, 0.5), mtwn_colorOklch, midLch) == mtwn_success &&
              fabs(midLch[2] - blueLch[2]) <= 2e-5,
          "a powerless hue takes the other end's");
}

// The OkLCh hue at the middle of a mix of two colors.
static double MiddleHue(mtwnColor from, mtwnColor to, mtwnHueMethod method)
{
    mtwnColorMix mix;
    double lch[4] = {0.0, 0.0, (double)NAN, 0.0};
    if (mtwnMakeColorMix(from, to, mtwn_colorOklch, method, &mix) != mtwn_success ||
        mtwnColorToSpace(mtwnMixColor(&mix, 0.5), mtwn_colorOklch, lch) != mtwn_success)
    {
        return (double)NAN;
    }
    return lch[2];
}

// Two colors either side of hue 0 in OkLCh, 20 degrees apart.
static void AcrossZero(mtwnColor* below, mtwnColor* above)
{
    const double degree = s_pi / 180.0;
    const double low[4] = {0.7, 0.1, 350.0 * degree, 1.0};
    const double high[4] = {0.7, 0.1, 10.0 * degree, 1.0};
    CHECK(mtwnColorFromSpace(low, mtwn_colorOklch, below) == mtwn_success &&
              mtwnColorFromSpace(high, mtwn_colorOklch, above) == mtwn_success,
          "colors either side of hue 0");
}

// Its distance from hue 0, whichever side.
static double FromZero(double hue)
{
    double h = fmod(hue, 2.0 * s_pi);
    h = h < 0.0 ? h + 2.0 * s_pi : h;
    return h < s_pi ? h : 2.0 * s_pi - h;
}

static void TestShorterAcrossZero(void)
{
    // The shorter way between 350 and 10 degrees crosses 0, from either
    // end: CSS Color 4 adds a turn to the smaller hue (to the first going
    // up, to the second going down); the longer way passes 180.
    mtwnColor below;
    mtwnColor above;
    AcrossZero(&below, &above);
    CHECK(FromZero(MiddleHue(below, above, mtwn_hueShorter)) <= 1e-4,
          "350 to 10 degrees the shorter way through 0");
    CHECK(FromZero(MiddleHue(above, below, mtwn_hueShorter)) <= 1e-4,
          "10 to 350 degrees the shorter way through 0");
    CHECK(fabs(FromZero(MiddleHue(below, above, mtwn_hueLonger)) - s_pi) <= 1e-4,
          "the longer way through 180");
}

static void TestEqualHues(void)
{
    // Equal hues, as a powerless end makes them: CSS's longer turns a full
    // circle, increasing and decreasing stay put, shorter too.
    mtwnColor white = {1.0f, 1.0f, 1.0f, 1.0f};
    mtwnColor blue = {0.0f, 0.0f, 1.0f, 1.0f};
    double lch[4];
    CHECK(mtwnColorToSpace(blue, mtwn_colorOklch, lch) == mtwn_success, "blue");
    double h = lch[2];
    double turned = fmod(h + s_pi, 2.0 * s_pi);
    CHECK(fabs(MiddleHue(white, blue, mtwn_hueShorter) - h) <= 2e-5, "shorter stays");
    CHECK(fabs(MiddleHue(white, blue, mtwn_hueLonger) - turned) <= 2e-5,
          "longer turns all the way");
    CHECK(fabs(MiddleHue(white, blue, mtwn_hueIncreasing) - h) <= 2e-5, "increasing stays");
    CHECK(fabs(MiddleHue(white, blue, mtwn_hueDecreasing) - h) <= 2e-5, "decreasing stays");
    CHECK(fabs(MiddleHue(blue, white, mtwn_hueShorter) - h) <= 2e-5,
          "a powerless end at 1 takes the hue at 0");
    CHECK(fabs(MiddleHue(blue, blue, mtwn_hueLonger) - turned) <= 2e-5,
          "the same color the long way turns too");
}

static void TestBelowZeroAlpha(void)
{
    // Past the end where alpha reaches 0 the interpolated alpha is below
    // 0: the channels stay premultiplied, never divided by it.
    mtwnColor red = {1.0f, 0.0f, 0.0f, 1.0f};
    mtwnColor clear = {1.0f, 0.0f, 0.0f, 0.0f};
    mtwnColorMix mix;
    CHECK(mtwnMakeColorMix(red, clear, mtwn_colorOklab, mtwn_hueShorter, &mix) == mtwn_success,
          "red fading out");
    double lab[4];
    CHECK(mtwnColorToSpace(red, mtwn_colorOklab, lab) == mtwn_success, "red in Oklab");
    double premultiplied[4] = {-0.5 * lab[0], -0.5 * lab[1], -0.5 * lab[2], 0.0};
    mtwnColor expected;
    CHECK(mtwnColorFromSpace(premultiplied, mtwn_colorOklab, &expected) == mtwn_success, "made");
    mtwnColor past = mtwnMixColor(&mix, 1.5);
    CHECK(fabsf(past.red - expected.red) <= 1e-6f && fabsf(past.green - expected.green) <= 1e-6f &&
              past.alpha == 0.0f,
          "at 1.5 the premultiplied channels, alpha 0");
}

static void TestRefusals(void)
{
    mtwnColorMix mix;
    mtwnColor ok = {0.5f, 0.5f, 0.5f, 1.0f};
    CHECK(mtwnMakeColorMix(ok, ok, 4, mtwn_hueShorter, &mix) == mtwn_errorInvalid, "space");
    CHECK(mtwnMakeColorMix(ok, ok, mtwn_colorOklch, 4, &mix) == mtwn_errorInvalid, "method");
    CHECK(mtwnMakeColorMix((mtwnColor){NAN, 0.0f, 0.0f, 1.0f}, ok, mtwn_colorOklab, mtwn_hueShorter,
                           &mix) == mtwn_errorInvalid,
          "a NaN channel");
    CHECK(mtwnMakeColorMix(ok, (mtwnColor){0.0f, INFINITY, 0.0f, 1.0f}, mtwn_colorOklab,
                           mtwn_hueShorter, &mix) == mtwn_errorInvalid,
          "an infinite channel");
    CHECK(mtwnMakeColorMix(ok, (mtwnColor){0.0f, 0.0f, 0.0f, 1.5f}, mtwn_colorOklab,
                           mtwn_hueShorter, &mix) == mtwn_errorInvalid,
          "alpha above 1");
    CHECK(mtwnMakeColorMix(ok, (mtwnColor){0.0f, 0.0f, 0.0f, NAN}, mtwn_colorOklab, mtwn_hueShorter,
                           &mix) == mtwn_errorInvalid,
          "a NaN alpha");
    CHECK(mtwnMakeColorMix(ok, ok, mtwn_colorOklab, mtwn_hueShorter, NULL) == mtwn_errorInvalid,
          "null out");
    double c[4];
    CHECK(mtwnColorToSpace(ok, 4, c) == mtwn_errorInvalid, "to an unknown space");
    CHECK(mtwnColorToSpace((mtwnColor){0.0f, 0.0f, 0.0f, -0.1f}, mtwn_colorOklab, c) ==
              mtwn_errorInvalid,
          "a negative alpha");
    mtwnColor out;
    double missing[4] = {0.5, 0.1, (double)NAN, 1.0};
    double zero[4] = {0.5, 0.1, 0.0, 1.0};
    mtwnColor atZero;
    CHECK(mtwnColorFromSpace(missing, mtwn_colorOklch, &out) == mtwn_success &&
              mtwnColorFromSpace(zero, mtwn_colorOklch, &atZero) == mtwn_success &&
              out.red == atZero.red && out.green == atZero.green && out.blue == atZero.blue,
          "a missing hue is 0");
    double bad[4] = {0.5, (double)NAN, 0.0, 1.0};
    CHECK(mtwnColorFromSpace(bad, mtwn_colorOklch, &out) == mtwn_errorInvalid, "a NaN chroma");
    CHECK(mtwnColorFromSpace(missing, mtwn_colorOklab, &out) == mtwn_errorInvalid,
          "a NaN b in Oklab");
    double loud[4] = {0.5, 0.0, 0.0, 3.0};
    CHECK(mtwnColorFromSpace(loud, mtwn_colorOklab, &out) == mtwn_success && out.alpha == 1.0f,
          "alpha clamped back");
    CHECK(mtwnColorFromSpace(NULL, mtwn_colorOklab, &out) == mtwn_errorInvalid, "no coordinates");
}

// Far past its ends a mix is the line: equal coordinates stay what they
// are (an alpha of 1 mixed to 1 was 0 at 1e20, the weighted sum's terms
// cancelling), and the channels are held within the floats, as a target's
// are (records mtwn-0069 and mtwn-0070).
static void TestFarProgress(void)
{
    mtwnColorMix mix;
    CHECK(mtwnMakeColorMix((mtwnColor){0, 0, 0, 1.0f}, (mtwnColor){1.0f, 1.0f, 1.0f, 1.0f},
                           mtwn_colorOklab, mtwn_hueShorter, &mix) == mtwn_success,
          "black to white");
    mtwnColor far = mtwnMixColor(&mix, 1e20);
    CHECK(far.alpha == 1.0f && isfinite(far.red) && far.red > 1e20f, "1e20 on: alpha still 1");
    mtwnColor farther = mtwnMixColor(&mix, 1e44);
    CHECK(farther.red == 0x1.fffffep127f && farther.green == 0x1.fffffep127f &&
              farther.blue == 0x1.fffffep127f && farther.alpha == 1.0f,
          "1e44 on: held at the largest float");
}

int main(void)
{
    TestConversionsAgainstConversionsJs();
    TestKnownValues();
    TestRoundTrips();
    TestHueMethodsOnCssExamples();
    TestPremultiplication();
    TestMixes();
    TestEqualHues();
    TestShorterAcrossZero();
    TestBelowZeroAlpha();
    TestRefusals();
    TestFarProgress();
    return s_failures == 0 ? 0 : 1;
}
