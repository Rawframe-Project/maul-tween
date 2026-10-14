// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Paths (records mtwn-0016 and mtwn-0031). A Catmull-Rom segment is the
// cubic Hermite whose end tangents Yuksel, Schaefer and Keyser give for
// knots spaced by the chord's length to the power alpha:
//   m1 = d12 ((P1 - P0) / d01 - (P2 - P0) / (d01 + d12) + (P2 - P1) / d12)
//   m2 = d12 ((P2 - P1) / d12 - (P3 - P1) / (d12 + d23) + (P3 - P2) / d23)
// with the ends' missing neighbors reflected. Arc length is adaptive
// Gauss-Legendre, orders 5 and 10 agreeing to 1e-9 relative, and the
// parameter at a length is safeguarded Newton on it.

#include "maul-tween/path.h"

#include "elementary.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

// The smallest knot gap: coincident keyframes hold still.
static const double s_smallestGap = 1e-12;
// Two quadrature orders agreeing to this, relative, end the splitting.
static const double s_lengthTolerance = 1e-9;
// The intervals the splitting may hold at once, and the narrowest it
// splits, as a share of the segment.
enum
{
    s_stackSize = 64
};
static const double s_narrowest = 0x1p-30;
// Newton on the length stops within this share of the segment's length.
static const double s_inverseTolerance = 1e-12;
enum
{
    s_newtonSteps = 40
};

// Gauss-Legendre on [-1, 1]: the nonnegative abscissas and their weights.
static const double s_gauss5[3][2] = {
    {0.0, 0.5688888888888888889},
    {0.5384693101056830910, 0.4786286704993664680},
    {0.9061798459386639928, 0.2369268850561890875},
};
static const double s_gauss10[5][2] = {
    {0.1488743389816312109, 0.2955242247147528702}, {0.4333953941292471908, 0.2692667193099963551},
    {0.6794095682990244062, 0.2190863625159820440}, {0.8650633666889845107, 0.1494513491505805932},
    {0.9739065285171717200, 0.0666713443086881376},
};

static bool IsFinite4(mtwnVec4 v, uint8_t channels)
{
    const float* c = &v.x;
    for (uint8_t i = 0; i < channels; ++i)
    {
        if (!isfinite(c[i]))
        {
            return false;
        }
    }
    return true;
}

static void Load(mtwnVec4 v, double out[4])
{
    out[0] = (double)v.x;
    out[1] = (double)v.y;
    out[2] = (double)v.z;
    out[3] = (double)v.w;
}

// A segment as a cubic in its parameter u: value a + u (b + u (c + u d)),
// or for a step the value alone.
typedef struct Cubic
{
    double a[4];
    double b[4];
    double c[4];
    double d[4];
    bool step;
} Cubic;

// The knot gap between two points: their distance to the power alpha.
static double Gap(const double p[4], const double q[4], uint8_t channels, double alpha)
{
    double sum = 0.0;
    for (uint8_t i = 0; i < channels; ++i)
    {
        double d = q[i] - p[i];
        sum += d * d;
    }
    double chord = sqrt(sum);
    double gap = 1.0;
    if (alpha == 0.5)
    {
        gap = sqrt(chord);
    }
    else if (alpha == 1.0)
    {
        gap = chord;
    }
    else if (alpha != 0.0)
    {
        gap = mtwnPow(chord, alpha);
    }
    return gap > s_smallestGap ? gap : s_smallestGap;
}

// A Hermite segment from p1 to p2 with end tangents m1 and m2.
static void Hermite(const double p1[4], const double p2[4], const double m1[4], const double m2[4],
                    Cubic* out)
{
    for (int i = 0; i < 4; ++i)
    {
        out->a[i] = p1[i];
        out->b[i] = m1[i];
        out->c[i] = 3.0 * (p2[i] - p1[i]) - 2.0 * m1[i] - m2[i];
        out->d[i] = 2.0 * (p1[i] - p2[i]) + m1[i] + m2[i];
    }
}

static void CatmullRom(const mtwnPath* path, uint32_t i, Cubic* out)
{
    const mtwnKeyframe* k = path->keyframes;
    double p0[4];
    double p1[4];
    double p2[4];
    double p3[4];
    Load(k[i].value, p1);
    Load(k[i + 1].value, p2);
    for (int c = 0; c < 4; ++c)
    {
        p0[c] = 2.0 * p1[c] - p2[c];
        p3[c] = 2.0 * p2[c] - p1[c];
    }
    if (i > 0)
    {
        Load(k[i - 1].value, p0);
    }
    if (i + 2 < path->count)
    {
        Load(k[i + 2].value, p3);
    }
    uint8_t n = path->channels;
    double d01 = Gap(p0, p1, n, path->alpha);
    double d12 = Gap(p1, p2, n, path->alpha);
    double d23 = Gap(p2, p3, n, path->alpha);
    double m1[4];
    double m2[4];
    for (int c = 0; c < 4; ++c)
    {
        m1[c] =
            d12 * ((p1[c] - p0[c]) / d01 - (p2[c] - p0[c]) / (d01 + d12) + (p2[c] - p1[c]) / d12);
        m2[c] =
            d12 * ((p2[c] - p1[c]) / d12 - (p3[c] - p1[c]) / (d12 + d23) + (p3[c] - p2[c]) / d23);
    }
    Hermite(p1, p2, m1, m2, out);
}

