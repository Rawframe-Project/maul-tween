// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Paths (records mtwn-0016, mtwn-0020, mtwn-0031): segments against their
// formulas written again in long double, Catmull-Rom against Barry and
// Goldman's pyramid (another construction of the same curve), arc length
// against a Richardson-extrapolated polyline and a number's against its
// total variation, and constant speed on lines whose parameter is uneven
// but whose length is exact and on a curve against its parameter found
// again by Simpson's rule.

#include "test_harness.h"

#include "maul-tween/path.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_SQRT  sqrtl
#define WIDE_POW   powl
#define WIDE_ASINH asinhl
#else
typedef double Wide;
#define WIDE_SQRT  sqrt
#define WIDE_POW   pow
#define WIDE_ASINH asinh
#endif

static mtwnKeyframe Key(double offset, float x, float y, mtwnSegmentKind kind)
{
    mtwnKeyframe k;
    memset(&k, 0, sizeof k);
    k.offset = offset;
    k.value = (mtwnVec4){x, y, 0.0f, 0.0f};
    k.easing = mtwnLinearEasing();
    k.kind = kind;
    return k;
}

static void TestLinearAndStep(void)
{
    mtwnKeyframe keys[3] = {Key(0.0, 0.0f, 10.0f, mtwn_segmentLinear),
                            Key(0.25, 4.0f, 6.0f, mtwn_segmentStep),
                            Key(1.0, 8.0f, -2.0f, mtwn_segmentLinear)};
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 3, 2, 0.5, NULL, &path) == mtwn_success, "made");
    double v[4];
    double t[4];
    mtwnSamplePath(&path, 0.0, v, t);
    CHECK(v[0] == 0.0 && v[1] == 10.0 && v[2] == 0.0 && v[3] == 0.0, "the start; channels past 0");
    mtwnSamplePath(&path, 0.125, v, t);
    CHECK(v[0] == 2.0 && v[1] == 8.0 && t[0] == 4.0 && t[1] == -4.0, "halfway along a line");
    mtwnSamplePath(&path, 0.25, v, NULL);
    CHECK(v[0] == 4.0 && v[1] == 6.0, "on a keyframe, its value");
    mtwnSamplePath(&path, 0.9, v, t);
    CHECK(v[0] == 4.0 && t[0] == 0.0, "a step holds");
    mtwnSamplePath(&path, 1.0, v, NULL);
    CHECK(v[0] == 8.0 && v[1] == -2.0, "and gives the last keyframe at 1");
    mtwnSamplePath(&path, -0.125, v, NULL);
    CHECK(v[0] == -2.0 && v[1] == 12.0, "before 0 the first segment extends");
    // Equal offsets jump: the later keyframe wins at the offset.
    mtwnKeyframe jump[4] = {
        Key(0.0, 0.0f, 0.0f, mtwn_segmentLinear), Key(0.5, 1.0f, 0.0f, mtwn_segmentLinear),
        Key(0.5, 5.0f, 0.0f, mtwn_segmentLinear), Key(1.0, 6.0f, 0.0f, mtwn_segmentLinear)};
    CHECK(mtwnMakePath(jump, 4, 1, 0.5, NULL, &path) == mtwn_success, "a jump");
    mtwnSamplePath(&path, 0.5, v, NULL);
    double below[4];
    mtwnSamplePath(&path, 0.5 - 1e-12, below, NULL);
    CHECK(v[0] == 5.0 && fabs(below[0] - 1.0) < 1e-9, "the jump at its offset");
    mtwnSamplePath(&path, 1.5, v, NULL);
    CHECK(v[0] == 7.0, "past 1 the last segment extends");
    // A segment's easing times it.
    mtwnKeyframe eased[2] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentLinear),
                             Key(1.0, 1.0f, 0.0f, mtwn_segmentLinear)};
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &eased[0].easing) == mtwn_success, "quad");
    CHECK(mtwnMakePath(eased, 2, 1, 0.5, NULL, &path) == mtwn_success, "eased");
    mtwnSamplePath(&path, 0.5, v, NULL);
    CHECK(v[0] == 0.25, "eased by the segment's easing");
    // The end exactly where start + (end - start) is not it, and the
    // tangent 0 past the path's channels whatever they hold (found by
    // mutants).
    mtwnKeyframe wide[2] = {Key(0.0, 1e30f, NAN, mtwn_segmentLinear),
                            Key(1.0, 1.0f, NAN, mtwn_segmentLinear)};
    CHECK(mtwnMakePath(wide, 2, 1, 0.5, NULL, &path) == mtwn_success, "a wide line");
    mtwnSamplePath(&path, 1.0, v, t);
    CHECK(v[0] == 1.0, "its end exactly at 1");
    CHECK(v[1] == 0.0 && t[1] == 0.0, "value and tangent 0 past its channel");
}

static void TestBezier(void)
{
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentBezier),
                            Key(1.0, 3.0f, 1.0f, mtwn_segmentLinear)};
    keys[0].out = (mtwnVec4){1.0f, 2.5f, 0, 0};
    keys[1].in = (mtwnVec4){2.25f, -1.5f, 0, 0};
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 2, 2, 0.5, NULL, &path) == mtwn_success, "made");
    double worst = 0.0;
    for (int k = 0; k <= 64; ++k)
    {
        Wide u = (Wide)k / 64;
        Wide w = 1 - u;
        double v[4];
        double t[4];
        mtwnSamplePath(&path, (double)u, v, t);
        Wide p[4][2] = {{0, 0}, {1, (Wide)2.5}, {(Wide)2.25, (Wide)-1.5}, {3, 1}};
        for (int c = 0; c < 2; ++c)
        {
            Wide value = w * w * w * p[0][c] + 3 * w * w * u * p[1][c] + 3 * w * u * u * p[2][c] +
                         u * u * u * p[3][c];
            Wide slope = 3 * w * w * (p[1][c] - p[0][c]) + 6 * w * u * (p[2][c] - p[1][c]) +
                         3 * u * u * (p[3][c] - p[2][c]);
            double e = fabs((double)((Wide)v[c] - value)) + fabs((double)((Wide)t[c] - slope));
            worst = e > worst ? e : worst;
        }
    }
    CHECK(worst <= 1e-14, "the Bernstein form, value and tangent");
}

