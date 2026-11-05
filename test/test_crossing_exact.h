// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Exact counts for the crossing suite and fuzz_values. A case is one
// level with a mark in each iteration, or a node's own iteration starts
// after its first: any iteration start, every direction, either sign of
// rate, with and without repeat delays. Its counts are worked out in exact
// arithmetic over the doubles' exact values. Positions are the node's
// scaled time from its first iteration's start, X = scaled + f * d (f the
// start's fraction, d the duration); offset k starts at X = k * (d + r),
// r the repeat delay, and a mark at child time c lies at that plus c in a
// forward iteration, plus d - c in a reversed one.
//
// The library rounds its own steps at the time's magnitude: the time less
// the delay, times the rate, over the duration, the progress into an
// iteration and its child time. A count may differ from the exact one only
// where a start or a mark lies within those roundings of a sweep's end
// (docs/internals/events.md). The window is worked out per end, from half
// an ulp of each step's result, not a fixed tolerance; a count that
// differs elsewhere is wrong.
//
// Includes src/crossing.h: the suites that use it are white-box.

#ifndef MAUL_TWEEN_TEST_CROSSING_EXACT_H
#define MAUL_TWEEN_TEST_CROSSING_EXACT_H

#include "crossing.h"
#include "timing_internal.h"

#include "maul-tween/timing.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Fixed point: a signed 320-bit integer in two's complement, little
// endian, in units of 2^-192. The cases keep every double's ulp at 2^-90
// or more and its magnitude below 2^62, so each one, each sum and each
// product of two of them (and of a sum and a double's significand) is
// exact here.
enum
{
    s_fxLimbs = 5,
    s_fxFraction = 192,
};

typedef struct Fx
{
    uint64_t w[s_fxLimbs];
} Fx;

static const Fx s_fxZero = {{0u, 0u, 0u, 0u, 0u}};

static Fx FxNegate(Fx a)
{
    Fx r;
    unsigned __int128 carry = 1;
    for (int i = 0; i < s_fxLimbs; ++i)
    {
        carry += (unsigned __int128)(~a.w[i]);
        r.w[i] = (uint64_t)carry;
        carry >>= 64;
    }
    return r;
}

static Fx FxAdd(Fx a, Fx b)
{
    Fx r;
    unsigned __int128 carry = 0;
    for (int i = 0; i < s_fxLimbs; ++i)
    {
        carry += (unsigned __int128)a.w[i] + b.w[i];
        r.w[i] = (uint64_t)carry;
        carry >>= 64;
    }
    return r;
}

static Fx FxSub(Fx a, Fx b)
{
    return FxAdd(a, FxNegate(b));
}

static bool FxNegative(Fx a)
{
    return (a.w[s_fxLimbs - 1] >> 63) != 0u;
}

static Fx FxAbs(Fx a)
{
    return FxNegative(a) ? FxNegate(a) : a;
}

// -1, 0 or 1 as a is below, at or above b.
static int FxCompare(Fx a, Fx b)
{
    Fx d = FxSub(a, b);
    if (FxNegative(d))
    {
        return -1;
    }
    for (int i = 0; i < s_fxLimbs; ++i)
    {
        if (d.w[i] != 0u)
        {
            return 1;
        }
    }
    return 0;
}

// A 128-bit magnitude times 2^(shift - 192), shift >= 0, as fixed point.
static Fx FxShifted(unsigned __int128 m, int shift, bool negative)
{
    Fx r = s_fxZero;
    int limb = shift / 64;
    int bit = shift % 64;
    uint64_t parts[3] = {(uint64_t)m, (uint64_t)(m >> 64), 0u};
    if (bit != 0)
    {
        parts[2] = parts[1] >> (64 - bit);
        parts[1] = (parts[1] << bit) | (parts[0] >> (64 - bit));
        parts[0] <<= bit;
    }
    for (int i = 0; i < 3 && limb + i < s_fxLimbs; ++i)
    {
        r.w[limb + i] = parts[i];
    }
    return negative ? FxNegate(r) : r;
}

