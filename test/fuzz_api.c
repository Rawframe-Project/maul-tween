// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The public API driven by bytes: clips
// built node by node with any timing, valid or not; targets of every
// kind; players with layered bindings, controlled and destroyed;
// transitions; targets destroyed under their players; reduced motion;
// updates any time apart. One script runs on three contexts: one alone,
// one on a task system that runs its ranges at once, and one counting
// every record without the sweep's shortcuts. Each run hashes every
// call's status, every target's state after every update, the changed
// targets and every record; the three must be the same bits, values
// finite and records ordered, counted and within their update's span.
//
// Built with MAUL_TWEEN_FUZZ (Clang) it is a libFuzzer target over a
// corpus directory; in the test suite it runs a fixed set of random
// inputs.

#include "test_hooks.h"

#include "maul-tween/clip.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    s_targets = 8,
    s_players = 4,
    s_steps = 64,
};

// The input, read a byte at a time; zeros past its end.
typedef struct Bytes
{
    const uint8_t* data;
    size_t size;
    size_t at;
} Bytes;

static uint32_t Byte(Bytes* in)
{
    return in->at < in->size ? in->data[in->at++] : 0u;
}

static uint32_t Below(Bytes* in, uint32_t n)
{
    return Byte(in) % n;
}

// A double from a few kinds: small multiples of 1/64, tenths, zero,
// negative, huge (1e12, or 1e308 past every time limit), infinite and
// NaN, so refusals are tried too.
static double Wild(Bytes* in, double scale)
{
    uint32_t b = Byte(in);
    switch (b % 12)
    {
    case 0:
        return 0.0;
    case 1:
        return -scale * (double)Byte(in) / 255.0;
    case 2:
        return HUGE_VAL;
    case 3:
        return (double)NAN;
    case 4:
        return b / 12 % 2 == 0 ? 1e12 : 1e308;
    case 5:
    case 6:
        return (double)(1 + Byte(in)) / 10.0 * scale / 8.0;
    default:
        return (double)(1 + Byte(in)) / 64.0 * scale / 2.0;
    }
}

// The context a script runs on, and whether a host easing ran on it while
// it was not busy: every call running host code must flag it,
// and this finds one that does not.
static mtwnContext* s_watched;
static bool s_unflagged;

// The power easing of 3.5 as a host's, watching its context.
static double Watched(void* unused, double progress)
{
    (void)unused;
    s_unflagged = s_unflagged || s_watched == NULL || !mtwnTestIsBusy(s_watched);
    return pow(progress, 3.5);
}

// A host curve 0 and 1 at its ends and near 1.5e44 between: whatever the
// change it eases, it goes past the floats' range, where its values are
// held. Watching its context too.
static double Overshooting(void* unused, double progress)
{
    (void)unused;
    s_unflagged = s_unflagged || s_watched == NULL || !mtwnTestIsBusy(s_watched);
    return progress + 1e45 * progress * progress * (1.0 - progress);
}

// A value or velocity: Wild's draws, but one in twelve a signed fraction
// of the largest float, so the script reaches the floats' range without
// a host curve tuned to. It takes the same bytes as
// Wild, so a seed's later draws keep their meaning.
static double Value(Bytes* in, double scale)
{
    size_t at = in->at;
    uint32_t b = Byte(in);
    if (b % 12 != 11)
    {
        in->at = at;
        return Wild(in, scale);
    }
    // About one draw in 64 (48 of this case's 256) is a value that breaks
    // arithmetic exactly; the rest a fraction of the largest float.
    static const double rare[6] = {0x1.fffffep127, -0x1.fffffep127, 0x1p-149, -0x1p-149,
                                   -0.0,           1.0 - 0x1p-24};
    uint32_t f = Byte(in);
    if (f < 48)
    {
        return rare[f % 6];
    }
    double fraction = (double)(1 + f) / 256.0;
    return (b / 12 % 2 == 0 ? fraction : -fraction) * 0x1.fffffep127;
}

