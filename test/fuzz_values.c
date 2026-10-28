// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The values layer driven by bytes: easings of every
// kind, springs, spring easings, decay, the rubber band, colors,
// rotations, paths, tracks, punch, shake and staggers, made from
// parameters ordinary or wild (zero, negative, tiny, huge, infinite,
// NaN). What is made, sampled at finite inputs, is never NaN, and never
// infinite when every parameter was ordinary; an easing, made from any
// parameters, is finite at every finite progress, and with a field a host
// wrote is refused or still finite; easings meet their ends
// where the API says they do, rotations stay unit, a punch rests outside
// its span, and a stagger that succeeds writes finite offsets.
//
// Built with MAUL_TWEEN_FUZZ (Clang) it is a libFuzzer target over a
// corpus directory; in the test suite it replays its seeds and a fixed
// set of random inputs. With MTWN_FUZZ_FIT it is fuzz_fit instead: the
// easings alone, each fitted to CSS linear() at a drawn tolerance, which
// the other target leaves out as it costs a thousand times the rest.

#include "maul-tween/easing.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
#include "maul-tween/sample.h"
#include "maul-tween/spring.h"
#include "maul-tween/timing.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Bytes
{
    const uint8_t* data;
    size_t size;
    size_t at;
    // Whether every parameter drawn since the last reset was ordinary, and
    // whether every one was finite.
    bool ordinary;
    bool finite;
} Bytes;

static uint32_t Byte(Bytes* in)
{
    return in->at < in->size ? in->data[in->at++] : 0u;
}

static uint32_t Below(Bytes* in, uint32_t n)
{
    return Byte(in) % n;
}

// An ordinary parameter, in (-scale, scale) on a grid of 1/256 of it, or a
// wild one.
static double Param(Bytes* in, double scale)
{
    uint32_t b = Byte(in);
    if (b % 16 < 12)
    {
        uint32_t high = Byte(in);
        uint32_t low = Byte(in);
        double unit = ((double)(high << 8 | low) - 32768.0) / 32768.0;
        return unit * scale;
    }
    in->ordinary = false;
    switch (b % 16)
    {
    case 12:
        return 1e300;
    case 13:
        in->finite = false;
        return (b & 16) != 0 ? HUGE_VAL : -HUGE_VAL;
    case 14:
        in->finite = false;
        return (double)NAN;
    default:
        return (b & 16) != 0 ? 1e-300 : -1e-300;
    }
}

// A positive ordinary parameter, or a wild one.
static double Positive(Bytes* in, double scale)
{
    double p = Param(in, scale);
    return in->ordinary && p == p ? fabs(p) : p;
}

// Parameters drawn in order (a call's arguments and an initializer's
// expressions are evaluated in no set order): positive or not, each to
// its scale.
static void Draw(Bytes* in, double* out, int count, const double* scales, bool positive)
{
    for (int i = 0; i < count; ++i)
    {
        out[i] = positive ? Positive(in, scales[i]) : Param(in, scales[i]);
    }
}

static mtwnVec4 Vec(Bytes* in, double xy, double z, double w)
{
    double p[4];
    Draw(in, p, 4, (const double[]){xy, xy, z, w}, false);
    return (mtwnVec4){(float)p[0], (float)p[1], (float)p[2], (float)p[3]};
}

static bool s_bad;

static void Fail(const char* what, double value)
{
    fprintf(stderr, "fuzz_values: %s (%.17g)\n", what, value);
    s_bad = true;
}

// A sampled result: never NaN from finite parameters, and finite from
// ordinary ones.
static void Check(const Bytes* in, const char* what, double value)
{
    if ((in->finite && isnan(value)) || (in->ordinary && isinf(value)))
    {
        Fail(what, value);
    }
}

// Each component checked alone: infinities of opposite signs in two of
// them are no NaN of the function's.
static void CheckEach(const Bytes* in, const char* what, const double* values, int count)
{
    for (int i = 0; i < count; ++i)
    {
        Check(in, what, values[i]);
    }
}

static const double s_progress[] = {-1.5, -0.25,         0.0, 0x1p-40, 0.1, 0.5,
                                    0.75, 1.0 - 0x1p-40, 1.0, 1.25,    2.5};

// And for an easing, progress so far out that a mode's 2p overflows.
static const double s_farProgress[] = {-DBL_MAX, -0x1.8p1023, 0x1.8p1023, DBL_MAX};

// The fit's tolerance (fuzz_fit's alone). A fit costs about as many
// points as the curve needs, so an even draw of the tolerance spent the
// target's time on a few fits near the limit (19 inputs a second): most
// draws are from 2^-2 to 2^-16 now, one in sixteen is one the fit
// refuses, and only an input drawn deep may take one that runs the fit
// to MTWN_MAX_FIT_POINTS, once. An easing of a wild parameter is the
// other way to the limit (Easings).
static double FitTolerance(Bytes* in, bool* deep)
{
    uint32_t b = Byte(in);
    if (b % 16 == 0)
    {
        static const double refused[4] = {0.0, -0x1p-8, (double)NAN, HUGE_VAL};
        return refused[b / 16 % 4];
    }
    if (b % 16 == 1 && *deep)
    {
        *deep = false;
        return (b & 16) != 0 ? 1e-300 : 0x1p-40;
    }
    return ldexp(1.0 + (double)Byte(in) / 256.0, -(int)(2 + b % 15));
}

// An easing a maker made, at a finite progress: taken, and finite.
static double Eased(const mtwnEasing* e, double progress, bool before)
{
    double eased = (double)NAN;
    if (mtwnEase(e, progress, before, &eased) != mtwn_success || !isfinite(eased))
    {
        Fail("an easing's value", eased);
    }
    return eased;
}