// Barry and Goldman's pyramid for the Catmull-Rom segment from q[1] to
// q[2], knots by chord to the power alpha, at u in [0, 1].
static void Pyramid(const Wide q[4][3], Wide alpha, Wide u, Wide out[3])
{
    Wide t[4] = {0, 0, 0, 0};
    for (int j = 1; j < 4; ++j)
    {
        Wide d = 0;
        for (int c = 0; c < 3; ++c)
        {
            d += (q[j][c] - q[j - 1][c]) * (q[j][c] - q[j - 1][c]);
        }
        t[j] = t[j - 1] + WIDE_POW(WIDE_SQRT(d), alpha);
    }
    Wide s = t[1] + u * (t[2] - t[1]);
    for (int c = 0; c < 3; ++c)
    {
        Wide a1 = ((t[1] - s) * q[0][c] + (s - t[0]) * q[1][c]) / (t[1] - t[0]);
        Wide a2 = ((t[2] - s) * q[1][c] + (s - t[1]) * q[2][c]) / (t[2] - t[1]);
        Wide a3 = ((t[3] - s) * q[2][c] + (s - t[2]) * q[3][c]) / (t[3] - t[2]);
        Wide b1 = ((t[2] - s) * a1 + (s - t[0]) * a2) / (t[2] - t[0]);
        Wide b2 = ((t[3] - s) * a2 + (s - t[1]) * a3) / (t[3] - t[1]);
        out[c] = ((t[2] - s) * b1 + (s - t[1]) * b2) / (t[2] - t[1]);
    }
}

static void TestCatmullRom(void)
{
    static const float points[6][3] = {{0, 0, 0},  {1, 2, 0.5f}, {4, 2.5f, -1},
                                       {5, -1, 2}, {7.5f, 0, 0}, {8, 3, 1}};
    mtwnKeyframe keys[6];
    for (int i = 0; i < 6; ++i)
    {
        keys[i] = Key((double)i / 5.0, points[i][0], points[i][1], mtwn_segmentCatmullRom);
        keys[i].value.z = points[i][2];
    }
    keys[5].offset = 1.0;
    static const double alphas[4] = {0.0, 0.5, 1.0, 0.25};
    double worst = 0.0;
    double kink = 0.0;
    for (int a = 0; a < 4; ++a)
    {
        mtwnPath path;
        CHECK(mtwnMakePath(keys, 6, 3, alphas[a], NULL, &path) == mtwn_success, "made");
        for (int seg = 0; seg < 5; ++seg)
        {
            Wide q[4][3];
            for (int j = 0; j < 4; ++j)
            {
                int index = seg - 1 + j;
                for (int c = 0; c < 3; ++c)
                {
                    // The ends' missing neighbors reflected, as the path does.
                    if (index < 0)
                    {
                        q[j][c] = 2 * (Wide)points[0][c] - (Wide)points[1][c];
                    }
                    else if (index > 5)
                    {
                        q[j][c] = 2 * (Wide)points[5][c] - (Wide)points[4][c];
                    }
                    else
                    {
                        q[j][c] = (Wide)points[index][c];
                    }
                }
            }
            for (int k = 0; k <= 32; ++k)
            {
                Wide u = (Wide)k / 32;
                Wide want[3];
                Pyramid(q, (Wide)alphas[a], u, want);
                double v[4];
                mtwnSamplePath(&path, (double)(((Wide)seg + u) / 5), v, NULL);
                for (int c = 0; c < 3; ++c)
                {
                    double e = fabs((double)((Wide)v[c] - want[c]));
                    worst = e > worst ? e : worst;
                }
            }
        }
        // Continuous in direction at every interior keyframe: the tangents
        // on both sides are parallel.
        for (int i = 1; i < 5; ++i)
        {
            double left[4];
            double right[4];
            double v[4];
            mtwnSamplePath(&path, (double)i / 5.0 - 1e-13, v, left);
            mtwnSamplePath(&path, (double)i / 5.0, v, right);
            double cross = fabs(left[0] * right[1] - left[1] * right[0]) +
                           fabs(left[1] * right[2] - left[2] * right[1]);
            double scale = sqrt(left[0] * left[0] + left[1] * left[1] + left[2] * left[2]) *
                           sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
            double dot = left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
            double e = cross / scale + (dot > 0.0 ? 0.0 : 1.0);
            kink = e > kink ? e : kink;
        }
    }
    if (!(worst <= 1e-13) || !(kink <= 1e-10))
    {
        printf("  catmull-rom %.3g, kink %.3g\n", worst, kink);
    }
    CHECK(worst <= 1e-13, "the Hermite form is Barry and Goldman's pyramid, alpha 0, 1/4, 1/2, 1");
    CHECK(kink <= 1e-10, "the direction continuous at every interior keyframe");
    // Coincident keyframes hold still rather than divide by zero.
    mtwnKeyframe same[4] = {
        Key(0.0, 1.0f, 1.0f, mtwn_segmentCatmullRom), Key(0.3, 1.0f, 1.0f, mtwn_segmentCatmullRom),
        Key(0.6, 1.0f, 1.0f, mtwn_segmentCatmullRom), Key(1.0, 2.0f, 1.0f, mtwn_segmentCatmullRom)};
    mtwnPath path;
    CHECK(mtwnMakePath(same, 4, 2, 0.5, NULL, &path) == mtwn_success, "coincident");
    int finite = 1;
    for (int k = 0; k <= 100; ++k)
    {
        double v[4];
        double t[4];
        mtwnSamplePath(&path, k / 100.0, v, t);
        finite &= isfinite(v[0]) && isfinite(v[1]) && isfinite(t[0]) && isfinite(t[1]);
        if (k <= 60)
        {
            finite &= fabs(v[0] - 1.0) < 1e-9;
        }
    }
    CHECK(finite, "coincident keyframes hold still, finite everywhere");
    // The same curve at any scale: the gaps' floor is relative. Scales
    // near 1e6, 1e-6 and 1e-24, powers of two so scaling is exact.
    static const float scales[3] = {0x1p20f, 0x1p-20f, 0x1p-80f};
    mtwnKeyframe base[5] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentCatmullRom),
                            Key(0.25, 1.0f, 2.0f, mtwn_segmentCatmullRom),
                            Key(0.5, 1.0f, 2.0f, mtwn_segmentCatmullRom),
                            Key(0.75, 3.0f, -1.0f, mtwn_segmentCatmullRom),
                            Key(1.0, 4.0f, 0.5f, mtwn_segmentCatmullRom)};
    mtwnPath unit;
    CHECK(mtwnMakePath(base, 5, 2, 0.5, NULL, &unit) == mtwn_success, "at scale 1");
    double apart = 0.0;
    for (int s = 0; s < 3; ++s)
    {
        mtwnKeyframe scaled[5];
        memcpy(scaled, base, sizeof base);
        for (int i = 0; i < 5; ++i)
        {
            scaled[i].value.x *= scales[s];
            scaled[i].value.y *= scales[s];
        }
        mtwnPath other;
        CHECK(mtwnMakePath(scaled, 5, 2, 0.5, NULL, &other) == mtwn_success, "scaled");
        for (int k = 0; k <= 100; ++k)
        {
            double a[4];
            double b[4];
            mtwnSamplePath(&unit, k / 100.0, a, NULL);
            mtwnSamplePath(&other, k / 100.0, b, NULL);
            for (int c = 0; c < 2; ++c)
            {
                double e = fabs(b[c] / (double)scales[s] - a[c]);
                apart = e > apart ? e : apart;
            }
        }
    }
    if (!(apart <= 1e-12))
    {
        printf("  scaled %.3g\n", apart);
    }
    CHECK(apart <= 1e-12, "scaled by 2^20, 2^-20 and 2^-80, the same curve, coincident keys too");
}

