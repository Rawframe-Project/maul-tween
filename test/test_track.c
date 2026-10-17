// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Tracks (records mtwn-0016, mtwn-0020, mtwn-0032): each value kind over
// keyframes against the values layer it is built from: paths, the
// integer and discrete rules, angles unwrapped, colors and rotations
// mixed per segment, rotations as glTF's normalized cubics.

#include "test_harness.h"

#include "maul-tween/path.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static const double s_pi = 3.141592653589793;

static mtwnKeyframe Key(double offset, mtwnVec4 value, mtwnSegmentKind kind)
{
    mtwnKeyframe k;
    memset(&k, 0, sizeof k);
    k.offset = offset;
    k.value = value;
    k.easing = mtwnLinearEasing();
    k.kind = kind;
    return k;
}

static bool Same(mtwnVec4 a, mtwnVec4 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static void TestNumbers(void)
{
    mtwnKeyframe keys[3] = {Key(0.0, (mtwnVec4){0, 1, 0, 0}, mtwn_segmentCatmullRom),
                            Key(0.4, (mtwnVec4){2, -1, 0, 0}, mtwn_segmentCatmullRom),
                            Key(1.0, (mtwnVec4){3, 4, 0, 0}, mtwn_segmentCatmullRom)};
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueVec2);
    mtwnTrack track;
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, NULL, &track) == mtwn_success, "a vector track");
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 3, 2, 0.5, NULL, &path) == mtwn_success, "its path");
    int same = 1;
    for (int k = -10; k <= 110; ++k)
    {
        double v[4];
        mtwnSamplePath(&path, k / 100.0, v, NULL);
        mtwnVec4 got = mtwnSampleTrack(&track, k / 100.0);
        same &= got.x == (float)v[0] && got.y == (float)v[1] && got.z == 0.0f && got.w == 0.0f;
    }
    CHECK(same, "a vector track is its path");
    mtwnKeyframe count[2] = {Key(0.0, (mtwnVec4){0, 0, 0, 0}, mtwn_segmentLinear),
                             Key(1.0, (mtwnVec4){3, 0, 0, 0}, mtwn_segmentLinear)};
    def = mtwnDefaultTrackDef(mtwn_valueInteger);
    CHECK(mtwnMakeTrack(&def, count, 2, NULL, NULL, &track) == mtwn_success, "an integer track");
    CHECK(mtwnSampleTrack(&track, 0.5).x == 2.0f && mtwnSampleTrack(&track, 0.4).x == 1.0f,
          "rounded to nearest, ties away from zero");
    count[1].value.x = -3.0f;
    CHECK(mtwnMakeTrack(&def, count, 2, NULL, NULL, &track) == mtwn_success &&
              mtwnSampleTrack(&track, 0.5).x == -2.0f,
          "below zero too");
}

static void TestDiscrete(void)
{
    mtwnKeyframe keys[3] = {Key(0.0, (mtwnVec4){4, 0, 0, 0}, mtwn_segmentLinear),
                            Key(0.5, (mtwnVec4){9, 0, 0, 0}, mtwn_segmentStep),
                            Key(1.0, (mtwnVec4){2, 0, 0, 0}, mtwn_segmentLinear)};
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &keys[0].easing) == mtwn_success, "quad");
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueDiscrete);
    mtwnTrack track;
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, NULL, &track) == mtwn_success, "a discrete track");
    // Eased 1/2 of the first segment is its local sqrt(1/2).
    double flip = 0.5 * sqrt(0.5);
    CHECK(mtwnSampleTrack(&track, flip - 1e-9).x == 4.0f &&
              mtwnSampleTrack(&track, flip + 1e-9).x == 9.0f,
          "a linear segment flips at an eased 1/2");
    CHECK(mtwnSampleTrack(&track, 0.99).x == 9.0f && mtwnSampleTrack(&track, 1.0).x == 2.0f,
          "a step holds to its end");
    keys[0].easing = mtwnLinearEasing();
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, NULL, &track) == mtwn_success &&
              mtwnSampleTrack(&track, 0.25).x == 9.0f,
          "at exactly 1/2 the end, as Web Animations steps it");
    keys[0].kind = mtwn_segmentBezier;
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, NULL, &track) == mtwn_errorInvalid,
          "no Bezier for discrete values");
}

