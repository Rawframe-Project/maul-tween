// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Scales and zooms: against 60-digit decimals
// (tools/oracle/zoom_oracle.py), van Wijk and Nuij's path written as the
// paper writes it, out to the doubles' largest progress; ends exact, equal
// ratios for equal steps, the zoom of equal centres the limit of a pan
// going to 0, no NaN however far a path extends, and the refusals.

#include "test_harness.h"

#include "maul-tween/interpolate.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct ScaleCase
{
    double from;
    double to;
    double progress;
    double value;
} ScaleCase;

typedef struct ZoomCase
{
    double x0;
    double y0;
    double w0;
    double x1;
    double y1;
    double w1;
    double rho;
    double progress;
    double x;
    double y;
    double width;
    double length;
} ZoomCase;

#include "generated/zoom_cases.h"

static void TestScaleCases(void)
{
    double worst = 0.0;
    int count = (int)(sizeof s_scaleCases / sizeof s_scaleCases[0]);
    for (int i = 0; i < count; ++i)
    {
        const ScaleCase* c = &s_scaleCases[i];
        mtwnScaleMix mix;
        if (mtwnMakeScaleMix(c->from, c->to, &mix) != mtwn_success)
        {
            worst = 1.0;
            continue;
        }
        double error = fabs(mtwnMixScale(&mix, c->progress) - c->value) / c->value;
        worst = error > worst ? error : worst;
    }
    if (!(worst <= 1e-12))
    {
        printf("  scale: %.3g\n", worst);
    }
    // The exponent's rounding scales with log(to / from), up to 1,400 here.
    CHECK(worst <= 1e-12, "every scale within 1e-12 relative of its decimals");
}

static void TestScaleShape(void)
{
    mtwnScaleMix mix;
    CHECK(mtwnMakeScaleMix(0.1, 1000.0, &mix) == mtwn_success, "a scale mix");
    CHECK(mtwnMixScale(&mix, 0.0) == 0.1 && mtwnMixScale(&mix, 1.0) == 1000.0, "its ends exact");
    // Equal steps, equal ratios: 0.1 to 1000 is four tenfolds.
    double worst = 0.0;
    for (int k = 0; k < 4; ++k)
    {
        double ratio = mtwnMixScale(&mix, (k + 1) / 4.0) / mtwnMixScale(&mix, k / 4.0);
        worst = fabs(ratio - 10.0) > worst ? fabs(ratio - 10.0) : worst;
    }
    CHECK(worst <= 1e-13, "each quarter a tenfold");
    // Across the doubles' range, no overflow on the way.
    CHECK(mtwnMakeScaleMix(1e-300, 1e300, &mix) == mtwn_success, "a scale across the range");
    double v = mtwnMixScale(&mix, 0.9);
    CHECK(isfinite(v) && fabs(v / 1e240 - 1.0) <= 1e-12, "1e-300 to 1e300 at 0.9 is 1e240");
    CHECK(mtwnMixScale(&mix, 2.0) == HUGE_VAL, "past the range it overflows");
    const double bad[] = {0.0, -1.0, HUGE_VAL, (double)NAN};
    int refused = 0;
    for (int i = 0; i < 4; ++i)
    {
        refused += mtwnMakeScaleMix(bad[i], 1.0, &mix) == mtwn_errorInvalid;
        refused += mtwnMakeScaleMix(1.0, bad[i], &mix) == mtwn_errorInvalid;
    }
    CHECK(refused == 8 && mtwnMakeScaleMix(1.0, 2.0, NULL) == mtwn_errorInvalid,
          "zero, negative, infinite and NaN scales refused");
}

// How far got is from want past four of want's ulps.
static double Beyond(double got, double want)
{
    double ulp = nextafter(fabs(want), HUGE_VAL) - fabs(want);
    return fmax(fabs(got - want) - 4.0 * ulp, 0.0);
}