// The arc length of a cubic Bezier by polylines of n and 2n pieces,
// Richardson-extrapolated.
static Wide Polyline(const Wide p[4][2], int n)
{
    Wide total = 0;
    Wide previous[2] = {p[0][0], p[0][1]};
    for (int k = 1; k <= n; ++k)
    {
        Wide u = (Wide)k / n;
        Wide w = 1 - u;
        Wide x[2];
        for (int c = 0; c < 2; ++c)
        {
            x[c] = w * w * w * p[0][c] + 3 * w * w * u * p[1][c] + 3 * w * u * u * p[2][c] +
                   u * u * u * p[3][c];
        }
        total += WIDE_SQRT((x[0] - previous[0]) * (x[0] - previous[0]) +
                           (x[1] - previous[1]) * (x[1] - previous[1]));
        previous[0] = x[0];
        previous[1] = x[1];
    }
    return total;
}

static void TestLength(void)
{
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentBezier),
                            Key(1.0, 3.0f, 1.0f, mtwn_segmentLinear)};
    keys[0].out = (mtwnVec4){0.0f, 4.0f, 0, 0};
    keys[1].in = (mtwnVec4){4.0f, -3.0f, 0, 0};
    double lengths[2];
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 2, 2, 0.5, lengths, &path) == mtwn_success, "measured");
    Wide p[4][2] = {{0, 0}, {0, 4}, {4, -3}, {3, 1}};
    Wide coarse = Polyline(p, 100000);
    Wide fine = Polyline(p, 200000);
    Wide reference = fine + (fine - coarse) / 3;
    double e = fabs((double)(((Wide)lengths[1] - reference) / reference));
    if (!(e <= 1e-10))
    {
        printf("  length %.17g, reference %.17g, %.3g\n", lengths[1], (double)reference, e);
    }
    CHECK(lengths[0] == 0.0 && e <= 1e-10, "a curved Bezier's length within 1e-10 relative");
    // Lines: exact lengths, and constant speed puts each share of the
    // length at its place though the parameter is uneven.
    mtwnKeyframe line[3] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentBezier),
                            Key(0.9, 3.0f, 4.0f, mtwn_segmentLinear),
                            Key(1.0, 3.0f, 14.0f, mtwn_segmentLinear)};
    line[0].out = (mtwnVec4){0.03f, 0.04f, 0, 0};
    line[1].in = (mtwnVec4){2.97f, 3.96f, 0, 0};
    double lineLengths[3];
    CHECK(mtwnMakePath(line, 3, 2, 0.5, lineLengths, &path) == mtwn_success, "a line");
    CHECK(fabs(lineLengths[1] - 5.0) <= 1e-12 && fabs(lineLengths[2] - 15.0) <= 1e-12,
          "segment lengths 5 and 10");
    double worst = 0.0;
    for (int k = 0; k <= 150; ++k)
    {
        double share = k / 150.0;
        double v[4];
        mtwnSamplePath(&path, share, v, NULL);
        double s = share * 15.0;
        double want[2] = {s <= 5.0 ? 0.6 * s : 3.0, s <= 5.0 ? 0.8 * s : 4.0 + (s - 5.0)};
        double error = fabs(v[0] - want[0]) + fabs(v[1] - want[1]);
        worst = error > worst ? error : worst;
    }
    if (!(worst <= 1e-10))
    {
        printf("  constant speed %.3g\n", worst);
    }
    CHECK(worst <= 1e-10, "at constant speed every share of the length at its place");
    double v[4];
    mtwnSamplePath(&path, 1.0, v, NULL);
    CHECK(v[0] == 3.0 && v[1] == 14.0, "the end exactly");
}

// A cubic Bezier's point in one channel, and its speed in two.
static Wide BezierAt(const Wide p[4], Wide u)
{
    Wide w = 1 - u;
    return w * w * w * p[0] + 3 * w * w * u * p[1] + 3 * w * u * u * p[2] + u * u * u * p[3];
}