static void TestAngles(void)
{
    float deg = (float)(s_pi / 180.0);
    mtwnKeyframe keys[3] = {Key(0.0, (mtwnVec4){350 * deg, 0, 0, 0}, mtwn_segmentLinear),
                            Key(0.5, (mtwnVec4){10 * deg, 0, 0, 0}, mtwn_segmentBezier),
                            Key(1.0, (mtwnVec4){30 * deg, 0, 0, 0}, mtwn_segmentLinear)};
    keys[1].out = (mtwnVec4){15 * deg, 0, 0, 0};
    keys[2].in = (mtwnVec4){25 * deg, 0, 0, 0};
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueAngle);
    mtwnTrack track;
    double lengths[3];
    CHECK(mtwnMakeTrack(&def, keys, 3, lengths, NULL, &track) == mtwn_success,
          "an angle track at constant speed");
    float turn = (float)(2.0 * s_pi);
    CHECK(fabsf(keys[1].value.x - (10 * deg + turn)) <= 1e-6f &&
              fabsf(keys[2].value.x - (30 * deg + turn)) <= 1e-6f,
          "unwrapped in place, the shorter way from each before");
    CHECK(fabsf(keys[1].out.x - (15 * deg + turn)) <= 1e-6f &&
              fabsf(keys[2].in.x - (25 * deg + turn)) <= 1e-6f,
          "the controls moved with their keyframes");
    CHECK(fabs(lengths[2] - 40.0 * (double)deg) <= 1e-5, "40 degrees of travel");
    mtwnVec4 middle = mtwnSampleTrack(&track, 0.25);
    CHECK(fabs((double)middle.x - 2.0 * s_pi) <= 1e-5, "a quarter of the way: through 360");
    mtwnKeyframe raw[2] = {Key(0.0, (mtwnVec4){350 * deg, 0, 0, 0}, mtwn_segmentLinear),
                           Key(1.0, (mtwnVec4){10 * deg, 0, 0, 0}, mtwn_segmentLinear)};
    def.angleMethod = mtwn_angleRaw;
    CHECK(mtwnMakeTrack(&def, raw, 2, NULL, NULL, &track) == mtwn_success &&
              raw[1].value.x == 10 * deg,
          "raw angles stay as given");
}

static void TestColors(void)
{
    mtwnKeyframe keys[3] = {Key(0.0, (mtwnVec4){1, 0, 0, 1}, mtwn_segmentLinear),
                            Key(0.5, (mtwnVec4){0, 0, 1, 1}, mtwn_segmentStep),
                            Key(1.0, (mtwnVec4){0, 1, 0, 0.5f}, mtwn_segmentLinear)};
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueColor);
    mtwnSegmentMix mixes[2];
    mtwnTrack track;
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, mixes, &track) == mtwn_success, "a color track");
    mtwnColorMix mix;
    CHECK(mtwnMakeColorMix((mtwnColor){1, 0, 0, 1}, (mtwnColor){0, 0, 1, 1}, mtwn_colorOklab,
                           mtwn_hueShorter, &mix) == mtwn_success,
          "a mix");
    mtwnColor want = mtwnMixColor(&mix, 0.5);
    mtwnVec4 got = mtwnSampleTrack(&track, 0.25);
    CHECK(got.x == want.red && got.y == want.green && got.z == want.blue && got.w == want.alpha,
          "mixed in Oklab per segment");
    CHECK(Same(mtwnSampleTrack(&track, 0.75), keys[1].value) &&
              Same(mtwnSampleTrack(&track, 1.0), keys[2].value),
          "a step holds, then the last");
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, NULL, &track) == mtwn_errorInvalid, "no mixes");
    double lengths[3];
    CHECK(mtwnMakeTrack(&def, keys, 3, lengths, mixes, &track) == mtwn_errorInvalid,
          "no constant speed");
    keys[2].value.w = 1.5f;
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, mixes, &track) == mtwn_errorInvalid, "alpha past 1");
    keys[2].value.w = 0.5f;
    keys[0].kind = mtwn_segmentCatmullRom;
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, mixes, &track) == mtwn_errorInvalid,
          "no color splines");
}

static mtwnVec4 About(double x, double y, double z, double angle)
{
    double s = sin(angle / 2.0);
    return (mtwnVec4){(float)(x * s), (float)(y * s), (float)(z * s), (float)cos(angle / 2.0)};
}