// An offset or a time: Wild's draws, but about one in 61 (10 of its last
// case's 256) a progress at the edge of its rounding (just below 1, at
// the floats' and the doubles' last steps there, the smallest subnormal,
// -0) or of a seek's size (MTWN_SEEK_LIMIT, an ulp past it, 1e308, its
// negative and the largest double). It takes the same bytes as Wild.
static double Progress(Bytes* in, double scale)
{
    size_t at = in->at;
    uint32_t b = Byte(in);
    uint32_t f = Byte(in);
    in->at = at;
    if (b % 12 < 7 || f >= 10)
    {
        return Wild(in, scale);
    }
    (void)Byte(in);
    (void)Byte(in);
    static const double rare[10] = {
        1.0 - 0x1p-24,   1.0 - 0x1p-53,          0x1p-24, 0x1p-149, -0.0,
        MTWN_SEEK_LIMIT, 0x1.0000000000001p1000, 1e308,   -1e308,   DBL_MAX};
    return rare[f];
}

// A rate: Wild's draws less 1, but about one in 61, as Progress, one at
// the edge of a rate's size: the smallest, either way, an ulp below it,
// the smallest subnormal and -1e300. It takes the same bytes as Wild.
static double Rate(Bytes* in)
{
    size_t at = in->at;
    uint32_t b = Byte(in);
    uint32_t f = Byte(in);
    in->at = at;
    if (b % 12 < 7 || f >= 10)
    {
        return Wild(in, 3.0) - 1.0;
    }
    (void)Byte(in);
    (void)Byte(in);
    static const double rare[5] = {0x1p-20, -0x1p-20, 0x1.fffffffffffffp-21, 0x1p-1074, -1e-300};
    return rare[f % 5];
}

// The host's points of the linear() easings a script hands in, wiped
// (NaN) once the call that copies them returns: a context reading them
// after would write values that are not finite.
static mtwnLinearPoint s_timingPoints[4];
static mtwnLinearPoint s_transitionPoints[4];

static mtwnResult MakeLinear(mtwnLinearPoint* points, mtwnEasing* easingOut)
{
    static const mtwnLinearPoint s_rising[4] = {{0.0, 0.0}, {0.4, 0.3}, {0.7, 0.8}, {1.0, 1.0}};
    memcpy(points, s_rising, sizeof s_rising);
    return mtwnMakeLinearEasing(points, 4, easingOut);
}

static void Scribble(mtwnLinearPoint* points)
{
    memset(points, 0xFF, 4 * sizeof *points);
}

static mtwnTiming RandomTiming(Bytes* in)
{
    mtwnTiming t = mtwnDefaultTiming();
    t.duration = Wild(in, 1.0);
    t.delay = Below(in, 3) == 0 ? Wild(in, 1.0) : 0.0;
    t.endDelay = Below(in, 4) == 0 ? Wild(in, 0.5) : 0.0;
    t.iterations = Below(in, 5) == 0 ? HUGE_VAL : (double)(1 + Below(in, 4)) * 0.5;
    t.iterationStart = Below(in, 4) == 0 ? 0.25 * (double)Below(in, 12) : 0.0;
    t.repeatDelay = Below(in, 3) == 0 ? Wild(in, 0.5) : 0.0;
    static const double rates[] = {1.0, 2.0, 0.5, -1.0, -1.5, 3.0};
    t.playbackRate = rates[Below(in, 6)];
    t.direction = (mtwnDirection)Below(in, 5);
    t.fill = (mtwnFill)Below(in, 4);
    t.alternateEasing = (mtwnAlternateEasing)Below(in, 2);
    if (Below(in, 3) == 0)
    {
        // Drawn in order, not as arguments, whose order C leaves open.
        mtwnEaseMode mode = (mtwnEaseMode)Below(in, 4);
        uint32_t power = Below(in, 3);
        if (power == 2)
        {
            (void)mtwnMakeHostEasing(mode, Watched, NULL, &t.easing);
        }
        else if (power == 1)
        {
            // linear(), whose points the clip copies: Scribble wipes them
            // once the timing is added.
            (void)MakeLinear(s_timingPoints, &t.easing);
            (void)mtwnSetEaseMode(&t.easing, mode);
        }
        else
        {
            (void)mtwnMakePowerEasing(mode, 1.5 + power, &t.easing);
        }
    }
    return t;
}

// The run's hash (FNV-1a) and whether an invariant failed.
typedef struct Run
{
    uint64_t hash;
    bool bad;
} Run;

static void Mix(Run* run, const void* bytes, size_t size)
{
    const uint8_t* b = (const uint8_t*)bytes;
    for (size_t i = 0; i < size; ++i)
    {
        run->hash = (run->hash ^ b[i]) * 1099511628211u;
    }
}