// A double's significand: |x| = m * 2^e.
static uint64_t Significand(double x, int* exponentOut)
{
    int e = 0;
    double fraction = frexp(fabs(x), &e);
    *exponentOut = e - 53;
    return (uint64_t)ldexp(fraction, 53);
}

static Fx FxOf(double x)
{
    int e = 0;
    uint64_t m = x == 0.0 ? 0u : Significand(x, &e);
    return x == 0.0 ? s_fxZero : FxShifted(m, e + s_fxFraction, x < 0.0);
}

// The exact product of a fixed-point value and a double.
static Fx FxTimes(Fx a, double x)
{
    if (x == 0.0)
    {
        return s_fxZero;
    }
    int e = 0;
    uint64_t m = Significand(x, &e);
    bool negative = FxNegative(a) != (x < 0.0);
    Fx magnitude = FxAbs(a);
    // magnitude * m, then times 2^e: e is negative for every double the
    // cases scale by (their significands' lowest bit is below 1), and the
    // shift drops no bit of a product kept at 2^-90 or more.
    uint64_t product[s_fxLimbs + 1];
    unsigned __int128 carry = 0;
    for (int i = 0; i < s_fxLimbs; ++i)
    {
        carry += (unsigned __int128)magnitude.w[i] * m;
        product[i] = (uint64_t)carry;
        carry >>= 64;
    }
    product[s_fxLimbs] = (uint64_t)carry;
    Fx r = s_fxZero;
    if (e >= 0)
    {
        for (int i = 0; i < s_fxLimbs; ++i)
        {
            r.w[i] = product[i];
        }
        for (int k = 0; k < e; ++k)
        {
            r = FxAdd(r, r);
        }
    }
    else
    {
        int limb = (-e) / 64;
        int bit = (-e) % 64;
        for (int i = 0; i < s_fxLimbs; ++i)
        {
            uint64_t lo = i + limb <= s_fxLimbs ? product[i + limb] : 0u;
            uint64_t hi = i + limb + 1 <= s_fxLimbs ? product[i + limb + 1] : 0u;
            r.w[i] = bit == 0 ? lo : (lo >> bit) | (hi << (64 - bit));
        }
    }
    return negative ? FxNegate(r) : r;
}

// Half an ulp of a double, as fixed point: 0 for 0.
static Fx FxHalfUlp(double x)
{
    if (x == 0.0 || !isfinite(x))
    {
        return s_fxZero;
    }
    int e = 0;
    (void)Significand(x, &e);
    return FxShifted(1u, e - 1 + s_fxFraction, false);
}

// An approximation of a fixed-point value, for choosing which iterations
// to look at.
static double FxApprox(Fx a)
{
    Fx m = FxAbs(a);
    double x = 0.0;
    for (int i = s_fxLimbs - 1; i >= 0; --i)
    {
        x = x * 0x1p64 + (double)m.w[i];
    }
    x = ldexp(x, -s_fxFraction);
    return FxNegative(a) ? -x : x;
}

// A case: a node's timing, the active duration the library works out for
// it, and a mark's child time in each of its iterations, or none for the
// node's own iteration starts after its first.
typedef struct ExactCase
{
    mtwnTiming timing;
    double active;
    bool marks;
    double mark;
} ExactCase;

// The case's node as a leaf, or its level and the mark below it.
static void ExactSetUp(const ExactCase* c, mtwnLevel* level, mtwnLeaf* leaf, uint32_t* countOut)
{
    memset(level, 0, sizeof *level);
    memset(leaf, 0, sizeof *leaf);
    if (c->marks)
    {
        level->timing = c->timing;
        leaf->kind = mtwn_leafInstant;
        leaf->at = c->mark;
        *countOut = 1;
    }
    else
    {
        leaf->kind = mtwn_leafIterations;
        leaf->timing = c->timing;
        *countOut = 0;
    }
    mtwnPrepareLevels(level, *countOut, leaf);
}