static void TestRotations(void)
{
    mtwnVec4 a = About(0, 0, 1, 0.0);
    mtwnVec4 b = About(0, 0, 1, 1.0);
    mtwnVec4 c = About(0, 1, 0, 2.0);
    mtwnVec4 negatedB = {-b.x, -b.y, -b.z, -b.w};
    mtwnKeyframe keys[3] = {Key(0.0, a, mtwn_segmentLinear), Key(0.5, negatedB, mtwn_segmentStep),
                            Key(1.0, c, mtwn_segmentLinear)};
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueRotation);
    mtwnSegmentMix mixes[2];
    mtwnTrack track;
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, mixes, &track) == mtwn_success, "a rotation track");
    CHECK(Same(keys[1].value, b), "flipped into the hemisphere of the one before");
    mtwnRotationMix turn;
    CHECK(mtwnMakeRotationMix((mtwnQuat){a.x, a.y, a.z, a.w}, (mtwnQuat){b.x, b.y, b.z, b.w},
                              mtwn_rotationShortest, &turn) == mtwn_success,
          "a mix");
    mtwnQuat q = mtwnMixRotation(&turn, 0.4);
    mtwnVec4 got = mtwnSampleTrack(&track, 0.2);
    CHECK(got.x == q.x && got.y == q.y && got.z == q.z && got.w == q.w, "slerp per segment");
    CHECK(Same(mtwnSampleTrack(&track, 0.7), b), "a step holds");
    // Catmull-Rom per component, normalized: glTF's cubic spline.
    mtwnKeyframe spline[4] = {
        Key(0.0, a, mtwn_segmentCatmullRom), Key(0.3, b, mtwn_segmentCatmullRom),
        Key(0.6, c, mtwn_segmentCatmullRom), Key(1.0, About(1, 0, 0, 0.5), mtwn_segmentCatmullRom)};
    mtwnSegmentMix splineMixes[3];
    CHECK(mtwnMakeTrack(&def, spline, 4, NULL, splineMixes, &track) == mtwn_success, "a spline");
    mtwnPath path;
    CHECK(mtwnMakePath(spline, 4, 4, 0.5, NULL, &path) == mtwn_success, "its components");
    double worst = 0.0;
    double unit = 0.0;
    for (int k = 0; k <= 100; ++k)
    {
        double v[4];
        mtwnSamplePath(&path, k / 100.0, v, NULL);
        double norm = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
        mtwnVec4 r = mtwnSampleTrack(&track, k / 100.0);
        double e = fabs((double)r.x - v[0] / norm) + fabs((double)r.y - v[1] / norm) +
                   fabs((double)r.z - v[2] / norm) + fabs((double)r.w - v[3] / norm);
        worst = e > worst ? e : worst;
        double length = sqrt((double)(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w));
        unit = fabs(length - 1.0) > unit ? fabs(length - 1.0) : unit;
    }
    CHECK(worst <= 2e-7 && unit <= 2e-7, "the component path normalized, of unit length");
    // Unnormalized keyframes come out unit too.
    mtwnKeyframe big[2] = {Key(0.0, (mtwnVec4){0, 0, 0, 2}, mtwn_segmentStep),
                           Key(1.0, (mtwnVec4){0, 0, 3, 0}, mtwn_segmentLinear)};
    mtwnSegmentMix bigMixes[1];
    CHECK(mtwnMakeTrack(&def, big, 2, NULL, bigMixes, &track) == mtwn_success &&
              mtwnSampleTrack(&track, 0.5).w == 1.0f && mtwnSampleTrack(&track, 1.0).z == 1.0f,
          "a step's rotations normalized");
    // Bezier controls flip with their keyframe; the longest path flips
    // nothing, and takes equal rotations on a spline, which no slerp uses.
    mtwnKeyframe curved[2] = {Key(0.0, a, mtwn_segmentBezier),
                              Key(1.0, negatedB, mtwn_segmentLinear)};
    curved[0].out = a;
    curved[1].in = negatedB;
    mtwnSegmentMix curvedMixes[1];
    def = mtwnDefaultTrackDef(mtwn_valueRotation);
    CHECK(mtwnMakeTrack(&def, curved, 2, NULL, curvedMixes, &track) == mtwn_success &&
              Same(curved[1].value, b) && Same(curved[1].in, b),
          "a Bezier rotation's controls flip with it");
    mtwnKeyframe far[2] = {Key(0.0, a, mtwn_segmentCatmullRom),
                           Key(1.0, negatedB, mtwn_segmentLinear)};
    def.rotationPath = mtwn_rotationLongest;
    CHECK(mtwnMakeTrack(&def, far, 2, NULL, curvedMixes, &track) == mtwn_success &&
              Same(far[1].value, negatedB),
          "the longest path flips nothing");
    mtwnKeyframe equal[2] = {Key(0.0, a, mtwn_segmentCatmullRom), Key(1.0, a, mtwn_segmentLinear)};
    CHECK(mtwnMakeTrack(&def, equal, 2, NULL, curvedMixes, &track) == mtwn_success,
          "equal rotations the long way on a spline, which turns no slerp");
    // An easing at 1 inside a spline segment: the end keyframe, normalized.
    mtwnVec4 odd = {0.1f, -0.7f, 0.3f, 0.6f};
    mtwnKeyframe early[3] = {Key(0.0, a, mtwn_segmentCatmullRom), Key(0.5, odd, mtwn_segmentLinear),
                             Key(1.0, c, mtwn_segmentLinear)};
    CHECK(mtwnMakeSteps(1, mtwn_jumpStart, &early[0].easing) == mtwn_success, "steps(1, start)");
    def = mtwnDefaultTrackDef(mtwn_valueRotation);
    mtwnSegmentMix earlyMixes[2];
    CHECK(mtwnMakeTrack(&def, early, 3, NULL, earlyMixes, &track) == mtwn_success, "made");
    double ox = (double)odd.x;
    double oy = (double)odd.y;
    double oz = (double)odd.z;
    double ow = (double)odd.w;
    double n = sqrt(ox * ox + oy * oy + oz * oz + ow * ow);
    mtwnVec4 end = mtwnSampleTrack(&track, 0.25);
    CHECK(end.x == (float)((double)odd.x / n) && end.y == (float)((double)odd.y / n) &&
              end.z == (float)((double)odd.z / n) && end.w == (float)((double)odd.w / n),
          "exactly the end keyframe normalized");
    // A refused track leaves its keyframes as they were.
    mtwnKeyframe still[2] = {Key(0.0, a, mtwn_segmentLinear), Key(1.0, a, mtwn_segmentLinear)};
    def.rotationPath = mtwn_rotationLongest;
    CHECK(mtwnMakeTrack(&def, still, 2, NULL, mixes, &track) == mtwn_errorInvalid,
          "the long way between equal rotations");
    mtwnKeyframe zero[2] = {Key(0.0, a, mtwn_segmentLinear),
                            Key(1.0, (mtwnVec4){0, 0, 0, 0}, mtwn_segmentLinear)};
    def.rotationPath = mtwn_rotationShortest;
    CHECK(mtwnMakeTrack(&def, zero, 2, NULL, mixes, &track) == mtwn_errorInvalid,
          "a zero rotation");
}