static Wide BezierSpeed(const Wide p[2][4], Wide u)
{
    Wide w = 1 - u;
    Wide sum = 0;
    for (int c = 0; c < 2; ++c)
    {
        Wide d = 3 * w * w * (p[c][1] - p[c][0]) + 6 * w * u * (p[c][2] - p[c][1]) +
                 3 * u * u * (p[c][3] - p[c][2]);
        sum += d * d;
    }
    return WIDE_SQRT(sum);
}

// A number's Bezier's length from 0 to u: its total variation, the
// changes between the turns where its derivative, A u^2 + B u + C, is 0.
static Wide Variation(const Wide p[4], Wide u)
{
    Wide d0 = p[1] - p[0];
    Wide d1 = p[2] - p[1];
    Wide d2 = p[3] - p[2];
    Wide a = d0 - 2 * d1 + d2;
    Wide b = 2 * (d1 - d0);
    Wide disc = b * b - 4 * a * d0;
    Wide at[4] = {0, 0, 0, 0};
    int n = 1;
    if (a != 0 && disc > 0)
    {
        Wide r[2] = {(-b - WIDE_SQRT(disc)) / (2 * a), (-b + WIDE_SQRT(disc)) / (2 * a)};
        Wide lo = r[0] < r[1] ? r[0] : r[1];
        Wide hi = r[0] < r[1] ? r[1] : r[0];
        at[n] = lo;
        n += lo > 0 && lo < u ? 1 : 0;
        at[n] = hi;
        n += hi > 0 && hi < u ? 1 : 0;
    }
    else if (a == 0 && b != 0)
    {
        at[n] = -d0 / b;
        n += at[n] > 0 && at[n] < u ? 1 : 0;
    }
    at[n] = u;
    Wide total = 0;
    for (int k = 1; k <= n; ++k)
    {
        Wide step = BezierAt(p, at[k]) - BezierAt(p, at[k - 1]);
        total += step < 0 ? -step : step;
    }
    return total;
}

// One channel: a number's Bezier has its total variation for a length,
// exactly; quadrature misses the kink each turn puts in |x'| (a point
// 1.1e-4 of the length off on the first here before it was measured
// so). At constant speed each share of that length is at its place.
// With more channels the same number moves along a line, channel c
// taking it times 2^-c (exact in float), its length the variation times
// the line's own, sqrt(sum of 4^-c); a turn back puts the same kink in
// the speed, and a quadrature whose points all missed the span between
// two turns agreed on the wrong length (0.4% short on the last of
// TestTurnBack's, found measuring the lengths against exact ones).
static mtwnVec4 Along(float x)
{
    return (mtwnVec4){x, x * 0.5f, x * 0.25f, x * 0.125f};
}

static void CheckLineLength(const float controls[4], uint8_t channels, const char* what)
{
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentBezier),
                            Key(1.0, 0.0f, 0.0f, mtwn_segmentLinear)};
    // The channels past the path's are not read.
    keys[0].value = Along(controls[0]);
    keys[0].out = Along(controls[1]);
    keys[1].in = Along(controls[2]);
    keys[1].value = Along(controls[3]);
    Wide line = 0;
    for (uint8_t c = 0; c < channels; ++c)
    {
        line += WIDE_POW(4, -(Wide)c);
    }
    line = WIDE_SQRT(line);
    Wide p[4] = {(Wide)controls[0], (Wide)controls[1], (Wide)controls[2], (Wide)controls[3]};
    double lengths[2];
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 2, channels, 0.5, lengths, &path) == mtwn_success, what);
    Wide total = Variation(p, 1) * line;
    double e = fabs((double)(((Wide)lengths[1] - total) / total));
    if (!(e <= 1e-14))
    {
        printf("  %s: length %.17g, exact %.17g\n", what, lengths[1], (double)total);
    }
    CHECK(e <= 1e-14, "its length the exact one, within 1e-14 relative");
    double worst = 0.0;
    for (int k = 0; k <= 400; ++k)
    {
        Wide target = total * k / 400;
        Wide lo = 0;
        Wide hi = 1;
        for (int step = 0; step < 80; ++step)
        {
            Wide mid = (lo + hi) / 2;
            if (Variation(p, mid) * line < target)
            {
                lo = mid;
            }
            else
            {
                hi = mid;
            }
        }
        double v[4];
        mtwnSamplePath(&path, k / 400.0, v, NULL);
        double error = fabs((double)((Wide)v[0] - BezierAt(p, (lo + hi) / 2)));
        worst = error > worst ? error : worst;
    }
    if (!(worst <= 1e-11 * (double)total))
    {
        printf("  %s: constant speed %.3g\n", what, worst);
    }
    CHECK(worst <= 1e-11 * (double)total, "at constant speed every share of it at its place");
}

static void TestNumberLength(void)
{
    static const float twice[4] = {0.0f, 2.0f, -1.0f, 1.0f};
    CheckLineLength(twice, 1, "a number turning twice");
    // No cubic term: the derivative is linear, its one root a turn.
    static const float once[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    CheckLineLength(once, 1, "a number turning once, a quadratic");
    // A quadratic whose derivative's slope is exactly 1: a uniform
    // Catmull-Rom through 0.75, 0, 0.25, 1.5 has -u/4 + u^2/2 between 0 and
    // 0.25, down 1/32 and up 9/32, 0.3125 in all.
    mtwnKeyframe keys[4];
    memset(keys, 0, sizeof keys);
    static const float values[4] = {0.75f, 0.0f, 0.25f, 1.5f};
    for (int i = 0; i < 4; ++i)
    {
        keys[i] = Key(i / 3.0, values[i], 0.0f, mtwn_segmentCatmullRom);
    }
    keys[3].offset = 1.0;
    double lengths[4];
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 4, 1, 0.0, lengths, &path) == mtwn_success &&
              lengths[2] - lengths[1] == 0.3125,
          "a turn where the derivative's slope is 1");
}