// Every call's status, and whether the call left its context busy: each
// one leaves what it enters, on every return.
static void Status(Run* run, mtwnResult r)
{
    int32_t v = (int32_t)r;
    Mix(run, &v, sizeof v);
    run->bad = run->bad || s_watched == NULL || mtwnTestIsBusy(s_watched);
}

static bool Finite(mtwnVec4 v)
{
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z) && isfinite(v.w);
}

static void* RunAtOnce(mtwnTaskFn* task, int32_t itemCount, int32_t minRange, void* taskContext,
                       void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext);
    return NULL;
}

static void WaitForNothing(void* handle, void* userContext)
{
    (void)handle;
    (void)userContext;
}

// One node of a clip, its kind and fields from the input.
static void AddNode(Bytes* in, Run* run, mtwnContext* context, mtwnClipId clip, mtwnNode* timelines,
                    uint32_t* timelineCount)
{
    mtwnNode parent = timelines[Below(in, *timelineCount)];
    parent = Below(in, 16) == 0 ? 1000u + Below(in, 8) : parent;
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    at.kind = (mtwnPositionKind)Below(in, 6);
    at.offset = Wild(in, 1.0);
    at.label = Below(in, 4);
    mtwnNode node = 0;
    mtwnResult r = mtwn_success;
    switch (Below(in, 4))
    {
    case 0:
    {
        mtwnClipTimelineDef def = mtwnDefaultClipTimelineDef();
        def.parent = parent;
        def.position = at;
        def.autoDuration = Below(in, 2) == 0;
        def.timing = RandomTiming(in);
        r = mtwnClip_AddTimeline(context, clip, &def, &node);
        Scribble(s_timingPoints);
        if (r == mtwn_success && *timelineCount < 8)
        {
            timelines[(*timelineCount)++] = node;
        }
        break;
    }
    case 1:
        r = mtwnClip_AddMark(context, clip, parent, at, Below(in, 4), &node);
        break;
    case 2:
        r = mtwnClip_AddLabel(context, clip, parent, at, Below(in, 4), &node);
        break;
    default:
    {
        mtwnKeyframe keys[3];
        memset(keys, 0, sizeof keys);
        uint32_t count = 1 + Below(in, 3);
        for (uint32_t k = 0; k < count; ++k)
        {
            keys[k].value = (mtwnVec4){(float)Value(in, 4.0), 0.5f, 0.0f, 1.0f};
            keys[k].offset = k == 0 ? 0.0 : (k + 1 == count ? 1.0 : Progress(in, 1.0));
            keys[k].easing = mtwnLinearEasing();
            if (k == 1)
            {
                (void)mtwnMakeHostEasing(mtwn_easeIn, Watched, NULL, &keys[k].easing);
            }
            // A first key drawn below 0 eases its segment past the floats.
            if (k == 0 && keys[k].value.x < 0.0f)
            {
                (void)mtwnMakeHostEasing(mtwn_easeIn, Overshooting, NULL, &keys[k].easing);
            }
            keys[k].kind = (mtwnSegmentKind)Below(in, 6);
        }
        mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
        def.parent = parent;
        def.position = at;
        def.slot = Below(in, 4);
        def.track.kind = (mtwnValueKind)Below(in, 11);
        def.keyframes = keys;
        def.keyframeCount = count;
        def.timing = RandomTiming(in);
        def.incremental = Below(in, 4) == 0;
        def.constantSpeed = Below(in, 4) == 0;
        r = mtwnClip_AddTrack(context, clip, &def, &node);
        Scribble(s_timingPoints);
        break;
    }
    }
    Status(run, r);
    Mix(run, &node, sizeof node);
}

static mtwnVec4 StartOf(mtwnValueKind kind)
{
    return kind == mtwn_valueRotation || kind == mtwn_valueColor ? (mtwnVec4){0, 0, 0, 1.0f}
                                                                 : (mtwnVec4){0.5f, 0, 0, 0};
}

