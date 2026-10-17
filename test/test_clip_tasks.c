// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The clips' players on the host's task system (record mtwn-0056): twin
// contexts run one random script, one serially and one through a task
// system that splits the groups into random ranges and runs them in a
// random order, now or at the wait. After every update the twins hold
// the same bits: every target's base, value and velocity, the changed
// targets in order, every record and the dropped count. Small queues,
// few bindings and a clip of more nodes than a group keeps make the
// staging run out, so every way back to the serial pass is taken.

#include "context_internal.h"
#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
typedef HANDLE Thread;
#elif !defined(__EMSCRIPTEN__)
#include <pthread.h>
typedef pthread_t Thread;
#else
typedef int Thread;
#endif

enum
{
    s_slots = 6,
    s_targetsPerSlot = 3,
    s_clips = 4,
    s_players = 48,
    s_frames = 120,
};

// Each slot's value kind.
static const mtwnValueKind s_kinds[s_slots] = {mtwn_valueScalar,  mtwn_valueVec2,
                                               mtwn_valueColor,   mtwn_valueRotation,
                                               mtwn_valueInteger, mtwn_valueDiscrete};

static uint64_t s_script;
// Sibling tracks found sharing their timing.
static uint32_t s_shared;
// Players made and refused (only past the capacities).
static uint32_t s_made;
static uint32_t s_refused;
static uint64_t s_hostDraws;
// Players bind targets of their own too, so the groups write those.
static bool s_ownTargets;
// The first clip played by one player at a time, so a group samples it
// in the clip's own node times, and the second by two at most, which
// must not be.
static bool s_lonelyClip;
static uint64_t s_alone;

static uint32_t NextOf(uint64_t* state)
{
    *state = *state * 6364136223846793005ull + 1442695040888963407ull;
    return (uint32_t)(*state >> 33);
}

static uint32_t Next(void)
{
    return NextOf(&s_script);
}

static double Unit(void)
{
    return (double)Next() / 2147483648.0;
}

// The twins and what tells them apart.
typedef struct Twin
{
    mtwnContext* context;
    mtwnClipId clips[s_clips];
    // Each slot's shared targets, then one of each player's own.
    mtwnTargetId targets[s_slots][s_targetsPerSlot + s_players];
    mtwnPlayerId players[s_players];
    // Its clips' sibling tracks never share their timing.
    bool unshared;
} Twin;

// Ranges run at the wait, for a host that returns a handle.
typedef struct Pending
{
    mtwnTaskFn* task;
    void* taskContext;
    int32_t starts[16];
    int32_t ends[16];
    int32_t count;
} Pending;

static Pending s_pending;
static uint32_t s_enqueued;

// Each range on a thread of its own, joined at the wait; the web build
// has no threads, so there they run one after another.
static bool s_threaded;
#if !defined(__EMSCRIPTEN__)
static Thread s_threads[16];
#endif

typedef struct Range
{
    mtwnTaskFn* task;
    void* taskContext;
    int32_t start;
    int32_t end;
} Range;

static Range s_ranges[16];

#if defined(_WIN32)
static DWORD WINAPI RunRange(LPVOID range)
{
    const Range* r = (const Range*)range;
    r->task(r->start, r->end, r->taskContext);
    return 0;
}
#elif !defined(__EMSCRIPTEN__)
static void* RunRange(void* range)
{
    const Range* r = (const Range*)range;
    r->task(r->start, r->end, r->taskContext);
    return NULL;
}
#endif

static void StartThreads(void)
{
    for (int32_t i = 0; i < s_pending.count; ++i)
    {
        s_ranges[i] =
            (Range){s_pending.task, s_pending.taskContext, s_pending.starts[i], s_pending.ends[i]};
#if defined(_WIN32)
        s_threads[i] = CreateThread(NULL, 0, RunRange, &s_ranges[i], 0, NULL);
        CHECK(s_threads[i] != NULL, "a thread");
#elif !defined(__EMSCRIPTEN__)
        CHECK(pthread_create(&s_threads[i], NULL, RunRange, &s_ranges[i]) == 0, "a thread");
#else
        s_ranges[i].task(s_ranges[i].start, s_ranges[i].end, s_ranges[i].taskContext);
#endif
    }
}

static void JoinThreads(void)
{
    for (int32_t i = 0; i < s_pending.count; ++i)
    {
#if defined(_WIN32)
        WaitForSingleObject(s_threads[i], INFINITE);
        CloseHandle(s_threads[i]);
#elif !defined(__EMSCRIPTEN__)
        pthread_join(s_threads[i], NULL);
#endif
    }
    s_pending.count = 0;
}

static void RunPending(void)
{
    for (int32_t i = 0; i < s_pending.count; ++i)
    {
        s_pending.task(s_pending.starts[i], s_pending.ends[i], s_pending.taskContext);
    }
    s_pending.count = 0;
}

// Splits [0, itemCount) into random ranges of at least minRange, the last
// maybe shorter, shuffles them, and runs them now or at the wait.
static void* Enqueue(mtwnTaskFn* task, int32_t itemCount, int32_t minRange, void* taskContext,
                     void* userContext)
{
    (void)userContext;
    ++s_enqueued;
    s_pending.task = task;
    s_pending.taskContext = taskContext;
    s_pending.count = 0;
    for (int32_t at = 0; at < itemCount;)
    {
        int32_t length = minRange + (int32_t)(NextOf(&s_hostDraws) % 4u);
        int32_t end = at + length < itemCount ? at + length : itemCount;
        s_pending.starts[s_pending.count] = at;
        s_pending.ends[s_pending.count] = end;
        ++s_pending.count;
        at = end;
    }
    for (int32_t i = s_pending.count - 1; i > 0; --i)
    {
        int32_t j = (int32_t)(NextOf(&s_hostDraws) % (uint32_t)(i + 1));
        int32_t start = s_pending.starts[i];
        int32_t end = s_pending.ends[i];
        s_pending.starts[i] = s_pending.starts[j];
        s_pending.ends[i] = s_pending.ends[j];
        s_pending.starts[j] = start;
        s_pending.ends[j] = end;
    }
    if (s_threaded)
    {
        StartThreads();
        return &s_pending;
    }
    if (NextOf(&s_hostDraws) % 2u == 0u)
    {
        RunPending();
        return NULL;
    }
    return &s_pending;
}

