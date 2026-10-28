// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Numbers near the floats' bounds against an oracle:
// each place a number is held, a transition's eased value, a clip track's
// segment, an incremental repeat and two players adding, gives the same
// arithmetic done in double from the inputs and held once where a target
// takes it (for players adding, once a layer).
// A value at the bound where the oracle is not, or short of it where the
// oracle is, fails; a check of finite values alone passes both.

#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/clip.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const double s_largest = 0x1.fffffep127;
static uint64_t s_state = 0x2545F4914F6CDD1Du;

static uint64_t Next(void)
{
    s_state ^= s_state << 13;
    s_state ^= s_state >> 7;
    s_state ^= s_state << 17;
    return s_state;
}

// A double from [0, 1).
static double Unit(void)
{
    return (double)(Next() >> 11) * 0x1p-53;
}

// A float of either sign: near the largest, a third or a tenth of it, an
// ordinary size or 0.
static float End(void)
{
    static const double s_sizes[] = {s_largest, s_largest / 3.0, 3e37, 1e30, 1.0, 0.0};
    double v = s_sizes[Next() % 6u] * (0.5 + 0.5 * Unit());
    return (float)((Next() & 1u) != 0 ? -v : v);
}

// The oracle's value as a target holds it: within the finite floats.
static float Held(double exact)
{
    return (float)fmin(fmax(exact, -s_largest), s_largest);
}

// The result against the oracle held: within a few float roundings of the
// terms summed (their magnitudes add to scale).
static bool Agrees(float got, double exact, double scale)
{
    double want = (double)Held(exact);
    return (double)got == want || fabs((double)got - want) <= 0x1p-21 * scale + 0x1p-149;
}

typedef struct World
{
    mtwnContext* context;
    mtwnTargetId target;
    uint64_t now;
} World;

static World MakeWorld(float base)
{
    World w = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &w.context) == mtwn_success, "a context");
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScalar, (mtwnVec4){base, 0, 0, 0}, &w.target) ==
              mtwn_success,
          "a target");
    CHECK(mtwnContext_Update(w.context, 0, NULL, 0, NULL) == mtwn_success, "the first update");
    return w;
}

// The target's value at seconds from the world's start.
static float At(World* w, double seconds)
{
    CHECK(mtwnContext_Update(w->context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) ==
              mtwn_success,
          "updated");
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(w->context, w->target, &state) == mtwn_success, "read");
    return state.value.x;
}

static mtwnEasing BackOut(void)
{
    mtwnEasing easing = mtwnLinearEasing();
    if (Next() % 4u != 0)
    {
        CHECK(mtwnMakeBackEasing(mtwn_easeOut, 100.0 * Unit(), &easing) == mtwn_success,
              "a back out");
    }
    return easing;
}

static void TestTransitions(void)
{
    uint32_t wrong = 0;
    uint32_t held = 0;
    for (uint32_t trial = 0; trial < 300u; ++trial)
    {
        float a = End();
        float b = End();
        World w = MakeWorld(a);
        mtwnTransitionDef def = mtwnDefaultTransitionDef();
        def.target = w.target;
        def.value = (mtwnVec4){b, 0, 0, 0};
        def.duration = 1.0;
        def.easing = BackOut();
        mtwnPlayerId player;
        CHECK(mtwnStartTransition(w.context, &def, &player) == mtwn_success, "a transition");
        At(&w, 1.0);
        for (int k = 1; k <= 16; ++k)
        {
            double p = (double)k / 16.0;
            double e = EaseAt(&def.easing, p, false);
            double change = e * ((double)b - (double)a);
            double exact = e == 1.0 ? (double)b : (double)a + change;
            float got = At(&w, 1.0 + p);
            wrong += !Agrees(got, exact, fabs((double)a) + fabs(change));
            held += fabs(exact) > s_largest;
        }
        mtwnDestroyContext(w.context);
    }
    CHECK(wrong == 0, "a transition's eased value as the oracle");
    CHECK(held > 0, "some past the floats, held");
}

