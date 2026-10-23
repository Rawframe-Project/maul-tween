// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Monotone segments: every channel within its two
// keyframes' values on every segment, at any spacing and through jumps;
// flat where a channel turns; the slope in time continuous across a
// keyframe and Steffen's there; the ends' slope of curvature 0; a line
// between two keyframes; scales in the logarithm, at constant speed too;
// colors refusing them.

#include "test_harness.h"

#include "maul-tween/path.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static mtwnKeyframe Key(double offset, float x, float y)
{
    mtwnKeyframe k;
    memset(&k, 0, sizeof k);
    k.offset = offset;
    k.value = (mtwnVec4){x, y, 0.0f, 0.0f};
    k.easing = mtwnLinearEasing();
    k.kind = mtwn_segmentMonotone;
    return k;
}

static uint64_t s_state = 0x9E3779B97F4A7C15u;

// A draw from 0 to 1 (SplitMix64).
static double Draw(void)
{
    uint64_t z = (s_state += 0x9E3779B97F4A7C15u);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    z ^= z >> 31;
    return (double)(z >> 11) * 0x1p-53;
}

static float Channel(const mtwnVec4* v, int c)
{
    return (&v->x)[c];
}

// Whether every channel of a sample within segment i lies between its
// keyframes' values, give or take the cubic's rounding.
static bool Within(const mtwnKeyframe* keys, uint32_t i, const double value[4], uint8_t channels)
{
    for (int c = 0; c < channels; ++c)
    {
        double a = (double)Channel(&keys[i].value, c);
        double b = (double)Channel(&keys[i + 1].value, c);
        double slack = 8.0 * DBL_EPSILON * fmax(fabs(a), fabs(b));
        if (value[c] < fmin(a, b) - slack || value[c] > fmax(a, b) + slack)
        {
            return false;
        }
    }
    return true;
}

static void TestWithin(void)
{
    enum
    {
        s_keys = 7
    };
    bool all = true;
    for (int round = 0; round < 2000 && all; ++round)
    {
        mtwnKeyframe keys[s_keys];
        double offset = 0.0;
        for (int i = 0; i < s_keys; ++i)
        {
            // Uneven widths, a jump now and then, values of any scale.
            double gap = Draw() < 0.1 ? 0.0 : pow(10.0, -4.0 * Draw());
            offset = i == 0 ? 0.0 : offset + gap;
            float scale = (float)pow(10.0, 6.0 * Draw() - 3.0);
            keys[i] = Key(offset, scale * (float)(2.0 * Draw() - 1.0),
                          scale * (float)(2.0 * Draw() - 1.0));
        }
        for (int i = 1; i < s_keys; ++i)
        {
            keys[i].offset = offset > 0.0 ? keys[i].offset / offset : 1.0;
        }
        keys[s_keys - 1].offset = 1.0;
        mtwnPath path;
        all = mtwnMakePath(keys, s_keys, 2, 0.5, NULL, &path) == mtwn_success;
        for (uint32_t i = 0; i + 1 < s_keys && all; ++i)
        {
            double width = keys[i + 1].offset - keys[i].offset;
            for (int j = 1; j < 64 && width > 0x1p-52; ++j)
            {
                double value[4];
                mtwnSamplePath(&path, keys[i].offset + width * j / 64.0, value, NULL);
                all = all && Within(keys, i, value, 2);
            }
        }
    }
    CHECK(all, "every channel between its keyframes, 2,000 random paths");
}

// The slope in time just before and at a progress, from the tangent in
// the segment's parameter over its width.
static double SlopeAt(const mtwnPath* path, double progress, double width)
{
    double value[4];
    double tangent[4];
    mtwnSamplePath(path, progress, value, tangent);
    return tangent[0] / width;
}