static void Finish(void* userTask, void* userContext)
{
    (void)userContext;
    CHECK(userTask == &s_pending, "the wait gets the handle");
    if (s_threaded)
    {
        JoinThreads();
        return;
    }
    RunPending();
}

static mtwnVec4 KeyValue(mtwnValueKind kind)
{
    double a = Unit() * 4.0 - 2.0;
    if (kind == mtwn_valueColor)
    {
        return (mtwnVec4){(float)Unit(), (float)Unit(), (float)Unit(), (float)(Unit() * 0.5)};
    }
    if (kind == mtwn_valueRotation)
    {
        return (mtwnVec4){(float)sin(a), 0.0f, 0.0f, (float)cos(a)};
    }
    return (mtwnVec4){(float)(a * 10.0), (float)(Unit() * 5.0), 0.0f, 0.0f};
}

static mtwnTiming Timing(void)
{
    mtwnTiming t = mtwnDefaultTiming();
    t.duration = 0.02 + Unit() * 0.4;
    t.delay = Next() % 3 == 0 ? Unit() * 0.2 : 0.0;
    t.iterations = Next() % 3 == 0 ? 1.0 + (double)(Next() % 4) : 1.0;
    // Repeating forever half the time, so records come every frame.
    if (Next() % 2 == 0)
    {
        t.iterations = HUGE_VAL;
    }
    t.direction = (mtwnDirection)(Next() % 4);
    t.fill = (mtwnFill)(Next() % 4);
    t.playbackRate = Next() % 6 == 0 ? -1.0 : 1.0;
    t.repeatDelay = Next() % 4 == 0 ? Unit() * 0.1 : 0.0;
    return t;
}

// A random tree under the root: timelines, tracks and marks, so many in
// the big clip that it passes the nodes a group keeps.
static void Build(Twin* twin, uint32_t c, uint32_t nodes)
{
    mtwnClipDef def = mtwnDefaultClipDef();
    def.nodeCapacity = nodes;
    def.keyframeCapacity = nodes * 3;
    CHECK(mtwnCreateClip(twin->context, &def, &twin->clips[c]) == mtwn_success, "a clip");
    mtwnNode timelines[4] = {MTWN_ROOT_NODE};
    uint32_t timelineCount = 1;
    // The last node added, when a track: the next may share its timing.
    mtwnClipTrackDef last = mtwnDefaultClipTrackDef();
    bool lastTrack = false;
    for (uint32_t k = 0; k < nodes; ++k)
    {
        mtwnPosition at = {0};
        at.kind = (mtwnPositionKind)(Next() % 3);
        at.offset = Unit();
        mtwnNode parent = timelines[Next() % timelineCount];
        mtwnNode node = 0;
        uint32_t what = Next() % 6;
        if (what == 0 && timelineCount < 4)
        {
            mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
            timeline.parent = parent;
            timeline.position = at;
            timeline.timing = Timing();
            timeline.autoDuration = Next() % 2 == 0;
            if (mtwnClip_AddTimeline(twin->context, twin->clips[c], &timeline, &node) ==
                mtwn_success)
            {
                timelines[timelineCount++] = node;
            }
        }
        else if (what == 1)
        {
            (void)mtwnClip_AddMark(twin->context, twin->clips[c], parent, at, Next() % 4, &node);
        }
        else
        {
            uint32_t slot = Next() % s_slots;
            mtwnKeyframe keys[3];
            memset(keys, 0, sizeof keys);
            uint32_t count = 2 + Next() % 2;
            for (uint32_t i = 0; i < count; ++i)
            {
                keys[i].value = KeyValue(s_kinds[slot]);
                keys[i].offset = (double)i / (double)(count - 1);
                keys[i].easing = mtwnLinearEasing();
            }
            mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
            track.parent = parent;
            track.position = at;
            track.slot = slot;
            track.track.kind = s_kinds[slot];
            track.keyframes = keys;
            track.keyframeCount = count;
            track.timing = Timing();
            if (nodes == 120)
            {
                // Always in effect: one player fills a group's track staging.
                track.timing.fill = mtwn_fillBoth;
                track.timing.iterations = HUGE_VAL;
            }
            track.incremental = Next() % 4 == 0;
            if (lastTrack && Next() % 2 == 0)
            {
                // A sibling of the same timing at the same start: its
                // records are the last one's, replayed.
                track.parent = last.parent;
                track.position = (mtwnPosition){0};
                track.position.kind = mtwn_positionWithPrevious;
                track.timing = last.timing;
                // Or one input apart, which must not share.
                switch (Next() % 10)
                {
                case 0:
                    track.timing.delay += 0.01;
                    break;
                case 1:
                    track.timing.fill = (mtwnFill)((track.timing.fill + 1u) % 4u);
                    break;
                case 2:
                    track.timing.repeatDelay += 0.01;
                    break;
                case 3:
                    track.timing.playbackRate = -track.timing.playbackRate;
                    break;
                case 4:
                case 5:
                    // Another timeline, when there is one.
                    for (uint32_t t = 0; t < timelineCount; ++t)
                    {
                        if (timelines[t] != last.parent)
                        {
                            track.parent = timelines[t];
                        }
                    }
                    break;
                default:
                    break;
                }
            }
            lastTrack =
                mtwnClip_AddTrack(twin->context, twin->clips[c], &track, &node) == mtwn_success;
            last = track;
            continue;
        }
        lastTrack = false;
    }
    CHECK(mtwnClip_Finish(twin->context, twin->clips[c]) == mtwn_success, "a finished clip");
    mtwnClipSlot* clip = mtwnClipAt(twin->context, twin->clips[c].index1);
    for (uint32_t i = 0; i < clip->nodeCount; ++i)
    {
        s_shared += clip->nodes[i].sharesTiming ? 1u : 0u;
        // The replay and the kept timing forced off.
        clip->nodes[i].sharesTiming = clip->nodes[i].sharesTiming && !(twin->unshared);
    }
}

