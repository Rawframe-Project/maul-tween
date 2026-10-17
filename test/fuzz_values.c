// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The values layer driven by bytes (record mtwn-0046): easings of every
// kind, springs, spring easings, decay, the rubber band, colors,
// rotations, paths, tracks, punch, shake and staggers, made from
// parameters ordinary or wild (zero, negative, tiny, huge, infinite,
// NaN). What is made, sampled at finite inputs, is never NaN, and never
// infinite when every parameter was ordinary; easings meet their ends
// where the API says they do, rotations stay unit, a punch rests outside
// its span, and a stagger that succeeds writes finite offsets.
//
// Built with MAUL_TWEEN_FUZZ (Clang) it is a libFuzzer target over a
// corpus directory; in the test suite it runs a fixed set of random
// inputs.

#include "maul-tween/easing.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
#include "maul-tween/sample.h"
#include "maul-tween/spring.h"
#include "maul-tween/timing.h"

#include <math.h>
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

static void Easings(Bytes* in)
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
        Check(in, "an easing's value", mtwnEase(&e, s_progress[i], false));
        Check(in, "an easing's value before", mtwnEase(&e, s_progress[i], true));
    }
    if (ends && in->ordinary &&
        (mtwnEase(&e, 0.0, false) != 0.0 || mtwnEase(&e, 1.0, false) != 1.0))
    {
        Fail("an easing's ends", mtwnEase(&e, 1.0, false));
    }
    float samples[16];
    if (mtwnSampleEasing(&e, 16, samples) == mtwn_success)
    {
        for (int i = 0; i < 16; ++i)
        {
            Check(in, "a sampled easing", (double)samples[i]);
        }
    }
    mtwnLinearPoint fitted[64];
    uint32_t fittedCount = 0;
    if (mtwnFitLinearEasing(&e, Positive(in, 0.05), fitted, 64, &fittedCount) == mtwn_success)
    {
        mtwnEasing linear;
        if (mtwnMakeLinearEasing(fitted, fittedCount, &linear) != mtwn_success)
        {
            Fail("a fitted linear() refused", (double)fittedCount);
            return;
        }
        Check(in, "a fitted linear()'s value", mtwnEase(&linear, 0.5, false));
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
    for (int i = 0; i < 5; ++i)
    {
        mtwnSpringMotion m = mtwnSpringMotionAt(&spring, seconds[i]);
        CheckEach(in, "a spring's motion",
                  (const double[]){m.offsetFromOffset, m.offsetFromVelocity, m.velocityFromOffset,
                                   m.velocityFromVelocity},
                  4);
        double o = 0.0;
        double v = 0.0;
        mtwnSampleSpring(&spring, offset, velocity, seconds[i], &o, &v);
        Check(in, "a spring's offset", o);
        Check(in, "a spring's velocity", v);
    }
    double tolerances[2];
    Draw(in, tolerances, 2, (const double[]){1.0, 1.0}, true);
    double settle = mtwnSpringSettlingTime(&spring, offset, velocity, tolerances[0], tolerances[1]);
    if ((in->finite && isnan(settle)) || settle < 0.0)
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
    for (int i = 0; i < 4; ++i)
    {
        double p = 0.0;
        double v = 0.0;
        mtwnSampleDecay(&decay, position, velocity, seconds[i], &p, &v);
        Check(in, "a decay's position", p);
        Check(in, "a decay's velocity", v);
    }
    double stop = mtwnDecayStopTime(&decay, velocity, Param(in, 10.0));
    double cross = mtwnDecayCrossingTime(&decay, position, velocity, Param(in, 1000.0));
    if ((in->finite && (isnan(stop) || isnan(cross))) || stop < 0.0 || cross < 0.0)
    {
        Fail("a decay's time", isnan(stop) || stop < 0.0 ? stop : cross);
    }
    in->ordinary = true;
    in->finite = true;
    double distance = Param(in, 1e4);
    double dimension = Param(in, 2000.0);
    double coefficient = Param(in, 1.0);
    double band = mtwnRubberBand(distance, dimension, coefficient);
    if (isnan(band) || (in->ordinary && isinf(band)) ||
        (band != 0.0 && (band > 0.0) != (distance > 0.0)))
    {
        Fail("the rubber band", band);
    }
    double back = mtwnRubberBandInverse(band, dimension, coefficient);
    if (isnan(back))
    {
        Fail("the rubber band's inverse", back);
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
    if (mtwnMakeColorMix(from, to, space, (mtwnHueMethod)Below(in, 5), &colors) == mtwn_success)
    {
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            mtwnColor c = mtwnMixColor(&colors, s_progress[i]);
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
    if (mtwnMakeRotationMix(qa, qb, (mtwnRotationPath)Below(in, 3), &rotations) == mtwn_success)
    {
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            mtwnQuat q = mtwnMixRotation(&rotations, s_progress[i]);
            double norm = sqrt((double)q.x * (double)q.x + (double)q.y * (double)q.y +
                               (double)q.z * (double)q.z + (double)q.w * (double)q.w);
            if (!(fabs(norm - 1.0) < 1e-5))
            {
                Fail("a rotation's length", norm);
            }
        }
    }
    in->ordinary = true;
    in->finite = true;
    double angles[3];
    Draw(in, angles, 3, (const double[]){720.0, 720.0, 2.0}, false);
    double angle = mtwnMixAngle(angles[0], angles[1], (mtwnAngleMethod)Below(in, 4), angles[2]);
    // A linear mix of finite angles and progress near 1e300 overflows to
    // opposite infinities, as the arithmetic must: checked when ordinary.
    if (in->ordinary && !isfinite(angle))
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
            double f = mtwnSamplePunch(&punch, times[i]);
            bool rests = i == 0 || i == 1 || i >= 4;
            if (isnan(f) || fabs(f) > 1.0 || (rests && f != 0.0))
            {
                Fail("a punch", f);
            }
        }
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
            double f = mtwnSampleShake(&shake, channel, Param(in, 3.0));
            if (isnan(f) || fabs(f) > 1.0)
            {
                Fail("a shake", f);
            }
        }
    }
}

