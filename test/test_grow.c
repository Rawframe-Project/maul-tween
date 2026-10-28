// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Growing a context: only when asked, to capacities at
// least its own (the sharing's any), with its own allocator; every id, a
// playing clip, a running transition, the queued records and the free
// slots carried over; on a refusal or an allocator's failure nothing
// changes. A context of no records plays as one with
// them, and grown to hold some, records from there on.

#include "test_harness.h"

#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// An allocator that can be told to fail, counting what it holds.
typedef struct Budget
{
    bool fail;
    uint64_t held;
    uint32_t blocks;
} Budget;

static void* Alloc(size_t size, size_t alignment, void* context)
{
    Budget* budget = (Budget*)context;
    if (budget->fail)
    {
        return NULL;
    }
    // Aligned by hand: some C libraries lack aligned_alloc.
    unsigned char* raw = (unsigned char*)malloc(size + alignment + sizeof(void*));
    if (raw == NULL)
    {
        return NULL;
    }
    uintptr_t start =
        ((uintptr_t)(raw + sizeof(void*)) + alignment - 1) & ~(uintptr_t)(alignment - 1);
    ((void**)start)[-1] = raw;
    budget->held += size;
    ++budget->blocks;
    return (void*)start;
}

static void Free(void* memory, size_t size, size_t alignment, void* context)
{
    (void)alignment;
    Budget* budget = (Budget*)context;
    if (memory != NULL)
    {
        free(((void**)memory)[-1]);
        budget->held -= size;
        --budget->blocks;
    }
}

static mtwnContextDef Small(Budget* budget)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.allocator = (mtwnAllocator){Alloc, Free, budget};
    def.targetCapacity = 2;
    def.playerCapacity = 2;
    def.clipCapacity = 1;
    def.eventCapacity = 4;
    def.bindingCapacity = 2;
    def.clipStorageBytes = 1u << 16;
    return def;
}

static void Update(mtwnContext* context, double seconds)
{
    CHECK(mtwnContext_Update(context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) ==
              mtwn_success,
          "updated");
}

static float ValueOf(mtwnContext* context, mtwnTargetId target)
{
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(context, target, &state) == mtwn_success, "read");
    return state.value.x;
}

