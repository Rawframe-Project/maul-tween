// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Paths (records mtwn-0016 and mtwn-0031). A Catmull-Rom segment is the
// cubic Hermite whose end tangents Yuksel, Schaefer and Keyser give for
// knots spaced by the chord's length to the power alpha:
//   m1 = d12 ((P1 - P0) / d01 - (P2 - P0) / (d01 + d12) + (P2 - P1) / d12)
//   m2 = d12 ((P2 - P1) / d12 - (P3 - P1) / (d12 + d23) + (P3 - P2) / d23)
// with the ends' missing neighbors reflected, and each gap raised to
// 1e-12 of the largest of the three, so the curve is the same at any
// scale and coincident keyframes hold still. Arc length is adaptive
// Gauss-Legendre, orders 5 and 10 agreeing to 1e-9 relative, and the
// parameter at a length is safeguarded Newton on it.

#include "maul-tween/path.h"

#include "elementary.h"
#include "path_internal.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

// The smallest knot gap, as a share of the segment's largest.
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
// A Halley step this short in u, at a speed at least this share of the
// segment's length, is measured by the length's Taylor series.
static const double s_taylorStep = 1e-4;
static const double s_taylorSpeed = 1e-3;
// Newton on the length stops within this share of the segment's length.
static const double s_inverseTolerance = 1e-12;
enum
{
    s_newtonSteps = 40
};