static void MakePlayer(Twin* twin, uint32_t p)
{
    uint32_t c = Next() % s_clips;
    // Clip 0 has one player at most, clip 1 two.
    if (s_lonelyClip && c < 2u && mtwnClipAt(twin->context, twin->clips[c].index1)->users > c)
    {
        c = 2u + Next() % (s_clips - 2u);
    }
    const mtwnClipSlot* clip = mtwnClipAt(twin->context, twin->clips[c].index1);
    // A binding for each slot the clip's tracks use; one no track uses is
    // refused.
    mtwnBinding bindings[s_slots];
    uint32_t bound = 0;
    for (uint32_t s = 0; s < s_slots; ++s)
    {
        uint32_t shared = Next() % s_targetsPerSlot;
        // Half of them on the player's own target when asked, its alone
        // unless a transition or a player made in its place shares it.
        uint32_t t = s_ownTargets && Next() % 2 == 0 ? s_targetsPerSlot + p : shared;
        mtwnBinding binding = mtwnBind(s, twin->targets[s][t]);
        binding.composite = Next() % 3 == 0 ? mtwn_compositeAdd : mtwn_compositeReplace;
        binding.weight = Next() % 3 == 0 ? 0.5f : 1.0f;
        if (s < clip->slotCount && clip->slotKinds[s] != 0)
        {
            bindings[bound++] = binding;
        }
    }
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = twin->clips[c];
    def.bindings = bindings;
    def.bindingCount = bound;
    def.keep = Next() % 2 == 0;
    def.rate = Next() % 5 == 0 ? -1.0 : 1.0;
    mtwnResult made = mtwnCreatePlayer(twin->context, &def, &twin->players[p]);
    s_made += made == mtwn_success ? 1u : 0u;
    s_refused += made == mtwn_success ? 0u : 1u;
    if (made != mtwn_success)
    {
        twin->players[p] = (mtwnPlayerId){0, 0};
    }
}

static void Control(Twin* twin, uint32_t p, uint32_t what, double argument)
{
    mtwnContext* context = twin->context;
    mtwnPlayerId player = twin->players[p];
    switch (what)
    {
    case 0:
        (void)mtwnPlayer_Seek(context, player, argument);
        break;
    case 1:
        (void)mtwnPlayer_Reverse(context, player);
        break;
    case 2:
        (void)mtwnPlayer_SetRate(context, player, argument - 1.0);
        break;
    case 3:
        (void)mtwnPlayer_Pause(context, player);
        break;
    case 4:
        (void)mtwnPlayer_Play(context, player);
        break;
    case 5:
        (void)mtwnPlayer_Sweep(context, player, argument);
        break;
    case 6:
        (void)mtwnPlayer_Finish(context, player);
        break;
    case 7:
        (void)mtwnContext_SetReducedMotion(context, argument > 1.0);
        break;
    case 8:
        (void)mtwnDestroyPlayer(context, player);
        break;
    default:
    {
        // A transition on a scalar target, between the clips' players.
        mtwnTransitionDef def = mtwnDefaultTransitionDef();
        def.target = twin->targets[0][s_ownTargets && p % 2 == 1 ? s_targetsPerSlot + p
                                                                 : p % s_targetsPerSlot];
        def.value = (mtwnVec4){(float)argument, 0.0f, 0.0f, 0.0f};
        def.duration = 0.1 + argument * 0.1;
        mtwnPlayerId ignored;
        (void)mtwnStartTransition(context, &def, &ignored);
        break;
    }
    }
}

static bool Same(mtwnVec4 a, mtwnVec4 b)
{
    return memcmp(&a, &b, sizeof a) == 0;
}

static bool SameRecord(const mtwnEvent* a, const mtwnEvent* b)
{
    return a->hostTime == b->hostTime && a->clipTime == b->clipTime &&
           (a->firstIteration == b->firstIteration ||
            (isnan(a->firstIteration) && isnan(b->firstIteration))) &&
           (a->lastIteration == b->lastIteration ||
            (isnan(a->lastIteration) && isnan(b->lastIteration))) &&
           a->count == b->count && a->player.index1 == b->player.index1 &&
           a->player.generation == b->player.generation && a->node == b->node && a->key == b->key &&
           a->kind == b->kind && a->backwards == b->backwards;
}

// What the parallel twin's groups did, to see each way taken, and what
// they must not have done.
typedef struct Taken
{
    uint32_t staged;
    uint32_t serial;
    // Left to the serial pass for staging that ran out.
    uint32_t ranOut;
    uint32_t dropped;
    // A clip of more nodes than a group keeps, staged anyway: with other
    // players, which must not be, or alone, in its own node times.
    uint32_t bigStaged;
    uint32_t bigAlone;
    // A clip's own node times written by a group.
    uint32_t touched;
    // The serial twin's mtwnContext_GetSerialAdvances, and the hooked
    // twin's.
    uint64_t serialUnhooked;
    uint64_t advances;
    // Updates in which a group sampled a clip of one player in its own
    // node times.
    uint32_t ownTimes;
    // Players with an alone target that a group ran, not left serial.
    uint32_t aloneInGroups;
} Taken;

// The clips' players before an update, in creation order: the order the
// groups stage them in, with their clips.
typedef struct Order
{
    uint32_t count;
    uint32_t clips[128];
} Order;

static const uint64_t s_poison = 0x7FF8DEADBEEF0001ull;

// Notes the order and poisons each clip's own node times, which only the
// serial pass may write.
static void Prepare(const Twin* twin, Order* order)
{
    const mtwnContext* context = twin->context;
    order->count = 0;
    for (uint32_t index1 = context->firstPlayer; index1 != 0;)
    {
        const mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
        if (slot->type == MTWN_PLAYER_CLIP)
        {
            order->clips[order->count++] = slot->play.clip.index1;
        }
        index1 = slot->later;
    }
    for (uint32_t c = 0; c < s_clips; ++c)
    {
        const mtwnClipSlot* clip = mtwnClipAt(context, twin->clips[c].index1);
        for (uint32_t k = 0; k < clip->nodeCount; ++k)
        {
            memcpy(&clip->times[k].now, &s_poison, sizeof s_poison);
        }
    }
}

