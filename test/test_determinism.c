// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Determinism: a fixed scenario of every value kind,
// every transition spec and clips of nested repeating timelines, marks,
// incremental and added tracks under seeks, reverses, rate changes and
// pauses, with a spell of reduced motion, updated over 600 irregular
// frames, and staggers of a grid and of a seeded order. The bits of every target's
// value, velocity and base after each update, and of every event record,
// are hashed (FNV-1a, 64 bits): the same constant from every compiler and
// platform the library is built on. Run with an argument, it prints the
// hash instead of checking it.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// The scenario's hash, the same everywhere.
static const uint64_t s_expected = 0x7b710979e70fff40u;

enum
{
    TARGETS = 36,
    FRAMES = 600
};

static uint64_t s_hash = 14695981039346656037u;

static void Hash(const void* bytes, size_t size)
{
    const unsigned char* p = (const unsigned char*)bytes;
    for (size_t i = 0; i < size; ++i)
    {
        s_hash = (s_hash ^ p[i]) * 1099511628211u;
    }
}

static void HashFloats(mtwnVec4 v)
{
    uint32_t bits[4];
    memcpy(bits, &v, sizeof bits);
    Hash(bits, sizeof bits);
}

static void HashDouble(double d)
{
    uint64_t bits = 0;
    memcpy(&bits, &d, sizeof bits);
    Hash(&bits, sizeof bits);
}

typedef struct World
{
    mtwnContext* context;
    mtwnTargetId targets[TARGETS];
    mtwnValueKind kinds[TARGETS];
    mtwnClipId clip;
    mtwnPlayerId players[3];
} World;

static mtwnVec4 ValueFor(mtwnValueKind kind, uint32_t seed)
{
    float a = (float)(seed % 7) * 0.375f - 1.0f;
    float b = (float)(seed % 5) * 0.625f;
    switch (kind)
    {
    case mtwn_valueColor:
        return (mtwnVec4){0.125f * (float)(seed % 8), 0.5f, 1.0f - 0.0625f * (float)(seed % 16),
                          seed % 3 == 0 ? 0.0f : 0.75f};
    case mtwn_valueRotation:
    {
        // Unit rotations from exact constants, no C library call: the
        // scenario's inputs are the same bits everywhere.
        static const mtwnVec4 rotations[5] = {{0.0f, 0.0f, 0.0f, 1.0f},
                                              {0.0f, 0.0f, 0.6f, 0.8f},
                                              {0.5f, 0.5f, 0.5f, 0.5f},
                                              {0.8f, 0.0f, 0.0f, -0.6f},
                                              {0.0f, -0.28f, 0.0f, 0.96f}};
        return rotations[seed % 5];
    }
    case mtwn_valueAngle:
        return (mtwnVec4){a * 4.0f, 0, 0, 0};
    case mtwn_valueInteger:
    case mtwn_valueDiscrete:
        return (mtwnVec4){(float)(seed % 11) * 3.0f, 0, 0, 0};
    default:
        return (mtwnVec4){a, b, a * b, b - a};
    }
}

static mtwnEasing EasingFor(uint32_t seed)
{
    mtwnEasing easing = mtwnLinearEasing();
    mtwnResult made = mtwn_success;
    switch (seed % 7)
    {
    case 0:
        made = mtwnMakePowerEasing(mtwn_easeOut, 2.0, &easing);
        break;
    case 1:
        made = mtwnMakeCubicBezier(0.25, 0.1, 0.25, 1.0, &easing);
        break;
    case 2:
        made = mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &easing);
        break;
    case 3:
        made = mtwnMakePennerEasing(mtwn_easingBounce, mtwn_easeInOut, &easing);
        break;
    case 4:
        made = mtwnMakeSteps(4, mtwn_jumpEnd, &easing);
        break;
    case 5:
        made = mtwnMakePowerEasing(mtwn_easeInOut, 2.5, &easing);
        break;
    default:
        made = mtwnMakeBackEasing(mtwn_easeOut, 1.70158, &easing);
        break;
    }
    CHECK(made == mtwn_success, "an easing");
    return easing;
}

