// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The clips' players on the host's task system: twin
// contexts run one random script, one serially and one through a task
// system that splits the groups into random ranges and runs them in a
// random order, now or at the wait. After every update the twins hold
// the same bits: every target's base, value and velocity, the changed
// targets in order, every record and the dropped count. Small queues,
// few bindings and a clip of more nodes than a group keeps make the
// staging run out, so every way back to the serial pass is taken.

#include "test_harness.h"
#include "test_hooks.h"

#include "maul-tween/clip.h"
#include "maul-tween/event.h"
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
    // The slots each clip's tracks use.
    bool used[s_clips][s_slots];
} Twin;

// Ranges run at the wait, for a host that returns a handle.
typedef struct Pending
{
    mtwnTaskFunction* task;
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
    mtwnTaskFunction* task;
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
static void* Enqueue(mtwnTaskFunction* task, int32_t itemCount, int32_t minRange, void* taskContext,
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
// Clips whose nodes all lie at the deepest chain a clip may hold, so
// each group's levels are filled to their end.
static bool s_deep;

static void Build(Twin* twin, uint32_t c, uint32_t nodes)
{
    mtwnClipDef def = mtwnDefaultClipDef();
    def.nodeCapacity = nodes + (s_deep ? MTWN_MAX_CLIP_DEPTH : 0u);
    def.keyframeCapacity = nodes * 3;
    CHECK(mtwnCreateClip(twin->context, &def, &twin->clips[c]) == mtwn_success, "a clip");
    mtwnNode timelines[4] = {MTWN_ROOT_NODE};
    uint32_t timelineCount = 1;
    for (uint32_t depth = 1; s_deep && depth < MTWN_MAX_CLIP_DEPTH; ++depth)
    {
        mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
        timeline.parent = timelines[0];
        timeline.timing.iterations = HUGE_VAL;
        timeline.timing.fill = mtwn_fillBoth;
        timeline.autoDuration = true;
        CHECK(mtwnClip_AddTimeline(twin->context, twin->clips[c], &timeline, &timelines[0]) ==
                  mtwn_success,
              "a timeline in the chain");
    }
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
        if (what == 0 && timelineCount < 4 && !s_deep)
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
                keys[i].easing = mtwnMakeLinearEasing();
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
            twin->used[c][slot] = twin->used[c][slot] || lastTrack;
            last = track;
            continue;
        }
        lastTrack = false;
    }
    CHECK(mtwnClip_Seal(twin->context, twin->clips[c]) == mtwn_success, "a sealed clip");
    // The replay and the kept timing forced off for the unshared twin.
    s_shared += mtwnTestShareTiming(twin->context, twin->clips[c], !twin->unshared);
}