static void Count(const Twin* twin, const Order* order, Taken* taken)
{
    const mtwnContext* context = twin->context;
    if (order->count < 2)
    {
        return;
    }
    bool serialClip[s_clips] = {false};
    uint32_t players[s_clips] = {0};
    for (uint32_t i = 0; i < order->count; ++i)
    {
        for (uint32_t c = 0; c < s_clips; ++c)
        {
            players[c] += twin->clips[c].index1 == order->clips[i] ? 1u : 0u;
        }
    }
    for (uint32_t i = 0; i < order->count; ++i)
    {
        uint32_t of = 0;
        for (uint32_t c = 0; c < s_clips; ++c)
        {
            of += twin->clips[c].index1 == order->clips[i] ? players[c] : 0u;
        }
        bool serial = context->staged[i].serial;
        const mtwnClipPlay* play = &mtwnPlayerAt(context, context->staged[i].player)->play;
        bool anyAlone = false;
        // A player that finished was freed in the update: not read.
        for (uint32_t s = 0; !context->staged[i].done && s < play->bindingCount; ++s)
        {
            anyAlone = anyAlone || context->bindings.entries[play->bindingStart + s].alone;
        }
        taken->aloneInGroups += anyAlone && !serial ? 1u : 0u;
        taken->serial += serial ? 1u : 0u;
        taken->staged += serial ? 0u : 1u;
        bool big = mtwnClipAt(context, order->clips[i])->nodeCount > MTWN_GROUP_NODES;
        taken->bigStaged += big && !serial && of != 1 ? 1u : 0u;
        taken->bigAlone += big && !serial && of == 1 ? 1u : 0u;
        taken->ranOut += serial && !big ? 1u : 0u;
        for (uint32_t c = 0; c < s_clips; ++c)
        {
            serialClip[c] = serialClip[c] || (serial && twin->clips[c].index1 == order->clips[i]);
        }
    }
    for (uint32_t c = 0; c < s_clips; ++c)
    {
        const mtwnClipSlot* clip = mtwnClipAt(context, twin->clips[c].index1);
        uint32_t written = 0;
        for (uint32_t k = 0; k < clip->nodeCount && !serialClip[c]; ++k)
        {
            written += memcmp(&clip->times[k].now, &s_poison, sizeof s_poison) == 0 ? 0u : 1u;
        }
        // A clip of one player is sampled in its own times; another's are
        // left alone, as groups may sample it side by side.
        taken->touched += players[c] == 1 ? 0u : written;
        taken->ownTimes += players[c] == 1 && written > 0 ? 1u : 0u;
    }
    for (uint32_t g = 0; g < MTWN_GROUPS; ++g)
    {
        taken->dropped += context->works[g].dropped > 0 ? 1u : 0u;
    }
}

// Compares the twins after an update; returns the differences.
static uint32_t Compare(const Twin* a, const Twin* b, const mtwnTargetId* changedA,
                        const mtwnTargetId* changedB, uint32_t totalA, uint32_t totalB)
{
    uint32_t differences = totalA == totalB ? 0u : 1u;
    for (uint32_t i = 0; i < totalA && i < totalB && i < 512; ++i)
    {
        differences += changedA[i].index1 == changedB[i].index1 ? 0u : 1u;
    }
    for (uint32_t s = 0; s < s_slots; ++s)
    {
        for (uint32_t t = 0; t < s_targetsPerSlot + s_players; ++t)
        {
            mtwnTargetState x;
            mtwnTargetState y;
            CHECK(mtwnTarget_GetState(a->context, a->targets[s][t], &x) == mtwn_success, "state");
            CHECK(mtwnTarget_GetState(b->context, b->targets[s][t], &y) == mtwn_success, "state");
            differences +=
                Same(x.base, y.base) && Same(x.value, y.value) && Same(x.velocity, y.velocity) ? 0u
                                                                                               : 1u;
        }
    }
    static mtwnEvent recordsA[4096];
    static mtwnEvent recordsB[4096];
    uint32_t nA = 0;
    uint32_t nB = 0;
    uint64_t droppedA = 0;
    uint64_t droppedB = 0;
    CHECK(mtwnReadEvents(a->context, recordsA, 4096, &nA, &droppedA) == mtwn_success, "read");
    CHECK(mtwnReadEvents(b->context, recordsB, 4096, &nB, &droppedB) == mtwn_success, "read");
    differences += nA == nB && droppedA == droppedB &&
                           mtwnContext_GetDroppedEvents(a->context) ==
                               mtwnContext_GetDroppedEvents(b->context) &&
                           mtwnContext_GetPlayingOnCount(a->context) ==
                               mtwnContext_GetPlayingOnCount(b->context)
                       ? 0u
                       : 1u;
    for (uint32_t i = 0; i < nA && i < nB; ++i)
    {
        differences += SameRecord(&recordsA[i], &recordsB[i]) ? 0u : 1u;
    }
    return differences;
}

// One run of the script on twins of a def: the differences found.
// Staging that holds nothing, of the kinds set: records 1, track results
// 2, changed targets 4. With records and results empty every player that
// makes anything runs out and is put back, the whole update made by the
// serial pass; one with targets of its own goes to it before it starts.
static unsigned s_emptyStaging;
// The second twin serial too, its sibling tracks never sharing timing:
// the replay of their records and their kept timing differ from none.
static bool s_unsharedMode;

// The bindings that were their target's alone in the last update.
static void CountAlone(const Twin* twin)
{
    const mtwnContext* context = twin->context;
    for (uint32_t p = 0; p < s_players; ++p)
    {
        mtwnPlayerId id = twin->players[p];
        if (id.index1 == 0 || mtwnPoolFind(&context->players, id.index1, id.generation) == NULL)
        {
            continue;
        }
        const mtwnClipPlay* play = &mtwnPlayerAt(context, id.index1)->play;
        for (uint32_t s = 0; s < play->bindingCount; ++s)
        {
            s_alone += context->bindings.entries[play->bindingStart + s].alone ? 1u : 0u;
        }
    }
}