// Turns back along a line in two to four channels (record mtwn-0031).
static void TestTurnBack(void)
{
    static const float twice[4] = {0.0f, 2.0f, -1.0f, 1.0f};
    static const float once[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    static const float narrow[4] = {0.0f, 6.0f, 1.0f, 5.0f};
    for (uint8_t channels = 2; channels <= 4; ++channels)
    {
        CheckLineLength(twice, channels, "a line turning twice");
        CheckLineLength(once, channels, "a line turning once");
        CheckLineLength(narrow, channels, "a line turning twice, close together");
    }
}

// Near a turn: x = 6h u (1 - u) and y = e u, so the speed is
// sqrt(k^2 (u - 1/2)^2 + e^2) with k = 12h, and the length is
// 2 F(1/2), F(t) = t/2 sqrt(k^2 t^2 + e^2) + e^2 / (2k) asinh(k t / e).
// Where e is small the speed's minimum is sharp, and each half is a
// piece ending there: lengths agreeing to 1e-9 came 1.2e-9 off (h 1/16,
// e 3 2^-19), at 1e-12 the worst is 2.0e-12 (record mtwn-0031).
static void TestNearTurn(void)
{
    double worst = 0.0;
    for (int hk = -4; hk <= 4; ++hk)
    {
        for (int ek = 0; ek <= 40; ++ek)
        {
            float h = ldexpf(1.0f, hk);
            float third = ldexpf(1.0f, -ek);
            mtwnKeyframe keys[2] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentBezier),
                                    Key(1.0, 0.0f, 3.0f * third, mtwn_segmentLinear)};
            keys[0].out = (mtwnVec4){2.0f * h, third, 0.0f, 0.0f};
            keys[1].in = (mtwnVec4){2.0f * h, 2.0f * third, 0.0f, 0.0f};
            double lengths[2];
            mtwnPath path;
            if (mtwnMakePath(keys, 2, 2, 0.5, lengths, &path) != mtwn_success)
            {
                worst = 1.0;
                continue;
            }
            Wide k = 12 * (Wide)h;
            Wide e = 3 * (Wide)third;
            Wide exact =
                (Wide)0.5 * WIDE_SQRT(k * k / 4 + e * e) + e * e / k * WIDE_ASINH(k / (2 * e));
            double error = fabs((double)(((Wide)lengths[1] - exact) / exact));
            worst = error > worst ? error : worst;
        }
    }
    if (!(worst <= 1e-11))
    {
        printf("  near a turn: %.3g\n", worst);
    }
    CHECK(worst <= 1e-11, "a sharp minimum's length within 1e-11 relative");
}

// The length of a two-channel Bezier from 0 to u by Simpson's rule.
static Wide Simpson(const Wide p[2][4], Wide u, int panels)
{
    Wide h = u / panels;
    Wide sum = BezierSpeed(p, 0) + BezierSpeed(p, u);
    for (int k = 1; k < 2 * panels; ++k)
    {
        sum += (k % 2 == 1 ? 4 : 2) * BezierSpeed(p, h * k / 2);
    }
    return sum * h / 6;
}

// Two channels: each share's point against the parameter at that length
// found again in Wide (Newton's steps on Simpson's rule), the worst error
// relative to the length; the extra share after the hundredths.
static double WorstShare(const float controls[2][4], double extra)
{
    mtwnKeyframe keys[2] = {Key(0.0, controls[0][0], controls[1][0], mtwn_segmentBezier),
                            Key(1.0, controls[0][3], controls[1][3], mtwn_segmentLinear)};
    keys[0].out = (mtwnVec4){controls[0][1], controls[1][1], 0, 0};
    keys[1].in = (mtwnVec4){controls[0][2], controls[1][2], 0, 0};
    Wide p[2][4];
    for (int c = 0; c < 2; ++c)
    {
        for (int k = 0; k < 4; ++k)
        {
            p[c][k] = (Wide)controls[c][k];
        }
    }
    double lengths[2];
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 2, 2, 0.5, lengths, &path) == mtwn_success, "a curve");
    Wide total = Simpson(p, 1, 4096);
    CHECK(fabs((double)((Simpson(p, 1, 2048) - total) / total)) <= 1e-13, "Simpson converged");
    double worst = 0.0;
    for (int k = 0; k <= 101; ++k)
    {
        double share = k <= 100 ? k / 100.0 : extra;
        Wide target = total * (Wide)share;
        Wide u = (Wide)share;
        for (int step = 0; step < 8; ++step)
        {
            u -= (Simpson(p, u, 2048) - target) / BezierSpeed(p, u);
        }
        CHECK(fabs((double)((Simpson(p, u, 2048) - target) / total)) <= 1e-13,
              "the reference's parameter found");
        double v[4];
        mtwnSamplePath(&path, share, v, NULL);
        double error = fabs((double)((Wide)v[0] - BezierAt(p[0], u))) +
                       fabs((double)((Wide)v[1] - BezierAt(p[1], u)));
        worst = error > worst ? error : worst;
    }
    return worst / (double)total;
}

// The share near 0.6 here once came 1.6e-7 off, the length found by
// summing each Newton step's length drifting from the length measured
// afresh (found comparing builds over random paths).
static void TestSpeedDrift(void)
{
    static const float controls[2][4] = {
        {0x1.b921eap+1f, 0x1.7ad7cap+1f, -0x1.649f38p+2f, -0x1.bafc4p+1f},
        {-0x1.231bc6p+3f, -0x1.e97f98p+2f, -0x1.d32788p+2f, -0x1.19e712p+3f}};
    double worst = WorstShare(controls, 0x1.3255b88bb5ef2p-1);
    if (!(worst <= 1e-10))
    {
        printf("  constant speed %.3g\n", worst);
    }
    CHECK(worst <= 1e-10, "every share at its place, no drift");
}