// The position at a local time, exactly, held at its ends outside the
// active interval: from the end back at a negative rate.
static Fx ExactX(const ExactCase* c, double t)
{
    const mtwnTiming* timing = &c->timing;
    double f = timing->iterationStart - floor(timing->iterationStart);
    Fx a = FxSub(FxOf(t), FxOf(timing->delay));
    a = FxNegative(a) ? s_fxZero : a;
    if (isinf(c->active))
    {
        // Endless, at a positive rate: never past its end.
        return FxAdd(FxTimes(a, timing->playbackRate), FxTimes(FxOf(f), timing->duration));
    }
    Fx active = FxOf(c->active);
    a = FxCompare(a, active) > 0 ? active : a;
    if (timing->playbackRate < 0.0)
    {
        a = FxSub(active, a);
    }
    Fx scaled = FxTimes(a, fabs(timing->playbackRate));
    return FxAdd(scaled, FxTimes(FxOf(f), timing->duration));
}

// The far end of the path: where its active interval ends (a positive
// rate) or begins (a negative one), past every position for an endless
// node.
static Fx ExactPathEnd(const ExactCase* c)
{
    if (isinf(c->active))
    {
        return FxShifted(1u, 120 + s_fxFraction, false);
    }
    const mtwnTiming* timing = &c->timing;
    return ExactX(c, timing->playbackRate > 0.0 ? timing->delay + c->active : timing->delay);
}

// The window around an end's position the library's roundings may move
// it in, in X: half an ulp of each step's result, carried through the
// later steps' scales.
static Fx ExactWindow(const ExactCase* c, double t)
{
    const mtwnTiming* timing = &c->timing;
    double d = timing->duration;
    double r = timing->repeatDelay;
    double rate = fabs(timing->playbackRate);
    double f = timing->iterationStart - floor(timing->iterationStart);
    double a = t - timing->delay;
    a = a < 0.0 ? 0.0 : (a > c->active ? c->active : a);
    double from = timing->playbackRate < 0.0 ? c->active - a : a;
    double scaled = from * rate;
    // The time less the delay, from the end, and times the rate.
    Fx w = FxTimes(FxAdd(FxHalfUlp(a), FxHalfUlp(from)), rate);
    w = FxAdd(w, FxHalfUlp(scaled));
    double q = 0.0;
    if (r > 0.0)
    {
        double cycle = d + r;
        double position = scaled + f * d;
        double k = floor(position / cycle);
        double within = position - k * cycle;
        w = FxAdd(w, FxAdd(FxHalfUlp(f * d), FxHalfUlp(position)));
        Fx drift = FxTimes(FxHalfUlp(cycle), fabs(k) + 1.0);
        w = FxAdd(w, FxAdd(drift, FxAdd(FxHalfUlp(k * cycle), FxHalfUlp(within))));
        double overall = k + within / d;
        w = FxAdd(w, FxTimes(FxAdd(FxHalfUlp(within / d), FxHalfUlp(overall)), d));
        q = within / d;
    }
    else
    {
        double quotient = scaled / d;
        w = FxAdd(w, FxTimes(FxHalfUlp(quotient), d));
        q = quotient + f - floor(quotient + f);
    }
    // The progress into the iteration, its complement for a reversed one,
    // and the child time.
    w = FxAdd(w, FxTimes(FxAdd(FxHalfUlp(q), FxHalfUlp(1.0 - q)), d));
    w = FxAdd(w, FxAdd(FxHalfUlp(q * d), FxHalfUlp((1.0 - q) * d)));
    return w;
}