static uint32_t Run(mtwnContextDef def, uint64_t seed, uint32_t firstNodes, Taken* taken)
{
    Twin twins[2];
    memset(twins, 0, sizeof twins);
    for (int k = 0; k < 2; ++k)
    {
        mtwnContextDef own = def;
        bool tasks = k == 1 && !s_unsharedMode;
        own.enqueueTask = tasks ? Enqueue : NULL;
        own.finishTask = tasks ? Finish : NULL;
        twins[k].unshared = k == 1 && s_unsharedMode;
        CHECK(mtwnCreateContext(&own, &twins[k].context) == mtwn_success, "a context");
        for (uint32_t g = 0; k == 1 && s_emptyStaging != 0 && g < MTWN_GROUPS; ++g)
        {
            mtwnWork* work = &twins[k].context->works[g];
            work->recordCapacity = (s_emptyStaging & 1u) != 0 ? 0u : work->recordCapacity;
            work->trackCapacity = (s_emptyStaging & 2u) != 0 ? 0u : work->trackCapacity;
            work->changeCapacity = (s_emptyStaging & 4u) != 0 ? 0u : work->changeCapacity;
            // Nowhere to write: a change staged past the capacity crashes.
            work->changes = (s_emptyStaging & 4u) != 0 ? NULL : work->changes;
        }
        s_script = seed;
        for (uint32_t s = 0; s < s_slots; ++s)
        {
            for (uint32_t t = 0; t < s_targetsPerSlot + s_players; ++t)
            {
                CHECK(mtwnCreateTarget(twins[k].context, s_kinds[s], KeyValue(s_kinds[s]),
                                       &twins[k].targets[s][t]) == mtwn_success,
                      "a target");
            }
        }
        for (uint32_t c = 0; c < s_clips; ++c)
        {
            Build(&twins[k], c, c == 0 && firstNodes > 0 ? firstNodes : 6 + Next() % 8);
        }
        for (uint32_t p = 0; p < s_players; ++p)
        {
            MakePlayer(&twins[k], p);
        }
    }
    uint32_t differences = 0;
    Order order;
    uint64_t now = 0;
    uint64_t script = s_script;
    for (uint32_t f = 0; f < s_frames; ++f)
    {
        now += 8000000u + (uint64_t)(Next() % 40) * 1000000u;
        uint32_t controls = Next() % 4;
        mtwnTargetId changed[2][512];
        uint32_t totals[2] = {0, 0};
        for (int k = 0; k < 2; ++k)
        {
            s_script = script;
            for (uint32_t c = 0; c < controls; ++c)
            {
                uint32_t p = Next() % s_players;
                uint32_t what = Next() % 12;
                double argument = Unit() * 2.0;
                if (what == 11)
                {
                    MakePlayer(&twins[k], p);
                }
                else
                {
                    Control(&twins[k], p, what, argument);
                }
            }
            // Midway, reduced motion finishes every running player at once:
            // its records share the update's time, ordered along its sweep.
            if (f == s_frames / 2 || f == s_frames / 2 + 8)
            {
                (void)mtwnContext_SetReducedMotion(twins[k].context, f == s_frames / 2);
            }
            if (k == 1 && !s_unsharedMode)
            {
                Prepare(&twins[k], &order);
            }
            CHECK(mtwnUpdate(twins[k].context, now, changed[k], 512, &totals[k]) == mtwn_success,
                  "an update");
        }
        script = s_script;
        if (!s_unsharedMode)
        {
            Count(&twins[1], &order, taken);
            CountAlone(&twins[1]);
        }
        differences += Compare(&twins[0], &twins[1], changed[0], changed[1], totals[0], totals[1]);
    }
    taken->serialUnhooked += mtwnContext_GetSerialAdvances(twins[0].context);
    taken->advances += mtwnContext_GetSerialAdvances(twins[1].context);
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
    return differences;
}