// A clip of one track on the world's target, played by players adding or
// replacing.
static void Play(World* w, const mtwnClipTrackDef* track, int players, mtwnComposite composite)
{
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(w->context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w->context, clip, track, &node) == mtwn_success &&
              mtwnClip_Seal(w->context, clip) == mtwn_success,
          "a track");
    for (int i = 0; i < players; ++i)
    {
        mtwnBinding binding = mtwnBind(0, w->target);
        binding.composite = composite;
        mtwnPlayerDef def = mtwnDefaultPlayerDef();
        def.clip = clip;
        def.bindings = &binding;
        def.bindingCount = 1;
        mtwnPlayerId id;
        CHECK(mtwnCreatePlayer(w->context, &def, &id) == mtwn_success, "a player");
    }
    At(w, 1.0);
}

static void TestTracks(void)
{
    uint32_t wrong = 0;
    for (uint32_t trial = 0; trial < 300u; ++trial)
    {
        World w = MakeWorld(0.0f);
        mtwnKeyframe keys[3];
        memset(keys, 0, sizeof keys);
        for (int i = 0; i < 3; ++i)
        {
            keys[i].offset = 0.5 * i;
            keys[i].value.x = End();
            keys[i].easing = BackOut();
        }
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.keyframes = keys;
        track.keyframeCount = 3;
        track.timing.duration = 1.0;
        Play(&w, &track, 1, mtwn_compositeReplace);
        for (int k = 1; k < 16; ++k)
        {
            double t = (double)k / 16.0;
            int i = t < 0.5 ? 0 : 1;
            double e = EaseAt(&keys[i].easing, (t - keys[i].offset) / 0.5, false);
            double a = (double)keys[i].value.x;
            double change = e * ((double)keys[i + 1].value.x - a);
            double exact = e == 1.0 ? (double)keys[i + 1].value.x : a + change;
            wrong += !Agrees(At(&w, 1.0 + t), exact, fabs(a) + fabs(change));
        }
        mtwnDestroyContext(w.context);
    }
    CHECK(wrong == 0, "a clip track's segments as the oracle");
}

static void TestIncremental(void)
{
    uint32_t wrong = 0;
    uint32_t held = 0;
    for (uint32_t trial = 0; trial < 300u; ++trial)
    {
        World w = MakeWorld(0.0f);
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 1.0;
        keys[0].value.x = End();
        keys[1].value.x = End();
        keys[0].easing = keys[1].easing = mtwnLinearEasing();
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 1.0;
        track.timing.iterations = 4.0;
        track.incremental = true;
        Play(&w, &track, 1, mtwn_compositeReplace);
        double first = (double)keys[0].value.x;
        double step = (double)keys[1].value.x - first;
        for (int k = 1; k < 64; ++k)
        {
            double t = (double)k / 16.0;
            double exact = first + t * step;
            wrong += !Agrees(At(&w, 1.0 + t), exact, fabs(first) + fabs(t * step));
            held += fabs(exact) > s_largest;
        }
        mtwnDestroyContext(w.context);
    }
    CHECK(wrong == 0, "an incremental repeat as the oracle");
    CHECK(held > 0, "some past the floats, held");
}

// A clip holding v from start to end.
static mtwnClipTrackDef Constant(mtwnKeyframe keys[2], float v)
{
    memset(keys, 0, 2 * sizeof *keys);
    keys[1].offset = 1.0;
    keys[0].value.x = keys[1].value.x = v;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    return track;
}

static void TestAdding(void)
{
    // A base and two players adding: each layer's sum in double, held
    // where the target takes it, the next layer composing onto that.
    // From 3e38, adding 3e38 holds at the bound, and
    // -3e38 then leaves the bound less 3e38.
    uint32_t wrong = 0;
    for (uint32_t trial = 0; trial < 300u; ++trial)
    {
        float base = trial == 0 ? 3e38f : End();
        float v[2] = {trial == 0 ? 3e38f : End(), trial == 0 ? -3e38f : End()};
        World w = MakeWorld(base);
        for (int i = 0; i < 2; ++i)
        {
            mtwnKeyframe keys[2];
            mtwnClipTrackDef track = Constant(keys, v[i]);
            Play(&w, &track, 1, mtwn_compositeAdd);
        }
        double under = (double)Held((double)base + (double)v[0]);
        double exact = under + (double)v[1];
        double scale = fabs(under) + fabs((double)v[1]);
        float got = At(&w, 1.5);
        wrong += !Agrees(got, exact, scale);
        if (trial == 0)
        {
            CHECK(got == (float)(s_largest - (double)3e38f), "a layer held, the next on it");
        }
        mtwnDestroyContext(w.context);
    }
    CHECK(wrong == 0, "two players adding as the oracle");
}