static void TestZoomCases(void)
{
    double worstView = 0.0;
    double worstLength = 0.0;
    int count = (int)(sizeof s_zoomCases / sizeof s_zoomCases[0]);
    for (int i = 0; i < count; ++i)
    {
        const ZoomCase* c = &s_zoomCases[i];
        mtwnZoomMix mix;
        mtwnZoomView from = {c->x0, c->y0, c->w0};
        mtwnZoomView to = {c->x1, c->y1, c->w1};
        if (mtwnMakeZoomMix(from, to, c->rho, &mix) != mtwn_success)
        {
            worstView = 1.0;
            continue;
        }
        mtwnZoomView v = mtwnMixZoom(&mix, c->progress);
        // Positions against the larger of the pan and the widths, past the
        // four ulps a coordinate far from 0 rounds to by itself.
        double scale = fmax(hypot(c->x1 - c->x0, c->y1 - c->y0), fmax(c->w0, c->w1));
        double error = fmax(Beyond(v.x, c->x), Beyond(v.y, c->y)) / scale;
        error = fmax(error, fabs(v.width - c->width) / c->width);
        worstView = error > worstView ? error : worstView;
        double length = fabs(mtwnZoomLength(&mix) - c->length) / fmax(c->length, 1e-300);
        worstLength = length > worstLength ? length : worstLength;
    }
    if (!(worstView <= 1e-12 && worstLength <= 1e-12))
    {
        printf("  zoom: views %.3g, lengths %.3g\n", worstView, worstLength);
    }
    CHECK(worstView <= 1e-12, "every view within 1e-12 of its decimals");
    CHECK(worstLength <= 1e-12, "every path's length within 1e-12 relative");
}

// Far past the ends: the decimals' view at progress 16, just past where
// sinh and cosh overflow for a share past the doubles' range, and from
// 1000 to the largest double either way, where the path settles; the
// width, going to 0, against the larger of itself and the views' widths.
// At 16 the path's parameters near -391 each carry about an ulp,
// 5.7e-14; the point along the path carries 16 times both, and the view
// twice that, 3.6e-12: within 1e-11. A NaN fails.
static void TestZoomFarCases(void)
{
    double worst = 0.0;
    int count = (int)(sizeof s_zoomFarCases / sizeof s_zoomFarCases[0]);
    for (int i = 0; i < count; ++i)
    {
        const ZoomCase* c = &s_zoomFarCases[i];
        mtwnZoomMix mix;
        mtwnZoomView from = {c->x0, c->y0, c->w0};
        mtwnZoomView to = {c->x1, c->y1, c->w1};
        if (mtwnMakeZoomMix(from, to, c->rho, &mix) != mtwn_success)
        {
            worst = 1.0;
            continue;
        }
        mtwnZoomView v = mtwnMixZoom(&mix, c->progress);
        // A centre settled far from the views is measured against itself.
        double scale = fmax(hypot(c->x1 - c->x0, c->y1 - c->y0), fmax(c->w0, c->w1));
        double x = Beyond(v.x, c->x) / fmax(scale, fabs(c->x));
        double y = Beyond(v.y, c->y) / fmax(scale, fabs(c->y));
        double width = fabs(v.width - c->width) / fmax(c->width, fmax(c->w0, c->w1));
        double error = fmax(fmax(x, y), width);
        worst = !(error <= worst) ? error : worst;
    }
    if (!(worst <= 1e-11))
    {
        printf("  far zooms: %.3g\n", worst);
    }
    CHECK(worst <= 1e-11, "every far view within 1e-11 of its decimals");
}

typedef struct ZoomFar
{
    mtwnZoomView from;
    mtwnZoomView to;
    double rho;
    // The first k from which 0.7.0's view at 2^k, and at -2^k, was not
    // finite: 1024 stands for the largest double, 1025 for none.
    int above;
    int below;
} ZoomFar;

// Paths a maker gives, at progress 2^k and -2^k for k from 0 to 1023 and
// at the largest double either way. No field is NaN at any of them. Where
// 0.7.0 gave a finite view, the view is the same to the bit: the hash of
// those views is the one 0.7.0's build gives.
static const ZoomFar s_zoomFar[] = {
    // #78's example.
    {{0.0, 0.0, 1.0}, {10.0, 0.0, 2.0}, 1.4142135623730951, 8, 8},
    // A zoom in, Mapbox's rho.
    {{0.0, 0.0, 1.0}, {100.0, 50.0, 0.01}, 1.42, 6, 6},
    {{3.0, 2.0, 8.0}, {-5.0, 7.0, 0.125}, 0.5, 8, 8},
    // A pan along x whose share passes the doubles' range.
    {{0.0, 0.0, 1.0}, {0x1p-500, 0.0, 0x1p33}, 1.0, 4, 4},
    {{1e6, -1e6, 1e3}, {2e6, 0.0, 1e5}, 3.0, 6, 6},
    // Widths near the doubles' largest, a small rho.
    {{0.0, 0.0, 0x1p1000}, {0x1p1000, 0.0, 0x1p999}, 0x1p-10, 10, 4},
    // A pan alone, a small rho.
    {{-7.0, 3.0, 1.0}, {-7.0, 9.0, 1.0}, 0.05, 16, 16},
    // A pan far under the width.
    {{0.0, 0.0, 1.0}, {0x1p-60, 0.0, 1.0 + 0x1p-52}, 2.0, 60, 60},
    // A zoom alone, to an infinite width below.
    {{5.0, 5.0, 8.0}, {5.0, 5.0, 1.0}, 2.0, 1025, 9},
    // A path the maker gives no length, where rho times the progress
    // overflows from 2^1023 on.
    {{0.0, 0.0, 1.0}, {0x1p-66, 0.0, 1.0 + 0x1p-52}, 2.0, 1023, 1023},
};
static const uint64_t s_zoomFarHash = 0xf984cedb9957c536u;