static void Paths(Bytes* in)
{
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
        keys[k].kind = (mtwnSegmentKind)Below(in, 4);
    }
    double lengths[6];
    mtwnPath path;
    uint8_t channels = (uint8_t)(1 + Below(in, 4));
    bool even = Below(in, 2) == 0;
    if (mtwnMakePath(keys, count, channels, Param(in, 1.0), even ? lengths : NULL, &path) ==
        mtwn_success)
    {
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            double value[4];
            double tangent[4];
            mtwnSamplePath(&path, s_progress[i], value, tangent);
            CheckEach(in, "a path's value", value, 4);
            CheckEach(in, "a path's tangent", tangent, 4);
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
    if (mtwnMakeTrack(&def, keys, count, Below(in, 2) == 0 ? trackLengths : NULL, mixes, &track) ==
        mtwn_success)
    {
        for (size_t i = 0; i < sizeof s_progress / sizeof s_progress[0]; ++i)
        {
            mtwnVec4 v = mtwnSampleTrack(&track, s_progress[i]);
            CheckEach(in, "a track's value",
                      (const double[]){(double)v.x, (double)v.y, (double)v.z, (double)v.w}, 4);
        }
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
    while (in.at < in.size)
    {
        switch (Below(&in, 6))
        {
        case 0:
            Easings(&in);
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
    if (s_bad)
    {
        __builtin_trap();
    }
    return 0;
}

#ifndef MTWN_FUZZING
#include "generated/fuzz_seeds_values.h"

// In the test suite: the committed seeds (tools/fuzz_seeds.py), one per
// edge a campaign reached, then a fixed set of random inputs
// (xorshift64).
int main(void)
{
    uint32_t begin = 0;
    for (size_t k = 0; k < sizeof s_seedEnds / sizeof s_seedEnds[0]; ++k)
    {
        (void)LLVMFuzzerTestOneInput(s_seedBytes + begin, s_seedEnds[k] - begin);
        begin = s_seedEnds[k];
    }
    uint64_t state = 0x2545F4914F6CDD1Du;
    static uint8_t input[512];
    for (int n = 0; n < 2000; ++n)
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