// A transition to a new value on target i: an easing, a spring or a decay.
static void Start(World* w, uint32_t i, uint32_t seed)
{
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = w->targets[i];
    def.value = ValueFor(w->kinds[i], seed);
    def.duration = 0.2 + 0.05 * (double)(seed % 9);
    def.delay = seed % 4 == 0 ? 0.1 : 0.0;
    def.easing = EasingFor(seed);
    def.colorSpace = (mtwnColorSpace)(seed % 4);
    def.hueMethod = (mtwnHueMethod)(seed % 4);
    def.angleMethod = (mtwnAngleMethod)(seed % 5);
    uint32_t spec = seed % 5;
    if (spec == 3 && w->kinds[i] != mtwn_valueDiscrete)
    {
        def.spec = mtwn_transitionSpring;
        CHECK(mtwnMakeSpringFromDuration(0.4, 0.25 * (double)(seed % 3), &def.spring) ==
                  mtwn_success,
              "a spring");
    }
    else if (spec == 4 && w->kinds[i] != mtwn_valueDiscrete && !(w->kinds[i] == mtwn_valueColor) &&
             w->kinds[i] != mtwn_valueRotation)
    {
        def.spec = mtwn_transitionDecay;
        def.velocity = (mtwnVec4){3.0f, -2.0f, 1.0f, 0.5f};
        def.velocitySource = mtwn_velocityGiven;
        CHECK(mtwnMakeDecay(4.0, &def.decay) == mtwn_success, "a decay");
    }
    mtwnPlayerId player;
    CHECK(mtwnStartTransition(w->context, &def, &player) == mtwn_success, "a transition");
}

static mtwnPosition At(double offset)
{
    mtwnPosition position;
    memset(&position, 0, sizeof position);
    position.offset = offset;
    return position;
}

static void AddTrack(World* w, mtwnNode parent, uint32_t slot, mtwnValueKind kind, bool incremental,
                     double duration)
{
    mtwnKeyframe keys[3];
    memset(keys, 0, sizeof keys);
    for (uint32_t k = 0; k < 3; ++k)
    {
        keys[k].value = ValueFor(kind, slot * 3 + k + 1);
        keys[k].offset = 0.5 * (double)k;
        keys[k].easing = EasingFor(slot + k);
        keys[k].kind = (mtwnSegmentKind)(k == 1 && kind <= mtwn_valueVec4 ? mtwn_segmentCatmullRom
                                                                          : mtwn_segmentLinear);
    }
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = parent;
    track.slot = slot;
    track.track.kind = kind;
    track.keyframes = keys;
    track.keyframeCount = 3;
    track.timing.duration = duration;
    track.timing.fill = mtwn_fillBoth;
    track.timing.iterations = incremental ? 3.0 : 1.0;
    track.timing.direction = slot % 2 == 0 ? mtwn_directionNormal : mtwn_directionAlternate;
    track.incremental = incremental;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w->context, w->clip, &track, &node) == mtwn_success, "a track");
}

