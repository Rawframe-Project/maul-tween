// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Paths (records mtwn-0016, mtwn-0020, mtwn-0031): segments against their
// formulas written again in long double, Catmull-Rom against Barry and
// Goldman's pyramid (another construction of the same curve), arc length
// against a Richardson-extrapolated polyline, and constant speed on lines
// whose parameter is uneven but whose length is exact.

#include "test_harness.h"

#include "maul-tween/path.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_SQRT sqrtl
#define WIDE_POW  powl
#else
typedef double Wide;
#define WIDE_SQRT sqrt
#define WIDE_POW  pow
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
    static const double alphas[3] = {0.0, 0.5, 1.0};
    double worst = 0.0;
    double kink = 0.0;
    for (int a = 0; a < 3; ++a)
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
    CHECK(worst <= 1e-13, "the Hermite form is Barry and Goldman's pyramid, alpha 0, 1/2, 1");
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
    keys[1].kind = 4;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_errorInvalid, "an unknown kind");
}

int main(void)
{
    TestLinearAndStep();
    TestBezier();
    TestCatmullRom();
    TestLength();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
