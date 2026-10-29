// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A clip track replacing its target at full weight leaves its velocity to
// be worked out when read. Read at once, after the player's rate changes
// or it seeks, under an additive player and played backwards, it is the
// velocity the values layer gives for the last update: the track's change
// over the last millisecond. A new base does not change it, and a
// transition started on the target inherits it.

#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/clip.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <string.h>

typedef struct Scene
{
    mtwnContext* context;
    mtwnClipId clip;
    mtwnNode track;
    mtwnTargetId target;
} Scene;

// A clip of one number from 0 to 10 over 2 s from a start, quadratic out,
// filling forwards.
static Scene MakeSceneAt(double start)
{
    Scene s = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &s.context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(s.context, &clipDef, &s.clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 10.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 2.0;
    track.timing.fill = mtwn_fillForwards;
    track.position.offset = start;
    CHECK(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &track.timing.easing) == mtwn_success, "easing");
    CHECK(mtwnClip_AddTrack(s.context, s.clip, &track, &s.track) == mtwn_success, "a track");
    CHECK(mtwnClip_Seal(s.context, s.clip) == mtwn_success, "sealed");
    CHECK(mtwnCreateTarget(s.context, mtwn_valueScalar, (mtwnVec4){0}, &s.target) == mtwn_success,
          "a target");
    return s;
}

static Scene MakeScene(void)
{
    return MakeSceneAt(0.0);
}

static void Update(Scene* s, double seconds)
{
    CHECK(mtwnContext_Update(s->context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) ==
              mtwn_success,
          "an update");
}

static mtwnPlayerId PlayKept(Scene* s, double rate, mtwnComposite composite, bool keep)
{
    mtwnBinding binding = mtwnMakeBinding(0, s->target);
    binding.composite = composite;
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = s->clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.rate = rate;
    def.keep = keep;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(s->context, &def, &id) == mtwn_success, "a player");
    return id;
}

static mtwnPlayerId Play(Scene* s, double rate, mtwnComposite composite)
{
    return PlayKept(s, rate, composite, true);
}

// The track's value at a clip time, by the values layer.
static float Sample(const Scene* s, double t)
{
    mtwnClipTrack track;
    CHECK(mtwnClip_GetTrack(s->context, s->clip, s->track, &track) == mtwn_success, "the track");
    mtwnTimingResult result = ComputeTiming(&track.timing, t - track.start, false);
    return SampleTrack(&track.track, result.transformedProgress).x;
}

// Its velocity at a clip time, moving at a rate: its change over the last
// millisecond of host time.
static float Expected(const Scene* s, double t, double rate)
{
    return (float)(((double)Sample(s, t) - (double)Sample(s, t - rate * 0.001)) / 0.001);
}

static float Velocity(const Scene* s, mtwnTargetId target)
{
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(s->context, target, &state) == mtwn_success, "a state");
    return state.velocity.x;
}

static bool Near(float a, float b)
{
    return fabsf(a - b) <= 1e-3f * (1.0f + fabsf(b));
}

static void TestReadLater(void)
{
    Scene s = MakeScene();
    mtwnPlayerId p = Play(&s, 1.0, mtwn_compositeReplace);
    Update(&s, 0.0);
    Update(&s, 0.5);
    float expected = Expected(&s, 0.5, 1.0);
    CHECK(expected > 1.0f, "the reference moves");
    CHECK(Near(Velocity(&s, s.target), expected), "read at once");
    CHECK(mtwnPlayer_SetRate(s.context, p, 3.0) == mtwn_success, "a new rate");
    CHECK(Near(Velocity(&s, s.target), expected), "the last update's after a rate change");
    CHECK(mtwnPlayer_Seek(s.context, p, 1.5) == mtwn_success, "a seek");
    CHECK(Near(Velocity(&s, s.target), expected), "and after a seek");
    mtwnDestroyContext(s.context);
}

static void TestBaseSet(void)
{
    // A target no transition writes shows a new base at once; the
    // velocity the last update left is the old value's, and stays.
    Scene s = MakeScene();
    Play(&s, 1.0, mtwn_compositeReplace);
    Update(&s, 0.0);
    Update(&s, 0.4);
    float expected = Expected(&s, 0.4, 1.0);
    CHECK(mtwnTarget_SetBase(s.context, s.target, (mtwnVec4){100.0f, 0, 0, 0}) == mtwn_success,
          "a new base");
    CHECK(Near(Velocity(&s, s.target), expected), "a new base keeps the last velocity");
    mtwnDestroyContext(s.context);
}

static void TestInherited(void)
{
    Scene s = MakeScene();
    Play(&s, 1.0, mtwn_compositeReplace);
    Update(&s, 0.0);
    Update(&s, 0.3);
    float expected = Expected(&s, 0.3, 1.0);
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = s.target;
    def.value = (mtwnVec4){0};
    def.kind = mtwn_transitionSpring;
    CHECK(mtwnMakeSpringFromDuration(0.5, 0.0, &def.spring) == mtwn_success, "a spring");
    mtwnPlayerId spring = {0, 0};
    CHECK(mtwnStartTransition(s.context, &def, &spring) == mtwn_success, "a spring started");
    // At its start a spring moves at the speed it inherited.
    CHECK(Near(Velocity(&s, s.target), expected), "the spring inherits the track's velocity");
    mtwnDestroyContext(s.context);
}

static void TestUnderAdditive(void)
{
    // A second player adding over the first reads its velocity in the
    // update: the sum of both tracks' velocities.
    Scene s = MakeScene();
    Play(&s, 1.0, mtwn_compositeReplace);
    Play(&s, 2.0, mtwn_compositeAdd);
    Update(&s, 0.0);
    Update(&s, 0.25);
    float expected = Expected(&s, 0.25, 1.0) + Expected(&s, 0.5, 2.0);
    CHECK(Near(Velocity(&s, s.target), expected), "an additive player over a lazy one");
    mtwnDestroyContext(s.context);
}