// Handles crossed far past each other: near 0.1 Halley's step lands
// outside the bracket and the search bisects it; a wrong midpoint there
// once went unseen (a sampled mutant).
static void TestBisected(void)
{
    static const float controls[2][4] = {
        {-0x1.cbfb1p-4f, 0x1.2dfa4p-2f, -0x1.31a028p+1f, 0x1.f645ap-1f},
        {0x1.5844dp-3f, 0x1.67453ap+0f, -0x1.3559b4p+1f, -0x1.786c2p-3f}};
    double worst = WorstShare(controls, 0.12);
    if (!(worst <= 1e-10))
    {
        printf("  bisected %.3g\n", worst);
    }
    CHECK(worst <= 1e-10, "every share at its place where the search bisects");
}

// A constant-speed point's bits, which 1.x keeps on every platform: the
// search's Halley steps taken wherever their denominator is above 0 (a
// sampled mutant bisecting below 1 moved this one in its last bits).
static void TestPointBits(void)
{
    mtwnKeyframe keys[2] = {Key(0.0, -0x1.15c29p+1f, 0x1.7eb85p+2f, mtwn_segmentBezier),
                            Key(1.0, -0x1p+3f, 0x1.c8b44p+0f, mtwn_segmentLinear)};
    keys[0].out = (mtwnVec4){-0x1.510624p+2f, -0x1.275c28p+3f, 0, 0};
    keys[1].in = (mtwnVec4){-0x1.a3d7p-3f, 0x1.2ab85p+3f, 0, 0};
    double lengths[2];
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 2, 2, 0.5, lengths, &path) == mtwn_success, "a curve");
    double v[4];
    mtwnSamplePath(&path, 0.43, v, NULL);
    CHECK(v[0] == -0x1.a9773f4ceb1ccp+1 && v[1] == -0x1.b277f33a74f6p-3,
          "43 hundredths of its length, to the bit");
    // And one the search's stop at a step of 2^-52 (not under it) keeps.
    mtwnKeyframe other[2] = {Key(0.0, -0x1.35fbe8p+3f, 0x1.eb85p-3f, mtwn_segmentBezier),
                             Key(1.0, 0x1.db646p+0f, -0x1.4p+0f, mtwn_segmentLinear)};
    other[0].out = (mtwnVec4){0x1.72d0e4p+2f, 0x1.2bc6a8p+1f, 0, 0};
    other[1].in = (mtwnVec4){-0x1.48a3d8p+2f, 0x1.d851e8p+1f, 0, 0};
    CHECK(mtwnMakePath(other, 2, 2, 0.5, lengths, &path) == mtwn_success, "another curve");
    mtwnSamplePath(&path, 0.67, v, NULL);
    CHECK(v[0] == -0x1.79e6b9feb79cp-1 && v[1] == 0x1.115e1a98f1002p+1,
          "67 hundredths of its length, to the bit");
    // And one the speed minima found where the slope rises through 0, and
    // the speed's own second derivative, keep.
    mtwnKeyframe third[2] = {Key(0.0, -0x1.2ab02p+2f, 0x1.2f3b68p+1f, mtwn_segmentBezier),
                             Key(1.0, 0x1.a65608p+2f, -0x1.c5604p+1f, mtwn_segmentLinear)};
    third[0].out = (mtwnVec4){0x1.926e8p-2f, -0x1.3f7cfp+0f, 0, 0};
    third[1].in = (mtwnVec4){-0x1.3c8312p+3f, 0x1.9c28f8p+1f, 0, 0};
    CHECK(mtwnMakePath(third, 2, 2, 0.5, lengths, &path) == mtwn_success, "a third curve");
    mtwnSamplePath(&path, 0.56, v, NULL);
    CHECK(v[0] == 0x1.28853c8fc0848p+0 && v[1] == -0x1.51ebd77eb25ep+0,
          "56 hundredths of its length, to the bit");
}

// A line drawn as a Bezier with both handles on its end, 100 (1 - u)^3:
// the speed falls to 0 there as (1 - u)^2, and its rounding keeps the
// length's two orders apart at any width; at constant speed a share 2^-40
// from the end measured that span until its stack filled, about 2^32 rule
// pairs (found by the values' fuzz). The rule pairs are bounded now.
static void TestStillEnds(void)
{
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f, 100.0f, mtwn_segmentBezier),
                            Key(1.0, 0.0f, 0.0f, mtwn_segmentLinear)};
    keys[0].out = keys[1].value;
    keys[1].in = keys[1].value;
    double lengths[2];
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 2, 2, 0.5, lengths, &path) == mtwn_success, "a still-ended line");
    // Shares 2^-1 to 2^-52 from each end: from 2^-25 to 2^-49 by the
    // still end the inverse's lengths reach the cap on rule pairs, and
    // the points stay within the inverse's own 1e-12 of the length.
    double worst = 0.0;
    for (int p = 1; p <= 52; ++p)
    {
        const double shares[2] = {ldexp(1.0, -p), 1.0 - ldexp(1.0, -p)};
        for (int i = 0; i < 2; ++i)
        {
            double v[4];
            mtwnSamplePath(&path, shares[i], v, NULL);
            double error = fabs(v[1] - 100.0 * (1.0 - shares[i])) + fabs(v[0]);
            worst = error > worst ? error : worst;
        }
    }
    CHECK(lengths[1] == 100.0, "a still-ended line's length");
    CHECK(worst <= 1.01e-12 * 100.0, "each share at its place, at once");
}

// Coincident keyframes hold still at constant speed too, extended past
// the path: a Catmull-Rom from a point to itself measures 1.6e-23 by its
// end tangents, and extended by its parameter took the path's millionth
// before its start to 1e33 (found by fuzz_values).
static void TestCoincidentStart(void)
{
    mtwnKeyframe keys[3] = {Key(0.0, 34.1156f, 34.1156f, mtwn_segmentCatmullRom),
                            Key(0.34, 34.1156f, 34.1156f, mtwn_segmentBezier),
                            Key(1.0, 64.7034f, 64.7034f, mtwn_segmentLinear)};
    keys[1].out = (mtwnVec4){64.7034f, 64.7034f, 0, 0};
    keys[2].in = (mtwnVec4){64.8407f, 64.7034f, 0, 0};
    double lengths[3];
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 3, 2, 0.647034, lengths, &path) == mtwn_success,
          "a path from a held start");
    CHECK(lengths[1] > 0.0 && lengths[1] < 1e-20, "its first segment all but none");
    int moved = 0;
    const double before[4] = {-1.5, -0.25, -1e-3, -1e-6};
    for (int i = 0; i < 4; ++i)
    {
        double v[4];
        mtwnSamplePath(&path, before[i], v, NULL);
        moved += v[0] != (double)keys[0].value.x || v[1] != (double)keys[0].value.y;
    }
    CHECK(moved == 0, "before it, held at the start");
}