static void MakePlayer(Twin* twin, uint32_t p)
{
    uint32_t c = Next() % s_clips;
    // Clip 0 has one player at most, clip 1 two.
    if (s_lonelyClip && c < 2u && mtwnTestClipUsers(twin->context, twin->clips[c]) > c)
    {
        c = 2u + Next() % (s_clips - 2u);
    }
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
        mtwnBinding binding = mtwnMakeBinding(s, twin->targets[s][t]);
        binding.composite = Next() % 3 == 0 ? mtwn_compositeAdd : mtwn_compositeReplace;
        binding.weight = Next() % 3 == 0 ? 0.5f : 1.0f;
        if (twin->used[c][s])
        {
            bindings[bound++] = binding;
        }
    }
    mtwnClipPlayerDef def = mtwnDefaultClipPlayerDef();
    def.clip = twin->clips[c];
    def.bindings = bindings;
    def.bindingCount = bound;
    def.keep = Next() % 2 == 0;
    def.rate = Next() % 5 == 0 ? -1.0 : 1.0;
    mtwnResult made = mtwnCreateClipPlayer(twin->context, &def, &twin->players[p]);
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
        (void)mtwnClipPlayer_Seek(context, player, argument);
        break;
    case 1:
        (void)mtwnClipPlayer_Reverse(context, player);
        break;
    case 2:
        (void)mtwnClipPlayer_SetRate(context, player, argument - 1.0);
        break;
    case 3:
        (void)mtwnClipPlayer_Pause(context, player);
        break;
    case 4:
        (void)mtwnClipPlayer_Play(context, player);
        break;
    case 5:
        (void)mtwnClipPlayer_Sweep(context, player, argument);
        break;
    case 6:
        (void)mtwnClipPlayer_Finish(context, player);
        break;
    case 7:
        mtwnContext_EnableReducedMotion(context, argument > 1.0);
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
// they must not have done; and the serial twin's
// mtwnContext_GetSerialAdvanceCount, and the hooked twin's.
typedef struct Taken
{
    mtwnTestTasks groups;
    uint64_t serialUnhooked;
    uint64_t advances;
} Taken;

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
    CHECK(mtwnContext_ReadEvents(a->context, recordsA, 4096, &nA, &droppedA) == mtwn_success,
          "read");
    CHECK(mtwnContext_ReadEvents(b->context, recordsB, 4096, &nB, &droppedB) == mtwn_success,
          "read");
    differences += nA == nB && droppedA == droppedB &&
                           mtwnContext_GetDroppedEventCount(a->context) ==
                               mtwnContext_GetDroppedEventCount(b->context) &&
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
    for (uint32_t p = 0; p < s_players; ++p)
    {
        s_alone += mtwnTestAloneCount(twin->context, twin->players[p]);
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
        if (k == 1 && s_emptyStaging != 0)
        {
            mtwnTestEmptyStaging(twins[k].context, s_emptyStaging);
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
    mtwnTestOrder order;
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
                mtwnContext_EnableReducedMotion(twins[k].context, f == s_frames / 2);
            }
            if (k == 1 && !s_unsharedMode)
            {
                mtwnTestTasksBefore(twins[k].context, twins[k].clips, s_clips, &order);
            }
            CHECK(mtwnContext_Update(twins[k].context, now, changed[k], 512, &totals[k]) ==
                      mtwn_success,
                  "an update");
        }
        script = s_script;
        if (!s_unsharedMode)
        {
            mtwnTestTasksAfter(twins[1].context, twins[1].clips, s_clips, &order, &taken->groups);
            CountAlone(&twins[1]);
        }
        differences += Compare(&twins[0], &twins[1], changed[0], changed[1], totals[0], totals[1]);
    }
    taken->serialUnhooked += mtwnContext_GetSerialAdvanceCount(twins[0].context);
    taken->advances += mtwnContext_GetSerialAdvanceCount(twins[1].context);
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
    CHECK(taken.groups.staged > 1000, "players staged by the groups");
    CHECK(taken.groups.serial > 10, "players left to the serial pass");
    CHECK(taken.groups.ranOut > 10, "players whose staging ran out put back and left to it");
    CHECK(taken.groups.dropped > 0, "records past a group's staging dropped as the queue would");
    CHECK(taken.groups.bigStaged == 0,
          "a clip of more nodes than a group keeps and other players plays serially");
    CHECK(taken.groups.touched == 0,
          "the groups leave a clip's own node times alone if it has players");

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
    CHECK(lonelyDifferences == 0 && lonely.groups.touched == 0 && lonely.groups.ownTimes > 10,
          "a clip of one player sampled in its own node times, the same bits");
    CHECK(lonely.groups.bigStaged == 0 && lonely.groups.bigAlone > 10,
          "a clip of one player in a group past the nodes a group keeps times for");
    CHECK(taken.serialUnhooked == 0 && taken.advances == taken.groups.serial,
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
    CHECK(empty.advances == empty.groups.serial, "each put back counted");
    CHECK(emptyDifferences == 0 && empty.groups.ranOut > 1000,
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
    CHECK(emptyOwnDifferences == 0 && emptyOwn.groups.serial > 100,
          "each staging empty, players with targets of their own left serial, the same bits");
    CHECK(own.groups.aloneInGroups > 10000,
          "players with targets of their own run by their groups when they fit");
    CHECK(s_alone > 10000 && own.groups.staged > 1000 && own.groups.serial > 10,
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
        mtwnTestStaging staged = mtwnTestStageRecords(context);
        uint32_t capacity = staged.capacity;
        CHECK(capacity == (large ? 1024u : 4u), "a group stages up to the queue, at most 1,024");
        CHECK(staged.staged == capacity && staged.lastKey == capacity,
              "records staged to the capacity");
        CHECK(large ? staged.overflow && staged.dropped == 0
                    : !staged.overflow && staged.dropped == 1,
              "past a staging like the queue a record is dropped, past a smaller one it runs out");
        CHECK(staged.nextUntouched, "the next group's staging untouched");
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
        keys[0].easing = mtwnMakeLinearEasing();
        keys[1].easing = mtwnMakeLinearEasing();
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
        CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "a sealed clip");
        mtwnBinding binding = mtwnMakeBinding(0, target);
        mtwnClipPlayerDef playerDef = mtwnDefaultClipPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = &binding;
        playerDef.bindingCount = 1;
        for (int p = 0; p < 2; ++p)
        {
            mtwnPlayerId player;
            CHECK(mtwnCreateClipPlayer(context, &playerDef, &player) == mtwn_success, "a player");
        }
        CHECK(mtwnContext_Update(context, 10000000u, NULL, 0, NULL) == mtwn_success, "an update");
        mtwnContext_EnableReducedMotion(context, true);
        CHECK(mtwnContext_IsReducedMotionEnabled(context), "reduced motion");
        CHECK(mtwnContext_Update(context, 20000000u, NULL, 0, NULL) == mtwn_success, "an update");
        uint64_t dropped = 0;
        CHECK(mtwnContext_ReadEvents(context, records[k], 64, &counts[k], &dropped) == mtwn_success,
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
// Groups on threads over clips at the deepest chain: each group's levels
// hold the whole chain at once, next to the next group's, which a layout
// a few levels short overlaps (a race the thread sanitizer reports).
// One seed here, two in `test_clip_tasks threads`.
static void TestDeepThreads(uint64_t last)
{
    s_threaded = true;
    s_deep = true;
    Taken taken = {0};
    uint32_t differences = 0;
    for (uint64_t seed = 51; seed <= last; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 16u * 1024u * 1024u;
        def.eventCapacity = 4096u;
        def.bindingCapacity = 2000u;
        differences += Run(def, seed, 0u, &taken);
    }
    s_deep = false;
    s_threaded = false;
    CHECK(differences == 0 && taken.groups.staged > 100,
          "deep clips' groups on threads at once, the same bits as serial");
}

// Two seeds here, the second with a first update of 240 players; four in
// `test_clip_tasks threads`.
static void TestThreads(uint64_t first)
{
    s_threaded = true;
    Taken taken = {0};
    uint32_t differences = 0;
    for (uint64_t seed = first; seed <= 44; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 8u * 1024u * 1024u;
        def.eventCapacity = seed % 2 == 0 ? 4096u : 64u;
        def.bindingCapacity = 2000u;
        differences += Run(def, seed, seed == 44 ? 240u : 0u, &taken);
    }
    s_threaded = false;
    CHECK(differences == 0 && taken.groups.staged > 1000,
          "groups on threads at once, the same bits as serial");
}

// A task system that runs every range at once.
static void* RunAtOnce(mtwnTaskFunction* task, int32_t itemCount, int32_t minRange,
                       void* taskContext, void* userContext)
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
    return mtwnTestIsAlone(context, player, slot);
}

// A kept player of a clip of two scalar tracks, slots 0 and 1, bound to
// the targets given (index1 0 for none).
static mtwnPlayerId PlayTwo(mtwnContext* context, mtwnClipId clip, mtwnTargetId first,
                            mtwnTargetId second)
{
    mtwnBinding bindings[2] = {mtwnMakeBinding(0, first), mtwnMakeBinding(1, second)};
    mtwnClipPlayerDef def = mtwnDefaultClipPlayerDef();
    def.clip = clip;
    def.bindings = first.index1 != 0 ? bindings : bindings + 1;
    def.bindingCount = (first.index1 != 0 ? 1u : 0u) + (second.index1 != 0 ? 1u : 0u);
    def.keep = true;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreateClipPlayer(context, &def, &id) == mtwn_success, "a player");
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
    keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
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
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
    return clip;
}

static void Step(mtwnContext* context, double seconds)
{
    CHECK(mtwnContext_Update(context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) ==
              mtwn_success,
          "an update");
}

// Each clip's player's binding is its target's alone when no other binding
// names the target and no transition is on it (with a task system): the
// writers kept as players are made and destroyed, transitions start and
// end and targets are destroyed, the context grown with them. The count of
// targets that need an order is asserted against a recount on every
// update with groups.
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

    CHECK(mtwnDestroyPlayer(context, two) == mtwn_success, "destroyed");
    Step(context, 0.05);
    CHECK(Alone(context, one, 1), "the other player gone, the target is the first's alone");
    Step(context, 0.2);
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
    CHECK(mtwnStartTransition(context, &move, &moving) == mtwn_success,
          "a transition on a target no player binds");
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
    CHECK(mtwnStartTransition(context, &move, &moving) == mtwn_success,
          "a transition on a target its only player left");

    def.playerCapacity *= 2;
    CHECK(mtwnContext_Grow(&context, &def) == mtwn_success, "grown");
    Step(context, 0.35);
    CHECK(Alone(context, three, 0) && !Alone(context, one, 1) && !Alone(context, four, 0),
          "the same after growing");
    // A layered target destroyed is layered no more.
    CHECK(mtwnDestroyTarget(context, t[1]) == mtwn_success, "a target two players bind destroyed");
    Step(context, 0.36);
    mtwnDestroyContext(context);

    // Without a task system the writers are counted, no alone worked out.
    mtwnContextDef plain = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&plain, &context) == mtwn_success, "a context");
    mtwnTargetId lone;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &lone) == mtwn_success,
          "a target");
    mtwnPlayerId only = PlayTwo(context, TwoTracks(context), lone, (mtwnTargetId){0, 0});
    Step(context, 0.0);
    CHECK(!Alone(context, only, 0), "without a task system, no alone worked out");
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
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Seal(context, clip) == mtwn_success,
          "a clip of one track");
    for (int p = 0; p < 2; ++p)
    {
        mtwnTargetId target = {0, 0};
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding binding = mtwnMakeBinding(0, target);
        mtwnClipPlayerDef player = mtwnDefaultClipPlayerDef();
        player.clip = clip;
        player.bindings = &binding;
        player.bindingCount = 1;
        mtwnPlayerId id = {0, 0};
        CHECK(mtwnCreateClipPlayer(context, &player, &id) == mtwn_success, "a player");
    }
    Step(context, 0.0);
    Step(context, 0.25);
    Step(context, 0.5);
    CHECK(mtwnContext_GetSerialAdvanceCount(context) == 0, "both players run by their groups");
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