static void TestGrow(void)
{
    Budget budget = {0};
    mtwnContextDef def = Small(&budget);
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a small context");
    mtwnTargetId a = {0, 0};
    mtwnTargetId b = {0, 0};
    mtwnTargetId c = {0, 0};
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &a) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &b) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &c) == mtwn_errorCapacity,
          "two targets fill it");
    // A clip playing on a, a transition on b.
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.nodeCapacity = 4;
    clipDef.keyframeCapacity = 4;
    clipDef.slotCapacity = 1;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 10.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 10.0;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a track");
    mtwnBinding binding = mtwnBind(0, a);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    playerDef.keep = true;
    mtwnPlayerId player;
    CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success, "a player");
    mtwnTransitionDef move = mtwnDefaultTransitionDef();
    move.target = b;
    move.value = (mtwnVec4){4.0f, 0, 0, 0};
    move.easing = mtwnLinearEasing();
    move.duration = 4.0;
    mtwnPlayerId transition;
    CHECK(mtwnStartTransition(context, &move, &transition) == mtwn_success, "a transition");
    Update(context, 0.0);
    // One record read before growing: the ring starts part way round.
    mtwnEvent first;
    uint32_t taken = 0;
    CHECK(mtwnContext_ReadEvents(context, &first, 1, &taken, NULL) == mtwn_success && taken == 1,
          "the first record read");
    Update(context, 1.0);
    CHECK(mtwnContext_GetEventCount(context) == 2, "two records queued");
    // A change made between updates waits in the changed list.
    CHECK(mtwnDestroyPlayer(context, transition) == mtwn_success, "the transition stopped");
    CHECK(mtwnStartTransition(context, &move, &transition) == mtwn_success, "and started again");
    uint64_t before = mtwnContext_GetMemoryBytes(context);
    mtwnContext* old = context;
    // Refusals change nothing.
    mtwnContextDef smaller = def;
    smaller.targetCapacity = 1;
    CHECK(mtwnGrowContext(&context, &smaller) == mtwn_errorInvalid && context == old,
          "no capacity below its own");
    mtwnContextDef other = def;
    other.allocator = (mtwnAllocator){0};
    CHECK(mtwnGrowContext(&context, &other) == mtwn_errorInvalid && context == old,
          "nor another allocator");
    Budget elsewhere = {0};
    other.allocator = (mtwnAllocator){Alloc, Free, &elsewhere};
    CHECK(mtwnGrowContext(&context, &other) == mtwn_errorInvalid && context == old,
          "nor the same functions with another context");
    other = def;
    other.clipStorageBytes = def.clipStorageBytes / 2;
    uint64_t misuse = mtwnGetContextMisuse(context);
    CHECK(mtwnGrowContext(&context, &other) == mtwn_errorInvalid && context == old &&
              mtwnGetContextMisuse(context) == misuse + 1,
          "nor less clip storage, the refusal counted");
    CHECK(mtwnGrowContext(&context, NULL) == mtwn_errorInvalid &&
              mtwnGrowContext(NULL, &def) == mtwn_errorInvalid,
          "nor null arguments");
    mtwnContextDef bigger = def;
    bigger.targetCapacity = 8;
    bigger.playerCapacity = 8;
    bigger.clipCapacity = 4;
    bigger.eventCapacity = 16;
    bigger.bindingCapacity = 8;
    budget.fail = true;
    CHECK(mtwnGrowContext(&context, &bigger) == mtwn_errorCapacity && context == old &&
              ValueOf(context, a) == 1.0f,
          "an allocator's failure: the context as it was");
    budget.fail = false;
    CHECK(mtwnGrowContext(&context, &bigger) == mtwn_success && budget.blocks == 2,
          "grown: one context block and the clip's");
    CHECK(mtwnContext_GetMemoryBytes(context) > before, "holding more");
    // Every id carries over, and what plays plays on.
    CHECK(ValueOf(context, a) == 1.0f && ValueOf(context, b) == 0.0f, "the values carried");
    CHECK(mtwnPlayer_IsValid(context, player) && mtwnPlayer_IsValid(context, transition),
          "the players' ids carried");
    mtwnTargetId changed[4];
    uint32_t total = 0;
    CHECK(mtwnContext_Update(context, 2000000000u, changed, 4, &total) == mtwn_success &&
              total == 2 && changed[0].index1 == b.index1 && changed[1].index1 == a.index1,
          "the change made before growing reported, then the clip's");
    CHECK(ValueOf(context, a) == 2.0f && ValueOf(context, b) == 0.0f, "and they play on");
    // The sharing is a cache made again: any capacities, here none, its
    // bytes (14,848 on a 64-bit target) no longer held.
    mtwnContextDef unshared = bigger;
    unshared.sharedNodeCapacity = 0;
    unshared.sharedRecordCapacity = 0;
    uint64_t shared = mtwnContext_GetMemoryBytes(context);
    CHECK(mtwnGrowContext(&context, &unshared) == mtwn_success &&
              mtwnContext_GetMemoryBytes(context) ==
                  shared - (128u * 80u + 64u * (sizeof(mtwnEvent) + sizeof(double))),
          "grown with no sharing, holding less");
    CHECK(ValueOf(context, a) == 2.0f, "the values carried again");
    mtwnEvent e[8];
    uint32_t n = 0;
    // Queued before: the track's start and the first transition's; after,
    // the second transition's start.
    CHECK(mtwnContext_ReadEvents(context, e, 8, &n, NULL) == mtwn_success && n == 3 &&
              e[0].kind == mtwn_eventStart && e[0].node == MTWN_ROOT_NODE + 1 &&
              e[0].player.index1 == player.index1 && e[1].kind == mtwn_eventStart &&
              e[1].node == 0 && e[2].kind == mtwn_eventStart && e[2].node == 0 &&
              e[2].player.index1 == transition.index1,
          "the queued records carried, in order");
    // The new room is there.
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &c) == mtwn_success,
          "a third target");
    mtwnBinding more = mtwnBind(0, c);
    playerDef.bindings = &more;
    playerDef.bindingCount = 1;
    mtwnPlayerId players[3];
    bool made = true;
    for (int i = 0; i < 3; ++i)
    {
        made = made && mtwnCreatePlayer(context, &playerDef, &players[i]) == mtwn_success;
    }
    CHECK(made, "three more clip players: the bindings' new room used");
    // Freed slots are reused first, as before growing.
    CHECK(mtwnDestroyTarget(context, c) == mtwn_success, "c freed");
    mtwnTargetId again;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &again) == mtwn_success &&
              again.index1 == c.index1 && again.generation != c.generation,
          "its slot reused, its generation advanced");
    mtwnDestroyContext(context);
    CHECK(budget.blocks == 0 && budget.held == 0, "everything returned");
}