static void TestEdges(void)
{
    // Ends exactly the keyframes' values, also where the arithmetic would
    // round: non-dyadic values on every kind.
    static const mtwnSegmentKind kinds[3] = {mtwn_segmentBezier, mtwn_segmentCatmullRom,
                                             mtwn_segmentLinear};
    int exact = 1;
    for (int k = 0; k < 3; ++k)
    {
        mtwnKeyframe keys[3] = {Key(0.0, 0.1f, 0.7f, kinds[k]), Key(0.3, 0.3f, -0.9f, kinds[k]),
                                Key(1.0, 1.1f, 0.13f, kinds[k])};
        keys[0].out = (mtwnVec4){0.37f, 0.71f, 0, 0};
        keys[1].in = (mtwnVec4){0.19f, -0.33f, 0, 0};
        keys[1].out = (mtwnVec4){0.77f, 0.17f, 0, 0};
        keys[2].in = (mtwnVec4){0.93f, 0.61f, 0, 0};
        mtwnPath path;
        exact &= mtwnMakePath(keys, 3, 2, 0.5, NULL, &path) == mtwn_success;
        double v[4];
        mtwnSamplePath(&path, 1.0, v, NULL);
        exact &= v[0] == (double)1.1f && v[1] == (double)0.13f;
        mtwnSamplePath(&path, 0.3, v, NULL);
        exact &= v[0] == (double)0.3f && v[1] == (double)-0.9f;
    }
    CHECK(exact, "every kind gives its keyframes' values exactly");
    // An easing at 1 inside a segment reaches the end keyframe exactly,
    // where the Hermite polynomial at 1 is a few ulps off it.
    float p[4] = {-0x1.0c3406p-2f, 0x1.d702d6p+2f, 0x1.da1122p+0f, -0x1.6d2be2p+2f};
    mtwnKeyframe curve[4];
    for (int i = 0; i < 4; ++i)
    {
        curve[i] = Key(i / 3.0, p[i], 0.0f, mtwn_segmentCatmullRom);
    }
    curve[3].offset = 1.0;
    CHECK(mtwnMakeSteps(1, mtwn_jumpStart, &curve[1].easing) == mtwn_success, "steps(1, start)");
    mtwnPath early;
    CHECK(mtwnMakePath(curve, 4, 1, 0.5, NULL, &early) == mtwn_success, "made");
    double at[4];
    mtwnSamplePath(&early, 0.5, at, NULL);
    CHECK(at[0] == (double)p[2], "the end keyframe exactly, not the polynomial's rounding");
    // Two keyframes at 1: the last wins there, no zero-width division.
    mtwnKeyframe end[3] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentLinear),
                           Key(1.0, 5.0f, 0.0f, mtwn_segmentLinear),
                           Key(1.0, 9.0f, 0.0f, mtwn_segmentLinear)};
    mtwnPath path;
    CHECK(mtwnMakePath(end, 3, 1, 0.5, NULL, &path) == mtwn_success, "two at 1");
    double v[4];
    mtwnSamplePath(&path, 1.0, v, NULL);
    double half[4];
    mtwnSamplePath(&path, 0.5, half, NULL);
    CHECK(v[0] == 9.0 && half[0] == 2.5, "the last at 1, the line before it");
    // Coincident keyframes at constant speed: a segment of no length.
    mtwnKeyframe still[4] = {
        Key(0.0, 0.0f, 0.0f, mtwn_segmentLinear), Key(0.4, 3.0f, 0.0f, mtwn_segmentLinear),
        Key(0.6, 3.0f, 0.0f, mtwn_segmentLinear), Key(1.0, 4.0f, 0.0f, mtwn_segmentLinear)};
    double lengths[4];
    CHECK(mtwnMakePath(still, 4, 1, 0.5, lengths, &path) == mtwn_success && lengths[3] == 4.0,
          "a path with a segment of no length");
    int finite = 1;
    for (int k = 0; k <= 40; ++k)
    {
        mtwnSamplePath(&path, k / 40.0, v, NULL);
        finite &= fabs(v[0] - k / 10.0) <= 1e-12;
    }
    CHECK(finite, "travels through it at constant speed");
    // A speed falling to 0 at the end: Newton must stay in its bracket.
    // The point is 1 - (1 - u)^3, its own arc length.
    mtwnKeyframe slow[2] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentBezier),
                            Key(1.0, 1.0f, 0.0f, mtwn_segmentLinear)};
    slow[0].out = (mtwnVec4){1.0f, 0, 0, 0};
    slow[1].in = (mtwnVec4){1.0f, 0, 0, 0};
    double slowLengths[2];
    CHECK(mtwnMakePath(slow, 2, 1, 0.5, slowLengths, &path) == mtwn_success, "slowing");
    double worst = 0.0;
    for (int k = 0; k <= 200; ++k)
    {
        double share = k / 200.0;
        mtwnSamplePath(&path, share, v, NULL);
        double e = fabs(v[0] - share);
        worst = e > worst ? e : worst;
    }
    CHECK(worst <= 1e-9, "every share at its place where the speed falls to 0");
    // A speed rising from 0, u^3: Newton from the start overshoots the
    // segment and must fall back into its bracket.
    slow[0].out = (mtwnVec4){0.0f, 0, 0, 0};
    slow[1].in = (mtwnVec4){0.0f, 0, 0, 0};
    CHECK(mtwnMakePath(slow, 2, 1, 0.5, slowLengths, &path) == mtwn_success, "speeding");
    worst = 0.0;
    for (int k = 0; k <= 200; ++k)
    {
        double share = k / 200.0;
        mtwnSamplePath(&path, share, v, NULL);
        double e = fabs(v[0] - share);
        worst = e > worst ? e : worst;
    }
    CHECK(worst <= 1e-9, "every share at its place where the speed rises from 0");
    // A speed of 0 mid-segment, 0.5 + 4 (u - 1/2)^3: Newton there leaps,
    // and the bracket keeps it to 1e-12 where it alone reaches 6e-10.
    slow[0].out = (mtwnVec4){1.0f, 0, 0, 0};
    slow[1].in = (mtwnVec4){0.0f, 0, 0, 0};
    CHECK(mtwnMakePath(slow, 2, 1, 0.5, slowLengths, &path) == mtwn_success, "pausing");
    worst = 0.0;
    for (int k = 0; k <= 1000; ++k)
    {
        double share = k / 1000.0;
        mtwnSamplePath(&path, share, v, NULL);
        double e = fabs(v[0] - share);
        worst = e > worst ? e : worst;
    }
    CHECK(worst <= 1e-10, "every share at its place through a pause");
    // The last two keyframes coincident: a segment of no length at the end.
    mtwnKeyframe tail[3] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentLinear),
                            Key(0.5, 3.0f, 0.0f, mtwn_segmentLinear),
                            Key(1.0, 3.0f, 0.0f, mtwn_segmentLinear)};
    double tailLengths[3];
    CHECK(mtwnMakePath(tail, 3, 1, 0.5, tailLengths, &path) == mtwn_success, "a still end");
    mtwnSamplePath(&path, 1.0, v, NULL);
    CHECK(v[0] == 3.0, "its end reached, not 0 / 0");
    // Channels past the path's are 0, whatever the keyframes hold there.
    mtwnKeyframe loose[2] = {Key(0.0, 0.0f, NAN, mtwn_segmentCatmullRom),
                             Key(1.0, 1.0f, NAN, mtwn_segmentCatmullRom)};
    CHECK(mtwnMakePath(loose, 2, 1, 0.5, NULL, &path) == mtwn_success, "NaN past the channel");
    double tangent[4];
    mtwnSamplePath(&path, 0.5, v, tangent);
    CHECK(v[0] == 0.5 && v[1] == 0.0 && tangent[1] == 0.0, "read as 0");
}

