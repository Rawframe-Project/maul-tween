// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Colors against CSS Color 4's conversions.js, run under Node by
// tools/oracle/rivals.mjs: 729 gamma sRGB colors,
// extended ones among them, to linear sRGB, Oklab and OkLCh agree to
// rounding; the hue to rounding over the chroma, and powerless where
// conversions.js says it is.

#include "test_harness.h"

#include "maul-tween/interpolate.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

typedef struct RivalColor
{
    double rgb[3];
    double linear[3];
    double lab[3];
    // Lightness, chroma, hue in degrees or NaN.
    double lch[3];
} RivalColor;

#include "generated/rival_color.h"

static const double s_pi = 3.141592653589793;

// Whether three coordinates agree to 16 ulps of their size, at least 1.
static bool Agree(const double* got, const double* expected, int count)
{
    bool ok = true;
    for (int k = 0; k < count; ++k)
    {
        double size = fabs(expected[k]) > 1.0 ? fabs(expected[k]) : 1.0;
        ok = ok && fabs(got[k] - expected[k]) <= 16.0 * DBL_EPSILON * size;
    }
    return ok;
}

// Whether a hue agrees: an error in a and b of 16 ulps turns it by at most
// that over the chroma.
static bool HueAgrees(const double* got, const double* expected)
{
    if (isnan(got[2]) || isnan(expected[2]))
    {
        return isnan(got[2]) && isnan(expected[2]);
    }
    double turned = fabs(got[2] * 180.0 / s_pi - expected[2]);
    turned = turned > 180.0 ? 360.0 - turned : turned;
    return turned <= 16.0 * DBL_EPSILON / expected[1] * 180.0 / s_pi;
}

static void TestColors(void)
{
    uint32_t count = (uint32_t)(sizeof s_rivalColors / sizeof s_rivalColors[0]);
    int linear = 0;
    int lab = 0;
    int lch = 0;
    int hues = 0;
    int powerless = 0;
    for (uint32_t c = 0; c < count; ++c)
    {
        const RivalColor* r = &s_rivalColors[c];
        mtwnColor color = {(float)r->rgb[0], (float)r->rgb[1], (float)r->rgb[2], 1.0f};
        double got[4];
        CHECK(mtwnColorToSpace(color, mtwn_colorLinearSrgb, got) == mtwn_success, "linear");
        linear += Agree(got, r->linear, 3);
        CHECK(mtwnColorToSpace(color, mtwn_colorOklab, got) == mtwn_success, "Oklab");
        lab += Agree(got, r->lab, 3);
        CHECK(mtwnColorToSpace(color, mtwn_colorOklch, got) == mtwn_success, "OkLCh");
        lch += Agree(got, r->lch, 2);
        hues += HueAgrees(got, r->lch);
        powerless += isnan(r->lch[2]);
    }
    CHECK(count == 729, "every color the oracle wrote");
    CHECK(linear == (int)count, "linear sRGB as conversions.js gives it");
    CHECK(lab == (int)count, "Oklab as conversions.js gives it");
    CHECK(lch == (int)count, "OkLCh's lightness and chroma as conversions.js gives them");
    CHECK(hues == (int)count && powerless == 9, "the hues, and the nine grays' powerless");
}

int main(void)
{
    TestColors();
    return s_failures == 0 ? 0 : 1;
}