// One control: a player's, a transition, a destruction or reduced motion.
static void Control(Bytes* in, Run* run, mtwnContext* context, const mtwnPlayerId* players,
                    mtwnTargetId* targets)
{
    mtwnPlayerId player = players[Below(in, s_players)];
    mtwnResult r = mtwn_success;
    switch (Below(in, 14))
    {
    case 0:
        r = mtwnPlayer_Seek(context, player, Progress(in, 4.0));
        break;
    case 1:
        r = mtwnPlayer_Reverse(context, player);
        break;
    case 2:
        r = mtwnPlayer_SetRate(context, player, Rate(in));
        break;
    case 3:
        r = mtwnPlayer_Pause(context, player);
        break;
    case 4:
        r = mtwnPlayer_Play(context, player);
        break;
    case 5:
        r = mtwnPlayer_Sweep(context, player, Progress(in, 4.0));
        break;
    case 6:
        r = mtwnPlayer_Finish(context, player);
        break;
    case 7:
        r = mtwnPlayer_SetWeight(context, player, (float)Wild(in, 1.0));
        break;
    case 8:
        r = mtwnContext_SetReducedMotion(context, Below(in, 2) == 0);
        break;
    case 9:
        r = mtwnDestroyPlayer(context, player);
        break;
    case 10:
    {
        uint32_t t = Below(in, s_targets);
        r = mtwnDestroyTarget(context, targets[t]);
        break;
    }
    case 11:
    {
        uint32_t t = Below(in, s_targets);
        r = mtwnCreateTarget(context, (mtwnValueKind)Below(in, 11), StartOf(mtwn_valueScalar),
                             &targets[t]);
        break;
    }
    default:
    {
        mtwnTransitionDef def = mtwnDefaultTransitionDef();
        def.target = targets[Below(in, s_targets)];
        def.value = (mtwnVec4){(float)Value(in, 4.0), 0.25f, 0.5f, 1.0f};
        // A value drawn below 0 eases past the floats on the way.
        if (def.value.x < 0.0f)
        {
            (void)mtwnMakeHostEasing(mtwn_easeIn, Overshooting, NULL, &def.easing);
        }
        def.delay = Below(in, 3) == 0 ? Wild(in, 0.5) : 0.0;
        def.duration = Wild(in, 1.0);
        def.spec = (mtwnTransitionSpec)Below(in, 3);
        double duration = 0.25 + 0.25 * Below(in, 4);
        (void)mtwnMakeSpringFromDuration(duration, 0.1 * Below(in, 5), &def.spring);
        if (def.spec == mtwn_transitionDecay && Below(in, 2) == 0)
        {
            // A fling, bounded or landing.
            def.velocitySource = mtwn_velocityGiven;
            def.velocity = (mtwnVec4){(float)Value(in, 8.0), (float)Value(in, 8.0), 0.0f, 0.0f};
            def.bounded = Below(in, 2) == 0;
            def.lands = !def.bounded && Below(in, 2) == 0;
            def.lower = (mtwnVec4){(float)Wild(in, 2.0), -1.0f, 0.0f, 0.0f};
            def.upper = (mtwnVec4){(float)Wild(in, 2.0), 1.0f, 1.0f, 1.0f};
        }
        // A value drawn from 1 to 2 eases by linear(), copied as it starts.
        if (def.spec == mtwn_transitionEasing && def.value.x >= 1.0f && def.value.x < 2.0f)
        {
            (void)MakeLinear(s_transitionPoints, &def.easing);
        }
        mtwnPlayerId id = {0, 0};
        r = mtwnStartTransition(context, &def, &id);
        Scribble(s_transitionPoints);
        break;
    }
    }
    Status(run, r);
}