// Any double, from eight bytes.
static double AnyDouble(Bytes* in)
{
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i)
    {
        bits = bits << 8 | Byte(in);
    }
    double value = 0.0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

// A forged easing at a progress: refused, or finite.
static void ForgedAt(const mtwnEasing* e, double progress)
{
    double eased = 0.0;
    if (mtwnEase(e, progress, false, &eased) == mtwn_success && !isfinite(eased))
    {
        Fail("a forged easing taken and not finite", eased);
    }
}

// A made easing with one field a host wrote: refused, or finite at every
// progress. Not for linear() or a host function, whose points and
// function the host owns.
static void Forged(Bytes* in, const mtwnEasing* made)
{
    mtwnEasing e = *made;
    uint32_t field = Below(in, 8);
    if (field < 5)
    {
        e.parameters[field] = AnyDouble(in);
    }
    else if (field == 5)
    {
        e.count = 0;
        for (int i = 0; i < 4; ++i)
        {
            e.count = e.count << 8 | Byte(in);
        }
    }
    else if (field == 6)
    {
        e.kind = (mtwnEasingKind)Byte(in);
    }
    else
    {
        e.position = (mtwnStepPosition)Byte(in);
    }
    for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
    {
        ForgedAt(&e, s_progress[i]);
    }
    for (size_t i = 0; i < sizeof s_farProgress / sizeof s_farProgress[0]; ++i)
    {
        ForgedAt(&e, s_farProgress[i]);
    }
}

// The fit of linear() is fuzz_fit's: at its limit it takes a thousand
// times another section's inputs, so here it would starve the rest.
static void Easings(Bytes* in, bool fit, bool* deep)
{
    in->ordinary = true;
    in->finite = true;
    mtwnEasing e;
    // A linear() easing references its points: they outlive it here.
    mtwnLinearPoint points[8];
    mtwnResult r = mtwn_errorInvalid;
    mtwnEaseMode mode = (mtwnEaseMode)Below(in, 5);
    uint32_t kind = Below(in, 9);
    bool ends = true;
    switch (kind)
    {
    case 0:
        r = mtwnMakePowerEasing(mode, Param(in, 8.0), &e);
        break;
    case 1:
        r = mtwnMakePennerEasing((mtwnEasingKind)Below(in, 14), mode, &e);
        break;
    case 2:
        r = mtwnMakeBackEasing(mode, Param(in, 10.0), &e);
        break;
    case 3:
    {
        double p[2];
        Draw(in, p, 2, (const double[]){4.0, 2.0}, false);
        r = mtwnMakeElasticEasing(mode, p[0], p[1], &e);
    }
    break;
    case 4:
    {
        double p[4];
        Draw(in, p, 4, (const double[]){1.5, 4.0, 1.5, 4.0}, false);
        r = mtwnMakeCubicBezier(p[0], p[1], p[2], p[3], &e);
    }
    break;
    case 5:
    {
        uint32_t steps = Below(in, 12);
        r = mtwnMakeSteps(steps, (mtwnStepPosition)Below(in, 5), &e);
    }
        ends = false;
        break;
    case 6:
        r = mtwnMakeCssEasing((mtwnCssKeyword)Below(in, 8), &e);
        ends = false;
        break;
    case 7:
    {
        uint32_t count = Below(in, 9);
        for (uint32_t i = 0; i < count; ++i)
        {
            points[i].input = Below(in, 4) == 0 ? (double)NAN : Param(in, 1.5);
            points[i].output = Param(in, 2.0);
        }
        r = mtwnMakeLinearEasing(points, count, &e);
        ends = false;
        break;
    }
    default:
    {
        mtwnSpring spring;
        double p[4];
        Draw(in, p, 1, (const double[]){2.0}, true);
        Draw(in, p + 1, 1, (const double[]){1.0}, false);
        Draw(in, p + 2, 1, (const double[]){3.0}, true);
        Draw(in, p + 3, 1, (const double[]){20.0}, false);
        if (mtwnMakeSpringFromDuration(p[0], p[1], &spring) != mtwn_success)
        {
            return;
        }
        r = mtwnMakeSpringEasing(&spring, p[2], p[3], mode, &e);
        break;
    }
    }
    if (r != mtwn_success)
    {
        return;
    }
    for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
    {
        (void)Eased(&e, s_progress[i], false);
        (void)Eased(&e, s_progress[i], true);
    }
    for (size_t i = 0; i < sizeof s_farProgress / sizeof s_farProgress[0]; ++i)
    {
        (void)Eased(&e, s_farProgress[i], false);
        (void)Eased(&e, s_farProgress[i], true);
    }
    if (ends && in->ordinary && (Eased(&e, 0.0, false) != 0.0 || Eased(&e, 1.0, false) != 1.0))
    {
        Fail("an easing's ends", Eased(&e, 1.0, false));
    }
    if (e.kind != mtwn_easingLinearPoints)
    {
        Forged(in, &e);
    }
    float samples[16];
    if (mtwnSampleEasingTable(&e, 16, samples) == mtwn_success)
    {
        for (int i = 0; i < 16; ++i)
        {
            Check(in, "a sampled easing", (double)samples[i]);
        }
    }
    if (!fit)
    {
        return;
    }
    // A wild parameter the makers take, a back's overshoot or a Bezier's
    // y or a linear() output of 1e300, runs the fit to its limit at any
    // tolerance: such an easing is fitted only on the deep draw, once.
    if (!in->ordinary)
    {
        if (!*deep)
        {
            return;
        }
        *deep = false;
    }
    static mtwnLinearPoint fitted[256];
    uint32_t fittedCount = 0;
    if (mtwnFitLinearEasing(&e, FitTolerance(in, deep), fitted, 256, &fittedCount) == mtwn_success)
    {
        mtwnEasing linear;
        if (mtwnMakeLinearEasing(fitted, fittedCount, &linear) != mtwn_success)
        {
            Fail("a fitted linear() refused", (double)fittedCount);
            return;
        }
        (void)Eased(&linear, 0.5, false);
    }
}