// Gauss-Kronrod on [-1, 1] (QUADPACK's qk15): the 15-point rule's
// nonnegative abscissas, its weights and the 7-point Gauss rule's on the
// odd ones (0 on the others), the centre last.
static const double s_kronrod[8][3] = {
    {0.991455371120812639206854697526329, 0.022935322010529224963732008058970, 0.0},
    {0.949107912342758524526189684047851, 0.063092092629978553290700663189204,
     0.129484966168869693270611432679082},
    {0.864864423359769072789712788640926, 0.104790010322250183839876322541518, 0.0},
    {0.741531185599394439863864773280788, 0.140653259715525918745189590510238,
     0.279705391489276667901467771423780},
    {0.586087235467691130294144845693013, 0.169004726639267902826583426598550, 0.0},
    {0.405845151377397166906606412076961, 0.190350578064785409913256402421014,
     0.381830050505118944950369775488975},
    {0.207784955007898467600689403773245, 0.204432940075298892414161999234649, 0.0},
    {0.0, 0.209482141084727828012999174891714, 0.417959183673469387755102040816327},
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

// The root of a sum of squares: fabs, the identity there, shows the
// compiler the root is never of a negative, so it calls no errno path.
static double RootOfSquares(double sum)
{
    return sqrt(fabs(sum));
}

// The knot gap between two points: their distance to the power alpha.
static double Gap(const double p[4], const double q[4], uint8_t channels, double alpha)
{
    double sum = 0.0;
    for (uint8_t i = 0; i < channels; ++i)
    {
        double d = q[i] - p[i];
        sum += d * d;
    }
    double chord = RootOfSquares(sum);
    double gap = 1.0;
    if (alpha == 0.5)
    {
        gap = sqrt(chord);
    }
    else if (alpha != 0.0)
    {
        gap = mtwnPow(chord, alpha);
    }
    return gap;
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
    // Gaps relative to the largest; all four points coincident, any gap
    // gives the same still segment.
    double largest = d01 > d12 ? d01 : d12;
    largest = d23 > largest ? d23 : largest;
    double least = largest > 0.0 ? s_smallestGap * largest : 1.0;
    d01 = d01 > least ? d01 : least;
    d12 = d12 > least ? d12 : least;
    d23 = d23 > least ? d23 : least;
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

// The cubic's value at u in its first channels, exact at its ends: at 0
// its start, at 1 the end keyframe's value; a step holds its start until
// u reaches 1. The tangent too, unless it is NULL.
static void Evaluate(const Cubic* cubic, const double end[4], double u, uint8_t channels,
                     double value[4], double* tangent)
{
    for (uint8_t c = 0; c < channels; ++c)
    {
        if (cubic->step)
        {
            value[c] = u >= 1.0 ? end[c] : cubic->a[c];
            continue;
        }
        value[c] = u == 1.0 ? end[c]
                            : cubic->a[c] + u * (cubic->b[c] + u * (cubic->c[c] + u * cubic->d[c]));
    }
    if (tangent == NULL)
    {
        return;
    }
    for (uint8_t c = 0; c < channels; ++c)
    {
        tangent[c] =
            cubic->step ? 0.0 : cubic->b[c] + u * (2.0 * cubic->c[c] + u * 3.0 * cubic->d[c]);
    }
}

// The speed at u and its first two derivatives in u: with v = B'(u),
// a = B''(u) and j = B'''(u), s = |v|, s' = v.a / s and
// s'' = (a.a + v.j - s'^2) / s.
static double SpeedChanges(const Cubic* cubic, double u, uint8_t channels, double* firstOut,
                           double* secondOut)
{
    double vv = 0.0;
    double va = 0.0;
    double aa = 0.0;
    double vj = 0.0;
    for (uint8_t c = 0; c < channels; ++c)
    {
        double v = cubic->b[c] + u * (2.0 * cubic->c[c] + u * 3.0 * cubic->d[c]);
        double a = 2.0 * cubic->c[c] + u * 6.0 * cubic->d[c];
        double j = 6.0 * cubic->d[c];
        vv += v * v;
        va += v * a;
        aa += a * a;
        vj += v * j;
    }
    double speed = RootOfSquares(vv);
    double first = speed > 0.0 ? va / speed : 0.0;
    *firstOut = first;
    *secondOut = speed > 0.0 ? (aa + vj - first * first) / speed : 0.0;
    return speed;
}

// The length from a to b by the 15-point Kronrod rule, and by the 7-point
// Gauss rule on its odd abscissas.
static void Kronrod(const Cubic* cubic, uint8_t channels, double a, double b, double* fine,
                    double* coarse)
{
    double middle = 0.5 * (a + b);
    double half = 0.5 * (b - a);
    // About the middle the derivative is v + w t + j t^2: a pair of
    // abscissas shares its even part.
    double v[4];
    double w[4];
    double j[4];
    double sum = 0.0;
    for (uint8_t c = 0; c < channels; ++c)
    {
        v[c] = cubic->b[c] + middle * (2.0 * cubic->c[c] + middle * 3.0 * cubic->d[c]);
        w[c] = 2.0 * cubic->c[c] + middle * 6.0 * cubic->d[c];
        j[c] = 3.0 * cubic->d[c];
        sum += v[c] * v[c];
    }
    double centre = RootOfSquares(sum);
    double k = s_kronrod[7][1] * centre;
    double g = s_kronrod[7][2] * centre;
    for (int i = 0; i < 7; ++i)
    {
        double t = half * s_kronrod[i][0];
        double tt = t * t;
        double below = 0.0;
        double above = 0.0;
        for (uint8_t c = 0; c < channels; ++c)
        {
            double even = v[c] + j[c] * tt;
            double odd = w[c] * t;
            below += (even - odd) * (even - odd);
            above += (even + odd) * (even + odd);
        }
        double pair = RootOfSquares(below) + RootOfSquares(above);
        k += s_kronrod[i][1] * pair;
        g += s_kronrod[i][2] * pair;
    }
    *fine = half * k;
    *coarse = half * g;
}

// The roots of a number's cubic's derivative, b + 2cu + 3du^2, in
// order: two, one or none.
static int DerivativeRoots(const Cubic* cubic, double roots[2])
{
    double qa = 3.0 * cubic->d[0];
    double qb = 2.0 * cubic->c[0];
    double qc = cubic->b[0];
    if (qa == 0.0)
    {
        roots[0] = qb != 0.0 ? -qc / qb : 0.0;
        return qb != 0.0 ? 1 : 0;
    }
    double disc = qb * qb - 4.0 * qa * qc;
    if (!(disc > 0.0))
    {
        return 0;
    }
    // The stable pair: one root from the sum, the other from it.
    double q = -0.5 * (qb + (qb >= 0.0 ? sqrt(disc) : -sqrt(disc)));
    double r1 = q / qa;
    double r2 = q != 0.0 ? qc / q : r1;
    roots[0] = r1 < r2 ? r1 : r2;
    roots[1] = r1 < r2 ? r2 : r1;
    return 2;
}

// A number's cubic's length from a to b: its total variation, the
// changes between the turns where its derivative is 0. Exact, where a
// quadrature misses the kink each turn puts in |x'|.
static double TurnsLength(const Cubic* cubic, double a, double b)
{
    double roots[2] = {0.0, 0.0};
    int count = DerivativeRoots(cubic, roots);
    double at[4] = {a, a, a, b};
    int turns = 0;
    for (int r = 0; r < count; ++r)
    {
        at[1 + turns] = roots[r];
        turns += roots[r] > a && roots[r] < b ? 1 : 0;
    }
    at[1 + turns] = b;
    double total = 0.0;
    double from = cubic->a[0] + a * (cubic->b[0] + a * (cubic->c[0] + a * cubic->d[0]));
    for (int k = 1; k <= 1 + turns; ++k)
    {
        double u = at[k];
        double to = cubic->a[0] + u * (cubic->b[0] + u * (cubic->c[0] + u * cubic->d[0]));
        total += fabs(to - from);
        from = to;
    }
    return total;
}

// The arc length of the cubic from a to b, a < b.
static double Length(const Cubic* cubic, uint8_t channels, double a, double b)
{
    if (cubic->step)
    {
        return 0.0;
    }
    if (channels == 1)
    {
        return TurnsLength(cubic, a, b);
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
        double fine = 0.0;
        double coarse = 0.0;
        Kronrod(cubic, channels, lo, hi, &fine, &coarse);
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
// the length, kept within a shrinking bracket; the lengths it measures
// counted in *lengths when not NULL.
static double ParameterAt(const Cubic* cubic, uint8_t channels, double target, double total,
                          uint32_t* lengths)
{
    if (target <= 0.0 || target >= total)
    {
        // Outside, the segment extends by its parameter.
        return target / total;
    }
    double lo = 0.0;
    double hi = 1.0;
    double u = target / total;
    // Measured from the nearer end, the shorter way.
    double at =
        u <= 0.5 ? Length(cubic, channels, 0.0, u) : total - Length(cubic, channels, u, 1.0);
    uint32_t measured = 1;
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
        // Halley's step: the length's derivatives in u are the speed and
        // its own.
        double turn = 0.0;
        double bend = 0.0;
        double speed = SpeedChanges(cubic, u, channels, &turn, &bend);
        double denominator = 2.0 * speed * speed - error * turn;
        double next = speed > 0.0 && denominator > 0.0 ? u - 2.0 * error * speed / denominator
                                                       : 0.5 * (lo + hi);
        if (!(next > lo && next < hi))
        {
            next = 0.5 * (lo + hi);
        }
        double h = next - u;
        if (fabs(h) <= s_taylorStep && speed >= s_taylorSpeed * total)
        {
            // A step this short, away from a cusp: the length's Taylor
            // series to h^3, its remainder about h^4 times the speed's third
            // derivative, far below the tolerance.
            at += h * (speed + h * (0.5 * turn + h * bend / 6.0));
        }
        else
        {
            at += next > u ? Length(cubic, channels, u, next) : -Length(cubic, channels, next, u);
            ++measured;
        }
        u = next;
    }
    if (lengths != NULL)
    {
        *lengths += measured;
    }
    return u;
}

// Segment i's parameter at a progress: its local progress eased.
static double Eased(const mtwnPath* path, uint32_t i, double progress)
{
    const mtwnKeyframe* k = path->keyframes;
    double width = k[i + 1].offset - k[i].offset;
    double local = width > 0.0 ? (progress - k[i].offset) / width : 1.0;
    return mtwnEase(&k[i].easing, local, false);
}

// The segment a progress falls in and the parameter within it.
static void Locate(const mtwnPath* path, double progress, uint32_t* segmentOut, double* uOut,
                   Cubic* cubic, uint32_t* lengths)
{
    uint32_t i = 0;
    double u = 0.0;
    if (path->lengths != NULL)
    {
        double total = path->lengths[path->count - 1];
        double distance = progress * total;
        i = FindSegment(path, distance, true);
        SegmentCubic(path, i, cubic);
        double length = path->lengths[i + 1] - path->lengths[i];
        u = length > 0.0
                ? ParameterAt(cubic, path->channels, distance - path->lengths[i], length, lengths)
                : 1.0;
    }
    else
    {
        i = FindSegment(path, progress, false);
        SegmentCubic(path, i, cubic);
        u = Eased(path, i, progress);
    }
    *segmentOut = i;
    *uOut = u;
}

// A linear segment's value and tangent, as Evaluate gives them for its
// cubic (a = p0, b = p1 - p0), the same bits without making it.
static void SampleLinear(const mtwnPath* path, uint32_t i, double u, double valueOut[4],
                         double tangentOut[4])
{
    double p0[4];
    double p1[4];
    Load(path->keyframes[i].value, p0);
    Load(path->keyframes[i + 1].value, p1);
    double tangent[4];
    for (int c = 0; c < 4; ++c)
    {
        tangent[c] = c < path->channels ? p1[c] - p0[c] : 0.0;
        valueOut[c] = c >= path->channels ? 0.0 : (u == 1.0 ? p1[c] : p0[c] + u * tangent[c]);
    }
    if (tangentOut != NULL)
    {
        memcpy(tangentOut, tangent, sizeof tangent);
    }
}

void mtwnSamplePathCounting(const mtwnPath* path, double progress, double valueOut[4],
                            double tangentOut[4], uint32_t* lengths)
{
    if (path->lengths == NULL)
    {
        // A linear segment needs no cubic.
        uint32_t s = FindSegment(path, progress, false);
        if (path->keyframes[s].kind == mtwn_segmentLinear)
        {
            SampleLinear(path, s, Eased(path, s, progress), valueOut, tangentOut);
            return;
        }
    }
    Cubic cubic;
    uint32_t i = 0;
    double u = 0.0;
    Locate(path, progress, &i, &u, &cubic, lengths);
    double end[4];
    Load(path->keyframes[i + 1].value, end);
    Evaluate(&cubic, end, u, path->channels, valueOut, tangentOut);
    for (uint8_t c = path->channels; c < 4; ++c)
    {
        valueOut[c] = 0.0;
        if (tangentOut != NULL)
        {
            tangentOut[c] = 0.0;
        }
    }
}

void mtwnSamplePath(const mtwnPath* path, double progress, double valueOut[4], double tangentOut[4])
{
    mtwnSamplePathCounting(path, progress, valueOut, tangentOut, NULL);
}

static const uint8_t s_kindChannels[] = {1, 2, 3, 4, 4, 1, 4, 1, 1};

static bool IsNumberKind(mtwnValueKind kind)
{
    return kind != mtwn_valueColor && kind != mtwn_valueRotation && kind != mtwn_valueDiscrete;
}

// Whether the track's kind takes every segment its keyframes start.
static bool TakesSegments(mtwnValueKind kind, const mtwnKeyframe* keyframes, uint32_t count)
{
    if (kind != mtwn_valueColor && kind != mtwn_valueDiscrete)
    {
        return true;
    }
    for (uint32_t i = 0; i + 1 < count; ++i)
    {
        if (keyframes[i].kind != mtwn_segmentLinear && keyframes[i].kind != mtwn_segmentStep)
        {
            return false;
        }
    }
    return true;
}

static bool IsZero4(mtwnVec4 v)
{
    return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f && v.w == 0.0f;
}

static bool CheckTrack(const mtwnTrackDef* def, const mtwnKeyframe* keyframes, uint32_t count,
                       bool lengths, const mtwnSegmentMix* mixes)
{
    if (def == NULL || def->kind > mtwn_valueDiscrete || def->rotationPath > mtwn_rotationLongest ||
        !IsValid(keyframes, count, s_kindChannels[def->kind], def->alpha, lengths) ||
        (lengths && !IsNumberKind(def->kind)) || !TakesSegments(def->kind, keyframes, count))
    {
        return false;
    }
    bool mixed = def->kind == mtwn_valueColor || def->kind == mtwn_valueRotation;
    if (mixed && mixes == NULL)
    {
        return false;
    }
    for (uint32_t i = 0; def->kind == mtwn_valueRotation && i < count; ++i)
    {
        if (IsZero4(keyframes[i].value))
        {
            return false;
        }
    }
    return true;
}

static void Shift(mtwnVec4* v, float by)
{
    v->x += by;
}

static void Negate(mtwnVec4* v)
{
    *v = (mtwnVec4){-v->x, -v->y, -v->z, -v->w};
}

static double Dot4(mtwnVec4 a, mtwnVec4 b)
{
    return (double)a.x * (double)b.x + (double)a.y * (double)b.y + (double)a.z * (double)b.z +
           (double)a.w * (double)b.w;
}

// Angles each moved by the method's whole turns from the one before;
// rotations on the shortest path each into the hemisphere of the one
// before; their controls with them.
static void Canonicalize(const mtwnTrackDef* def, mtwnKeyframe* keyframes, uint32_t count)
{
    for (uint32_t i = 1; i < count; ++i)
    {
        mtwnKeyframe* k = &keyframes[i];
        if (def->kind == mtwn_valueAngle)
        {
            float moved = (float)mtwnMixAngle((double)keyframes[i - 1].value.x, (double)k->value.x,
                                              def->angleMethod, 1.0);
            float by = moved - k->value.x;
            k->value.x = moved;
            Shift(&k->in, by);
            Shift(&k->out, by);
        }
        else if (def->kind == mtwn_valueRotation && def->rotationPath == mtwn_rotationShortest &&
                 Dot4(keyframes[i - 1].value, k->value) < 0.0)
        {
            Negate(&k->value);
            Negate(&k->in);
            Negate(&k->out);
        }
    }
}

mtwnResult mtwnMakeTrack(const mtwnTrackDef* def, mtwnKeyframe* keyframes, uint32_t count,
                         double* lengthsOut, mtwnSegmentMix* mixesOut, mtwnTrack* trackOut)
{
    if (trackOut == NULL || !CheckTrack(def, keyframes, count, lengthsOut != NULL, mixesOut))
    {
        return mtwn_errorInvalid;
    }
    // Only the longest way between equal rotations refuses a mix, and it
    // flips nothing, so a refusal leaves the keyframes as they were.
    Canonicalize(def, keyframes, count);
    bool mixed = def->kind == mtwn_valueColor || def->kind == mtwn_valueRotation;
    for (uint32_t i = 0; mixed && i + 1 < count; ++i)
    {
        mtwnVec4 a = keyframes[i].value;
        mtwnVec4 b = keyframes[i + 1].value;
        mtwnResult made = mtwn_success;
        if (def->kind == mtwn_valueColor)
        {
            made =
                mtwnMakeColorMix((mtwnColor){a.x, a.y, a.z, a.w}, (mtwnColor){b.x, b.y, b.z, b.w},
                                 def->colorSpace, def->hueMethod, &mixesOut[i].color);
        }
        else if (keyframes[i].kind == mtwn_segmentLinear)
        {
            made =
                mtwnMakeRotationMix((mtwnQuat){a.x, a.y, a.z, a.w}, (mtwnQuat){b.x, b.y, b.z, b.w},
                                    def->rotationPath, &mixesOut[i].rotation);
        }
        if (made != mtwn_success)
        {
            return mtwn_errorInvalid;
        }
    }
    mtwnTrack track = {{0}, mixed ? mixesOut : NULL, def->kind, {0}};
    mtwnResult result = mtwnMakePath(keyframes, count, s_kindChannels[def->kind], def->alpha,
                                     lengthsOut, &track.path);
    if (result != mtwn_success)
    {
        return result;
    }
    *trackOut = track;
    return mtwn_success;
}

static mtwnVec4 ToVec4(const double v[4])
{
    return (mtwnVec4){(float)v[0], (float)v[1], (float)v[2], (float)v[3]};
}

// A rotation's per-component cubic, normalized; the segment's start where
// it cancels to zero.
static mtwnVec4 Normalized(const double v[4], mtwnVec4 start)
{
    double norm = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
    if (!(norm > 0.0))
    {
        return start;
    }
    return (mtwnVec4){(float)(v[0] / norm), (float)(v[1] / norm), (float)(v[2] / norm),
                      (float)(v[3] / norm)};
}

mtwnVec4 mtwnSampleTrack(const mtwnTrack* track, double progress)
{
    const mtwnPath* path = &track->path;
    double value[4];
    if (IsNumberKind(track->kind))
    {
        mtwnSamplePath(path, progress, value, NULL);
        if (track->kind == mtwn_valueInteger)
        {
            value[0] = round(value[0]);
        }
        return ToVec4(value);
    }
    Cubic cubic;
    uint32_t i = 0;
    double u = 0.0;
    Locate(path, progress, &i, &u, &cubic, NULL);
    const mtwnKeyframe* k = path->keyframes;
    mtwnSegmentKind kind = k[i].kind;
    mtwnVec4 held = u >= 1.0 ? k[i + 1].value : k[i].value;
    switch (track->kind)
    {
    case mtwn_valueDiscrete:
        return kind == mtwn_segmentStep ? held : (u < 0.5 ? k[i].value : k[i + 1].value);
    case mtwn_valueColor:
    {
        if (kind == mtwn_segmentStep)
        {
            return held;
        }
        mtwnColor color = mtwnMixColor(&track->mixes[i].color, u);
        return (mtwnVec4){color.red, color.green, color.blue, color.alpha};
    }
    default:
        break;
    }
    // A rotation, normalized whatever its segment.
    if (kind == mtwn_segmentLinear)
    {
        mtwnQuat q = mtwnMixRotation(&track->mixes[i].rotation, u);
        return (mtwnVec4){q.x, q.y, q.z, q.w};
    }
    if (kind == mtwn_segmentStep || u == 1.0)
    {
        Load(held, value);
    }
    else
    {
        double end[4];
        Load(k[i + 1].value, end);
        Evaluate(&cubic, end, u, 4, value, NULL);
    }
    return Normalized(value, k[i].value);
}