// The calls the steps do not draw, in up to four rounds once the steps
// end, their arguments drawn from the bytes left (none left, each draw is
// 0): nodes and labels queried there or not, the clip's duration, some or
// all of the targets read, a base set, a measure, seeks by progress past
// the ends and by labels not found, cancels (again across rounds), the
// clip destroyed with players on it or not, and an update after each
// round. The steps read their bytes as before, so every seed keeps its
// meaning up to here. Structs are read for their status only, their
// padding unhashed.
static void Remaining(Bytes* in, Run* run, mtwnContext* context, mtwnClipId clip,
                      const mtwnPlayerId* players, const mtwnTargetId* targets, uint64_t now)
{
    for (uint32_t round = 0; round < 4 && (round == 0 || in->at < in->size); ++round)
    {
        for (uint32_t q = 0; q < 4; ++q)
        {
            mtwnNode node = MTWN_ROOT_NODE + Below(in, 24);
            mtwnClipNodeInfo info;
            mtwnClipTrack track;
            double label = 0.0;
            Status(run, mtwnClip_GetNode(context, clip, node, &info));
            Status(run, mtwnClip_GetTrack(context, clip, node, &track));
            Status(run, mtwnClip_FindLabel(context, clip, node, Below(in, 6), &label));
            Mix(run, &label, sizeof label);
        }
        double duration = 0.0;
        Status(run, mtwnClip_GetDuration(context, clip, &duration));
        Mix(run, &duration, sizeof duration);
        uint32_t first = Below(in, s_targets);
        uint32_t count = Below(in, s_targets - first + 1);
        mtwnVec4 values[s_targets];
        memset(values, 0, sizeof values);
        Status(run, mtwnReadTargets(context, targets + first, count, values));
        Mix(run, values, sizeof values);
        mtwnVec4 value = {0};
        Status(run, mtwnTarget_GetValue(context, targets[Below(in, s_targets)], &value));
        Mix(run, &value, sizeof value);
        uint32_t based = Below(in, s_targets);
        Status(run, mtwnTarget_SetBase(context, targets[based],
                                       (mtwnVec4){(float)Value(in, 4.0), 0, 0, 1.0f}));
        mtwnTransitionDef def = mtwnDefaultTransitionDef();
        def.target = targets[Below(in, s_targets)];
        def.value = (mtwnVec4){(float)Value(in, 4.0), 0.25f, 0.5f, 1.0f};
        def.duration = Wild(in, 1.0);
        def.spec = (mtwnTransitionSpec)Below(in, 3);
        (void)mtwnMakeSpringFromDuration(0.25 + 0.25 * Below(in, 4), 0.1 * Below(in, 5),
                                         &def.spring);
        double distance = 0.0;
        Status(run, mtwnMeasureTransition(context, &def, &distance));
        Mix(run, &distance, sizeof distance);
        uint32_t sought = Below(in, s_players);
        Status(run, mtwnPlayer_SeekProgress(context, players[sought], Progress(in, 1.5)));
        uint32_t labelled = Below(in, s_players);
        Status(run, mtwnPlayer_SeekLabel(context, players[labelled], Below(in, 6)));
        if (Below(in, 2) == 0)
        {
            Status(run, mtwnPlayer_Cancel(context, players[Below(in, s_players)]));
        }
        for (uint32_t p = 0; p < s_players; ++p)
        {
            mtwnPlayerState state;
            Status(run, mtwnPlayer_GetState(context, players[p], &state));
        }
        if (Below(in, 4) == 0)
        {
            Status(run, mtwnDestroyClip(context, clip));
        }
        now += 1000000u + (uint64_t)Byte(in) * 4000000u;
        uint32_t total = 0;
        Status(run, mtwnUpdate(context, now, NULL, 0, &total));
        Mix(run, &total, sizeof total);
        for (uint32_t t = 0; t < s_targets; ++t)
        {
            mtwnTargetState state;
            mtwnResult r = mtwnTarget_GetState(context, targets[t], &state);
            Status(run, r);
            if (r == mtwn_success)
            {
                Mix(run, &state.value, sizeof state.value);
                run->bad = run->bad || !Finite(state.value) || !Finite(state.velocity);
            }
        }
    }
}