static void TestGrowFreeSlots(void)
{
    // A slot freed before growing is still the first reused after.
    Budget budget = {0};
    mtwnContextDef def = Small(&budget);
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnTargetId a = {0, 0};
    mtwnTargetId b = {0, 0};
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &a) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &b) == mtwn_success &&
              mtwnDestroyTarget(context, a) == mtwn_success,
          "a freed");
    def.targetCapacity = 4;
    CHECK(mtwnGrowContext(&context, &def) == mtwn_success, "grown");
    mtwnTargetId c;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &c) == mtwn_success &&
              c.index1 == a.index1 && mtwnTarget_IsValid(context, b) &&
              !mtwnTarget_IsValid(context, a),
          "a's slot first, b alive, a's old id stale");
    mtwnDestroyContext(context);
}

// A def that keeps some capacities as they are and grows others is taken,
// each capacity at least the context's.
static void TestGrowSome(void)
{
    Budget budget = {0};
    mtwnContextDef def = Small(&budget);
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnContextDef more = def;
    more.playerCapacity = 4;
    CHECK(mtwnGrowContext(&context, &more) == mtwn_success,
          "players grown, every other capacity kept");
    mtwnContextDef fewer = more;
    fewer.targetCapacity = 1;
    CHECK(mtwnGrowContext(&context, &fewer) == mtwn_errorInvalid, "fewer targets refused");
    mtwnDestroyContext(context);
    CHECK(budget.blocks == 0 && budget.held == 0, "all given back");
}

// A context and a player on a clip of a track repeating every 0.25 s
// and a mark, with a capacity of records.
static mtwnContext* Repeating(uint32_t events, mtwnTargetId* target, mtwnPlayerId* player)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.eventCapacity = events;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, target) == mtwn_success,
          "a target");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 3.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 0.25;
    track.timing.iterations = 12.0;
    mtwnNode node;
    mtwnNode mark;
    mtwnPosition at = {0};
    at.offset = 1.1;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, at, 7, &mark) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a repeating track and a mark");
    mtwnBinding binding = mtwnBind(0, *target);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    CHECK(mtwnCreatePlayer(context, &playerDef, player) == mtwn_success, "a player");
    return context;
}

static void TestNoRecords(void)
{
    mtwnTargetId quietTarget;
    mtwnTargetId loudTarget;
    mtwnPlayerId quietPlayer;
    mtwnPlayerId loudPlayer;
    mtwnContext* quiet = Repeating(0, &quietTarget, &quietPlayer);
    mtwnContext* loud = Repeating(256, &loudTarget, &loudPlayer);
    mtwnEvent events[32];
    uint32_t count = 0;
    uint64_t dropped = 0;
    int differ = 0;
    for (int frame = 0; frame <= 60; ++frame)
    {
        Update(quiet, frame / 60.0);
        Update(loud, frame / 60.0);
        differ += ValueOf(quiet, quietTarget) != ValueOf(loud, loudTarget);
        if (frame == 30)
        {
            // A seek's record, which no sweep makes, is not made either;
            // the frames after it still keep the time they reached.
            CHECK(mtwnPlayer_Seek(quiet, quietPlayer, 0.5) == mtwn_success &&
                      mtwnPlayer_Seek(loud, loudPlayer, 0.5) == mtwn_success,
                  "both sought where they are");
        }
    }
    CHECK(differ == 0, "no records, the same values");
    CHECK(mtwnContext_ReadEvents(quiet, events, 32, &count, &dropped) == mtwn_success &&
              count == 0 && dropped == 0 && mtwnContext_GetDroppedEvents(quiet) == 0,
          "none made, none dropped");
    CHECK(mtwnContext_ReadEvents(loud, events, 32, &count, &dropped) == mtwn_success && count > 4,
          "where the other made its starts and repeats");
    // Grown to hold records, it records what is crossed from there on:
    // the mark at 1.1 and the repeat at 1.25, not the second before.
    mtwnContextDef def = mtwnDefaultContextDef();
    def.eventCapacity = 16;
    CHECK(mtwnGrowContext(&quiet, &def) == mtwn_success, "grown to hold records");
    Update(quiet, 1.3);
    Update(loud, 1.3);
    mtwnEvent heard[32];
    uint32_t heardCount = 0;
    CHECK(mtwnContext_ReadEvents(quiet, heard, 32, &heardCount, &dropped) == mtwn_success &&
              mtwnContext_ReadEvents(loud, events, 32, &count, NULL) == mtwn_success,
          "read");
    bool same = heardCount == count && heardCount == 2;
    for (uint32_t k = 0; same && k < count; ++k)
    {
        same = heard[k].kind == events[k].kind && heard[k].node == events[k].node &&
               heard[k].clipTime == events[k].clipTime && heard[k].count == events[k].count;
    }
    CHECK(same && dropped == 0, "the frame's repeat and mark, as the other recorded them");
    mtwnDestroyContext(quiet);
    mtwnDestroyContext(loud);
}