// The same with a writer added before the target is destroyed: each
// moves the writers' epoch on, so the two cannot bring it back to the
// one the alone bindings were worked out at.
static void TestStaleAloneAfterWriter(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = RunAtOnce;
    def.finishTask = WaitForNothing;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnTargetId a = {0, 0};
    mtwnTargetId b = {0, 0};
    mtwnTargetId c = {0, 0};
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &a) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &b) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &c) == mtwn_success,
          "three targets");
    mtwnClipId clip = TwoTracks(context);
    mtwnPlayerId kept = PlayTwo(context, clip, a, (mtwnTargetId){0, 0});
    (void)PlayTwo(context, clip, b, (mtwnTargetId){0, 0});
    Step(context, 0.0);
    CHECK(Alone(context, kept, 0), "counted alone with two players");
    (void)PlayTwo(context, clip, c, (mtwnTargetId){0, 0});
    CHECK(mtwnDestroyTarget(context, a) == mtwn_success, "a writer added, then a target destroyed");
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

// A group writes a value that changed in any channel: here z alone, x, y
// and w held (a check of whole values let pass by its last two
// channels missed it).
static void TestOneChannel(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = RunAtOnce;
    def.finishTask = WaitForNothing;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnTargetId a = {0, 0};
    mtwnTargetId b = {0, 0};
    const mtwnVec4 still = {0, 0, 0, 1.0f};
    CHECK(mtwnCreateTarget(context, mtwn_valueVec4, still, &a) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueVec4, still, &b) == mtwn_success,
          "two targets");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value = still;
    keys[1].value = (mtwnVec4){0, 0, 1.0f, 1.0f};
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.track.kind = mtwn_valueVec4;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Seal(context, clip) == mtwn_success,
          "a track moving z");
    (void)PlayTwo(context, clip, a, (mtwnTargetId){0, 0});
    (void)PlayTwo(context, clip, b, (mtwnTargetId){0, 0});
    Step(context, 0.0);
    Step(context, 0.5);
    CHECK(mtwnContext_GetSerialAdvanceCount(context) == 0, "both players run by their groups");
    mtwnVec4 value = {0};
    CHECK(mtwnTarget_GetValue(context, a, &value) == mtwn_success && value.x == 0.0f &&
              value.y == 0.0f && fabsf(value.z - 0.5f) <= 1e-6f && value.w == 1.0f,
          "z half way, written by its group");
    mtwnDestroyContext(context);
}

