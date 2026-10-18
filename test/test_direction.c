// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A path's direction (record mtwn-0064), by SVG 2's rules: a line's own,
// the later segment's at a keyframe, a derivative of 0 by the next one
// that is not, a step and coincident keyframes by their neighbours, a
// path of no length along the first channel, at constant speed alike;
// and its angle.

#include "test_harness.h"

#include "maul-tween/path.h"

#include <math.h>
#include <string.h>

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

// Whether a direction is (x, y) within a few ulps.
static bool Along(const mtwnPath* path, double progress, double x, double y)
{
    double d[4];
    mtwnPathDirection(path, progress, d);
    return fabs(d[0] - x) <= 1e-15 && fabs(d[1] - y) <= 1e-15 && d[2] == 0.0 && d[3] == 0.0;
}

static void TestLines(void)
{
    mtwnKeyframe keys[3] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentLinear),
                            Key(0.5, 3.0f, 4.0f, mtwn_segmentLinear),
                            Key(1.0, 3.0f, 10.0f, mtwn_segmentLinear)};
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 3, 2, 0.5, NULL, &path) == mtwn_success, "two lines");
    CHECK(Along(&path, 0.0, 0.6, 0.8) && Along(&path, 0.25, 0.6, 0.8) &&
              Along(&path, -0.5, 0.6, 0.8),
          "along the first, extended before it too");
    CHECK(Along(&path, 0.5, 0.0, 1.0) && Along(&path, 1.0, 0.0, 1.0),
          "at the keyframe the later segment's, to the end");
    CHECK(fabs(mtwnPathAngle(&path, 0.25) - atan2(0.8, 0.6)) <= 1e-15 &&
              fabs(mtwnPathAngle(&path, 0.75) - atan2(1.0, 0.0)) <= 1e-15,
          "its angles");
    double lengths[3];
    CHECK(mtwnMakePath(keys, 3, 2, 0.5, lengths, &path) == mtwn_success, "at constant speed");
    CHECK(Along(&path, 0.4, 0.6, 0.8) && Along(&path, 0.6, 0.0, 1.0),
          "5 of 11 along the first, then the second");
}

static void TestStillDerivatives(void)
{
    // A control on its start: the second derivative's way, (1, 1).
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f, 0.0f, mtwn_segmentBezier),
                            Key(1.0, 2.0f, 0.0f, mtwn_segmentLinear)};
    keys[0].out = keys[0].value;
    keys[1].in = (mtwnVec4){1.0f, 1.0f, 0, 0};
    mtwnPath path;
    CHECK(mtwnMakePath(keys, 2, 2, 0.5, NULL, &path) == mtwn_success, "a control on its start");
    double r = sqrt(0.5);
    CHECK(Along(&path, 0.0, r, r), "leaving toward its other control");
    // Both controls on the start: the third derivative's, toward the end.
    keys[1].value = (mtwnVec4){0.0f, -2.0f, 0, 0};
    keys[1].in = keys[0].value;
    CHECK(mtwnMakePath(keys, 2, 2, 0.5, NULL, &path) == mtwn_success, "both on its start");
    CHECK(Along(&path, 0.0, 0.0, -1.0) && Along(&path, 0.5, 0.0, -1.0), "straight down");
}

static void TestNeighbours(void)
{
    // A step holds the heading of the line before it.
    mtwnKeyframe held[4] = {
        Key(0.0, 0.0f, 0.0f, mtwn_segmentLinear), Key(0.3, 1.0f, 0.0f, mtwn_segmentStep),
        Key(0.6, 5.0f, 5.0f, mtwn_segmentLinear), Key(1.0, 5.0f, 6.0f, mtwn_segmentLinear)};
    mtwnPath path;
    CHECK(mtwnMakePath(held, 4, 2, 0.5, NULL, &path) == mtwn_success, "a step between lines");
    CHECK(Along(&path, 0.45, 1.0, 0.0) && Along(&path, 0.8, 0.0, 1.0),
          "held, the line before's; then its own");
    // Coincident keyframes first: the line after them.
    mtwnKeyframe still[3] = {Key(0.0, 2.0f, 2.0f, mtwn_segmentLinear),
                             Key(0.4, 2.0f, 2.0f, mtwn_segmentLinear),
                             Key(1.0, 2.0f, 5.0f, mtwn_segmentLinear)};
    CHECK(mtwnMakePath(still, 3, 2, 0.5, NULL, &path) == mtwn_success, "coincident first");
    CHECK(Along(&path, 0.1, 0.0, 1.0) && Along(&path, -1.0, 0.0, 1.0), "the line after's");
    // And as a Catmull-Rom between them at constant speed, whose length
    // is rounding (record mtwn-0031, amended).
    still[0].kind = mtwn_segmentCatmullRom;
    double lengths[3];
    CHECK(mtwnMakePath(still, 3, 2, 0.5, lengths, &path) == mtwn_success, "at constant speed");
    CHECK(Along(&path, -0.5, 0.0, 1.0), "the same");
    // Without constant speed too: the Catmull-Rom between coincident
    // keyframes has derivatives of rounding, whose own way is noise ((-1,
    // 0) here); it takes the curve after it.
    mtwnKeyframe curve[4] = {
        Key(0.0, 2.0f, 2.0f, mtwn_segmentCatmullRom), Key(0.3, 2.0f, 2.0f, mtwn_segmentCatmullRom),
        Key(0.6, 5.0f, 2.0f, mtwn_segmentCatmullRom), Key(1.0, 5.0f, 5.0f, mtwn_segmentLinear)};
    CHECK(mtwnMakePath(curve, 4, 2, 0.5, NULL, &path) == mtwn_success, "a held curve's start");
    CHECK(Along(&path, 0.1, 1.0, 0.0), "the curve after it");
    double curveLengths[4];
    CHECK(mtwnMakePath(curve, 4, 2, 0.5, curveLengths, &path) == mtwn_success &&
              Along(&path, -0.1, 1.0, 0.0),
          "and at constant speed");
    // A path of no length: along the first channel.
    mtwnKeyframe point[2] = {Key(0.0, 1.0f, 1.0f, mtwn_segmentLinear),
                             Key(1.0, 1.0f, 1.0f, mtwn_segmentLinear)};
    CHECK(mtwnMakePath(point, 2, 2, 0.5, NULL, &path) == mtwn_success, "a point");
    CHECK(Along(&path, 0.5, 1.0, 0.0) && mtwnPathAngle(&path, 0.5) == 0.0, "along x, angle 0");
    // One channel: forwards or back.
    mtwnKeyframe back[2] = {Key(0.0, 3.0f, 0.0f, mtwn_segmentLinear),
                            Key(1.0, 1.0f, 0.0f, mtwn_segmentLinear)};
    CHECK(mtwnMakePath(back, 2, 1, 0.5, NULL, &path) == mtwn_success, "one channel");
    double d[4];
    mtwnPathDirection(&path, 0.5, d);
    CHECK(d[0] == -1.0 && d[1] == 0.0 &&
              fabs(mtwnPathAngle(&path, 0.5) - atan2(0.0, -1.0)) <= 1e-15,
          "back, pi");
}

int main(void)
{
    TestLines();
    TestStillDerivatives();
    TestNeighbours();
    return s_failures == 0 ? 0 : 1;
}