// What the case's points are: offset k's start, or its mark, and whether
// the node runs forwards in it.
static bool ExactForwards(const ExactCase* c, double k)
{
    double first = floor(c->timing.iterationStart);
    bool odd = (fmod(first, 2.0) != 0.0) != (fmod(k, 2.0) != 0.0);
    return mtwnIterationForwards(c->timing.direction, odd ? 1.0 : 0.0);
}

// The last offset, as the library shapes the node from the timing
// model's step (the fraction plus the iterations, rounded once).
static double ExactLast(const ExactCase* c)
{
    double f = c->timing.iterationStart - floor(c->timing.iterationStart);
    return isinf(c->timing.iterations) ? HUGE_VAL : ceil(f + c->timing.iterations) - 1.0;
}

// The position of point k, and whether it counts as the leaf moving
// forwards; false for no point there (a mark outside the node's path).
static bool ExactPoint(const ExactCase* c, double k, Fx lo, Fx hi, Fx* positionOut,
                       bool* sameOut)
{
    const mtwnTiming* timing = &c->timing;
    Fx cycle = FxAdd(FxOf(timing->duration), FxOf(timing->repeatDelay));
    Fx start = FxTimes(cycle, k);
    bool forwards = ExactForwards(c, k);
    if (!c->marks)
    {
        *positionOut = start;
        *sameOut = true;
        return k >= 1.0;
    }
    double within = forwards ? c->mark : timing->duration - c->mark;
    *positionOut = FxAdd(start, FxOf(within));
    *sameOut = forwards == (timing->playbackRate > 0.0);
    return FxCompare(*positionOut, lo) >= 0 && FxCompare(*positionOut, hi) <= 0;
}

// The forward iterations among offsets k0 to k1, all inside the path.
static double ExactForwardCount(const ExactCase* c, double k0, double k1)
{
    double n = k1 - k0 + 1.0;
    switch (c->timing.direction)
    {
    case mtwn_directionNormal:
        return n;
    case mtwn_directionReverse:
        return 0.0;
    default:
    {
        // Every other one from k0, which runs forwards or not.
        double firstOnes = floor((n + 1.0) * 0.5);
        return ExactForwards(c, k0) ? firstOnes : n - firstOnes;
    }
    }
}

// Every point of offsets k0 to k1, each way (none for k0 > k1): the
// ends checked for lying on the path, those between always do.
static void ExactAll(const ExactCase* c, double k0, double k1, Fx lo, Fx hi, double* sameOut,
                     double* oppositeOut)
{
    if (k0 > k1)
    {
        return;
    }
    double ends[2] = {k0, k1};
    for (int e = 0; e < (k1 > k0 ? 2 : 1); ++e)
    {
        Fx p;
        bool same = false;
        if (ExactPoint(c, ends[e], lo, hi, &p, &same))
        {
            *(same ? sameOut : oppositeOut) += 1.0;
        }
    }
    if (k1 - k0 >= 2.0)
    {
        double n = k1 - k0 - 1.0;
        double forwards = ExactForwardCount(c, k0 + 1.0, k1 - 1.0);
        double same = !c->marks ? n : (c->timing.playbackRate > 0.0 ? forwards : n - forwards);
        *sameOut += same;
        *oppositeOut += n - same;
    }
}