// Twin contexts each run two transitions eased by linear(), one grown
// mid-transition to more players and more points for each: its copies of
// the points move to the new block at the new stride, and every value
// after is the other's to the bit, though the host's points are gone.
static void TestGrowLinearPoints(void)
{
    Budget budget = {0};
    mtwnContextDef def = Small(&budget);
    mtwnContext* twins[2] = {NULL, NULL};
    mtwnTargetId targets[2][2];
    mtwnLinearPoint points[2][5] = {{{0.0, 0.0}, {0.2, 0.5}, {0.5, 0.6}, {0.8, 1.1}, {1.0, 1.0}},
                                    {{0.0, 0.0}, {0.1, -0.2}, {0.4, 0.3}, {0.9, 0.95}, {1.0, 1.0}}};
    mtwnEasing easings[2];
    CHECK(mtwnMakeLinearEasing(points[0], 5, &easings[0]) == mtwn_success &&
              mtwnMakeLinearEasing(points[1], 5, &easings[1]) == mtwn_success,
          "two linear() easings");
    for (int k = 0; k < 2; ++k)
    {
        CHECK(mtwnCreateContext(&def, &twins[k]) == mtwn_success, "a twin");
        for (int t = 0; t < 2; ++t)
        {
            CHECK(mtwnCreateTarget(twins[k], mtwn_valueScalar, (mtwnVec4){0}, &targets[k][t]) ==
                      mtwn_success,
                  "a target");
            mtwnTransitionDef transition = mtwnDefaultTransitionDef();
            transition.target = targets[k][t];
            transition.value = (mtwnVec4){100.0f, 0, 0, 0};
            transition.duration = 1.0;
            transition.easing = easings[t];
            mtwnPlayerId id;
            CHECK(mtwnStartTransition(twins[k], &transition, &id) == mtwn_success,
                  "a transition eased by linear()");
        }
        Update(twins[k], 0.0);
        Update(twins[k], 0.3);
    }
    mtwnContextDef wider = def;
    wider.playerCapacity = 8;
    wider.pointsPerTransition = def.pointsPerTransition + 32;
    mtwnContextDef narrower = def;
    narrower.pointsPerTransition = def.pointsPerTransition - 1;
    CHECK(mtwnGrowContext(&twins[1], &narrower) == mtwn_errorInvalid,
          "fewer points for each transition refused");
    CHECK(mtwnGrowContext(&twins[1], &wider) == mtwn_success, "grown mid-transition");
    memset(points, 0xFF, sizeof points);
    int same = 0;
    for (int step = 4; step <= 11; ++step)
    {
        Update(twins[0], step * 0.1);
        Update(twins[1], step * 0.1);
        for (int t = 0; t < 2; ++t)
        {
            float a = ValueOf(twins[0], targets[0][t]);
            float b = ValueOf(twins[1], targets[1][t]);
            same += memcmp(&a, &b, sizeof a) == 0 && isfinite(a);
        }
    }
    CHECK(same == 16, "the grown twin samples its moved copies identically");
    mtwnDestroyContext(twins[0]);
    mtwnDestroyContext(twins[1]);
    CHECK(budget.held == 0 && budget.blocks == 0, "everything freed");
}

int main(void)
{
    TestNoRecords();
    TestGrowLinearPoints();
    TestGrowSome();
    TestGrow();
    TestGrowFreeSlots();
    return s_failures == 0 ? 0 : 1;
}