static void Springs(Bytes* in)
{
    in->ordinary = true;
    in->finite = true;
    mtwnSpring spring;
    mtwnResult r = mtwn_errorInvalid;
    switch (Below(in, 3))
    {
    case 0:
    {
        double p[3];
        Draw(in, p, 3, (const double[]){1000.0, 60.0, 4.0}, true);
        r = mtwnMakeSpring(p[0], p[1], p[2], &spring);
    }
    break;
    case 1:
    {
        double p[2];
        Draw(in, p, 2, (const double[]){20.0, 2.0}, true);
        r = mtwnMakeSpringFromFrequency(p[0], p[1], &spring);
    }
    break;
    default:
    {
        double p[2];
        Draw(in, p, 1, (const double[]){3.0}, true);
        Draw(in, p + 1, 1, (const double[]){1.0}, false);
        r = mtwnMakeSpringFromDuration(p[0], p[1], &spring);
    }
    break;
    }
    if (r != mtwn_success)
    {
        return;
    }
    double offset = Param(in, 100.0);
    double velocity = Param(in, 1000.0);
    double seconds[] = {0.0, 0.016, 0.5, 3.0, 60.0};
    // A start not finite is refused, its outputs left as they were.
    bool start = isfinite(offset) && isfinite(velocity);
    for (int i = 0; i < 5; ++i)
    {
        mtwnSpringMotion m = {0.0, 0.0, 0.0, 0.0};
        if (mtwnComputeSpringMotion(&spring, seconds[i], &m) != mtwn_success)
        {
            Fail("a made spring's motion refused", seconds[i]);
        }
        CheckEach(in, "a spring's motion",
                  (const double[]){m.offsetFromOffset, m.offsetFromVelocity, m.velocityFromOffset,
                                   m.velocityFromVelocity},
                  4);
        double o = 0.0;
        double v = 0.0;
        bool sampled =
            mtwnSampleSpring(&spring, offset, velocity, seconds[i], &o, &v) == mtwn_success;
        if (sampled != start || (!sampled && (o != 0.0 || v != 0.0)))
        {
            Fail("a spring's sample refused or taken", offset);
        }
        Check(in, "a spring's offset", o);
        Check(in, "a spring's velocity", v);
    }
    double tolerances[2];
    Draw(in, tolerances, 2, (const double[]){1.0, 1.0}, true);
    double settle = 0.0;
    bool settled = mtwnComputeSpringSettlingTime(&spring, offset, velocity, tolerances[0],
                                                 tolerances[1], &settle) == mtwn_success;
    if (settled != (start && tolerances[0] > 0.0 && tolerances[1] > 0.0) || isnan(settle) ||
        settle < 0.0)
    {
        Fail("a spring's settling time", settle);
    }
}

static void Decays(Bytes* in)
{
    in->ordinary = true;
    in->finite = true;
    mtwnDecay decay;
    if (mtwnMakeDecay(Positive(in, 20.0), &decay) != mtwn_success)
    {
        return;
    }
    double position = Param(in, 1000.0);
    double velocity = Param(in, 5000.0);
    double seconds[] = {0.0, 0.016, 1.0, 100.0};
    bool startFinite = isfinite(position) && isfinite(velocity);
    for (int i = 0; i < 4; ++i)
    {
        double p = 0.0;
        double v = 0.0;
        bool sampled =
            mtwnSampleDecay(&decay, position, velocity, seconds[i], &p, &v) == mtwn_success;
        if (sampled != startFinite || (!sampled && (p != 0.0 || v != 0.0)))
        {
            Fail("a decay's sample refused or taken", position);
        }
        Check(in, "a decay's position", p);
        Check(in, "a decay's velocity", v);
    }
    double stopSpeed = Param(in, 10.0);
    double target = Param(in, 1000.0);
    double stop = 0.0;
    double cross = 0.0;
    bool stopped = mtwnComputeDecayStopTime(&decay, velocity, stopSpeed, &stop) == mtwn_success;
    bool crossed =
        mtwnComputeDecayCrossingTime(&decay, position, velocity, target, &cross) == mtwn_success;
    if (stopped != (isfinite(velocity) && stopSpeed > 0.0) ||
        crossed != (startFinite && isfinite(target)) || isnan(stop) || isnan(cross) || stop < 0.0 ||
        cross < 0.0)
    {
        Fail("a decay's time", isnan(stop) || stop < 0.0 ? stop : cross);
    }
    in->ordinary = true;
    in->finite = true;
    double distance = Param(in, 1e4);
    double dimension = Param(in, 2000.0);
    double coefficient = Param(in, 1.0);
    // Refused exactly where its arguments are outside what it takes.
    bool bands = dimension > 0.0 && coefficient > 0.0 && !isnan(distance);
    double band = 0.0;
    if ((mtwnApplyRubberBand(distance, dimension, coefficient, &band) == mtwn_success) != bands)
    {
        Fail("the rubber band's refusal", distance);
    }
    if (bands && (isnan(band) || (in->ordinary && isinf(band)) ||
                  (band != 0.0 && (band > 0.0) != (distance > 0.0))))
    {
        Fail("the rubber band", band);
    }
    double back = 0.0;
    if (bands &&
        (mtwnInvertRubberBand(band, dimension, coefficient, &back) != mtwn_success || isnan(back)))
    {
        Fail("the rubber band's inverse", back);
    }
    // Inertia: a landing's rate, and a bounded decay
    // with its bounce.
    in->ordinary = true;
    in->finite = true;
    double start[3];
    Draw(in, start, 3, (const double[]){1000.0, 5000.0, 1000.0}, false);
    mtwnDecay landing;
    if (mtwnMakeDecayToRest(start[0], start[1], start[2], &landing) == mtwn_success &&
        !(landing.rate > 0.0 && isfinite(landing.rate)))
    {
        Fail("a landing's rate", landing.rate);
    }
    double bounds[4];
    Draw(in, bounds, 4, (const double[]){1000.0, 1000.0, 1000.0, 50.0}, false);
    mtwnSpring bounce;
    mtwnBoundedDecay bounded;
    if (mtwnMakeSpring(fabs(bounds[2]), fabs(bounds[3]), 1.0, &bounce) != mtwn_success ||
        mtwnMakeBoundedDecay(&decay, &bounce, fmin(bounds[0], bounds[1]),
                             fmax(bounds[0], bounds[1]), &bounded) != mtwn_success)
    {
        return;
    }
    bool finite = isfinite(start[0]) && isfinite(start[1]);
    for (int i = 0; i < 4; ++i)
    {
        double p = 0.0;
        double v = 0.0;
        bool sampled = mtwnSampleBoundedDecay(&bounded, start[0], start[1], seconds[i], &p, &v) ==
                       mtwn_success;
        if (sampled != finite || (!sampled && (p != 0.0 || v != 0.0)))
        {
            Fail("a bounded decay's sample refused or taken", start[0]);
        }
        Check(in, "a bounded decay's position", p);
        Check(in, "a bounded decay's velocity", v);
    }
    double still = 0.0;
    bool stilled = mtwnComputeBoundedDecayStopTime(&bounded, start[0], start[1], 0.5, 1.0,
                                                   &still) == mtwn_success;
    if (stilled != finite || isnan(still) || still < 0.0)
    {
        Fail("a bounded decay's stop", still);
    }
}

