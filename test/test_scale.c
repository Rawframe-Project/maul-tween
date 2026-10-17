// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Scales and zooms (record mtwn-0059): against 60-digit decimals
// (tools/oracle/zoom_oracle.py), van Wijk and Nuij's path written as the
// paper writes it; ends exact, equal ratios for equal steps, the zoom of
// equal centres the limit of a pan going to 0, and the refusals.

#include "test_harness.h"

#include "maul-tween/interpolate.h"

#include <math.h>

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
    CHECK(refused == 6, "no width, NaN, rho 0 or infinite, NULL and a path past the range refused");
}

int main(void)
{
    TestScaleCases();
    TestScaleShape();
    TestZoomCases();
    TestZoomShape();
    return s_failures == 0 ? 0 : 1;
}