// The script on one context: 0 alone, 1 with a task system, 2 counting
// every record.
static Run Script(const uint8_t* data, size_t size, int mode)
{
    Bytes in = {data, size, 0};
    Run run = {14695981039346656037u, false};
    mtwnContextDef def = mtwnDefaultContextDef();
    def.eventCapacity = 256;
    def.enqueueTask = mode == 1 ? RunAtOnce : NULL;
    def.finishTask = mode == 1 ? WaitForNothing : NULL;
    mtwnContext* context = NULL;
    if (mtwnCreateContext(&def, &context) != mtwn_success)
    {
        run.bad = true;
        return run;
    }
    mtwnTestSetCountEvery(context, mode == 2);
    s_watched = context;
    s_unflagged = false;
    mtwnTargetId targets[s_targets];
    for (uint32_t t = 0; t < s_targets; ++t)
    {
        mtwnValueKind kind = (mtwnValueKind)Below(&in, 11);
        Status(&run, mtwnCreateTarget(context, kind, StartOf(kind), &targets[t]));
    }
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
    Status(&run, mtwnCreateClip(context, &clipDef, &clip));
    mtwnNode timelines[8] = {MTWN_ROOT_NODE};
    uint32_t timelineCount = 1;
    uint32_t nodes = 1 + Below(&in, 12);
    for (uint32_t k = 0; k < nodes; ++k)
    {
        AddNode(&in, &run, context, clip, timelines, &timelineCount);
    }
    Status(&run, mtwnClip_Seal(context, clip));
    mtwnPlayerId players[s_players];
    memset(players, 0, sizeof players);
    uint32_t playerCount = 1 + Below(&in, s_players);
    for (uint32_t p = 0; p < playerCount; ++p)
    {
        mtwnBinding bindings[4];
        for (uint32_t s = 0; s < 4; ++s)
        {
            bindings[s] = mtwnBind(s, targets[Below(&in, s_targets)]);
            bindings[s].composite = (mtwnComposite)Below(&in, 3);
            bindings[s].weight = Below(&in, 4) == 0 ? 0.5f : 1.0f;
        }
        mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = bindings;
        playerDef.bindingCount = Below(&in, 5);
        playerDef.keep = Below(&in, 2) == 0;
        playerDef.essential = Below(&in, 4) == 0;
        playerDef.rate = Below(&in, 4) == 0 ? -1.0 : 1.0;
        Status(&run, mtwnCreatePlayer(context, &playerDef, &players[p]));
    }
    uint64_t now = 0;
    for (uint32_t step = 0; step < s_steps && in.at < in.size; ++step)
    {
        for (uint32_t c = Below(&in, 3); c > 0; --c)
        {
            Control(&in, &run, context, players, targets);
        }
        uint64_t before = now;
        uint32_t gap = Byte(&in);
        now += gap < 200 ? (uint64_t)gap * 2000000u : (uint64_t)(gap - 199) * 500000000u;
        mtwnTargetId changed[64];
        uint32_t total = 0;
        Status(&run, mtwnUpdate(context, now, changed, 64, &total));
        Mix(&run, &total, sizeof total);
        for (uint32_t i = 0; i < total && i < 64; ++i)
        {
            Mix(&run, &changed[i], sizeof changed[i]);
        }
        for (uint32_t t = 0; t < s_targets; ++t)
        {
            mtwnTargetState state;
            mtwnResult r = mtwnTarget_GetState(context, targets[t], &state);
            Status(&run, r);
            if (r == mtwn_success)
            {
                Mix(&run, &state.value, sizeof state.value);
                Mix(&run, &state.velocity, sizeof state.velocity);
                Mix(&run, &state.base, sizeof state.base);
                run.bad = run.bad || !Finite(state.value) || !Finite(state.velocity);
            }
        }
        mtwnEvent events[256];
        uint32_t n = 0;
        uint64_t dropped = 0;
        Status(&run, mtwnReadEvents(context, events, 256, &n, &dropped));
        Mix(&run, &dropped, sizeof dropped);
        for (uint32_t e = 0; e < n; ++e)
        {
            const mtwnEvent* ev = &events[e];
            double fields[4] = {ev->hostTime, ev->clipTime, ev->firstIteration, ev->lastIteration};
            uint64_t ids[6] = {ev->count, ev->player.index1, ev->node,
                               ev->key,   ev->kind,          ev->backwards ? 1u : 0u};
            Mix(&run, fields, sizeof fields);
            Mix(&run, ids, sizeof ids);
            bool ordered = e == 0 || events[e - 1].hostTime <= ev->hostTime;
            bool spanned = ev->hostTime >= (double)before / 1e9 - 1e-9 &&
                           ev->hostTime <= (double)now / 1e9 + 1e-9;
            bool counted =
                ev->count >= 1 && ev->kind >= mtwn_eventStart && ev->kind <= mtwn_eventCancel;
            run.bad = run.bad || !ordered || !spanned || !counted;
        }
    }
    Remaining(&in, &run, context, clip, players, targets, now);
    run.bad = run.bad || s_unflagged;
    s_watched = NULL;
    mtwnDestroyContext(context);
    return run;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Run alone = Script(data, size, 0);
    Run tasks = Script(data, size, 1);
    Run every = Script(data, size, 2);
    if (alone.bad || tasks.bad || every.bad || alone.hash != tasks.hash || alone.hash != every.hash)
    {
        fprintf(stderr, "fuzz_api: bad %d %d %d, hashes %016llx %016llx %016llx\n", alone.bad,
                tasks.bad, every.bad, (unsigned long long)alone.hash,
                (unsigned long long)tasks.hash, (unsigned long long)every.hash);
        __builtin_trap();
    }
    return 0;
}

#ifndef MTWN_FUZZING
#include "generated/fuzz_seeds_api.h"

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
    uint64_t state = 0x9E3779B97F4A7C15u;
    static uint8_t input[768];
    for (int n = 0; n < 300; ++n)
    {
        size_t size = 64 + (size_t)(n * 37 % 700);
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