// A rotation track refuses an all-zero keyframe and takes others, one of
// four nonzero channels and a half turn of one (found by mutants).
static void TestRotationZero(void)
{
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueRotation);
    mtwnSegmentMix mixes[1];
    mtwnTrack track;
    mtwnKeyframe zero[2] = {Key(0.0, (mtwnVec4){0, 0, 0, 0}, mtwn_segmentLinear),
                            Key(1.0, (mtwnVec4){0, 0, 0, 1}, mtwn_segmentLinear)};
    CHECK(mtwnMakeTrack(&def, zero, 2, NULL, mixes, &track) == mtwn_errorInvalid,
          "an all-zero rotation refused");
    mtwnKeyframe full[2] = {Key(0.0, (mtwnVec4){0.5f, 0.5f, 0.5f, 0.5f}, mtwn_segmentLinear),
                            Key(1.0, (mtwnVec4){0, 0, 0, 1}, mtwn_segmentLinear)};
    CHECK(mtwnMakeTrack(&def, full, 2, NULL, mixes, &track) == mtwn_success,
          "a rotation of four nonzero channels taken");
    mtwnKeyframe half[2] = {Key(0.0, (mtwnVec4){1, 0, 0, 0}, mtwn_segmentLinear),
                            Key(1.0, (mtwnVec4){0, 0, 0, 1}, mtwn_segmentLinear)};
    CHECK(mtwnMakeTrack(&def, half, 2, NULL, mixes, &track) == mtwn_success,
          "a half turn about x taken");
}