static uint64_t HashDouble(uint64_t hash, double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    for (int i = 0; i < 8; ++i)
    {
        hash = (hash ^ ((bits >> (8 * i)) & 0xffu)) * 0x100000001b3u;
    }
    return hash;
}

static void TestZoomFar(void)
{
    uint64_t hash = 0xcbf29ce484222325u;
    int nan = 0;
    int count = (int)(sizeof s_zoomFar / sizeof s_zoomFar[0]);
    for (int i = 0; i < count; ++i)
    {
        const ZoomFar* f = &s_zoomFar[i];
        mtwnZoomMix mix;
        CHECK(mtwnMakeZoomMix(f->from, f->to, f->rho, &mix) == mtwn_success, "a far zoom");
        for (int sign = 0; sign < 2; ++sign)
        {
            int finite = sign == 0 ? f->above : f->below;
            for (int k = 0; k <= 1024; ++k)
            {
                double progress = k == 1024 ? DBL_MAX : ldexp(1.0, k);
                mtwnZoomView v = mtwnMixZoom(&mix, sign == 0 ? progress : -progress);
                nan += isnan(v.x) || isnan(v.y) || isnan(v.width);
                if (k < finite)
                {
                    hash = HashDouble(HashDouble(HashDouble(hash, v.x), v.y), v.width);
                }
            }
        }
    }
    CHECK(nan == 0, "no NaN out to the largest progress either way");
    CHECK(hash == s_zoomFarHash, "every view 0.7.0 gave finite the same to the bit");
}

static void TestZoomShape(void)
{
    mtwnZoomMix mix;
    mtwnZoomView a = {0.1, -3.0, 2.0};
    mtwnZoomView b = {7.3, 4.0, 0.25};
    CHECK(mtwnMakeZoomMix(a, b, sqrt(2.0), &mix) == mtwn_success, "a zoom");
    mtwnZoomView at0 = mtwnMixZoom(&mix, 0.0);
    mtwnZoomView at1 = mtwnMixZoom(&mix, 1.0);
    CHECK(at0.x == a.x && at0.y == a.y && at0.width == a.width && at1.x == b.x && at1.y == b.y &&
              at1.width == b.width,
          "its ends exact");
    // A pan of 7 widths' worth zooms out on the way.
    double widest = 0.0;
    for (int k = 1; k < 64; ++k)
    {
        widest = fmax(widest, mtwnMixZoom(&mix, k / 64.0).width);
    }
    CHECK(widest > a.width, "it zooms out past both ends to pan");
    // Equal centres: a geometric zoom, the length |log(w1 / w0)| / rho,
    // also the limit of a pan going to 0.
    mtwnZoomView c = {5.0, 5.0, 8.0};
    mtwnZoomView d = {5.0, 5.0, 1.0};
    CHECK(mtwnMakeZoomMix(c, d, 2.0, &mix) == mtwn_success, "a zoom alone");
    CHECK(fabs(mtwnZoomLength(&mix) - log(8.0) / 2.0) <= 1e-15, "its length");
    CHECK(fabs(mtwnMixZoom(&mix, 1.0 / 3.0).width - 4.0) <= 1e-14, "a third of the way, 4");
    mtwnZoomMix near;
    mtwnZoomView e = {5.0 + 1e-12, 5.0, 1.0};
    CHECK(mtwnMakeZoomMix(c, e, 2.0, &near) == mtwn_success, "a zoom with a tiny pan");
    double gap = fabs(mtwnZoomLength(&near) - mtwnZoomLength(&mix)) / mtwnZoomLength(&mix);
    gap = fmax(gap, fabs(mtwnMixZoom(&near, 0.4).width - mtwnMixZoom(&mix, 0.4).width) /
                        mtwnMixZoom(&mix, 0.4).width);
    CHECK(gap <= 1e-10, "a pan going to 0 meets the zoom alone");
    // Zooming in has a positive length, as zooming out (d3's
    // interpolateZoom gives a negative duration there).
    mtwnZoomMix in;
    CHECK(mtwnMakeZoomMix(d, c, 2.0, &in) == mtwn_success &&
              mtwnZoomLength(&in) == mtwnZoomLength(&mix),
          "in and out the same length");
    CHECK(mtwnMakeZoomMix(c, c, 2.0, &mix) == mtwn_success && mtwnZoomLength(&mix) == 0.0 &&
              mtwnMixZoom(&mix, 0.5).width == c.width,
          "equal views stay");
    int refused = 0;
    mtwnZoomView flat = {0.0, 0.0, 0.0};
    mtwnZoomView far = {(double)NAN, 0.0, 1.0};
    refused += mtwnMakeZoomMix(flat, a, 1.0, &mix) == mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, far, 1.0, &mix) == mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, b, 0.0, &mix) == mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, b, HUGE_VAL, &mix) == mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, b, 1.0, NULL) == mtwn_errorInvalid;
    mtwnZoomView huge = {1e200, 0.0, 1.0};
    refused += mtwnMakeZoomMix(a, huge, 1.0, &mix) == mtwn_errorInvalid;
    // A negative width with a pan, which the path would read only squared.
    mtwnZoomView inverted = {a.x + 5.0, a.y, -a.width};
    refused += mtwnMakeZoomMix(a, inverted, 1.0, &mix) == mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(inverted, a, 1.0, &mix) == mtwn_errorInvalid;
    CHECK(refused == 8, "no width, NaN, rho 0 or infinite, NULL, a path past the range and a "
                        "negative width at either end refused");
}