static void MakeClip(World* w)
{
    mtwnClipDef def = mtwnDefaultClipDef();
    def.nodeCapacity = 16;
    def.keyframeCapacity = 32;
    def.slotCapacity = 4;
    CHECK(mtwnCreateClip(w->context, &def, &w->clip) == mtwn_success, "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = 1.5;
    group.timing.iterations = 4.0;
    group.timing.direction = mtwn_directionAlternate;
    group.timing.fill = mtwn_fillBoth;
    mtwnNode outer;
    CHECK(mtwnClip_AddTimeline(w->context, w->clip, &group, &outer) == mtwn_success, "outer");
    group.parent = outer;
    group.timing.duration = 0.4;
    group.timing.iterations = HUGE_VAL;
    group.timing.direction = mtwn_directionNormal;
    group.timing.iterationStart = 0.25;
    mtwnNode inner;
    CHECK(mtwnClip_AddTimeline(w->context, w->clip, &group, &inner) == mtwn_success, "inner");
    AddTrack(w, outer, 0, mtwn_valueVec2, false, 1.2);
    AddTrack(w, inner, 1, mtwn_valueScalar, true, 0.3);
    AddTrack(w, outer, 2, mtwn_valueRotation, true, 0.7);
    AddTrack(w, inner, 3, mtwn_valueColor, false, 0.35);
    mtwnNode mark;
    CHECK(mtwnClip_AddMark(w->context, w->clip, inner, At(0.1), 7, &mark) == mtwn_success &&
              mtwnClip_AddMark(w->context, w->clip, outer, At(0.9), 8, &mark) == mtwn_success,
          "marks");
    CHECK(mtwnClip_Finish(w->context, w->clip) == mtwn_success, "finished");
}

static void MakePlayers(World* w)
{
    // Three players of the clip on the first targets of the clip's kinds:
    // one replacing, one added at a weight, one replacing slower.
    static const uint32_t bound[3][4] = {{1, 0, 6, 4}, {10, 9, 15, 13}, {1, 18, 6, 22}};
    for (uint32_t p = 0; p < 3; ++p)
    {
        mtwnBinding bindings[4];
        for (uint32_t s = 0; s < 4; ++s)
        {
            bindings[s] = mtwnBind(s, w->targets[bound[p][s]]);
            bindings[s].composite = p == 1 ? mtwn_compositeAdd : mtwn_compositeReplace;
            bindings[s].weight = p == 1 ? 0.5f : 1.0f;
        }
        mtwnPlayerDef def = mtwnDefaultPlayerDef();
        def.clip = w->clip;
        def.bindings = bindings;
        def.bindingCount = 4;
        def.keep = true;
        def.rate = p == 2 ? 0.75 : 1.0;
        def.weight = p == 2 ? 0.6 : 1.0;
        CHECK(mtwnCreatePlayer(w->context, &def, &w->players[p]) == mtwn_success, "a player");
    }
}

static World MakeWorld(void)
{
    World w;
    memset(&w, 0, sizeof w);
    mtwnContextDef def = mtwnDefaultContextDef();
    def.eventCapacity = 64;
    def.clipStorageBytes = 1u << 17;
    CHECK(mtwnCreateContext(&def, &w.context) == mtwn_success, "a context");
    for (uint32_t i = 0; i < TARGETS; ++i)
    {
        w.kinds[i] = (mtwnValueKind)(i % 9);
        CHECK(mtwnCreateTarget(w.context, w.kinds[i], ValueFor(w.kinds[i], i), &w.targets[i]) ==
                  mtwn_success,
              "a target");
    }
    MakeClip(&w);
    MakePlayers(&w);
    return w;
}

// A seed for a target at a frame, its bits well mixed, so the choices
// made from it by different moduli are apart.
static uint32_t Seed(uint32_t i, uint32_t frame)
{
    uint32_t x = (i * 0x9E3779B1u) ^ (frame * 0x85EBCA77u);
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return x;
}

// The host's controls at a frame.
static void Control(World* w, uint32_t frame)
{
    mtwnContext* c = w->context;
    mtwnResult r = mtwn_success;
    switch (frame)
    {
    case 90:
        r = mtwnPlayer_Pause(c, w->players[0]);
        break;
    case 120:
        r = mtwnPlayer_Play(c, w->players[0]);
        break;
    case 150:
        r = mtwnPlayer_Reverse(c, w->players[1]);
        break;
    case 200:
        r = mtwnPlayer_Seek(c, w->players[2], 2.25);
        break;
    case 260:
        r = mtwnPlayer_SetRate(c, w->players[0], -1.5);
        break;
    case 330:
        r = mtwnPlayer_SetWeight(c, w->players[1], 0.25);
        break;
    case 400:
        r = mtwnPlayer_Sweep(c, w->players[2], 0.5);
        break;
    case 450:
        r = mtwnContext_SetReducedMotion(c, true);
        break;
    case 500:
        r = mtwnContext_SetReducedMotion(c, false);
        break;
    default:
        break;
    }
    CHECK(r == mtwn_success, "a control");
    // New transitions on the targets no clip player binds, now and then,
    // often enough to replace running ones.
    if (frame % 9 == 0)
    {
        for (uint32_t i = 0; i < TARGETS; ++i)
        {
            uint32_t seed = Seed(i, frame);
            if ((seed >> 20) % 2 == 0 && i != 0 && i != 1 && i != 4 && i != 6 && i != 9 &&
                i != 10 && i != 13 && i != 15 && i != 18 && i != 22)
            {
                Start(w, i, seed);
            }
        }
    }
}

static void HashFrame(World* w)
{
    for (uint32_t i = 0; i < TARGETS; ++i)
    {
        mtwnTargetState state;
        CHECK(mtwnTarget_GetState(w->context, w->targets[i], &state) == mtwn_success, "read");
        HashFloats(state.value);
        HashFloats(state.velocity);
        HashFloats(state.base);
    }
    mtwnEvent events[64];
    uint32_t n = 0;
    uint64_t dropped = 0;
    CHECK(mtwnReadEvents(w->context, events, 64, &n, &dropped) == mtwn_success, "events");
    Hash(&dropped, sizeof dropped);
    for (uint32_t e = 0; e < n; ++e)
    {
        const mtwnEvent* event = &events[e];
        HashDouble(event->hostTime);
        HashDouble(event->clipTime);
        HashDouble(event->firstIteration);
        HashDouble(event->lastIteration);
        Hash(&event->count, sizeof event->count);
        Hash(&event->player.index1, sizeof event->player.index1);
        Hash(&event->node, sizeof event->node);
        Hash(&event->key, sizeof event->key);
        Hash(&event->kind, sizeof event->kind);
        Hash(&event->backwards, sizeof event->backwards);
    }
}

// Staggers: a grid from its centre, eased, and a seeded order, as
// offsets hashed.
static void HashStaggers(void)
{
    double offsets[TARGETS];
    mtwnStagger grid = mtwnDefaultStagger();
    grid.columns = 6;
    grid.origin = mtwn_staggerCentre;
    grid.easing = EasingFor(5);
    CHECK(mtwnComputeStagger(&grid, TARGETS, offsets) == mtwn_success, "a grid stagger");
    for (uint32_t i = 0; i < TARGETS; ++i)
    {
        HashDouble(offsets[i]);
    }
    mtwnStagger order = mtwnDefaultStagger();
    order.origin = mtwn_staggerRandom;
    order.seed = 0x5EEDu;
    order.spacingKind = mtwn_staggerAmount;
    order.spacing = 0.7;
    order.easing = EasingFor(1);
    CHECK(mtwnComputeStagger(&order, TARGETS, offsets) == mtwn_success, "a seeded stagger");
    for (uint32_t i = 0; i < TARGETS; ++i)
    {
        HashDouble(offsets[i]);
    }
}

int main(int argc, char** argv)
{
    (void)argv;
    HashStaggers();
    World w = MakeWorld();
    uint64_t now = 0;
    for (uint32_t frame = 0; frame < FRAMES; ++frame)
    {
        Control(&w, frame);
        // Irregular steps of 4 to 30 ms.
        now += 4000000u + (uint64_t)((frame * 7919u) % 27u) * 1000000u;
        CHECK(mtwnUpdate(w.context, now, NULL, 0, NULL) == mtwn_success, "an update");
        HashFrame(&w);
    }
    mtwnDestroyContext(w.context);
    if (argc > 1)
    {
        printf("0x%016" PRIx64 "\n", s_hash);
        return s_failures == 0 ? 0 : 1;
    }
    CHECK(s_hash == s_expected, "the scenario's hash");
    return s_failures == 0 ? 0 : 1;
}