// A curved rotation extrapolated so far its cubic overflows: still a unit
// rotation, along the overflowing components (found by the values' fuzz,
// NaN before).
static void TestRotationFarOut(void)
{
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueRotation);
    mtwnSegmentMix mixes[2];
    mtwnTrack track;
    mtwnKeyframe keys[3] = {Key(0.0, (mtwnVec4){0, -0.64f, 0, 1}, mtwn_segmentCatmullRom),
                            Key(1e-300, (mtwnVec4){0, 0, 0, 1}, mtwn_segmentCatmullRom),
                            Key(1.0, (mtwnVec4){0, 0.3f, 0, 1}, mtwn_segmentCatmullRom)};
    CHECK(mtwnMakeTrack(&def, keys, 3, NULL, mixes, &track) == mtwn_success, "made");
    bool unit = true;
    for (double progress = -1.5; progress < 3.0; progress += 0.5)
    {
        mtwnVec4 q = mtwnSampleTrack(&track, progress);
        double norm = sqrt((double)q.x * (double)q.x + (double)q.y * (double)q.y +
                           (double)q.z * (double)q.z + (double)q.w * (double)q.w);
        unit = unit && fabs(norm - 1.0) < 1e-6;
    }
    // At -1e-220 the segment's progress is -1e80: the cubic finite, its
    // squares not.
    mtwnVec4 q = mtwnSampleTrack(&track, -1e-220);
    double norm = sqrt((double)q.x * (double)q.x + (double)q.y * (double)q.y +
                       (double)q.z * (double)q.z + (double)q.w * (double)q.w);
    CHECK(unit && fabs(norm - 1.0) < 1e-6, "unit far past the first segment");
}

static void TestRefusals(void)
{
    float deg = (float)(s_pi / 180.0);
    mtwnKeyframe keys[2] = {Key(0.0, (mtwnVec4){350 * deg, 0, 0, 0}, mtwn_segmentLinear),
                            Key(1.0, (mtwnVec4){10 * deg, 0, 0, 0}, mtwn_segmentLinear)};
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueAngle);
    mtwnTrack track;
    def.alpha = 2.0;
    CHECK(mtwnMakeTrack(&def, keys, 2, NULL, NULL, &track) == mtwn_errorInvalid &&
              keys[1].value.x == 10 * deg,
          "refused, and the keyframes not unwrapped");
    def = mtwnDefaultTrackDef(9);
    CHECK(mtwnMakeTrack(&def, keys, 2, NULL, NULL, &track) == mtwn_errorInvalid, "a kind");
    def = mtwnDefaultTrackDef(mtwn_valueRotation);
    def.rotationPath = 2;
    mtwnSegmentMix mixes[1];
    CHECK(mtwnMakeTrack(&def, keys, 2, NULL, mixes, &track) == mtwn_errorInvalid, "a path");
    keys[0].kind = mtwn_segmentStep;
    CHECK(mtwnMakeTrack(&def, keys, 2, NULL, mixes, &track) == mtwn_errorInvalid,
          "a path, though only steps, which make no mix");
    keys[0].kind = mtwn_segmentLinear;
    def = mtwnDefaultTrackDef(mtwn_valueScalar);
    CHECK(mtwnMakeTrack(NULL, keys, 2, NULL, NULL, &track) == mtwn_errorInvalid, "no def");
    CHECK(mtwnMakeTrack(&def, keys, 2, NULL, NULL, NULL) == mtwn_errorInvalid, "no out");
    def = mtwnDefaultTrackDef(mtwn_valueDiscrete);
    double lengths[2];
    CHECK(mtwnMakeTrack(&def, keys, 2, lengths, NULL, &track) == mtwn_errorInvalid,
          "no constant speed for discrete values");
}

int main(void)
{
    TestRotationZero();
    TestRotationFarOut();
    TestNumbers();
    TestDiscrete();
    TestAngles();
    TestColors();
    TestRotations();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