// A player whose every binding is layered marks nothing alone, so it
// needs no room for alone changes: two players binding the same two
// targets, each two bindings against a group's room for three changes,
// run by their groups (asked to fit four changes, they went serial).
// A task system that counts the work it is handed and runs it at once.
static uint32_t s_handed;

static void* CountAndRun(mtwnTaskFunction* task, int32_t itemCount, int32_t minRange,
                         void* taskContext, void* userContext)
{
    ++s_handed;
    return RunAtOnce(task, itemCount, minRange, taskContext, userContext);
}

// One clip's player runs on the calling thread; a second makes groups,
// which go to the task system.
static void TestGroupsFromTwo(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = CountAndRun;
    def.finishTask = WaitForNothing;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnTargetId t[2];
    for (int k = 0; k < 2; ++k)
    {
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &t[k]) == mtwn_success,
              "a target");
    }
    mtwnClipId clip = TwoTracks(context);
    (void)PlayTwo(context, clip, t[0], (mtwnTargetId){0, 0});
    s_handed = 0;
    Step(context, 0.0);
    Step(context, 0.1);
    CHECK(s_handed == 0, "one player, nothing handed to the task system");
    (void)PlayTwo(context, clip, t[1], (mtwnTargetId){0, 0});
    Step(context, 0.2);
    CHECK(s_handed > 0, "two players, their groups handed to it");
    mtwnDestroyContext(context);

    // A lone player at half weight mixes from the base every update, with a
    // task system as without one.
    mtwnVec4 values[2][4];
    for (int k = 0; k < 2; ++k)
    {
        mtwnContextDef own = mtwnDefaultContextDef();
        own.enqueueTask = k == 1 ? CountAndRun : NULL;
        own.finishTask = k == 1 ? WaitForNothing : NULL;
        CHECK(mtwnCreateContext(&own, &context) == mtwn_success, "a context");
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding half = mtwnMakeBinding(0, target);
        half.weight = 0.5f;
        mtwnClipPlayerDef play = mtwnDefaultClipPlayerDef();
        play.clip = TwoTracks(context);
        play.bindings = &half;
        play.bindingCount = 1;
        mtwnPlayerId player;
        CHECK(mtwnCreateClipPlayer(context, &play, &player) == mtwn_success, "a player");
        for (int step = 0; step < 4; ++step)
        {
            Step(context, 0.1 * step);
            CHECK(mtwnTarget_GetValue(context, target, &values[k][step]) == mtwn_success,
                  "a value");
        }
        mtwnDestroyContext(context);
    }
    CHECK(memcmp(values[0], values[1], sizeof values[0]) == 0 && values[1][3].x > 0.0f,
          "a lone player's half weight from the base, the serial bits");
}

