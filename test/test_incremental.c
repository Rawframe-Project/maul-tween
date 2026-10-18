// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Incremental tracks (record mtwn-0040): each iteration moves on by the
// last keyframe minus the first, continuous at the boundaries, counted
// from the iteration start as Web Animations Level 2 counts; integers;
// rotations turning on; colors and discrete values refused.

#include "test_harness.h"

#include "maul-tween/player.h"
#include "maul-tween/target.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct World
{
    mtwnContext* context;
    mtwnClipId clip;
    mtwnTargetId target;
} World;

static World MakeWorld(mtwnValueKind kind, mtwnVec4 base)
{
    World w = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &w.context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(w.context, &clipDef, &w.clip) == mtwn_success, "a clip");
    CHECK(mtwnCreateTarget(w.context, kind, base, &w.target) == mtwn_success, "a target");
    return w;
}

static mtwnClipTrackDef Incremental(mtwnValueKind kind, mtwnKeyframe* keys, double iterations)
{
    keys[0].offset = 0.0;
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.track.kind = kind;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    track.timing.iterations = iterations;
    track.timing.fill = mtwn_fillForwards;
    track.incremental = true;
    return track;
}

static void Play(World* w)
{
    mtwnBinding binding = mtwnBind(0, w->target);
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = w->clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.keep = true;
    mtwnPlayerId p;
    CHECK(mtwnCreatePlayer(w->context, &def, &p) == mtwn_success, "a player");
    CHECK(mtwnUpdate(w->context, 0, NULL, 0, NULL) == mtwn_success, "started");
}

static mtwnTargetState At(World* w, double seconds)
{
    CHECK(mtwnUpdate(w->context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) == mtwn_success,
          "updated");
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(w->context, w->target, &state) == mtwn_success, "read");
    return state;
}

static bool Near(double a, double b, double tolerance)
{
    return fabs(a - b) <= tolerance;
}

static void TestNumbers(void)
{
    World w = MakeWorld(mtwn_valueVec4, (mtwnVec4){0});
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value = (mtwnVec4){2.0f, 10.0f, 0.0f, 1.0f};
    keys[1].value = (mtwnVec4){3.0f, 8.0f, 0.5f, 3.0f};
    mtwnClipTrackDef track = Incremental(mtwn_valueVec4, keys, 3.0);
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    mtwnTargetState s = At(&w, 0.5);
    CHECK(Near((double)s.value.x, 2.5, 1e-6) && Near((double)s.value.y, 9.0, 1e-6),
          "the first iteration as it is");
    s = At(&w, 1.5);
    CHECK(Near((double)s.value.x, 3.5, 1e-6) && Near((double)s.value.y, 7.0, 1e-6),
          "the second moved on by (1, -2)");
    s = At(&w, 2.0005);
    CHECK(Near((double)s.velocity.x, 1.0, 1e-3) && Near((double)s.velocity.y, -2.0, 1e-3),
          "continuous across the boundary: the velocity unbroken");
    s = At(&w, 9.0);
    CHECK(Near((double)s.value.x, 5.0, 1e-6) && Near((double)s.value.y, 4.0, 1e-6) &&
              Near((double)s.value.z, 1.5, 1e-6) && Near((double)s.value.w, 7.0, 1e-6),
          "filled at the third's end, every channel moved on");
    mtwnDestroyContext(w.context);
}

static void TestCounting(void)
{
    // An iteration start of 1 counts from 1, as Level 2 does.
    World w = MakeWorld(mtwn_valueScalar, (mtwnVec4){0});
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].value.x = 1.0f;
    mtwnClipTrackDef track = Incremental(mtwn_valueScalar, keys, 2.0);
    track.timing.iterationStart = 1.0;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    CHECK(Near((double)At(&w, 0.5).value.x, 1.5, 1e-6), "from iteration 1");
    mtwnDestroyContext(w.context);
    // Alternate: the reversed iteration runs back, moved on as it goes.
    w = MakeWorld(mtwn_valueScalar, (mtwnVec4){0});
    track = Incremental(mtwn_valueScalar, keys, 2.0);
    track.timing.direction = mtwn_directionAlternate;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    CHECK(Near((double)At(&w, 1.25).value.x, 1.75, 1e-6), "back from 1, plus 1");
    mtwnDestroyContext(w.context);
    // Integers move on unrounded and round once.
    w = MakeWorld(mtwn_valueInteger, (mtwnVec4){0});
    memset(keys, 0, sizeof keys);
    keys[1].value.x = 3.0f;
    track = Incremental(mtwn_valueInteger, keys, 3.0);
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    CHECK(At(&w, 2.5).value.x == 8.0f, "two iterations of 3 and half of one: 7.5, rounded 8");
    mtwnDestroyContext(w.context);
}