static void TestTwins(void)
{
    s_hostDraws = 11;
    Taken taken = {0};
    uint32_t differences = 0;
    for (uint64_t seed = 1; seed <= 12; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 8u * 1024u * 1024u;
        // Queues of each kind: one the groups' staging matches, so what
        // passes it is dropped; one past what a group stages; one barely
        // read.
        def.eventCapacity = seed % 3 == 0 ? 2u : (seed % 3 == 1 ? 4096u : 512u);
        // Few bindings: little track staging, so some groups run out.
        def.bindingCapacity = seed % 2 == 0 ? 300u : 2000u;
        // A clip past the nodes a group keeps; or, with few bindings, one of
        // many tracks, which fills a group's track staging.
        uint32_t first = seed % 4 == 1 ? 240u : (seed % 2 == 0 ? 120u : 0u);
        differences += Run(def, seed, first, &taken);
    }
    CHECK(differences == 0, "the twins hold the same bits after every update");
    CHECK(s_made > 10 * s_refused, "nearly every player made");
    CHECK(s_enqueued > 100, "the task system was handed the groups");
    CHECK(taken.staged > 1000, "players staged by the groups");
    CHECK(taken.serial > 10, "players left to the serial pass");
    CHECK(taken.ranOut > 10, "players whose staging ran out put back and left to it");
    CHECK(taken.dropped > 0, "records past a group's staging dropped as the queue would");
    CHECK(taken.bigStaged == 0,
          "a clip of more nodes than a group keeps and other players plays serially");
    CHECK(taken.touched == 0, "the groups leave a clip's own node times alone if it has players");

    // A clip of one player: its group samples it in the clip's own times.
    s_lonelyClip = true;
    Taken lonely = {0};
    uint32_t lonelyDifferences = 0;
    for (uint64_t seed = 1; seed <= 6; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 8u * 1024u * 1024u;
        def.eventCapacity = seed % 2 == 0 ? 2u : 4096u;
        def.bindingCapacity = 2000u;
        // Half with the lonely clip past the nodes a group keeps times for.
        lonelyDifferences += Run(def, seed, seed % 2 == 0 ? 240u : 0u, &lonely);
    }
    s_lonelyClip = false;
    CHECK(lonelyDifferences == 0 && lonely.touched == 0 && lonely.ownTimes > 10,
          "a clip of one player sampled in its own node times, the same bits");
    CHECK(lonely.bigStaged == 0 && lonely.bigAlone > 10,
          "a clip of one player in a group past the nodes a group keeps times for");
    CHECK(taken.serialUnhooked == 0 && taken.advances == taken.serial,
          "every advance left serial counted, none without a task system");

    // Every player put back: still the serial update's bits.
    s_emptyStaging = 3;
    Taken empty = {0};
    uint32_t emptyDifferences = 0;
    for (uint64_t seed = 1; seed <= 3; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 8u * 1024u * 1024u;
        def.bindingCapacity = 2000u;
        emptyDifferences += Run(def, seed, 0, &empty);
    }
    s_emptyStaging = 0;
    CHECK(empty.advances == empty.serial, "each put back counted");
    CHECK(emptyDifferences == 0 && empty.ranOut > 1000,
          "with staging that holds nothing, every player put back, the same bits");

    // Targets of the players' own, written in the groups, and others
    // layered: the same bits, the changed list in the same order.
    s_ownTargets = true;
    s_alone = 0;
    Taken own = {0};
    uint32_t ownDifferences = 0;
    for (uint64_t seed = 1; seed <= 12; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 8u * 1024u * 1024u;
        def.eventCapacity = seed % 3 == 0 ? 2u : (seed % 3 == 1 ? 4096u : 512u);
        def.bindingCapacity = seed % 2 == 0 ? 300u : 2000u;
        ownDifferences += Run(def, seed, seed % 4 == 1 ? 240u : 0u, &own);
    }
    // Each staging empty in turn: a player with targets of its own that
    // might not fit is left to the serial pass before its group writes
    // any of them.
    uint32_t emptyOwnDifferences = 0;
    Taken emptyOwn = {0};
    for (unsigned kind = 1; kind <= 4; kind *= 2)
    {
        s_emptyStaging = kind;
        for (uint64_t seed = 1; seed <= 3; ++seed)
        {
            mtwnContextDef def = mtwnDefaultContextDef();
            def.playerCapacity = 128;
            def.clipStorageBytes = 8u * 1024u * 1024u;
            def.bindingCapacity = 2000u;
            emptyOwnDifferences += Run(def, seed, 0, &emptyOwn);
        }
    }
    s_emptyStaging = 0;
    s_ownTargets = false;
    CHECK(ownDifferences == 0, "with targets of their own, the twins hold the same bits");
    CHECK(emptyOwnDifferences == 0 && emptyOwn.serial > 100,
          "each staging empty, players with targets of their own left serial, the same bits");
    CHECK(own.aloneInGroups > 10000,
          "players with targets of their own run by their groups when they fit");
    CHECK(s_alone > 10000 && own.staged > 1000 && own.serial > 10,
          "alone targets written in the groups, and players left serial among them");
}

// The replay of sibling tracks' records and their kept timing rest on
// sharesTiming naming every input their records and values depend on: a
// random script with them and with them forced off makes the same bits.
static void TestReplayOff(void)
{
    s_unsharedMode = true;
    s_shared = 0;
    Taken unused = {0};
    uint32_t differences = 0;
    for (uint64_t seed = 21; seed <= 32; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 8u * 1024u * 1024u;
        def.eventCapacity = 4096u;
        def.bindingCapacity = 2000u;
        differences += Run(def, seed, 0, &unused);
    }
    s_unsharedMode = false;
    CHECK(s_shared > 30, "sibling tracks sharing their timing");
    CHECK(differences == 0, "the same bits with the replay and kept timing forced off");
}

static void Fill(mtwnContext* context, uint32_t count, mtwnWork* work)
{
    work->recordCount = 0;
    work->dropped = 0;
    work->overflow = false;
    mtwnEvent event = {0};
    event.count = 1;
    for (uint32_t i = 0; i < count; ++i)
    {
        event.key = i + 1;
        mtwnWorkEvent(context, work, &event, 0.0);
    }
}

// A group's records stop at its staging: past one as large as the queue
// they are dropped, past a smaller one the player runs out; the next
// group's staging is never written.
static void TestStagingBounds(void)
{
    for (int large = 0; large < 2; ++large)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.eventCapacity = large ? 2000u : 4u;
        def.enqueueTask = Enqueue;
        def.finishTask = Finish;
        mtwnContext* context = NULL;
        CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
        mtwnWork* first = &context->works[0];
        mtwnWork* next = &context->works[1];
        next->records[0].key = 77;
        uint32_t capacity = first->recordCapacity;
        CHECK(capacity == (large ? 1024u : 4u), "a group stages up to the queue, at most 1,024");
        Fill(context, capacity + 1, first);
        CHECK(first->recordCount == capacity && first->records[capacity - 1].key == capacity,
              "records staged to the capacity");
        CHECK(large ? first->overflow && first->dropped == 0
                    : !first->overflow && first->dropped == 1,
              "past a staging like the queue a record is dropped, past a smaller one it runs out");
        CHECK(next->records[0].key == 77, "the next group's staging untouched");
        mtwnDestroyContext(context);
    }
}