// A clip of a track and 140 labels: more nodes than a group keeps times
// for (128).
static mtwnClipId ManyNodes(mtwnContext* context)
{
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.nodeCapacity = 160;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
    uint32_t labels = 0;
    for (uint32_t key = 1; key <= 140; ++key)
    {
        labels += mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, (mtwnPosition){0}, key, &node) ==
                  mtwn_success;
    }
    CHECK(labels == 140, "its labels");
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
    return clip;
}

// A player of a clip of more nodes than a group keeps times for stays in
// its group while it is the clip's only player, however many others run,
// and plays on the calling thread once a second player plays the clip.
// Every player writes one target, so none has targets of its own, whose
// staging would keep it on the calling thread too.
static void TestManyNodesInGroups(void)
{
    for (int players = 1; players <= 2; ++players)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.enqueueTask = CountAndRun;
        def.finishTask = WaitForNothing;
        def.clipStorageBytes = 1u << 22;
        mtwnContext* context = NULL;
        CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnClipId many = ManyNodes(context);
        mtwnClipId few = TwoTracks(context);
        mtwnPlayerId first = PlayTwo(context, many, target, (mtwnTargetId){0, 0});
        for (int k = 1; k < players; ++k)
        {
            (void)PlayTwo(context, many, target, (mtwnTargetId){0, 0});
        }
        for (int k = 0; k < 8; ++k)
        {
            (void)PlayTwo(context, few, target, (mtwnTargetId){0, 0});
        }
        s_handed = 0;
        for (int step = 0; step < 4; ++step)
        {
            Step(context, 0.1 * step);
        }
        CHECK(!Alone(context, first, 0), "no player's target its own");
        CHECK(s_handed > 0, "the groups handed to the task system");
        uint64_t serial = mtwnContext_GetSerialAdvanceCount(context);
        CHECK(players == 1 ? serial == 0 : serial > 0,
              players == 1 ? "the clip's one player stays in its group"
                           : "with a second, its players play on the calling thread");
        mtwnDestroyContext(context);
    }
}