// A made punch or shake with one field a host wrote, from a generator
// seeded by the input so far, so it consumes no more: each call refuses
// it, or gives a factor from -1 to 1; never an undefined cast (UBSan),
// whatever the frequency times the time.
static void ForgedSignals(const Bytes* in, const mtwnPunch* punch, const mtwnShake* shake)
{
    static const double s_wild[] = {0.0,       -0.0,   -1.0,   (double)NAN, (double)INFINITY,
                                    0x1p-1074, 0x1p53, 0x1p64, 0x1p100,     DBL_MAX,
                                    1e-300,    0.5};
    uint64_t state = 0x2545F4914F6CDD1Du;
    for (size_t i = 0; i < in->at; ++i)
    {
        state = (state ^ in->data[i]) * 0x100000001B3u;
    }
    static const double s_times[] = {-1.0, 0.0, 1e-3, 0.5, 2.0, 1e30};
    for (int round = 0; round < 3; ++round)
    {
        state = state * 6364136223846793005u + 1442695040888963407u;
        uint32_t field = (uint32_t)((state >> 33) % 3);
        double wild = s_wild[(state >> 40) % (sizeof s_wild / sizeof s_wild[0])];
        mtwnPunch p = punch != NULL ? *punch : (mtwnPunch){1.0, 1.0, 1.0};
        mtwnShake k = shake != NULL ? *shake : (mtwnShake){1.0, 1.0, 1.0, 0};
        *(field == 0 ? &p.frequency : field == 1 ? &p.duration : &p.exponent) = wild;
        *(field == 0 ? &k.frequency : field == 1 ? &k.duration : &k.exponent) = wild;
        for (size_t t = 0; t < sizeof s_times / sizeof s_times[0]; ++t)
        {
            double f = 0.0;
            if (punch != NULL && mtwnSamplePunch(&p, s_times[t] * p.duration, &f) == mtwn_success &&
                !(fabs(f) <= 1.0))
            {
                Fail("a forged punch taken and out of range", f);
            }
            double seconds = s_times[t] * k.duration;
            if (shake != NULL &&
                ((mtwnSampleShake(&k, 1, seconds, &f) == mtwn_success && !(fabs(f) <= 1.0)) ||
                 (mtwnSampleShakeNoise(&k, 1, seconds, &f) == mtwn_success && !(fabs(f) <= 1.0))))
            {
                Fail("a forged shake taken and out of range", f);
            }
        }
    }
}

// The next of a generator's doubles: a sign, 53 bits and a power of two
// within 2^20 of 1 or anywhere from 2^-1074 to 2^1023.
static double Drawn(uint64_t* state)
{
    *state = *state * 6364136223846793005u + 1442695040888963407u;
    uint64_t bits = *state;
    *state = *state * 6364136223846793005u + 1442695040888963407u;
    uint64_t spread = *state >> 32;
    int exponent = spread % 2 == 0 ? (int)(spread % 41) - 20 : (int)(spread % 2098) - 1074;
    double value = ldexp((double)(bits >> 11) * 0x1p-53, exponent);
    return (bits & 1u) != 0 ? -value : value;
}

// A generator's state seeded by the input so far, so that what it draws
// takes no bytes and the seeds keep their paths.
static uint64_t Seeded(const Bytes* in, uint64_t seed)
{
    for (size_t i = 0; i < in->at; ++i)
    {
        seed = (seed ^ in->data[i]) * 0x100000001B3u;
    }
    return seed;
}

static uint64_t Next(uint64_t* state)
{
    *state = *state * 6364136223846793005u + 1442695040888963407u;
    return *state >> 16;
}