// Records of one player at one time keep their order along its sweep:
// reduced motion finishes players of a clip whose nodes run against their
// time, so a group stages records out of that order.
static void TestOneTimeOrder(void)
{
    mtwnEvent records[2][64];
    uint32_t counts[2] = {0, 0};
    mtwnTargetState finished[2];
    for (int k = 0; k < 2; ++k)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.enqueueTask = k == 1 ? Enqueue : NULL;
        def.finishTask = k == 1 ? Finish : NULL;
        mtwnContext* context = NULL;
        CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnClipDef clipDef = mtwnDefaultClipDef();
        mtwnClipId clip;
        CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 1.0;
        keys[1].value.x = 1.0f;
        keys[0].easing = mtwnLinearEasing();
        keys[1].easing = mtwnLinearEasing();
        // Later in time first: tracks at 0.5, 0.3 and 0.1.
        for (int n = 0; n < 3; ++n)
        {
            mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
            track.position.offset = 0.5 - 0.2 * n;
            track.keyframes = keys;
            track.keyframeCount = 2;
            track.timing.duration = 0.1;
            mtwnNode node;
            CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
        }
        CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "a finished clip");
        mtwnBinding binding = mtwnBind(0, target);
        mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = &binding;
        playerDef.bindingCount = 1;
        for (int p = 0; p < 2; ++p)
        {
            mtwnPlayerId player;
            CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success, "a player");
        }
        CHECK(mtwnUpdate(context, 10000000u, NULL, 0, NULL) == mtwn_success, "an update");
        CHECK(mtwnContext_SetReducedMotion(context, true) == mtwn_success, "reduced motion");
        CHECK(mtwnUpdate(context, 20000000u, NULL, 0, NULL) == mtwn_success, "an update");
        uint64_t dropped = 0;
        CHECK(mtwnReadEvents(context, records[k], 64, &counts[k], &dropped) == mtwn_success,
              "read");
        CHECK(mtwnTarget_GetState(context, target, &finished[k]) == mtwn_success, "state");
        mtwnDestroyContext(context);
    }
    bool same = counts[0] == counts[1] && counts[0] >= 12;
    for (uint32_t i = 0; same && i < counts[0]; ++i)
    {
        same = SameRecord(&records[0][i], &records[1][i]);
    }
    // After the two starts, the first player's records from the finish: the
    // track added last, crossed first, leads.
    bool ordered = counts[0] >= 3 && records[0][2].node == 4 && records[0][2].clipTime < 0.15;
    CHECK(same && ordered, "one player's records of one time in order along its sweep");
    // Finished and not kept, the players committed their end as the base.
    CHECK(finished[0].base.x == 1.0f && Same(finished[0].base, finished[1].base) &&
              Same(finished[0].value, finished[1].value),
          "finished players' ends committed as the base alike");
}

// The groups on real threads, at once: the same bits as the serial twin.
// Under ThreadSanitizer (the CI's TSan job) any memory two groups touch
// is reported.
static void TestThreads(void)
{
    s_threaded = true;
    Taken taken = {0};
    uint32_t differences = 0;
    for (uint64_t seed = 41; seed <= 44; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 8u * 1024u * 1024u;
        def.eventCapacity = seed % 2 == 0 ? 4096u : 64u;
        def.bindingCapacity = 2000u;
        differences += Run(def, seed, seed == 44 ? 240u : 0u, &taken);
    }
    s_threaded = false;
    CHECK(differences == 0 && taken.staged > 1000,
          "groups on threads at once, the same bits as serial");
}

// A task system that runs every range at once.
static void* RunAtOnce(mtwnTaskFn* task, int32_t itemCount, int32_t minRange, void* taskContext,
                       void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext);
    return NULL;
}

static void WaitForNothing(void* userTask, void* userContext)
{
    (void)userTask;
    (void)userContext;
}

static bool Alone(const mtwnContext* context, mtwnPlayerId player, uint32_t slot)
{
    const mtwnClipPlay* play = &mtwnPlayerAt(context, player.index1)->play;
    return context->bindings.entries[play->bindingStart + slot].alone;
}

// A kept player of a clip of two scalar tracks, slots 0 and 1, bound to
// the targets given (index1 0 for none).
static mtwnPlayerId PlayTwo(mtwnContext* context, mtwnClipId clip, mtwnTargetId first,
                            mtwnTargetId second)
{
    mtwnBinding bindings[2] = {mtwnBind(0, first), mtwnBind(1, second)};
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = first.index1 != 0 ? bindings : bindings + 1;
    def.bindingCount = (first.index1 != 0 ? 1u : 0u) + (second.index1 != 0 ? 1u : 0u);
    def.keep = true;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(context, &def, &id) == mtwn_success, "a player");
    return id;
}

static mtwnClipId TwoTracks(mtwnContext* context)
{
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    for (uint32_t slot = 0; slot < 2; ++slot)
    {
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.slot = slot;
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 1.0;
        mtwnNode node = 0;
        CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
    }
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    return clip;
}

static void Step(mtwnContext* context, double seconds)
{
    CHECK(mtwnUpdate(context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) == mtwn_success,
          "an update");
}