static void TestLayeredFit(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.bindingCapacity = 24;
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
    (void)PlayTwo(context, clip, a, b);
    (void)PlayTwo(context, clip, a, b);
    Step(context, 0.0);
    Step(context, 0.25);
    Step(context, 0.5);
    CHECK(mtwnContext_GetSerialAdvanceCount(context) == 0,
          "both layered players run by their groups");
    mtwnDestroyContext(context);
}

// With no argument, the fixed cases; with "twins", the random scripts on
// twin contexts, the nightly run's; with "threads", every seed of the
// groups on real threads, the dense test's.
int main(int argc, char** argv)
{
    bool twins = argc == 2 && strcmp(argv[1], "twins") == 0;
    bool threads = argc == 2 && strcmp(argv[1], "threads") == 0;
    if (argc > 2 || (argc == 2 && !twins && !threads))
    {
        printf("usage: test_clip_tasks [twins | threads]\n");
        return 2;
    }
    if (threads)
    {
        TestThreads(41);
        TestDeepThreads(52);
        return s_failures == 0 ? 0 : 1;
    }
    if (twins)
    {
        TestTwins();
        TestReplayOff();
        return s_failures == 0 ? 0 : 1;
    }
    TestWriters();
    TestLayeredFit();
    TestGroupsFromTwo();
    TestManyNodesInGroups();
    TestOneChannel();
    TestStaleAlone();
    TestStaleAloneAfterWriter();
    TestExactFit();
    TestStagingBounds();
    TestOneTimeOrder();
    TestThreads(43);
    TestDeepThreads(51);
    return s_failures == 0 ? 0 : 1;
}