// The values a forged mix's field takes: refusals' edges, a mix's own
// bounds (pi, two turns, 700, a squared length an ulp past 1) and wild
// ones.
static const double s_wildMix[] = {0.0,
                                   -0.0,
                                   -1.0,
                                   (double)NAN,
                                   (double)INFINITY,
                                   -(double)INFINITY,
                                   0x1p-1074,
                                   1e-300,
                                   0.5,
                                   1.0,
                                   1.0 + 0x1p-50,
                                   3.141592653589793,
                                   3.1415926535897936,
                                   12.566370614359172,
                                   700.0,
                                   -700.0,
                                   0x1p53,
                                   0x1p100,
                                   1e300,
                                   DBL_MAX,
                                   -DBL_MAX};

// One of a mix's fields written by a host: a double among its first ones,
// a float among those after them, or a byte after those.
static void Overwrite(void* mix, size_t doubles, size_t floats, size_t bytes, uint64_t* state)
{
    unsigned char* at = (unsigned char*)mix;
    double wild = s_wildMix[Next(state) % (sizeof s_wildMix / sizeof s_wildMix[0])];
    size_t field = (size_t)(Next(state) % (doubles + floats + bytes));
    if (field < doubles)
    {
        memcpy(at + field * sizeof(double), &wild, sizeof wild);
    }
    else if (field < doubles + floats)
    {
        float narrow = (float)fmax(fmin(wild, (double)FLT_MAX), -(double)FLT_MAX);
        narrow = isnan(wild) ? (float)NAN : narrow;
        memcpy(at + doubles * sizeof(double) + (field - doubles) * sizeof(float), &narrow,
               sizeof narrow);
    }
    else
    {
        at[doubles * sizeof(double) + floats * sizeof(float) + (field - doubles - floats)] =
            (unsigned char)Next(state);
    }
}

// The progresses a forged mix is sampled at.
static double ForgedProgress(int i, uint64_t* state)
{
    if (i < 4)
    {
        return s_farProgress[i];
    }
    if (i < 4 + (int)(sizeof s_progress / sizeof s_progress[0]))
    {
        return s_progress[i - 4];
    }
    return Drawn(state);
}

static bool Finite4(double a, double b, double c, double d)
{
    return isfinite(a) && isfinite(b) && isfinite(c) && isfinite(d);
}

// A made color or rotation mix with fields a host wrote, three rounds of
// one each: refused, or as the check promises, finite.
static void ForgedTurns(const Bytes* in, const mtwnColorMix* colors,
                        const mtwnRotationMix* rotations)
{
    uint64_t state = Seeded(in, 0x6A09E667F3BCC909u);
    int count = 4 + (int)(sizeof s_progress / sizeof s_progress[0]) + 4;
    for (int round = 0; round < 3; ++round)
    {
        mtwnColorMix c = colors != NULL ? *colors : (mtwnColorMix){0};
        mtwnRotationMix r = rotations != NULL ? *rotations : (mtwnRotationMix){0};
        Overwrite(&c, 8, 8, 8, &state);
        Overwrite(&r, 9, 8, 0, &state);
        for (int i = 0; i < count; ++i)
        {
            double progress = ForgedProgress(i, &state);
            mtwnColor color = {0};
            if (colors != NULL && mtwnMixColor(&c, progress, &color) == mtwn_success &&
                !Finite4((double)color.red, (double)color.green, (double)color.blue,
                         (double)color.alpha))
            {
                Fail("a forged color mix taken and not finite", progress);
            }
            mtwnQuat q = {0};
            if (rotations != NULL && mtwnMixRotation(&r, progress, &q) == mtwn_success &&
                !Finite4((double)q.x, (double)q.y, (double)q.z, (double)q.w))
            {
                Fail("a forged rotation mix taken and not finite", progress);
            }
        }
    }
}

// Scales and zooms a maker gives, from a generator seeded by the input so
// far: no NaN at any progress, out to the largest double either way; then
// each with fields a host wrote, refused or with no NaN either.
static void Zooms(const Bytes* in)
{
    uint64_t state = Seeded(in, 0x9E3779B97F4A7C15u);
    int count = 4 + (int)(sizeof s_progress / sizeof s_progress[0]) + 4;
    for (int round = 0; round < 4; ++round)
    {
        mtwnZoomView from = {Drawn(&state), Drawn(&state), fabs(Drawn(&state))};
        mtwnZoomView to = {Drawn(&state), Drawn(&state), fabs(Drawn(&state))};
        double rho = fabs(Drawn(&state));
        // A pan along one axis, or none.
        to.y = round == 1 ? from.y : to.y;
        to.x = round == 2 ? from.x : to.x;
        mtwnScaleMix scale;
        if (mtwnMakeScaleMix(from.width, to.width, &scale) == mtwn_success)
        {
            for (int forged = 0; forged < 4; ++forged)
            {
                mtwnScaleMix s = scale;
                if (forged > 0)
                {
                    Overwrite(&s, 4, 0, 0, &state);
                }
                for (int i = 0; i < count; ++i)
                {
                    double progress = ForgedProgress(i, &state);
                    double v = 0.0;
                    mtwnResult r = mtwnMixScale(&s, progress, &v);
                    if ((forged == 0 && r != mtwn_success) || (r == mtwn_success && isnan(v)))
                    {
                        Fail("a scale refused or NaN", progress);
                    }
                }
            }
        }
        mtwnZoomMix zoom;
        if (mtwnMakeZoomMix(from, to, rho, &zoom) != mtwn_success)
        {
            continue;
        }
        for (int forged = 0; forged < 4; ++forged)
        {
            mtwnZoomMix z = zoom;
            if (forged > 0)
            {
                Overwrite(&z, 15, 0, 0, &state);
            }
            double length = 0.0;
            mtwnResult measured = mtwnComputeZoomLength(&z, &length);
            if ((forged == 0 && measured != mtwn_success) ||
                (measured == mtwn_success && isnan(length)))
            {
                Fail("a zoom's length refused or NaN", length);
            }
            for (int i = 0; i < count; ++i)
            {
                double progress = ForgedProgress(i, &state);
                mtwnZoomView v = {0};
                mtwnResult r = mtwnMixZoom(&z, progress, &v);
                if ((forged == 0 && r != mtwn_success) ||
                    (r == mtwn_success && (isnan(v.x) || isnan(v.y) || isnan(v.width))))
                {
                    Fail("a zoom's view refused or NaN", progress);
                }
            }
        }
    }
}

