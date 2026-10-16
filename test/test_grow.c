// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Growing a context (record mtwn-0041): only when asked, to capacities at
// least its own (the sharing's any), with its own allocator; every id, a
// playing clip, a running transition, the queued records and the free
// slots carried over; on a refusal or an allocator's failure nothing
// changes.

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
    CHECK(mtwnUpdate(context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) == mtwn_success,
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
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &c) == mtwn_errorLimit,
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
    CHECK(mtwnReadEvents(context, &first, 1, &taken, NULL) == mtwn_success && taken == 1,
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
    uint64_t misuse = mtwnContext_GetMisuseCount(context);
    CHECK(mtwnGrowContext(&context, &other) == mtwn_errorInvalid && context == old &&
              mtwnContext_GetMisuseCount(context) == misuse + 1,
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
    CHECK(mtwnUpdate(context, 2000000000u, changed, 4, &total) == mtwn_success && total == 2 &&
              changed[0].index1 == b.index1 && changed[1].index1 == a.index1,
          "the change made before growing reported, then the clip's");
    CHECK(ValueOf(context, a) == 2.0f && ValueOf(context, b) == 0.0f, "and they play on");
    // The sharing is a cache made again: any capacities, here none, its
    // 14,848 bytes no longer held.
    mtwnContextDef unshared = bigger;
    unshared.sharedNodeCapacity = 0;
    unshared.sharedRecordCapacity = 0;
    uint64_t shared = mtwnContext_GetMemoryBytes(context);
    CHECK(mtwnGrowContext(&context, &unshared) == mtwn_success &&
              mtwnContext_GetMemoryBytes(context) == shared - 14848u,
          "grown with no sharing, holding less");
    CHECK(ValueOf(context, a) == 2.0f, "the values carried again");
    mtwnEvent e[8];
    uint32_t n = 0;
    // Queued before: the track's start and the first transition's; after,
    // the second transition's start.
    CHECK(mtwnReadEvents(context, e, 8, &n, NULL) == mtwn_success && n == 3 &&
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
// each capacity at least the context's (found by mutants).
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

int main(void)
{
    TestGrowSome();
    TestGrow();
    TestGrowFreeSlots();
    return s_failures == 0 ? 0 : 1;
}