// Each clip's player's binding is its target's alone when no other binding
// names the target and no transition is on it (with a task system): the
// writers kept as players are made and destroyed, transitions start and
// end and targets are destroyed, the targets that need an order counted
// exactly, the context grown with them.
static void TestWriters(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = RunAtOnce;
    def.finishTask = WaitForNothing;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnTargetId t[5];
    for (int k = 0; k < 5; ++k)
    {
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &t[k]) == mtwn_success,
              "a target");
    }
    mtwnClipId clip = TwoTracks(context);
    mtwnPlayerId one = PlayTwo(context, clip, t[0], t[1]);
    mtwnPlayerId two = PlayTwo(context, clip, t[1], (mtwnTargetId){0, 0});
    mtwnPlayerId three = PlayTwo(context, clip, t[2], t[3]);
    mtwnPlayerId twice = PlayTwo(context, clip, t[4], t[4]);
    mtwnTransitionDef move = mtwnDefaultTransitionDef();
    move.target = t[2];
    move.value = (mtwnVec4){1.0f, 0.0f, 0.0f, 0.0f};
    move.duration = 0.1;
    mtwnPlayerId moving;
    CHECK(mtwnStartTransition(context, &move, &moving) == mtwn_success, "a transition");
    Step(context, 0.0);
    CHECK(Alone(context, one, 0) && !Alone(context, one, 1) && !Alone(context, two, 0) &&
              !Alone(context, two, 1),
          "a target bound by two players is neither's alone, an unbound slot no one's");
    CHECK(!Alone(context, three, 0) && Alone(context, three, 1),
          "a target with a transition on it is not a player's alone");
    CHECK(!Alone(context, twice, 0) && !Alone(context, twice, 1),
          "a target bound twice by one player is not alone either");
    CHECK(context->layeredTargets == 3,
          "layered: a target two bind, one with a transition, one bound twice");

    CHECK(mtwnDestroyPlayer(context, two) == mtwn_success, "destroyed");
    Step(context, 0.05);
    CHECK(Alone(context, one, 1), "the other player gone, the target is the first's alone");
    Step(context, 0.2);
    CHECK(context->layeredTargets == 1, "a player and a transition gone, two left layered");
    Step(context, 0.25);
    CHECK(Alone(context, three, 0), "the transition ended, the target is the player's alone");

    CHECK(mtwnDestroyTarget(context, t[0]) == mtwn_success, "destroyed");
    Step(context, 0.3);
    CHECK(!Alone(context, one, 0), "a destroyed target is no binding's alone");
    // A target made in its place starts with no writers: a transition on it
    // changes no binding's alone.
    mtwnTargetId reused;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &reused) == mtwn_success &&
              reused.index1 == t[0].index1,
          "the slot reused");
    move.target = reused;
    CHECK(mtwnStartTransition(context, &move, &moving) == mtwn_success &&
              context->layeredTargets == 1,
          "a transition on a target no player binds layers nothing");
    move.target = t[3];
    CHECK(mtwnStartTransition(context, &move, &moving) == mtwn_success, "a transition");
    Step(context, 0.32);
    CHECK(!Alone(context, three, 1), "a transition started on a bound target");
    mtwnPlayerId four = PlayTwo(context, clip, t[1], (mtwnTargetId){0, 0});
    Step(context, 0.34);
    CHECK(!Alone(context, one, 1) && !Alone(context, four, 0), "a player made binds it too");
    // A player destroyed leaves its targets with no writers counted.
    CHECK(mtwnDestroyPlayer(context, twice) == mtwn_success, "destroyed");
    Step(context, 0.345);
    move.target = t[4];
    CHECK(mtwnStartTransition(context, &move, &moving) == mtwn_success &&
              context->layeredTargets == 2,
          "a transition on a target its only player left layers nothing");

    def.playerCapacity *= 2;
    CHECK(mtwnGrowContext(&context, &def) == mtwn_success, "grown");
    CHECK(context->layeredTargets == 2, "the counts kept across growing");
    Step(context, 0.35);
    CHECK(Alone(context, three, 0) && !Alone(context, one, 1) && !Alone(context, four, 0),
          "the same after growing");
    // A layered target destroyed is layered no more.
    CHECK(mtwnDestroyTarget(context, t[1]) == mtwn_success && context->layeredTargets == 1,
          "a target two players bind destroyed");
    mtwnDestroyContext(context);

    // Without a task system the writers are counted, no alone worked out.
    mtwnContextDef plain = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&plain, &context) == mtwn_success, "a context");
    mtwnTargetId lone;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &lone) == mtwn_success,
          "a target");
    mtwnPlayerId only = PlayTwo(context, TwoTracks(context), lone, (mtwnTargetId){0, 0});
    Step(context, 0.0);
    CHECK(!Alone(context, only, 0) && mtwnTargetAt(context, lone.index1)->clipWriters == 1,
          "without a task system, writers counted and no alone worked out");
    mtwnDestroyContext(context);
}

// A player whose results and changes fill its group's staging exactly
// fits: 16 bindings give each group room for two results and two
// changes, which a clip of one track (its root and the track) and one
// binding take.
static void TestExactFit(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.bindingCapacity = 16;
    def.enqueueTask = RunAtOnce;
    def.finishTask = WaitForNothing;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    CHECK(context->works[0].trackCapacity == 2 && context->works[0].changeCapacity == 2,
          "room for two results and two changes a group");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a clip of one track");
    for (int p = 0; p < 2; ++p)
    {
        mtwnTargetId target = {0, 0};
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding binding = mtwnBind(0, target);
        mtwnPlayerDef player = mtwnDefaultPlayerDef();
        player.clip = clip;
        player.bindings = &binding;
        player.bindingCount = 1;
        mtwnPlayerId id = {0, 0};
        CHECK(mtwnCreatePlayer(context, &player, &id) == mtwn_success, "a player");
    }
    Step(context, 0.0);
    Step(context, 0.25);
    Step(context, 0.5);
    CHECK(mtwnContext_GetSerialAdvances(context) == 0, "both players run by their groups");
    mtwnDestroyContext(context);
}

// A binding's alone is worked out only by the groups, so it is as fresh
// as the last update with groups to run: with one player left an update
// runs none, so a binding alone before its target was destroyed must
// still look its target up, or it writes the target made in the slot
// since.
static void TestStaleAlone(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = RunAtOnce;
    def.finishTask = WaitForNothing;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnTargetId a = {0, 0};
    mtwnTargetId b = {0, 0};
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &a) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &b) == mtwn_success,
          "two targets");
    mtwnClipId clip = TwoTracks(context);
    mtwnPlayerId kept = PlayTwo(context, clip, a, (mtwnTargetId){0, 0});
    mtwnPlayerId other = PlayTwo(context, clip, b, (mtwnTargetId){0, 0});
    Step(context, 0.0);
    CHECK(Alone(context, kept, 0), "counted alone with two players");
    CHECK(mtwnDestroyPlayer(context, other) == mtwn_success &&
              mtwnDestroyTarget(context, a) == mtwn_success,
          "one player left, its target destroyed");
    mtwnTargetId made = {0, 0};
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &made) == mtwn_success &&
              made.index1 == a.index1,
          "a target made in the slot");
    Step(context, 0.05);
    Step(context, 0.1);
    mtwnVec4 value = {1.0f, 1.0f, 1.0f, 1.0f};
    CHECK(mtwnTarget_GetValue(context, made, &value) == mtwn_success && value.x == 0.0f,
          "the new target written by no player");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestWriters();
    TestStaleAlone();
    TestExactFit();
    TestStagingBounds();
    TestOneTimeOrder();
    TestReplayOff();
    TestThreads();
    TestTwins();
    return s_failures == 0 ? 0 : 1;
}