static void TestHalfWeight(void)
{
    // Half way from the base toward the track: half its velocity, so not
    // the track's own.
    Scene s = MakeScene();
    mtwnBinding binding = mtwnMakeBinding(0, s.target);
    binding.weight = 0.5f;
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = s.clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(s.context, &def, &id) == mtwn_success, "a player");
    Update(&s, 0.0);
    Update(&s, 0.5);
    CHECK(Near(Velocity(&s, s.target), 0.5f * Expected(&s, 0.5, 1.0)), "at half weight");
    mtwnDestroyContext(s.context);
    // Worked out at once, from the track's own time a moment ago, on a
    // track starting 0.5 s into its clip.
    s = MakeSceneAt(0.5);
    binding.target = s.target;
    def.clip = s.clip;
    CHECK(mtwnCreatePlayer(s.context, &def, &id) == mtwn_success, "a player");
    Update(&s, 0.0);
    Update(&s, 1.0);
    float expected = 0.5f * Expected(&s, 1.0, 1.0);
    CHECK(expected > 1.0f && Near(Velocity(&s, s.target), expected),
          "at half weight on a track starting late");
    mtwnDestroyContext(s.context);
}

static void TestInteger(void)
{
    // An integer target holds its rounded value; its velocity is the
    // unrounded curve's.
    Scene s = MakeScene();
    mtwnTargetId integer;
    CHECK(mtwnCreateTarget(s.context, mtwn_valueInteger, (mtwnVec4){0}, &integer) == mtwn_success,
          "an integer target");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
    CHECK(mtwnCreateClip(s.context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 10.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.track = mtwnDefaultTrackDef(mtwn_valueInteger);
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 2.0;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(s.context, clip, &track, &node) == mtwn_success, "an integer track");
    CHECK(mtwnClip_Seal(s.context, clip) == mtwn_success, "sealed");
    mtwnBinding binding = mtwnMakeBinding(0, integer);
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(s.context, &def, &id) == mtwn_success, "a player");
    Update(&s, 0.0);
    Update(&s, 0.73);
    // Linear, 10 over 2 s: 5 a second, whatever the rounding.
    CHECK(fabsf(Velocity(&s, integer) - 5.0f) < 0.01f, "an integer's from its unrounded curve");
    mtwnDestroyContext(s.context);
}

static void TestBackwards(void)
{
    Scene s = MakeScene();
    mtwnPlayerId p = Play(&s, -1.0, mtwn_compositeReplace);
    Update(&s, 0.0);
    Update(&s, 0.6);
    mtwnPlayerState state;
    CHECK(mtwnPlayer_GetState(s.context, p, &state) == mtwn_success, "a state");
    float expected = Expected(&s, state.currentTime, -1.0);
    CHECK(expected < -1.0f, "the reference moves back");
    CHECK(Near(Velocity(&s, s.target), expected), "played backwards");
    mtwnDestroyContext(s.context);
}

static void TestBackwardsAtEnd(void)
{
    // Played backwards from its end, a track filling none is found a
    // moment ago exactly at its active end: backwards, that is inside it
    // (Web Animations), so its value then is its end and it moves; taken
    // forwards it would have had no effect and no velocity.
    bool found = false;
    for (uint64_t ns = 999990u; ns <= 1000010u && !found; ++ns)
    {
        Scene s = MakeScene();
        mtwnClipId clip = {0, 0};
        mtwnClipDef clipDef = mtwnDefaultClipDef();
        CHECK(mtwnCreateClip(s.context, &clipDef, &clip) == mtwn_success, "a clip");
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 1.0;
        keys[1].value.x = 10.0f;
        keys[0].easing = keys[1].easing = mtwnLinearEasing();
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 2.0;
        mtwnNode node = 0;
        CHECK(mtwnClip_AddTrack(s.context, clip, &track, &node) == mtwn_success, "a track");
        CHECK(mtwnClip_Seal(s.context, clip) == mtwn_success, "sealed");
        mtwnBinding binding = mtwnMakeBinding(0, s.target);
        mtwnPlayerDef def = mtwnDefaultPlayerDef();
        def.clip = clip;
        def.bindings = &binding;
        def.bindingCount = 1;
        def.rate = -1.0;
        def.keep = true;
        mtwnPlayerId id = {0, 0};
        CHECK(mtwnCreatePlayer(s.context, &def, &id) == mtwn_success, "a player");
        CHECK(mtwnContext_Update(s.context, 1000000000u, NULL, 0, NULL) == mtwn_success,
              "the start");
        CHECK(mtwnContext_Update(s.context, 1000000000u + ns, NULL, 0, NULL) == mtwn_success,
              "an update");
        mtwnPlayerState state;
        CHECK(mtwnPlayer_GetState(s.context, id, &state) == mtwn_success, "a state");
        if (state.currentTime + 0.001 == 2.0)
        {
            found = true;
            CHECK(Velocity(&s, s.target) < -1.0f, "a moment ago at its end, backwards, it moved");
        }
        mtwnDestroyContext(s.context);
    }
    CHECK(found, "an update whose moment ago is the track's active end");
}

int main(void)
{
    TestReadLater();
    TestBaseSet();
    TestInherited();
    TestUnderAdditive();
    TestHalfWeight();
    TestInteger();
    TestBackwards();
    TestBackwardsAtEnd();
    return s_failures == 0 ? 0 : 1;
}