// The points a position has reached, each way, over the offsets the node
// has: at a positive rate those at or below it (strictly below when
// strict), at a negative one those at or above it. Offsets 3 or more
// below the one x lies in hold points wholly below it, and 3 or more
// above wholly above; those between are compared one by one.
static void ExactReached(const ExactCase* c, Fx x, bool strict, double* sameOut,
                         double* oppositeOut)
{
    const mtwnTiming* timing = &c->timing;
    double cycle = timing->duration + timing->repeatDelay;
    double f = timing->iterationStart - floor(timing->iterationStart);
    double last = ExactLast(c);
    bool forwardRate = timing->playbackRate > 0.0;
    Fx lo = FxTimes(FxOf(f), timing->duration);
    Fx hi = ExactPathEnd(c);
    double around = floor(FxApprox(x) / cycle);
    double near0 = around - 2.0 < 0.0 ? 0.0 : around - 2.0;
    double near1 = around + 2.0 > last ? last : around + 2.0;
    *sameOut = 0.0;
    *oppositeOut = 0.0;
    if (forwardRate)
    {
        ExactAll(c, 0.0, near0 - 1.0, lo, hi, sameOut, oppositeOut);
    }
    else
    {
        ExactAll(c, near1 + 1.0, last, lo, hi, sameOut, oppositeOut);
    }
    for (double k = near0; k <= near1; k += 1.0)
    {
        Fx p;
        bool same = false;
        if (!ExactPoint(c, k, lo, hi, &p, &same))
        {
            continue;
        }
        int side = FxCompare(x, p);
        bool reached = forwardRate ? (strict ? side > 0 : side >= 0) : (strict ? side < 0 : side <= 0);
        *(same ? sameOut : oppositeOut) += reached ? 1.0 : 0.0;
    }
}

// The exact counts of a sweep: (from, to] forwards, [to, from) backwards,
// from itself too when closed; the backward sweep's leaf moving against
// it.
static mtwnCrossings ExactCount(const ExactCase* c, double from, double to, bool fromClosed)
{
    double s[2] = {0.0, 0.0};
    double o[2] = {0.0, 0.0};
    if (to >= from)
    {
        if (to == from && !fromClosed)
        {
            return (mtwnCrossings){0.0, 0.0};
        }
        ExactReached(c, ExactX(c, to), false, &s[1], &o[1]);
        ExactReached(c, ExactX(c, from), fromClosed, &s[0], &o[0]);
        return (mtwnCrossings){s[1] - s[0], o[1] - o[0]};
    }
    ExactReached(c, ExactX(c, from), !fromClosed, &s[1], &o[1]);
    ExactReached(c, ExactX(c, to), true, &s[0], &o[0]);
    return (mtwnCrossings){o[1] - o[0], s[1] - s[0]};
}

// Whether a point lies within the window of a sweep's end.
static bool ExactNear(const ExactCase* c, double t)
{
    Fx x = ExactX(c, t);
    Fx w = ExactWindow(c, t);
    Fx lo = FxSub(x, w);
    Fx hi = FxAdd(x, w);
    const mtwnTiming* timing = &c->timing;
    double cycle = timing->duration + timing->repeatDelay;
    double around = floor(FxApprox(x) / cycle);
    double f = timing->iterationStart - floor(timing->iterationStart);
    Fx pathLo = FxTimes(FxOf(f), timing->duration);
    Fx pathHi = ExactPathEnd(c);
    double last = ExactLast(c);
    for (double k = around - 2.0; k <= around + 2.0; k += 1.0)
    {
        Fx p;
        bool same = false;
        if (k < 0.0 || k > last || !ExactPoint(c, k, pathLo, pathHi, &p, &same))
        {
            continue;
        }
        if (FxCompare(p, lo) >= 0 && FxCompare(p, hi) <= 0)
        {
            return true;
        }
    }
    return false;
}

// What a case's sweep shows against the exact counts.
typedef enum ExactVerdict
{
    s_exactAgrees,
    s_exactInWindow,
    s_exactWrong,
} ExactVerdict;

static ExactVerdict ExactCheck(const ExactCase* c, const mtwnLevel* level, uint32_t count,
                               const mtwnLeaf* leaf, double from, double to, bool fromClosed,
                               mtwnCrossings* countOut, mtwnCrossings* exactOut)
{
    *countOut = mtwnCountCrossings(level, count, leaf, from, to, fromClosed);
    *exactOut = ExactCount(c, from, to, fromClosed);
    if (countOut->same == exactOut->same && countOut->opposite == exactOut->opposite)
    {
        return s_exactAgrees;
    }
    return ExactNear(c, from) || ExactNear(c, to) ? s_exactInWindow : s_exactWrong;
}

#endif