// Segment i, from keyframe i to keyframe i + 1, as a cubic.
static void SegmentCubic(const mtwnPath* path, uint32_t i, Cubic* out)
{
    const mtwnKeyframe* k = path->keyframes;
    memset(out, 0, sizeof *out);
    double p0[4];
    double p3[4];
    Load(k[i].value, p0);
    Load(k[i + 1].value, p3);
    switch (k[i].kind)
    {
    case mtwn_segmentStep:
        memcpy(out->a, p0, sizeof p0);
        out->step = true;
        return;
    case mtwn_segmentBezier:
    {
        double p1[4];
        double p2[4];
        Load(k[i].out, p1);
        Load(k[i + 1].in, p2);
        for (int c = 0; c < 4; ++c)
        {
            out->a[c] = p0[c];
            out->b[c] = 3.0 * (p1[c] - p0[c]);
            out->c[c] = 3.0 * (p0[c] - 2.0 * p1[c] + p2[c]);
            out->d[c] = p3[c] - p0[c] + 3.0 * (p1[c] - p2[c]);
        }
        return;
    }
    case mtwn_segmentCatmullRom:
        CatmullRom(path, i, out);
        return;
    default:
        for (int c = 0; c < 4; ++c)
        {
            out->a[c] = p0[c];
            out->b[c] = p3[c] - p0[c];
        }
        return;
    }
}

// The cubic's value at u, exact at its ends: at 0 its start, at 1 the
// end keyframe's value; a step holds its start until u reaches 1.
static void Evaluate(const Cubic* cubic, const double end[4], double u, double value[4],
                     double tangent[4])
{
    for (int c = 0; c < 4; ++c)
    {
        if (cubic->step)
        {
            value[c] = u >= 1.0 ? end[c] : cubic->a[c];
            tangent[c] = 0.0;
            continue;
        }
        value[c] = u == 1.0 ? end[c]
                            : cubic->a[c] + u * (cubic->b[c] + u * (cubic->c[c] + u * cubic->d[c]));
        tangent[c] = cubic->b[c] + u * (2.0 * cubic->c[c] + u * 3.0 * cubic->d[c]);
    }
}

static double Speed(const Cubic* cubic, double u, uint8_t channels)
{
    double sum = 0.0;
    for (uint8_t c = 0; c < channels; ++c)
    {
        double t = cubic->b[c] + u * (2.0 * cubic->c[c] + u * 3.0 * cubic->d[c]);
        sum += t * t;
    }
    return sqrt(sum);
}

static double Gauss(const Cubic* cubic, uint8_t channels, double a, double b,
                    const double (*nodes)[2], int count)
{
    double middle = 0.5 * (a + b);
    double half = 0.5 * (b - a);
    double sum = 0.0;
    for (int i = 0; i < count; ++i)
    {
        double x = nodes[i][0];
        double w = nodes[i][1];
        sum += x == 0.0 ? w * Speed(cubic, middle, channels)
                        : w * (Speed(cubic, middle - half * x, channels) +
                               Speed(cubic, middle + half * x, channels));
    }
    return half * sum;
}

// The arc length of the cubic from a to b, a <= b.
static double Length(const Cubic* cubic, uint8_t channels, double a, double b)
{
    if (cubic->step || b <= a)
    {
        return 0.0;
    }
    double stack[s_stackSize][2];
    int top = 0;
    stack[top][0] = a;
    stack[top][1] = b;
    ++top;
    double total = 0.0;
    double narrowest = (b - a) * s_narrowest;
    while (top > 0)
    {
        --top;
        double lo = stack[top][0];
        double hi = stack[top][1];
        double fine = Gauss(cubic, channels, lo, hi, s_gauss10, 5);
        double coarse = Gauss(cubic, channels, lo, hi, s_gauss5, 3);
        bool agreed = fabs(fine - coarse) <= s_lengthTolerance * fine;
        if (agreed || hi - lo <= narrowest || top + 2 > s_stackSize)
        {
            total += fine;
            continue;
        }
        double middle = 0.5 * (lo + hi);
        stack[top][0] = middle;
        stack[top][1] = hi;
        stack[top + 1][0] = lo;
        stack[top + 1][1] = middle;
        top += 2;
    }
    return total;
}