// Views and paths a zoom cannot take: a NaN or infinite coordinate; a
// path whose parameter passes the exponential's range, zooming from 1
// into 1e-305 while panning a width; a rho whose square is below the
// normal floats, the pan's scale overflowing; a rho so small the
// length is infinite.
static void TestZoomRefusals(void)
{
    mtwnZoomMix mix;
    mtwnZoomView a = {0.0, 0.0, 1.0};
    int refused = 0;
    refused += mtwnMakeZoomMix(a, (mtwnZoomView){0.0, (double)NAN, 1.0}, sqrt(2.0), &mix) ==
               mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, (mtwnZoomView){0.0, 0.0, (double)INFINITY}, sqrt(2.0), &mix) ==
               mtwn_errorInvalid;
    refused +=
        mtwnMakeZoomMix(a, (mtwnZoomView){1.0, 0.0, 1e-305}, sqrt(2.0), &mix) == mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, (mtwnZoomView){1.0, 0.0, 1.0}, 1e-160, &mix) == mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, (mtwnZoomView){0.0, 0.0, 2.0}, 1e-320, &mix) == mtwn_errorInvalid;
    CHECK(refused == 5, "a NaN y, an infinite width, a range passed, a pan's scale and a "
                        "length overflowing refused");
    CHECK(mtwnMakeZoomMix(a, (mtwnZoomView){1.0, 0.0, 1e-300}, sqrt(2.0), &mix) == mtwn_success,
          "into 1e-300 still taken");
}

// A path parameter of exactly 700 at the start (zooming out to a width of
// 1.007e152) or at the end (into 1.97e-304), on the range's bound, is
// refused. A negative rho with equal views, whose
// length would be -0, is refused as every rho not above 0.
static void TestZoomRange(void)
{
    mtwnZoomMix mix;
    mtwnZoomView a = {0.0, 0.0, 1.0};
    CHECK(mtwnMakeZoomMix(a, (mtwnZoomView){1.0, 0.0, 2.0}, 1.0, &mix) == mtwn_success,
          "a zoom out with a pan");
    int refused = 0;
    refused += mtwnMakeZoomMix(a, (mtwnZoomView){1.0, 0.0, 0x1.ec417a3e63b75p+504}, 1.0, &mix) ==
               mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, (mtwnZoomView){1.0, 0.0, 0x1.14f2b0fb93194p-1009}, 1.0, &mix) ==
               mtwn_errorInvalid;
    refused += mtwnMakeZoomMix(a, a, -1.0, &mix) == mtwn_errorInvalid;
    CHECK(refused == 3, "a parameter of 700 at either end and a negative rho refused");
}

int main(void)
{
    TestScaleCases();
    TestScaleShape();
    TestZoomCases();
    TestZoomFarCases();
    TestZoomFar();
    TestZoomShape();
    TestZoomRefusals();
    TestZoomRange();
    return s_failures == 0 ? 0 : 1;
}
