// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Determinism: a fixed scenario of every value kind,
// every transition spec and clips of nested repeating timelines, marks,
// incremental and added tracks under seeks, reverses, rate changes and
// pauses, with a spell of reduced motion, updated over 600 irregular
// frames; staggers of a grid and of a seeded order; and the parts no
// target holds, sampled across and past their range: paths of every
// segment kind and channel count, at constant speed and not, with their
// directions and angles, a scale track, scale mixes, the zoom, punch and
// shake, springs' motion and settling, easings, their tables and fitted
// linear() points, rough easings, decays and the rubber band, with
// inputs on either side of each switch between two forms. The bits
// of every target's value, velocity and base after each update, of every
// event record and of every sample are hashed (FNV-1a, 64 bits): the
// same constant from every compiler and platform the library is built
// on. Run with an argument, it prints the hash instead of checking it.

#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/clip.h"
#include "maul-tween/event.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
#include "maul-tween/player.h"
#include "maul-tween/sample.h"
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
static const uint64_t s_expected = 0xd952916a5edaf12fu;

enum
{
    // Four targets of each of the ten value kinds.
    KINDS = 10,
    TARGETS = 40,
    FRAMES = 600,
    // Samples of each part from progress -0.25 to 1.25.
    SAMPLES = 33
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
    case mtwn_valueScale:
        return (mtwnVec4){0.25f + 0.5f * (float)(seed % 7), 0, 0, 0};
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
        made = mtwnMakeCubicBezierEasing(0.25, 0.1, 0.25, 1.0, &easing);
        break;
    case 2:
        made = mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &easing);
        break;
    case 3:
        made = mtwnMakePennerEasing(mtwn_easingBounce, mtwn_easeInOut, &easing);
        break;
    case 4:
        made = mtwnMakeStepsEasing(4, mtwn_jumpEnd, &easing);
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
    CHECK(mtwnClip_Seal(w->context, w->clip) == mtwn_success, "sealed");
}