static void TestSlopes(void)
{
    // 0, 1, 2 at 0, 0.1 and 1: secants 10 and 10/9; Steffen's slope is
    // twice the lesser, 20/9, from either side.
    mtwnKeyframe keys[3] = {Key(0.0, 0.0f, 0.0f), Key(0.1, 1.0f, 0.0f), Key(1.0, 2.0f, 0.0f)};
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 3, 1, 0.5, NULL, &path) == mtwn_success, "uneven widths");
    double left = SlopeAt(&path, 0.1 - 1e-9, 0.1);
    double right = SlopeAt(&path, 0.1, 0.9);
    CHECK(fabs(left - 20.0 / 9.0) < 1e-6 && fabs(right - 20.0 / 9.0) < 1e-12,
          "the slope in time continuous, Steffen's");
    // Secants close enough that the parabola binds: 0, 1, 2 at 0, 0.4
    // and 1, secants 5/2 and 5/3, the slope the parabola's, weighted by
    // the widths: (5/2 0.6 + 5/3 0.4) / 1 = 13/6 (2.08 unweighted).
    mtwnKeyframe near[3] = {Key(0.0, 0.0f, 0.0f), Key(0.4, 1.0f, 0.0f), Key(1.0, 2.0f, 0.0f)};
    mtwnPath nearPath;
    CHECK(mtwnMakePath(near, 3, 1, 0.5, NULL, &nearPath) == mtwn_success, "near secants");
    CHECK(fabs(SlopeAt(&nearPath, 0.4, 0.6) - 13.0 / 6.0) < 1e-12, "the parabola's slope");
    // Past the second keyframe, offsets and values away from 0, the
    // widths and secants are differences too: 0, 1, 1.25, 3 at 0, 0.5,
    // 0.75 and 1, secants 1 and 7 about 0.75, its slope 2, twice the
    // lesser, the earlier.
    mtwnKeyframe later[4] = {Key(0.0, 0.0f, 0.0f), Key(0.5, 1.0f, 0.0f), Key(0.75, 1.25f, 0.0f),
                             Key(1.0, 3.0f, 0.0f)};
    mtwnPath laterPath;
    CHECK(mtwnMakePath(later, 4, 1, 0.5, NULL, &laterPath) == mtwn_success &&
              fabs(SlopeAt(&laterPath, 0.75, 0.25) - 2.0) < 1e-12,
          "Steffen's slope at a third keyframe");
    // The ends' slope (3 s - t) / 2: curvature 0 there.
    double start = SlopeAt(&path, 0.0, 0.1);
    double end = SlopeAt(&path, 1.0, 0.9);
    CHECK(fabs(start - 0.5 * (30.0 - 20.0 / 9.0)) < 1e-12 &&
              fabs(end - 0.5 * (30.0 / 9.0 - 20.0 / 9.0)) < 1e-12,
          "the ends one-sided");
    // A peak and a plateau: flat at the turn.
    mtwnKeyframe peak[4] = {Key(0.0, 0.0f, 0.0f), Key(0.3, 1.0f, 0.0f), Key(0.6, 1.0f, 0.0f),
                            Key(1.0, 0.0f, 0.0f)};
    CHECK(mtwnMakePath(peak, 4, 1, 0.5, NULL, &path) == mtwn_success, "a plateau");
    double value[4];
    mtwnSamplePath(&path, 0.45, value, NULL);
    CHECK(SlopeAt(&path, 0.3, 0.3) == 0.0 && SlopeAt(&path, 0.6, 0.4) == 0.0 && value[0] == 1.0,
          "flat at the turns, held between them");
    // A flat secant counts as rising, so where it meets a fall the slope
    // is +0, not -0.
    CHECK(!signbit(SlopeAt(&path, 0.6, 0.4)), "the plateau's end slope +0");
    // A width within rounding of none is a jump, not a secant: after
    // keyframes 1e-17 apart the next segment starts as an end, here as
    // its line (a secant of 1e17 would start it at 2).
    mtwnKeyframe jump[3] = {Key(0.0, 0.0f, 0.0f), Key(1e-17, 1.0f, 0.0f), Key(1.0, 2.0f, 0.0f)};
    CHECK(mtwnMakePath(jump, 3, 1, 0.5, NULL, &path) == mtwn_success, "a near jump");
    CHECK(fabs(SlopeAt(&path, 1e-17, 1.0 - 1e-17) - 1.0) < 1e-12, "measured as a jump");
    // At the bound itself, 2^-52, as a segment's easing takes it.
    jump[1].offset = 0x1p-52;
    CHECK(mtwnMakePath(jump, 3, 1, 0.5, NULL, &path) == mtwn_success &&
              fabs(SlopeAt(&path, 0x1p-52, 1.0 - 0x1p-52) - 1.0) < 1e-12,
          "a jump at the bound");
    // At constant speed segments go by length, so a jump's own segment is
    // travelled: its line, from 1 to 2.
    mtwnKeyframe jumped[4] = {Key(0.0, 0.0f, 0.0f), Key(0.5, 1.0f, 0.0f), Key(0.5, 2.0f, 0.0f),
                              Key(1.0, 3.0f, 0.0f)};
    double lengths[4];
    CHECK(mtwnMakePath(jumped, 4, 1, 0.5, lengths, &path) == mtwn_success, "a jump at speed");
    mtwnSamplePath(&path, 0.5 * (lengths[1] + lengths[2]) / lengths[3], value, NULL);
    CHECK(fabs(value[0] - 1.5) < 1e-9, "the jump travelled as its line");
    // Two keyframes: a line.
    mtwnKeyframe two[2] = {Key(0.0, 2.0f, -1.0f), Key(1.0, 4.0f, 3.0f)};
    CHECK(mtwnMakePath(two, 2, 2, 0.5, NULL, &path) == mtwn_success, "two keyframes");
    mtwnSamplePath(&path, 0.25, value, NULL);
    CHECK(value[0] == 2.5 && value[1] == 0.0, "a line");
}

