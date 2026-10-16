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
static uint64_t s_host;

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
    mtwnTargetId targets[s_slots][s_targetsPerSlot];
    mtwnPlayerId players[s_players];
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
        int32_t length = minRange + (int32_t)(NextOf(&s_host) % 4u);
        int32_t end = at + length < itemCount ? at + length : itemCount;
        s_pending.starts[s_pending.count] = at;
        s_pending.ends[s_pending.count] = end;
        ++s_pending.count;
        at = end;
    }
    for (int32_t i = s_pending.count - 1; i > 0; --i)
    {
        int32_t j = (int32_t)(NextOf(&s_host) % (uint32_t)(i + 1));
        int32_t start = s_pending.starts[i];
        int32_t end = s_pending.ends[i];
        s_pending.starts[i] = s_pending.starts[j];
        s_pending.ends[i] = s_pending.ends[j];
        s_pending.starts[j] = start;
        s_pending.ends[j] = end;
    }
    if (NextOf(&s_host) % 2u == 0u)
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
    for (uint32_t k = 0; k < nodes; ++k)
    {
        mtwnPosition at = {0};
        at.kind = (mtwnPositionKind)(Next() % 3);
        at.offset = Unit() * 0.3;
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
            track.incremental = Next() % 4 == 0;
            (void)mtwnClip_AddTrack(twin->context, twin->clips[c], &track, &node);
        }
    }
    CHECK(mtwnClip_Finish(twin->context, twin->clips[c]) == mtwn_success, "a finished clip");
}

static void MakePlayer(Twin* twin, uint32_t p)
{
    uint32_t c = Next() % s_clips;
    mtwnBinding bindings[s_slots];
    for (uint32_t s = 0; s < s_slots; ++s)
    {
        bindings[s] = mtwnBind(s, twin->targets[s][Next() % s_targetsPerSlot]);
        bindings[s].composite = Next() % 3 == 0 ? mtwn_compositeAdd : mtwn_compositeReplace;
        bindings[s].weight = Next() % 3 == 0 ? 0.5f : 1.0f;
    }
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = twin->clips[c];
    def.bindings = bindings;
    def.bindingCount = mtwnClipAt(twin->context, twin->clips[c].index1)->slotCount;
    def.keep = Next() % 2 == 0;
    def.rate = Next() % 5 == 0 ? -1.0 : 1.0;
    if (mtwnCreatePlayer(twin->context, &def, &twin->players[p]) != mtwn_success)
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
        def.target = twin->targets[0][p % s_targetsPerSlot];
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

// What the parallel twin's groups did, to see each way taken.
typedef struct Taken
{
    uint32_t staged;
    uint32_t serial;
    uint32_t dropped;
} Taken;

static void Count(const mtwnContext* context, Taken* taken)
{
    uint32_t clipPlayers = 0;
    for (uint32_t index1 = context->firstPlayer; index1 != 0;)
    {
        const mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
        clipPlayers += slot->type == MTWN_PLAYER_CLIP ? 1u : 0u;
        index1 = slot->later;
    }
    for (uint32_t i = 0; i < clipPlayers && clipPlayers >= 2; ++i)
    {
        taken->serial += context->staged[i].serial ? 1u : 0u;
        taken->staged += context->staged[i].serial ? 0u : 1u;
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
    for (uint32_t i = 0; i < totalA && i < totalB && i < 64; ++i)
    {
        differences += changedA[i].index1 == changedB[i].index1 ? 0u : 1u;
    }
    for (uint32_t s = 0; s < s_slots; ++s)
    {
        for (uint32_t t = 0; t < s_targetsPerSlot; ++t)
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
    differences += nA == nB && droppedA == droppedB ? 0u : 1u;
    for (uint32_t i = 0; i < nA && i < nB; ++i)
    {
        differences += SameRecord(&recordsA[i], &recordsB[i]) ? 0u : 1u;
    }
    return differences;
}

// One run of the script on twins of a def: the differences found.
static uint32_t Run(mtwnContextDef def, uint64_t seed, uint32_t bigNodes, Taken* taken)
{
    Twin twins[2];
    memset(twins, 0, sizeof twins);
    for (int k = 0; k < 2; ++k)
    {
        mtwnContextDef own = def;
        own.enqueueTask = k == 1 ? Enqueue : NULL;
        own.finishTask = k == 1 ? Finish : NULL;
        CHECK(mtwnCreateContext(&own, &twins[k].context) == mtwn_success, "a context");
        s_script = seed;
        for (uint32_t s = 0; s < s_slots; ++s)
        {
            for (uint32_t t = 0; t < s_targetsPerSlot; ++t)
            {
                CHECK(mtwnCreateTarget(twins[k].context, s_kinds[s], KeyValue(s_kinds[s]),
                                       &twins[k].targets[s][t]) == mtwn_success,
                      "a target");
            }
        }
        for (uint32_t c = 0; c < s_clips; ++c)
        {
            Build(&twins[k], c, c == 0 && bigNodes > 0 ? bigNodes : 6 + Next() % 8);
        }
        for (uint32_t p = 0; p < s_players; ++p)
        {
            MakePlayer(&twins[k], p);
        }
    }
    uint32_t differences = 0;
    uint64_t now = 0;
    uint64_t script = s_script;
    for (uint32_t f = 0; f < s_frames; ++f)
    {
        now += 8000000u + (uint64_t)(Next() % 40) * 1000000u;
        uint32_t controls = Next() % 4;
        mtwnTargetId changed[2][64];
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
            CHECK(mtwnUpdate(twins[k].context, now, changed[k], 64, &totals[k]) == mtwn_success,
                  "an update");
        }
        script = s_script;
        Count(twins[1].context, taken);
        differences += Compare(&twins[0], &twins[1], changed[0], changed[1], totals[0], totals[1]);
    }
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
    return differences;
}

static void TestTwins(void)
{
    s_host = 11;
    Taken taken = {0, 0, 0};
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
        differences += Run(def, seed, seed % 4 == 0 ? 160u : 0u, &taken);
    }
    CHECK(differences == 0, "the twins hold the same bits after every update");
    CHECK(s_enqueued > 100, "the task system was handed the groups");
    CHECK(taken.staged > 1000, "players staged by the groups");
    CHECK(taken.serial > 10, "players left to the serial pass");
    CHECK(taken.dropped > 0, "records past a group's staging dropped as the queue would");
}

int main(void)
{
    TestTwins();
    return s_failures == 0 ? 0 : 1;
}