static void TestRefusals(void)
{
    mtwnKeyframe keys[3] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentStep),
                            Key(0.5, 1.0f, 0.0f, mtwn_segmentLinear),
                            Key(1.0, 2.0f, 0.0f, mtwn_segmentStep)};
    mtwnPath path;
    double lengths[3];
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_success, "valid");
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, lengths, &path) == mtwn_errorInvalid,
          "a step at constant speed");
    keys[0].kind = mtwn_segmentLinear;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, lengths, &path) == mtwn_success,
          "the last keyframe's kind starts no segment");
    CHECK(mtwnMakePath(keys, 1, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "one keyframe");
    CHECK(mtwnMakePath(NULL, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "none");
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, NULL) == mtwn_errorInvalid, "no out");
    CHECK(mtwnMakePath(keys, 3, 0, 0.5, NULL, &path) == mtwn_errorInvalid, "no channels");
    CHECK(mtwnMakePath(keys, 3, 5, 0.5, NULL, &path) == mtwn_errorInvalid, "five channels");
    CHECK(mtwnMakePath(keys, 3, 1, 1.5, NULL, &path) == mtwn_errorInvalid, "alpha past 1");
    CHECK(mtwnMakePath(keys, 3, 1, (double)NAN, NULL, &path) == mtwn_errorInvalid, "NaN alpha");
    keys[1].offset = 1.5;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "offsets decreasing");
    keys[1].offset = 0.5;
    keys[2].offset = 0.9;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "the last not 1");
    keys[2].offset = 1.0;
    keys[0].offset = 0.1;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "the first not 0");
    keys[0].offset = 0.0;
    keys[1].value.x = INFINITY;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "an infinite value");
    keys[1].value = (mtwnVec4){1.0f, NAN, 0, 0};
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_success,
          "channels past the path's are not read");
    keys[1].out.x = NAN;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "a NaN control");
    keys[1].out.x = 0.0f;
    keys[1].in.x = NAN;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "a NaN in control");
    keys[1].in.x = 0.0f;
    keys[1].kind = mtwn_segmentMonotone + 1;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "an unknown kind");
}

// At constant speed a curve sampled past its ends goes on along its end
// segments, by their parameter, as without constant speed (found by
// mutants).
static void TestBeyondAtConstantSpeed(void)
{
    mtwnKeyframe keys[3] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentCatmullRom),
                            Key(0.5, 1.0f, 2.0f, mtwn_segmentCatmullRom),
                            Key(1.0, 3.0f, 0.0f, mtwn_segmentCatmullRom)};
    double lengths[3];
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 3, 2, 0.5, lengths, &path) == mtwn_success, "a curve");
    double end[4];
    double past[4];
    double start[4];
    double before[4];
    double tangent[4];
    mtwnSamplePath(&path, 1.0, end, tangent);
    mtwnSamplePath(&path, 1.25, past, tangent);
    mtwnSamplePath(&path, 0.0, start, tangent);
    mtwnSamplePath(&path, -0.25, before, tangent);
    CHECK(fabs(past[0] - end[0]) > 0.1 &&
              fabs(before[1] - start[1]) + fabs(before[0] - start[0]) > 0.1,
          "past the ends, the curve goes on");
}

int main(void)
{
    TestBeyondAtConstantSpeed();
    TestLinearAndStep();
    TestBezier();
    TestCatmullRom();
    TestLength();
    TestNumberLength();
    TestSpeedDrift();
    TestPointBits();
    TestBisected();
    TestTurnBack();
    TestNearTurn();
    TestStillEnds();
    TestCoincidentStart();
    TestEdges();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