static void Values(Bytes* in)
{
    in->ordinary = true;
    in->finite = true;
    mtwnVec4 a = Vec(in, 1.5, 1.5, 1.2);
    mtwnVec4 b = Vec(in, 1.5, 1.5, 1.2);
    mtwnColor from = {a.x, a.y, a.z, a.w};
    mtwnColor to = {b.x, b.y, b.z, b.w};
    mtwnColorSpace space = (mtwnColorSpace)Below(in, 5);
    mtwnColorMix colors;
    bool coloured =
        mtwnMakeColorMix(from, to, space, (mtwnHueMethod)Below(in, 5), &colors) == mtwn_success;
    if (coloured)
    {
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            mtwnColor c = {0};
            if (mtwnMixColor(&colors, s_progress[i], &c) != mtwn_success)
            {
                Fail("a color mix refused", s_progress[i]);
            }
            CheckEach(
                in, "a color",
                (const double[]){(double)c.red, (double)c.green, (double)c.blue, (double)c.alpha},
                4);
        }
    }
    in->ordinary = true;
    in->finite = true;
    mtwnVec4 c = Vec(in, 1.0, 1.0, 1.0);
    mtwnVec4 d = Vec(in, 1.0, 1.0, 1.0);
    mtwnQuat qa = {c.x, c.y, c.z, c.w};
    mtwnQuat qb = {d.x, d.y, d.z, d.w};
    mtwnRotationMix rotations;
    bool turned =
        mtwnMakeRotationMix(qa, qb, (mtwnRotationPath)Below(in, 3), &rotations) == mtwn_success;
    if (turned)
    {
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            mtwnQuat q = {0};
            if (mtwnMixRotation(&rotations, s_progress[i], &q) != mtwn_success)
            {
                Fail("a rotation mix refused", s_progress[i]);
            }
            double norm = sqrt((double)q.x * (double)q.x + (double)q.y * (double)q.y +
                               (double)q.z * (double)q.z + (double)q.w * (double)q.w);
            if (!(fabs(norm - 1.0) < 1e-5))
            {
                Fail("a rotation's length", norm);
            }
        }
    }
    ForgedTurns(in, coloured ? &colors : NULL, turned ? &rotations : NULL);
    in->ordinary = true;
    in->finite = true;
    double angles[3];
    Draw(in, angles, 3, (const double[]){720.0, 720.0, 2.0}, false);
    // Every method and one past them, so an unknown one is refused.
    mtwnAngleMethod method = (mtwnAngleMethod)Below(in, 6);
    double angle = 0.0;
    mtwnResult angled = mtwnMixAngle(angles[0], angles[1], method, angles[2], &angle);
    bool takes = method <= mtwn_angleDecreasing && isfinite(angles[0]) && isfinite(angles[1]) &&
                 isfinite(angles[2]);
    if ((angled == mtwn_success) != takes)
    {
        Fail("an angle's refusal", (double)angled);
    }
    // A linear mix of finite angles and progress near 1e300 overflows to
    // opposite infinities, as the arithmetic must: checked when ordinary.
    if (angled == mtwn_success && in->ordinary && !isfinite(angle))
    {
        Fail("an angle", angle);
    }
    in->ordinary = true;
    in->finite = true;
    mtwnPunch punch;
    double punchParams[3];
    Draw(in, punchParams, 3, (const double[]){20.0, 2.0, 4.0}, true);
    double duration = punchParams[1];
    if (mtwnMakePunch(punchParams[0], duration, punchParams[2], &punch) == mtwn_success)
    {
        double times[] = {-1.0, 0.0, 0.25 * duration, 0.5 * duration, duration, duration + 1.0};
        for (int i = 0; i < 6; ++i)
        {
            double f = 0.0;
            bool rests = i == 0 || i == 1 || i >= 4;
            if (mtwnSamplePunch(&punch, times[i], &f) != mtwn_success || isnan(f) ||
                fabs(f) > 1.0 || (rests && f != 0.0))
            {
                Fail("a punch", f);
            }
        }
        ForgedSignals(in, &punch, NULL);
    }
    in->ordinary = true;
    in->finite = true;
    mtwnShake shake;
    double shakeParams[3];
    Draw(in, shakeParams, 3, (const double[]){30.0, 2.0, 4.0}, true);
    if (mtwnMakeShake(shakeParams[0], shakeParams[1], shakeParams[2], Byte(in), &shake) ==
        mtwn_success)
    {
        for (uint32_t channel = 0; channel < 4; ++channel)
        {
            double seconds = Param(in, 3.0);
            double f = 0.0;
            mtwnResult sampled = mtwnSampleShake(&shake, channel, seconds, &f);
            if ((sampled == mtwn_success) == isnan(seconds) ||
                (sampled == mtwn_success && (isnan(f) || fabs(f) > 1.0)))
            {
                Fail("a shake", f);
            }
        }
        ForgedSignals(in, NULL, &shake);
    }
    Zooms(in);
}