static bool IsValid(const mtwnKeyframe* keyframes, uint32_t count, uint8_t channels, double alpha,
                    bool constantSpeed)
{
    if (keyframes == NULL || count < 2 || channels < 1 || channels > 4 || !isfinite(alpha) ||
        alpha < 0.0 || alpha > 1.0 || keyframes[0].offset != 0.0 ||
        keyframes[count - 1].offset != 1.0)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; ++i)
    {
        const mtwnKeyframe* k = &keyframes[i];
        bool segment = i + 1 < count;
        if (!isfinite(k->offset) || (segment && keyframes[i + 1].offset < k->offset) ||
            k->kind > mtwn_segmentCatmullRom || !IsFinite4(k->value, channels) ||
            !IsFinite4(k->in, channels) || !IsFinite4(k->out, channels) ||
            (constantSpeed && segment && k->kind == mtwn_segmentStep))
        {
            return false;
        }
    }
    return true;
}

mtwnResult mtwnMakePath(const mtwnKeyframe* keyframes, uint32_t count, uint8_t channels,
                        double alpha, double* lengthsOut, mtwnPath* pathOut)
{
    if (pathOut == NULL || !IsValid(keyframes, count, channels, alpha, lengthsOut != NULL))
    {
        return mtwn_errorInvalid;
    }
    mtwnPath path = {keyframes, lengthsOut, alpha, count, channels, {0}};
    if (lengthsOut != NULL)
    {
        lengthsOut[0] = 0.0;
        for (uint32_t i = 0; i + 1 < count; ++i)
        {
            Cubic cubic;
            SegmentCubic(&path, i, &cubic);
            lengthsOut[i + 1] = lengthsOut[i] + Length(&cubic, channels, 0.0, 1.0);
        }
    }
    *pathOut = path;
    return mtwn_success;
}

// The last segment whose start is at or below key in values, from 0 to
// count - 2.
static uint32_t FindSegment(const mtwnPath* path, double key, bool byLength)
{
    uint32_t lo = 0;
    uint32_t hi = path->count - 2;
    while (lo < hi)
    {
        uint32_t mid = lo + (hi - lo + 1) / 2;
        double start = byLength ? path->lengths[mid] : path->keyframes[mid].offset;
        if (start <= key)
        {
            lo = mid;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return lo;
}

// The parameter at a length into a segment of that total, by Newton on
// the length, kept within a shrinking bracket.
static double ParameterAt(const Cubic* cubic, uint8_t channels, double target, double total)
{
    if (target <= 0.0 || target >= total)
    {
        // Outside, the segment extends by its parameter.
        return target / total;
    }
    double lo = 0.0;
    double hi = 1.0;
    double u = target / total;
    double at = Length(cubic, channels, 0.0, u);
    for (int step = 0; step < s_newtonSteps; ++step)
    {
        double error = at - target;
        if (fabs(error) <= s_inverseTolerance * total)
        {
            break;
        }
        if (error < 0.0)
        {
            lo = u;
        }
        else
        {
            hi = u;
        }
        double speed = Speed(cubic, u, channels);
        double next = speed > 0.0 ? u - error / speed : 0.5 * (lo + hi);
        if (!(next > lo && next < hi))
        {
            next = 0.5 * (lo + hi);
        }
        at += next > u ? Length(cubic, channels, u, next) : -Length(cubic, channels, next, u);
        u = next;
    }
    return u;
}

void mtwnSamplePath(const mtwnPath* path, double progress, double valueOut[4], double tangentOut[4])
{
    double unused[4];
    double* tangent = tangentOut != NULL ? tangentOut : unused;
    const mtwnKeyframe* k = path->keyframes;
    Cubic cubic;
    uint32_t i = 0;
    double u = 0.0;
    if (path->lengths != NULL)
    {
        double total = path->lengths[path->count - 1];
        double distance = progress * total;
        i = FindSegment(path, distance, true);
        SegmentCubic(path, i, &cubic);
        double length = path->lengths[i + 1] - path->lengths[i];
        u = length > 0.0 ? ParameterAt(&cubic, path->channels, distance - path->lengths[i], length)
                         : 1.0;
    }
    else
    {
        i = FindSegment(path, progress, false);
        SegmentCubic(path, i, &cubic);
        double width = k[i + 1].offset - k[i].offset;
        double local = width > 0.0 ? (progress - k[i].offset) / width : 1.0;
        u = mtwnEase(&k[i].easing, local, false);
    }
    double end[4];
    Load(k[i + 1].value, end);
    Evaluate(&cubic, end, u, valueOut, tangent);
    for (uint8_t c = path->channels; c < 4; ++c)
    {
        valueOut[c] = 0.0;
        tangent[c] = 0.0;
    }
}