static void TestRotations(void)
{
    // Quarter turns about z, three times over: half way through the second,
    // three eighths of a turn.
    World w = MakeWorld(mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f});
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    float s = (float)sin(3.14159265358979323846 / 4.0);
    float c = (float)cos(3.14159265358979323846 / 4.0);
    keys[0].value = (mtwnVec4){0, 0, 0, 1.0f};
    keys[1].value = (mtwnVec4){0, 0, s, c};
    mtwnClipTrackDef track = Incremental(mtwn_valueRotation, keys, 3.0);
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    mtwnTargetState state = At(&w, 1.5);
    double half = 3.0 * 3.14159265358979323846 / 8.0;
    CHECK(Near((double)state.value.z, sin(half), 1e-6) &&
              Near((double)state.value.w, cos(half), 1e-6),
          "turned on past the first quarter");
    state = At(&w, 9.0);
    double end = 3.0 * 3.14159265358979323846 / 4.0;
    CHECK(Near((double)state.value.z, sin(end), 1e-6) &&
              Near((double)state.value.w, cos(end), 1e-6),
          "three quarters at the end");
    mtwnDestroyContext(w.context);
    // From a quarter turn about x to a quarter about z after it: the step
    // is the quarter about z, turning before the path, three times over;
    // the end is three quarters about z after the first.
    w = MakeWorld(mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f});
    keys[0].value = (mtwnVec4){s, 0, 0, c};
    keys[1].value = (mtwnVec4){0.5f, 0.5f, 0.5f, 0.5f};
    track = Incremental(mtwn_valueRotation, keys, 3.0);
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    state = At(&w, 9.0);
    double sign = state.value.w < 0.0f ? -1.0 : 1.0;
    CHECK(Near(sign * (double)state.value.x, 0.5, 1e-6) &&
              Near(sign * (double)state.value.y, -0.5, 1e-6) &&
              Near(sign * (double)state.value.z, -0.5, 1e-6) &&
              Near(sign * (double)state.value.w, 0.5, 1e-6),
          "the step from the first's inverse, turning before the path");
    mtwnDestroyContext(w.context);
    // A path that ends where it starts: no turn to repeat, the start held,
    // half way through the second iteration too.
    w = MakeWorld(mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f});
    keys[0].value = keys[1].value = (mtwnVec4){s, 0, 0, c};
    track = Incremental(mtwn_valueRotation, keys, 3.0);
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    state = At(&w, 1.5);
    CHECK(Near((double)state.value.x, (double)s, 1e-6) && Near((double)state.value.y, 0.0, 1e-6) &&
              Near((double)state.value.z, 0.0, 1e-6) &&
              Near((double)state.value.w, (double)c, 1e-6),
          "no turn: the start, not NaN");
    mtwnDestroyContext(w.context);
}

static void TestLargeCount(void)
{
    // A step of 0.1 rad about z, endless: half way through iteration 10^6
    // the turn is the closed form, the step's angle times 10^6.5, computed
    // from the axis and angle, not by repeated products.
    World w = MakeWorld(mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f});
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value = (mtwnVec4){0, 0, 0, 1.0f};
    keys[1].value = (mtwnVec4){0, 0, (float)sin(0.05), (float)cos(0.05)};
    mtwnClipTrackDef track = Incremental(mtwn_valueRotation, keys, HUGE_VAL);
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    mtwnTargetState state = At(&w, 1e6 + 0.5);
    double step = 2.0 * atan2((double)keys[1].value.z, (double)keys[1].value.w);
    double half = step * (1e6 + 0.5) / 2.0;
    double sign = (double)state.value.z * sin(half) < 0.0 ? -1.0 : 1.0;
    CHECK(Near(sign * (double)state.value.z, sin(half), 2e-6) &&
              Near(sign * (double)state.value.w, cos(half), 2e-6),
          "iteration 10^6 at the closed form");
    mtwnDestroyContext(w.context);
}

static void TestRefused(void)
{
    World w = MakeWorld(mtwn_valueScalar, (mtwnVec4){0});
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value.w = keys[1].value.w = 1.0f;
    mtwnNode node;
    mtwnClipTrackDef track = Incremental(mtwn_valueColor, keys, 2.0);
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_errorInvalid,
          "a color does not move on");
    track = Incremental(mtwn_valueDiscrete, keys, 2.0);
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_errorInvalid,
          "nor a discrete value");
    // Endless iterations of no duration pass at once, to an infinite
    // iteration no value moves on by (found by the API's fuzz).
    track = Incremental(mtwn_valueScalar, keys, HUGE_VAL);
    track.timing.duration = 0.0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_errorInvalid,
          "nor endless iterations of no duration");
    track.incremental = false;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success,
          "which a track not moving on may have");
    track = Incremental(mtwn_valueScalar, keys, HUGE_VAL);
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success,
          "as may endless iterations that take time");
    track = Incremental(mtwn_valueScalar, keys, 3.0);
    track.timing.duration = 0.0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success,
          "and finitely many of no duration (a mutant found it untested)");
    mtwnDestroyContext(w.context);
}

static void TestStepPastFloats(void)
{
    // From the lowest float to 2^104: the step, 2^104 past the largest
    // float, has no float, yet each value moved on by it is one. Iteration
    // 1 a quarter in is 2^104 + (FLT_MAX + 2^104) / 4, not the bound.
    World w = MakeWorld(mtwn_valueScalar, (mtwnVec4){0});
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value = (mtwnVec4){-0x1.fffffep127f, 0.0f, 0.0f, 0.0f};
    keys[1].value = (mtwnVec4){0x1p104f, 0.0f, 0.0f, 0.0f};
    mtwnClipTrackDef track = Incremental(mtwn_valueScalar, keys, 2.0);
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w);
    mtwnTargetState s = At(&w, 1.25);
    double step = 0x1.fffffep127 + 0x1p104;
    CHECK(Near((double)s.value.x, 0x1p104 + 0.25 * step, 0x1p-20 * step),
          "moved on by a step past the floats: the value, not the bound");
    s = At(&w, 9.0);
    CHECK(s.value.x == 0x1.fffffep127f, "filled past the floats: held at the largest");
    mtwnDestroyContext(w.context);
}

int main(void)
{
    TestNumbers();
    TestCounting();
    TestRotations();
    TestLargeCount();
    TestStepPastFloats();
    TestRefused();
    return s_failures == 0 ? 0 : 1;
}