static void TestStill(void)
{
    // Springs between ends near the bounds, some overshooting past them:
    // a number held at a bound is still, its velocity
    // that of the value held, not the curve's.
    uint32_t moving = 0;
    uint32_t held = 0;
    for (uint32_t trial = 0; trial < 100u; ++trial)
    {
        float a = trial == 0 ? 0.0f : End();
        float b = trial == 0 ? 3e38f : End();
        World w = MakeWorld(a);
        mtwnTransitionDef def = mtwnDefaultTransitionDef();
        def.target = w.target;
        def.value = (mtwnVec4){b, 0, 0, 0};
        def.spec = mtwn_transitionSpring;
        CHECK(mtwnMakeSpringFromFrequency(1.0 + 3.0 * Unit(), 0.05 + 0.5 * Unit(), &def.spring) ==
                  mtwn_success,
              "a spring");
        mtwnPlayerId player;
        CHECK(mtwnStartTransition(w.context, &def, &player) == mtwn_success, "a transition");
        for (int k = 1; k <= 120; ++k)
        {
            CHECK(mtwnContext_Update(w.context, (uint64_t)k * 25000000u, NULL, 0, NULL) ==
                      mtwn_success,
                  "updated");
            mtwnTargetState state;
            CHECK(mtwnTarget_GetState(w.context, w.target, &state) == mtwn_success, "read");
            bool bound = fabsf(state.value.x) == 0x1.fffffep127f;
            held += bound;
            moving += bound && state.velocity.x != 0.0f;
        }
        mtwnDestroyContext(w.context);
    }
    CHECK(held > 0, "some held at a bound");
    CHECK(moving == 0, "held at a bound: still");
    // Exactly the largest float is inside the floats, not held past them: a
    // decay from there moves at its velocity.
    World top = MakeWorld(0x1.fffffep127f);
    mtwnTransitionDef fling = mtwnDefaultTransitionDef();
    fling.target = top.target;
    fling.spec = mtwn_transitionDecay;
    fling.velocitySource = mtwn_velocityGiven;
    fling.velocity = (mtwnVec4){-100.0f, 0, 0, 0};
    mtwnPlayerId flung;
    mtwnTargetState start;
    CHECK(mtwnStartTransition(top.context, &fling, &flung) == mtwn_success &&
              mtwnContext_Update(top.context, 25000000u, NULL, 0, NULL) == mtwn_success &&
              mtwnTarget_GetState(top.context, top.target, &start) == mtwn_success,
          "flung down from the largest float");
    CHECK(start.value.x == 0x1.fffffep127f && start.velocity.x == -100.0f,
          "there, at its velocity");
    mtwnDestroyContext(top.context);
    // A clip track eased past the bound, by one player replacing and by
    // two adding.
    for (int players = 1; players <= 2; ++players)
    {
        held = 0;
        moving = 0;
        World w = MakeWorld(0.0f);
        for (int i = 0; i < players; ++i)
        {
            mtwnKeyframe keys[2];
            memset(keys, 0, sizeof keys);
            keys[1].offset = 1.0;
            keys[1].value.x = 3e38f;
            keys[1].easing = mtwnLinearEasing();
            CHECK(mtwnMakeBackEasing(mtwn_easeOut, 100.0, &keys[0].easing) == mtwn_success,
                  "a back out");
            mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
            track.keyframes = keys;
            track.keyframeCount = 2;
            track.timing.duration = 1.0;
            Play(&w, &track, 1, players == 1 ? mtwn_compositeReplace : mtwn_compositeAdd);
        }
        for (int k = 1; k < 60; ++k)
        {
            CHECK(mtwnContext_Update(w.context, 1000000000u + (uint64_t)k * 16666667u, NULL, 0,
                                     NULL) == mtwn_success,
                  "updated");
            mtwnTargetState state;
            CHECK(mtwnTarget_GetState(w.context, w.target, &state) == mtwn_success, "read");
            bool bound = fabsf(state.value.x) == 0x1.fffffep127f;
            held += bound;
            moving += bound && state.velocity.x != 0.0f;
        }
        CHECK(held > 0 && moving == 0,
              players == 1 ? "a track held at a bound: still" : "two adding, held: still");
        mtwnDestroyContext(w.context);
    }
}

int main(void)
{
    TestTransitions();
    TestTracks();
    TestIncremental();
    TestAdding();
    TestStill();
    return s_failures == 0 ? 0 : 1;
}