static void TestTracks(void)
{
    // Scales in the logarithm: 1, 10, 100 rise by tenfold steps, never
    // past them.
    mtwnKeyframe scales[3] = {Key(0.0, 1.0f, 0.0f), Key(0.2, 10.0f, 0.0f), Key(1.0, 100.0f, 0.0f)};
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueScale);
    mtwnTrack track;
    CHECK(mtwnMakeTrack(&def, scales, 3, NULL, NULL, &track) == mtwn_success, "a scale track");
    bool within = true;
    float last = 0.0f;
    for (int j = 0; j <= 100; ++j)
    {
        float x = mtwnSampleTrack(&track, j / 100.0).x;
        within = within && x >= last && x >= 1.0f && x <= 100.0f;
        last = x;
    }
    CHECK(within && mtwnSampleTrack(&track, 0.2).x == 10.0f, "rising, within, exact at a key");
    double lengths[3];
    CHECK(mtwnMakeTrack(&def, scales, 3, lengths, NULL, &track) == mtwn_success &&
              mtwnSampleTrack(&track, 1.0).x == 100.0f,
          "at constant speed");
    // Colors mix per segment: refused, as Catmull-Rom is.
    mtwnKeyframe colors[2] = {Key(0.0, 0.0f, 0.0f), Key(1.0, 1.0f, 1.0f)};
    mtwnSegmentMix mixes[1];
    mtwnTrackDef color = mtwnDefaultTrackDef(mtwn_valueColor);
    CHECK(mtwnMakeTrack(&color, colors, 2, NULL, mixes, &track) == mtwn_errorInvalid,
          "a color track refuses it");
    // A kind past it is unknown.
    colors[0].kind = mtwn_segmentMonotone + 1;
    mtwnPath path;
    CHECK(mtwnMakePath(colors, 2, 2, 0.5, NULL, &path) == mtwn_errorInvalid, "an unknown kind");
}

// Steffen's slopes are per channel: a path of four channels is, channel
// by channel, the path of one through the same values, to the bit.
static void TestFourChannels(void)
{
    mtwnKeyframe keys[5];
    for (int i = 0; i < 5; ++i)
    {
        keys[i] = Key(0.25 * i, 0.0f, 0.0f);
        keys[i].value = (mtwnVec4){(float)(Draw() * 4.0 - 2.0), (float)(Draw() * 4.0 - 2.0),
                                   (float)(Draw() * 4.0 - 2.0), (float)(Draw() * 4.0 - 2.0)};
    }
    mtwnPath four;
    CHECK(mtwnMakePath(keys, 5, 4, 0.5, NULL, &four) == mtwn_success, "four channels");
    uint32_t differ = 0;
    for (int c = 0; c < 4; ++c)
    {
        mtwnKeyframe one[5];
        memcpy(one, keys, sizeof one);
        for (int i = 0; i < 5; ++i)
        {
            float v = (&keys[i].value.x)[c];
            one[i].value = (mtwnVec4){v, 0.0f, 0.0f, 0.0f};
        }
        mtwnPath path;
        CHECK(mtwnMakePath(one, 5, 1, 0.5, NULL, &path) == mtwn_success, "one channel");
        for (int k = -10; k <= 110; ++k)
        {
            double a[4];
            double b[4];
            mtwnSamplePath(&four, k / 100.0, a, NULL);
            mtwnSamplePath(&path, k / 100.0, b, NULL);
            differ += a[c] != b[0];
        }
    }
    CHECK(differ == 0, "each channel its own path");
}

int main(void)
{
    TestWithin();
    TestSlopes();
    TestTracks();
    TestFourChannels();
    return s_failures == 0 ? 0 : 1;
}