static void MakePlayers(World* w)
{
    // Three players of the clip on the first targets of the clip's kinds:
    // one replacing, one added at a weight, one replacing slower.
    static const uint32_t bound[3][4] = {{1, 0, 6, 4}, {11, 10, 16, 14}, {1, 20, 6, 24}};
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
        def.weight = p == 2 ? 0.6f : 1.0f;
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
        w.kinds[i] = (mtwnValueKind)(i % KINDS);
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
        r = mtwnPlayer_SetWeight(c, w->players[1], 0.25f);
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
            if ((seed >> 20) % 2 == 0 && i != 0 && i != 1 && i != 4 && i != 6 && i != 10 &&
                i != 11 && i != 14 && i != 16 && i != 20 && i != 24)
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
    CHECK(mtwnContext_ReadEvents(w->context, events, 64, &n, &dropped) == mtwn_success, "events");
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

// Staggers: a grid from its center, eased, and a seeded order, as
// offsets hashed.
static void HashStaggers(void)
{
    double offsets[TARGETS];
    mtwnStagger grid = mtwnDefaultStagger();
    grid.columns = 6;
    grid.origin = mtwn_staggerCenter;
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

// The k-th of SAMPLES progresses from -0.25 to 1.25, each exact.
static double Progress(uint32_t k)
{
    return -0.25 + 0.046875 * (double)k;
}

static void HashDoubles(const double* values, uint32_t count)
{
    for (uint32_t c = 0; c < count; ++c)
    {
        HashDouble(values[c]);
    }
}

// Progresses at the ends that Progress never gives: both zeros, the
// smallest step past 0 either way, and the doubles beside 1.
static const double s_ends[6] = {
    -0.0, 0.0, 0x1p-1074, -0x1p-1074, 0x1.fffffffffffffp-1, 0x1.0000000000001p0};

static void HashPathAt(const mtwnPath* path, double progress)
{
    double value[4];
    double tangent[4];
    double direction[4];
    SamplePath(path, progress, value, tangent);
    ComputePathDirection(path, progress, direction);
    HashDoubles(value, 4);
    HashDoubles(tangent, 4);
    HashDoubles(direction, 4);
    HashDouble(ComputePathAngle(path, progress));
}

static void HashPath(const mtwnPath* path)
{
    for (uint32_t k = 0; k < SAMPLES; ++k)
    {
        HashPathAt(path, Progress(k));
    }
    for (uint32_t e = 0; e < 6; ++e)
    {
        HashPathAt(path, s_ends[e]);
    }
}

// Paths of every segment kind and channel count, timed by their easings
// and at constant speed (which a step, of no length, cannot be), with a
// scale track at constant speed in its logarithm.
static void HashPaths(void)
{
    static const double offsets[4] = {0.0, 0.3, 0.55, 1.0};
    for (uint32_t kind = mtwn_segmentLinear; kind <= mtwn_segmentMonotone; ++kind)
    {
        for (uint8_t channels = 1; channels <= 4; ++channels)
        {
            for (uint32_t constant = 0; constant < 2; ++constant)
            {
                if (constant == 1 && kind == mtwn_segmentStep)
                {
                    continue;
                }
                mtwnKeyframe keys[4];
                memset(keys, 0, sizeof keys);
                for (uint32_t k = 0; k < 4; ++k)
                {
                    keys[k].value = ValueFor(mtwn_valueVec4, 3 * k + kind + channels);
                    keys[k].in = ValueFor(mtwn_valueVec4, 5 * k + 2);
                    keys[k].out = ValueFor(mtwn_valueVec4, 7 * k + 3);
                    keys[k].offset = offsets[k];
                    keys[k].easing = EasingFor(k + kind);
                    keys[k].kind = (mtwnSegmentKind)kind;
                }
                double lengths[4];
                mtwnPath path;
                CHECK(mtwnMakePath(keys, 4, channels, 0.5, constant == 1 ? lengths : NULL, &path) ==
                          mtwn_success,
                      "a path");
                HashPath(&path);
            }
        }
    }
    mtwnKeyframe keys[3];
    memset(keys, 0, sizeof keys);
    for (uint32_t k = 0; k < 3; ++k)
    {
        keys[k].value = ValueFor(mtwn_valueScale, 2 * k + 1);
        keys[k].offset = 0.5 * (double)k;
        keys[k].easing = EasingFor(k);
        keys[k].kind = k == 0 ? mtwn_segmentCatmullRom : mtwn_segmentMonotone;
    }
    double lengths[3];
    mtwnTrack track;
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueScale);
    CHECK(mtwnMakeTrack(&def, keys, 3, lengths, NULL, &track) == mtwn_success, "a scale track");
    HashPath(&track.path);
    for (uint32_t k = 0; k < SAMPLES; ++k)
    {
        HashFloats(SampleTrack(&track, Progress(k)));
    }
}

// Scale mixes, the zoom between views, punch and shake.
static void HashValues(void)
{
    static const double scales[5][2] = {
        {1.0, 4.0}, {0.25, 8.0}, {3.0, 0.5}, {2.0, 2.0}, {1e-3, 1e3}};
    for (uint32_t s = 0; s < 5; ++s)
    {
        mtwnScaleMix mix;
        CHECK(mtwnMakeScaleMix(scales[s][0], scales[s][1], &mix) == mtwn_success, "a scale mix");
        for (uint32_t k = 0; k < SAMPLES; ++k)
        {
            HashDouble(MixScale(&mix, Progress(k)));
        }
    }
    static const mtwnZoomView views[4] = {
        {0.0, 0.0, 1.0}, {10.0, -4.0, 0.5}, {3.0, 2.0, 8.0}, {3.0, 2.0, 0.125}};
    static const double rhos[2] = {1.4142135623730951, 0.5};
    for (uint32_t v = 0; v < 3; ++v)
    {
        for (uint32_t r = 0; r < 2; ++r)
        {
            mtwnZoomMix mix;
            CHECK(mtwnMakeZoomMix(views[v], views[v + 1], rhos[r], &mix) == mtwn_success, "a zoom");
            HashDouble(ComputeZoomLength(&mix));
            for (uint32_t k = 0; k < SAMPLES; ++k)
            {
                mtwnZoomView view = MixZoom(&mix, Progress(k));
                HashDouble(view.x);
                HashDouble(view.y);
                HashDouble(view.width);
            }
        }
    }
    mtwnPunch punch;
    mtwnShake shake;
    CHECK(mtwnMakePunch(4.0, 0.8, 2.0, &punch) == mtwn_success &&
              mtwnMakeShake(12.0, 0.9, 1.5, 0x5EEDu, &shake) == mtwn_success,
          "a punch and a shake");
    for (uint32_t k = 0; k < SAMPLES; ++k)
    {
        double seconds = 0.03125 * (double)k;
        HashDouble(SamplePunch(&punch, seconds));
        for (uint32_t channel = 0; channel < 4; ++channel)
        {
            HashDouble(SampleShake(&shake, channel, seconds));
            HashDouble(SampleShakeNoise(&shake, channel, seconds));
        }
    }
}

// Springs under-, critically and overdamped, undamped and from a
// duration: their motion, a sample from an offset and a velocity, and
// their settling time.
static void HashSprings(void)
{
    mtwnSpring springs[5];
    CHECK(mtwnMakeSpring(170.0, 26.0, 1.0, &springs[0]) == mtwn_success &&
              mtwnMakeSpring(100.0, 20.0, 1.0, &springs[1]) == mtwn_success &&
              mtwnMakeSpring(100.0, 40.0, 1.0, &springs[2]) == mtwn_success &&
              mtwnMakeSpringFromFrequency(2.0, 0.0, &springs[3]) == mtwn_success &&
              mtwnMakeSpringFromDuration(0.5, 0.3, &springs[4]) == mtwn_success,
          "springs");
    for (uint32_t s = 0; s < 5; ++s)
    {
        for (uint32_t k = 0; k < SAMPLES; ++k)
        {
            double seconds = 0.03125 * (double)k;
            mtwnSpringMotion motion = ComputeSpringMotion(&springs[s], seconds);
            HashDouble(motion.offsetFromOffset);
            HashDouble(motion.offsetFromVelocity);
            HashDouble(motion.velocityFromOffset);
            HashDouble(motion.velocityFromVelocity);
            double offset = 0.0;
            double velocity = 0.0;
            SampleSpring(&springs[s], -1.0, 2.5, seconds, &offset, &velocity);
            HashDouble(offset);
            HashDouble(velocity);
        }
        HashDouble(ComputeSpringSettlingTime(&springs[s], -1.0, 2.5, 1e-3, 1e-2));
    }
}

static void HashEasing(const mtwnEasing* easing)
{
    for (uint32_t k = 0; k < SAMPLES; ++k)
    {
        HashDouble(EaseAt(easing, Progress(k), false));
        HashDouble(EaseAt(easing, Progress(k), true));
    }
    for (uint32_t e = 0; e < 6; ++e)
    {
        HashDouble(EaseAt(easing, s_ends[e], false));
        HashDouble(EaseAt(easing, s_ends[e], true));
    }
}

static void HashPoints(const mtwnLinearPoint* points, uint32_t count)
{
    Hash(&count, sizeof count);
    for (uint32_t p = 0; p < count; ++p)
    {
        HashDouble(points[p].input);
        HashDouble(points[p].output);
    }
}

// Easings of every maker the scenario uses and their even tables, rough
// easings of every taper and flag, and linear() points fitted to easings
// that are not steps.
static void HashEasings(void)
{
    for (uint32_t seed = 0; seed < 7; ++seed)
    {
        mtwnEasing easing = EasingFor(seed);
        HashEasing(&easing);
        float table[SAMPLES];
        CHECK(mtwnSampleEasingTable(&easing, SAMPLES, table) == mtwn_success, "a table");
        Hash(table, sizeof table);
        mtwnLinearPoint points[1024];
        uint32_t count = 0;
        if (seed % 7 != 4)
        {
            CHECK(mtwnFitLinearEasing(&easing, 1e-3, points, 1024, &count) == mtwn_success &&
                      count <= 1024,
                  "a fit");
            HashPoints(points, count <= 1024 ? count : 0);
        }
    }
    static const uint8_t flags[4] = {0, mtwn_roughClamp, mtwn_roughRising, mtwn_roughZigZag};
    for (uint32_t taper = mtwn_roughTaperNone; taper <= mtwn_roughTaperBoth; ++taper)
    {
        for (uint32_t f = 0; f < 4; ++f)
        {
            mtwnRoughDef def;
            memset(&def, 0, sizeof def);
            def.shape = EasingFor(taper + f);
            def.strength = 0.3;
            def.seed = 0x5EEDu + taper * 4u + f;
            def.taper = (mtwnRoughTaper)taper;
            def.flags = flags[f];
            mtwnLinearPoint points[20];
            mtwnEasing rough;
            CHECK(mtwnMakeRoughEasing(&def, points, 20, &rough) == mtwn_success, "a rough easing");
            HashPoints(points, 20);
            HashEasing(&rough);
        }
    }
}

// Decay alone, landing and bounded, and the rubber band and its inverse.
static void HashDecays(void)
{
    mtwnDecay decay;
    mtwnDecay landing;
    mtwnBoundedDecay bounded;
    mtwnSpring spring;
    CHECK(mtwnMakeDecay(4.0, &decay) == mtwn_success &&
              mtwnMakeDecayToRest(0.0, 900.0, 300.0, &landing) == mtwn_success &&
              mtwnMakeSpring(170.0, 26.0, 1.0, &spring) == mtwn_success &&
              mtwnMakeBoundedDecay(&decay, &spring, -50.0, 200.0, &bounded) == mtwn_success,
          "decays");
    for (uint32_t k = 0; k < SAMPLES; ++k)
    {
        double seconds = 0.0625 * (double)k;
        double out[4];
        SampleDecay(&decay, 10.0, 900.0, seconds, &out[0], &out[1]);
        SampleDecay(&landing, 0.0, 900.0, seconds, &out[2], &out[3]);
        HashDoubles(out, 4);
        SampleBoundedDecay(&bounded, 10.0, 900.0, seconds, &out[0], &out[1]);
        HashDoubles(out, 2);
        double distance = 40.0 * Progress(k);
        HashDouble(ApplyRubberBand(distance, 320.0, 0.55));
        HashDouble(InvertRubberBand(distance, 320.0, 0.55));
    }
    HashDouble(ComputeDecayStopTime(&decay, 900.0, 0.5));
    HashDouble(ComputeDecayCrossingTime(&decay, 10.0, 900.0, 150.0));
    HashDouble(ComputeBoundedDecayStopTime(&bounded, 10.0, 900.0, 1e-3, 1e-2));
}

// Inputs just below and just above each switch between two forms, which
// samples across the range miss.
static void HashEdges(void)
{
    // Sine and circ in every mode, and Beziers whose extensions take each
    // tangent: through P1 or P2 past each end, or flat at 0 and at 1.
    for (uint32_t mode = mtwn_easeIn; mode <= mtwn_easeOutIn; ++mode)
    {
        mtwnEasing sine;
        mtwnEasing circ;
        CHECK(mtwnMakePennerEasing(mtwn_easingSine, (mtwnEaseMode)mode, &sine) == mtwn_success &&
                  mtwnMakePennerEasing(mtwn_easingCirc, (mtwnEaseMode)mode, &circ) == mtwn_success,
              "sine and circ");
        HashEasing(&sine);
        HashEasing(&circ);
    }
    static const double beziers[5][4] = {{0.25, 0.1, 0.25, 1.0},
                                         {0.0, 0.0, 0.5, 2.0},
                                         {0.5, 0.0, 1.0, 1.0},
                                         {0.0, 0.5, 0.0, 0.5},
                                         {1.0, 0.5, 1.0, 0.5}};
    for (uint32_t b = 0; b < 5; ++b)
    {
        mtwnEasing bezier;
        CHECK(mtwnMakeCubicBezierEasing(beziers[b][0], beziers[b][1], beziers[b][2], beziers[b][3],
                                        &bezier) == mtwn_success,
              "a Bezier");
        HashEasing(&bezier);
    }
    // A scale mix's product switches to the sum of logarithms at an
    // exponent of 700: from 1e-300 to 1e300 the ratio's logarithm is about
    // 1381.55, so 0.5066 and 0.5068 sit either side, and past both ends.
    mtwnScaleMix wide;
    CHECK(mtwnMakeScaleMix(1e-300, 1e300, &wide) == mtwn_success, "a wide scale mix");
    static const double across[6] = {0.5066, 0.5068, -0.5066, -0.5068, 1.5, -0.0};
    for (uint32_t a = 0; a < 6; ++a)
    {
        HashDouble(MixScale(&wide, across[a]));
    }
    // An undamped spring of 2 Hz turns 4 pi a second, so its phase passes
    // 2^52, where it holds no angle, between 3.58e14 and 3.59e14 seconds;
    // and infinity.
    mtwnSpring undamped;
    mtwnSpring nearCritical;
    mtwnSpring over;
    CHECK(mtwnMakeSpringFromFrequency(2.0, 0.0, &undamped) == mtwn_success &&
              mtwnMakeSpring(100.0, 20.02, 1.0, &nearCritical) == mtwn_success &&
              mtwnMakeSpring(100.0, 40.0, 1.0, &over) == mtwn_success,
          "edge springs");
    static const double far[3] = {3.58e14, 3.59e14, HUGE_VAL};
    // Overdamped springs switch from the series at a damped rate times the
    // time of 1/2: about 0.4473 a second for the nearly critical one, so
    // 1.1 and 1.13 seconds, and 17.32 for the other, so 0.0288 and 0.0289.
    static const double series[2][2] = {{1.1, 1.13}, {0.0288, 0.0289}};
    for (uint32_t i = 0; i < 3; ++i)
    {
        mtwnSpringMotion motion = ComputeSpringMotion(&undamped, far[i]);
        HashDouble(motion.offsetFromOffset);
        HashDouble(motion.offsetFromVelocity);
        HashDouble(motion.velocityFromOffset);
        HashDouble(motion.velocityFromVelocity);
    }
    for (uint32_t i = 0; i < 2; ++i)
    {
        for (uint32_t s = 0; s < 2; ++s)
        {
            double offset = 0.0;
            double velocity = 0.0;
            SampleSpring(s == 0 ? &nearCritical : &over, -1.0, 2.5, series[s][i], &offset,
                         &velocity);
            HashDouble(offset);
            HashDouble(velocity);
        }
    }
    // A shake's noise holds lattice coordinates below 2^53: at 12 points a
    // second, 7.5e14 seconds is inside and 7.51e14 past it; a shake's
    // points over its duration must stay below 2^53 too.
    mtwnShake shake;
    mtwnShake refused;
    CHECK(mtwnMakeShake(12.0, 0.9, 1.5, 0x5EEDu, &shake) == mtwn_success, "a shake");
    static const double noiseAt[4] = {7.5e14, 7.51e14, -0.0, -1e-300};
    for (uint32_t i = 0; i < 4; ++i)
    {
        HashDouble(SampleShakeNoise(&shake, 1, noiseAt[i]));
    }
    mtwnResult inside = mtwnMakeShake(1.0, 0x1.fffffffffffffp52, 1.0, 1, &refused);
    mtwnResult past = mtwnMakeShake(1.0, 0x1p53, 1.0, 1, &refused);
    Hash(&inside, sizeof inside);
    Hash(&past, sizeof past);
}

int main(int argc, char** argv)
{
    (void)argv;
    HashStaggers();
    HashPaths();
    HashValues();
    HashSprings();
    HashEasings();
    HashDecays();
    HashEdges();
    World w = MakeWorld();
    uint64_t now = 0;
    for (uint32_t frame = 0; frame < FRAMES; ++frame)
    {
        Control(&w, frame);
        // Irregular steps of 4 to 30 ms.
        now += 4000000u + (uint64_t)((frame * 7919u) % 27u) * 1000000u;
        CHECK(mtwnContext_Update(w.context, now, NULL, 0, NULL) == mtwn_success, "an update");
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