// A monotone segment's value lies between its keyframes' in every
// channel.
static void CheckMonotone(const Bytes* in, const mtwnKeyframe* keys, uint32_t count,
                          uint8_t channels, double progress, const double value[4])
{
    if (!in->ordinary || count < 2 || progress < 0.0 || progress > 1.0)
    {
        return;
    }
    uint32_t i = 0;
    while (i + 2 < count && keys[i + 1].offset <= progress)
    {
        ++i;
    }
    for (int c = 0; c < channels && keys[i].kind == mtwn_segmentMonotone; ++c)
    {
        double a = (double)(&keys[i].value.x)[c];
        double b = (double)(&keys[i + 1].value.x)[c];
        double slack = 8.0 * DBL_EPSILON * fmax(fabs(a), fabs(b));
        if (value[c] < fmin(a, b) - slack || value[c] > fmax(a, b) + slack)
        {
            Fail("a monotone segment's value", value[c]);
        }
    }
}

// A path and a track as a host that changed their arrays after making
// them leaves them: rounds of bytes written over the keyframes and the
// lengths and the keyframes' linear() points, from a generator seeded by
// the section's own input so it consumes no more, the structs (their
// count, channels and pointers) kept, and so what a host gives an easing
// to reference (its function, its context, linear()'s points and their
// count), which no call can check. Each sample,
// direction and angle then refuses or gives some value; neither reads
// past the arrays nor is undefined, which the sanitizers check.
static void Tampered(const Bytes* in, size_t from, mtwnKeyframe* keys, uint32_t count,
                     double* lengths, const mtwnPath* path, double* trackLengths,
                     const mtwnTrack* track, mtwnLinearPoint* points, size_t pointBytes)
{
    uint64_t state = 0x9E3779B97F4A7C15u;
    for (size_t i = from; i < in->at; ++i)
    {
        state = (state ^ in->data[i]) * 0x100000001B3u;
    }
    for (int round = 0; round < 4; ++round)
    {
        for (int write = 0; write < 12; ++write)
        {
            state = state * 6364136223846793005u + 1442695040888963407u;
            uint64_t bits = state >> 11;
            uint32_t where = (uint32_t)(bits % 4);
            uint8_t byte = (uint8_t)(bits >> 8);
            if (where == 3)
            {
                ((uint8_t*)points)[(bits >> 16) % pointBytes] = byte;
            }
            else if (where == 0 || (where == 1 && path == NULL) || (where == 2 && track == NULL))
            {
                size_t at = (bits >> 16) % (count * sizeof *keys);
                size_t within = at % sizeof *keys;
                size_t pointers = offsetof(mtwnKeyframe, easing) + offsetof(mtwnEasing, function);
                size_t past =
                    offsetof(mtwnKeyframe, easing) + offsetof(mtwnEasing, count) + sizeof(uint32_t);
                if (within < pointers || within >= past)
                {
                    ((uint8_t*)keys)[at] = byte;
                }
            }
            else
            {
                double* tampered = where == 1 ? lengths : trackLengths;
                ((uint8_t*)tampered)[(bits >> 16) % (count * sizeof *tampered)] = byte;
            }
        }
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            double value[4];
            double tangent[4];
            double angle = 0.0;
            mtwnVec4 v = {0};
            if (path != NULL)
            {
                (void)mtwnSamplePath(path, s_progress[i], value, tangent);
                (void)mtwnComputePathDirection(path, s_progress[i], value);
                (void)mtwnComputePathAngle(path, s_progress[i], &angle);
            }
            if (track != NULL)
            {
                (void)mtwnSampleTrack(track, s_progress[i], &v);
            }
        }
    }
}

static void Paths(Bytes* in)
{
    size_t from = in->at;
    in->ordinary = true;
    in->finite = true;
    mtwnKeyframe keys[6];
    memset(keys, 0, sizeof keys);
    uint32_t count = Below(in, 7);
    for (uint32_t k = 0; k < count; ++k)
    {
        keys[k].offset = k == 0 ? 0.0 : (k + 1 == count ? 1.0 : Param(in, 1.0));
        keys[k].value = Vec(in, 100.0, 100.0, 1.0);
        keys[k].in = Vec(in, 100.0, 0.0, 0.0);
        keys[k].out = Vec(in, 100.0, 0.0, 0.0);
        keys[k].easing = mtwnLinearEasing();
        keys[k].kind = (mtwnSegmentKind)Below(in, 5);
    }
    // Every other keyframe eased by linear() over points of its own, set
    // without drawing, so the tampering changes points a sample reads.
    mtwnLinearPoint points[6][4];
    for (uint32_t k = 1; k < count; k += 2)
    {
        const mtwnLinearPoint shape[4] = {{0.0, 0.0}, {0.25, 0.4}, {0.75, 0.7}, {1.0, 1.0}};
        memcpy(points[k], shape, sizeof shape);
        if (mtwnMakeLinearEasing(points[k], 4, &keys[k].easing) != mtwn_success)
        {
            Fail("a linear() easing", 0.0);
        }
    }
    double lengths[6];
    mtwnPath path;
    bool made = false;
    uint8_t channels = (uint8_t)(1 + Below(in, 4));
    bool even = Below(in, 2) == 0;
    if (mtwnMakePath(keys, count, channels, Param(in, 1.0), even ? lengths : NULL, &path) ==
        mtwn_success)
    {
        made = true;
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            double value[4];
            double tangent[4];
            if (mtwnSamplePath(&path, s_progress[i], value, tangent) != mtwn_success)
            {
                Fail("a path's sample refused", s_progress[i]);
                continue;
            }
            CheckEach(in, "a path's value", value, 4);
            CheckEach(in, "a path's tangent", tangent, 4);
            if (!even)
            {
                CheckMonotone(in, keys, count, channels, s_progress[i], value);
            }
            // Its direction is a unit vector wherever the path is finite.
            double direction[4] = {0};
            if (mtwnComputePathDirection(&path, s_progress[i], direction) != mtwn_success)
            {
                Fail("a path's direction refused", s_progress[i]);
            }
            double norm = 0.0;
            for (int c = 0; c < 4; ++c)
            {
                norm += direction[c] * direction[c];
            }
            if (in->ordinary && !(fabs(norm - 1.0) <= 1e-12))
            {
                Fail("a path's direction", norm);
            }
        }
    }
    mtwnTrackDef def = mtwnDefaultTrackDef((mtwnValueKind)Below(in, 11));
    def.alpha = Param(in, 1.0);
    def.colorSpace = (mtwnColorSpace)Below(in, 4);
    def.hueMethod = (mtwnHueMethod)Below(in, 4);
    def.angleMethod = (mtwnAngleMethod)Below(in, 4);
    def.rotationPath = (mtwnRotationPath)Below(in, 2);
    if (def.kind == mtwn_valueRotation)
    {
        for (uint32_t k = 0; k < count; ++k)
        {
            keys[k].value = (mtwnVec4){0.0f, (float)Param(in, 1.0), 0.0f, 1.0f};
        }
    }
    double trackLengths[6];
    mtwnSegmentMix mixes[6];
    mtwnTrack track;
    bool tracked = false;
    if (mtwnMakeTrack(&def, keys, count, Below(in, 2) == 0 ? trackLengths : NULL, mixes, &track) ==
        mtwn_success)
    {
        tracked = true;
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            mtwnVec4 v = {0};
            if (mtwnSampleTrack(&track, s_progress[i], &v) != mtwn_success)
            {
                Fail("a track's sample refused", s_progress[i]);
                continue;
            }
            CheckEach(in, "a track's value",
                      (const double[]){(double)v.x, (double)v.y, (double)v.z, (double)v.w}, 4);
        }
    }
    if (made || tracked)
    {
        Tampered(in, from, keys, count, lengths, made ? &path : NULL, trackLengths,
                 tracked ? &track : NULL, &points[0][0], sizeof points);
    }
}

