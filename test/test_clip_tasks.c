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
// Sibling tracks found sharing their timing.
static uint32_t s_shared;
// Players made and refused (only past the capacities).
static uint32_t s_made;
static uint32_t s_refused;
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
    const mtwnClipSlot* clip = mtwnClipAt(twin->context, twin->clips[c].index1);
    // A binding for each slot the clip's tracks use; one no track uses is
    // refused.
    mtwnBinding bindings[s_slots];
    uint32_t bound = 0;
    for (uint32_t s = 0; s < s_slots; ++s)
    {
        mtwnBinding binding = mtwnBind(s, twin->targets[s][Next() % s_targetsPerSlot]);
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

// What the parallel twin's groups did, to see each way taken, and what
// they must not have done.
typedef struct Taken
{
    uint32_t staged;
    uint32_t serial;
    // Left to the serial pass for staging that ran out.
    uint32_t ranOut;
    uint32_t dropped;
    // A clip of more nodes than a group keeps, staged anyway.
    uint32_t bigStaged;
    // A clip's own node times written by a group.
    uint32_t touched;
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
    for (uint32_t i = 0; i < order->count; ++i)
    {
        bool serial = context->staged[i].serial;
        taken->serial += serial ? 1u : 0u;
        taken->staged += serial ? 0u : 1u;
        bool big = mtwnClipAt(context, order->clips[i])->nodeCount > MTWN_GROUP_NODES;
        taken->bigStaged += big && !serial ? 1u : 0u;
        taken->ranOut += serial && !big ? 1u : 0u;
        for (uint32_t c = 0; c < s_clips; ++c)
        {
            serialClip[c] = serialClip[c] || (serial && twin->clips[c].index1 == order->clips[i]);
        }
    }
    for (uint32_t c = 0; c < s_clips; ++c)
    {
        const mtwnClipSlot* clip = mtwnClipAt(context, twin->clips[c].index1);
        for (uint32_t k = 0; k < clip->nodeCount && !serialClip[c]; ++k)
        {
            taken->touched +=
                memcmp(&clip->times[k].now, &s_poison, sizeof s_poison) == 0 ? 0u : 1u;
        }
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
// Staging that holds nothing: every player that makes anything runs out
// and is put back, the whole update made by the serial pass.
static bool s_emptyStaging;
// The second twin serial too, its sibling tracks never sharing timing:
// the replay of their records and their kept timing differ from none.
static bool s_unsharedMode;

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
        for (uint32_t g = 0; k == 1 && s_emptyStaging && g < MTWN_GROUPS; ++g)
        {
            twins[k].context->works[g].recordCapacity = 0;
            twins[k].context->works[g].trackCapacity = 0;
        }
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
            CHECK(mtwnUpdate(twins[k].context, now, changed[k], 64, &totals[k]) == mtwn_success,
                  "an update");
        }
        script = s_script;
        if (!s_unsharedMode)
        {
            Count(&twins[1], &order, taken);
        }
        differences += Compare(&twins[0], &twins[1], changed[0], changed[1], totals[0], totals[1]);
    }
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
    return differences;
}

static void TestTwins(void)
{
    s_host = 11;
    Taken taken = {0, 0, 0, 0, 0, 0};
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
    CHECK(taken.bigStaged == 0, "a clip of more nodes than a group keeps plays serially");
    CHECK(taken.touched == 0, "the groups leave each clip's own node times alone");

    // Every player put back: still the serial update's bits.
    s_emptyStaging = true;
    Taken empty = {0, 0, 0, 0, 0, 0};
    uint32_t emptyDifferences = 0;
    for (uint64_t seed = 1; seed <= 3; ++seed)
    {
        mtwnContextDef def = mtwnDefaultContextDef();
        def.playerCapacity = 128;
        def.clipStorageBytes = 8u * 1024u * 1024u;
        def.bindingCapacity = 2000u;
        emptyDifferences += Run(def, seed, 0, &empty);
    }
    s_emptyStaging = false;
    CHECK(emptyDifferences == 0 && empty.ranOut > 1000,
          "with staging that holds nothing, every player put back, the same bits");
}

// The replay of sibling tracks' records and their kept timing rest on
// sharesTiming naming every input their records and values depend on: a
// random script with them and with them forced off makes the same bits.
static void TestReplayOff(void)
{
    s_unsharedMode = true;
    s_shared = 0;
    Taken unused = {0, 0, 0, 0, 0, 0};
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

int main(void)
{
    TestStagingBounds();
    TestOneTimeOrder();
    TestReplayOff();
    TestTwins();
    return s_failures == 0 ? 0 : 1;
}