static void Staggers(Bytes* in)
{
    in->ordinary = true;
    in->finite = true;
    mtwnStagger stagger = mtwnDefaultStagger();
    stagger.start = Param(in, 2.0);
    stagger.spacing = Param(in, 0.5);
    stagger.pointX = Param(in, 8.0);
    stagger.pointY = Param(in, 8.0);
    stagger.origin = (mtwnStaggerOrigin)Below(in, 7);
    stagger.index = Below(in, 40);
    stagger.columns = Below(in, 8);
    stagger.axis = (mtwnStaggerAxis)Below(in, 4);
    stagger.spacingKind = (mtwnStaggerSpacing)Below(in, 3);
    stagger.seed = Byte(in);
    if (Below(in, 3) == 0)
    {
        (void)mtwnMakePowerEasing((mtwnEaseMode)Below(in, 4), 2.0, &stagger.easing);
    }
    double offsets[40];
    uint32_t count = Below(in, 41);
    if (mtwnComputeStagger(&stagger, count, offsets) == mtwn_success)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            Check(in, "a stagger's offset", offsets[i]);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Bytes in = {data, size, 0, true, true};
    s_bad = false;
#ifdef MTWN_FUZZ_FIT
    // The other sections are fuzz_values'.
    (void)Springs;
    (void)Decays;
    (void)Values;
    (void)Paths;
    (void)Staggers;
    // One input in sixteen may run one fit to its limit.
    bool deep = Below(&in, 16) == 0;
    while (in.at < in.size)
    {
        Easings(&in, true, &deep);
    }
#else
    while (in.at < in.size)
    {
        switch (Below(&in, 6))
        {
        case 0:
            Easings(&in, false, NULL);
            break;
        case 1:
            Springs(&in);
            break;
        case 2:
            Decays(&in);
            break;
        case 3:
            Values(&in);
            break;
        case 4:
            Paths(&in);
            break;
        default:
            Staggers(&in);
            break;
        }
    }
#endif
    if (s_bad)
    {
        __builtin_trap();
    }
    return 0;
}

#ifndef MTWN_FUZZING
// The random inputs after the seeds: a fit's, at its limit, costs a
// thousand times the rest, so the fit's replay takes 10 of them and its
// dense test (`test_fuzz_fit dense`) 50.
#ifdef MTWN_FUZZ_FIT
#include "generated/fuzz_seeds_fit.h"
enum
{
    s_randomInputs = 10,
    s_denseInputs = 50
};
#else
#include "generated/fuzz_seeds_values.h"
enum
{
    s_randomInputs = 2000,
    s_denseInputs = 2000
};
#endif

// In the test suite: the committed seeds (tools/fuzz_seeds.py), one per
// edge a campaign reached, then a fixed set of random inputs
// (xorshift64).
int main(int argc, char** argv)
{
    bool dense = argc == 2 && strcmp(argv[1], "dense") == 0;
    if (argc > 2 || (argc == 2 && !dense))
    {
        printf("usage: %s [dense]\n", argv[0]);
        return 2;
    }
    int inputs = dense ? s_denseInputs : s_randomInputs;
    uint32_t begin = 0;
    for (size_t k = 0; k < sizeof s_seedEnds / sizeof s_seedEnds[0]; ++k)
    {
        (void)LLVMFuzzerTestOneInput(s_seedBytes + begin, s_seedEnds[k] - begin);
        begin = s_seedEnds[k];
    }
    uint64_t state = 0x2545F4914F6CDD1Du;
    static uint8_t input[512];
    for (int n = 0; n < inputs; ++n)
    {
        size_t size = 16 + (size_t)(n * 29 % 480);
        for (size_t i = 0; i < size; ++i)
        {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            input[i] = (uint8_t)(state >> 56);
        }
        (void)LLVMFuzzerTestOneInput(input, size);
    }
    return 0;
}
#endif
