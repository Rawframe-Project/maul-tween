// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Calls back in from the host's code a context runs: a
// host easing, the allocator and the task system calling a function of the
// context they serve are refused, each counted as misuse, the context
// intact.
#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const uint64_t s_second = 1000000000u;

// What a call back in may reach: live ids of every kind, so a call that
// went through would change the context.
typedef struct Probe
{
    mtwnContext* context;
    mtwnTargetId target;
    mtwnTargetId spare;
    mtwnClipId clip;
    mtwnPlayerId player;
    // Calls made back in, those refused with mtwn_errorBusy, and those
    // whose outputs the refusal cleared.
    uint32_t calls;
    uint32_t refused;
    uint32_t cleared;
} Probe;

static Probe s_probe;

// Calls every function taking the context non-const, each with an
// argument it refuses as invalid or stale (a null id or output, a time
// gone by), so none can change the context and none returns
// mtwn_errorBusy of itself: that status is the busy refusal alone. Counts
// those: 37, the functions returning a status. Busy, it ends with the
// destroy, which returns nothing and must leave the context alive.
static uint32_t CallEverything(mtwnContext* c, bool busy)
{
    uint32_t r = 0;
    const mtwnClipId noClip = {0, 0};
    const mtwnPlayerId noPlayer = {0, 0};
    const mtwnTargetId noTarget = {0, 0};
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    r += mtwnCreateClip(c, &clipDef, NULL) == mtwn_errorBusy;
    r += mtwnDestroyClip(c, noClip) == mtwn_errorBusy;
    mtwnClipTrackDef trackDef = mtwnDefaultClipTrackDef();
    r += mtwnClip_AddTrack(c, noClip, &trackDef, NULL) == mtwn_errorBusy;
    mtwnClipTimelineDef timelineDef = mtwnDefaultClipTimelineDef();
    r += mtwnClip_AddTimeline(c, noClip, &timelineDef, NULL) == mtwn_errorBusy;
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    r += mtwnClip_AddLabel(c, noClip, MTWN_ROOT_NODE, at, 1, NULL) == mtwn_errorBusy;
    r += mtwnClip_AddMark(c, noClip, MTWN_ROOT_NODE, at, 2, NULL) == mtwn_errorBusy;
    r += mtwnClip_Finish(c, noClip) == mtwn_errorBusy;
    r += mtwnClip_GetTrack(c, noClip, 1, NULL) == mtwn_errorBusy;
    r += mtwnClip_GetNode(c, noClip, MTWN_ROOT_NODE, NULL) == mtwn_errorBusy;
    r += mtwnClip_FindLabel(c, noClip, MTWN_ROOT_NODE, 1, NULL) == mtwn_errorBusy;
    r += mtwnClip_GetDuration(c, noClip, NULL) == mtwn_errorBusy;
    mtwnContext* grown = c;
    r += mtwnGrowContext(&grown, NULL) == mtwn_errorBusy;
    // Reduced motion as it is: the one call that succeeds when not busy.
    r += mtwnContext_SetReducedMotion(c, false) == mtwn_errorBusy;
    r += mtwnContext_Update(c, 0, NULL, 1, NULL) == mtwn_errorBusy;
    r += mtwnContext_ReadEvents(c, NULL, 1, NULL, NULL) == mtwn_errorBusy;
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    r += mtwnCreatePlayer(c, &playerDef, NULL) == mtwn_errorBusy;
    r += mtwnPlayer_Play(c, noPlayer) == mtwn_errorBusy;
    r += mtwnPlayer_Pause(c, noPlayer) == mtwn_errorBusy;
    r += mtwnPlayer_Reverse(c, noPlayer) == mtwn_errorBusy;
    r += mtwnPlayer_Finish(c, noPlayer) == mtwn_errorBusy;
    r += mtwnPlayer_Cancel(c, noPlayer) == mtwn_errorBusy;
    r += mtwnPlayer_Seek(c, noPlayer, 0.25) == mtwn_errorBusy;
    r += mtwnPlayer_Sweep(c, noPlayer, 0.25) == mtwn_errorBusy;
    r += mtwnPlayer_SeekProgress(c, noPlayer, 0.5) == mtwn_errorBusy;
    r += mtwnPlayer_SeekLabel(c, noPlayer, 1) == mtwn_errorBusy;
    r += mtwnPlayer_SetRate(c, noPlayer, 2.0) == mtwn_errorBusy;
    r += mtwnPlayer_SetWeight(c, noPlayer, 0.5f) == mtwn_errorBusy;
    r += mtwnPlayer_GetState(c, noPlayer, NULL) == mtwn_errorBusy;
    r += mtwnDestroyPlayer(c, noPlayer) == mtwn_errorBusy;
    r += mtwnCreateTarget(c, mtwn_valueScalar, (mtwnVec4){0}, NULL) == mtwn_errorBusy;
    r += mtwnDestroyTarget(c, noTarget) == mtwn_errorBusy;
    r += mtwnTarget_SetBase(c, noTarget, (mtwnVec4){0}) == mtwn_errorBusy;
    r += mtwnTarget_GetValue(c, noTarget, NULL) == mtwn_errorBusy;
    r += mtwnTarget_GetState(c, noTarget, NULL) == mtwn_errorBusy;
    r += mtwnContext_ReadTargets(c, NULL, 1, NULL) == mtwn_errorBusy;
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    r += mtwnStartTransition(c, &transition, NULL) == mtwn_errorBusy;
    r += mtwnMeasureTransition(c, &transition, NULL) == mtwn_errorBusy;
    if (busy)
    {
        mtwnDestroyContext(c);
    }
    return r;
}

// Bytes of 0xAB, then whether a busy call cleared them as its header says.
#define BUSY_CLEARS(out, call)                                                                     \
    do                                                                                             \
    {                                                                                              \
        memset(&(out), 0xAB, sizeof(out));                                                         \
        cleared += (call) == mtwn_errorBusy && Zeroed(&(out), sizeof(out));                        \
    } while (0)

static bool Zeroed(const void* bytes, size_t size)
{
    const unsigned char* b = bytes;
    for (size_t i = 0; i < size; ++i)
    {
        if (b[i] != 0)
        {
            return false;
        }
    }
    return true;
}

// The calls whose outputs a failure clears, made busy: each refused and
// its output cleared, as a missing context's is. Counts those: 11.
static uint32_t ClearedWhenBusy(mtwnContext* c)
{
    uint32_t cleared = 0;
    const mtwnTargetId noTarget = {0, 0};
    const mtwnPlayerId noPlayer = {0, 0};
    const mtwnClipId noClip = {0, 0};
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    mtwnTargetId target;
    BUSY_CLEARS(target, mtwnCreateTarget(c, mtwn_valueScalar, (mtwnVec4){0}, &target));
    mtwnVec4 value;
    BUSY_CLEARS(value, mtwnTarget_GetValue(c, noTarget, &value));
    mtwnTargetState targetState;
    BUSY_CLEARS(targetState, mtwnTarget_GetState(c, noTarget, &targetState));
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    mtwnPlayerId player;
    BUSY_CLEARS(player, mtwnStartTransition(c, &transition, &player));
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    BUSY_CLEARS(clip, mtwnCreateClip(c, &clipDef, &clip));
    mtwnClipTrackDef trackDef = mtwnDefaultClipTrackDef();
    mtwnNode node;
    BUSY_CLEARS(node, mtwnClip_AddTrack(c, noClip, &trackDef, &node));
    mtwnClipTimelineDef timelineDef = mtwnDefaultClipTimelineDef();
    BUSY_CLEARS(node, mtwnClip_AddTimeline(c, noClip, &timelineDef, &node));
    BUSY_CLEARS(node, mtwnClip_AddLabel(c, noClip, MTWN_ROOT_NODE, at, 1, &node));
    BUSY_CLEARS(node, mtwnClip_AddMark(c, noClip, MTWN_ROOT_NODE, at, 2, &node));
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    BUSY_CLEARS(player, mtwnCreatePlayer(c, &playerDef, &player));
    mtwnPlayerState playerState;
    BUSY_CLEARS(playerState, mtwnPlayer_GetState(c, noPlayer, &playerState));
    return cleared;
}

// A host easing, the identity, calling back in on its first call.
static double CallingIn(void* unused, double progress)
{
    (void)unused;
    if (s_probe.calls == 0)
    {
        s_probe.cleared = ClearedWhenBusy(s_probe.context);
        s_probe.refused = CallEverything(s_probe.context, true);
        s_probe.calls = 1;
    }
    return progress;
}

static void TestHostEasing(void)
{
    memset(&s_probe, 0, sizeof s_probe);
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &s_probe.context) == mtwn_success, "a context");
    mtwnContext* context = s_probe.context;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &s_probe.target) ==
                  mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &s_probe.spare) ==
                  mtwn_success,
          "targets");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(context, &clipDef, &s_probe.clip) == mtwn_success, "a clip");
    mtwnTransitionDef def2 = mtwnDefaultTransitionDef();
    def2.target = s_probe.target;
    def2.value = (mtwnVec4){1.0f, 0, 0, 0};
    def2.duration = 1.0;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, CallingIn, NULL, &def2.easing) == mtwn_success,
          "a host easing");
    CHECK(mtwnStartTransition(context, &def2, &s_probe.player) == mtwn_success, "started");
    CHECK(CallEverything(context, false) == 0 &&
              mtwnContext_SetReducedMotion(context, false) == mtwn_success,
          "not busy, none of the calls returns a state error, the last left too");
    uint64_t before = mtwnGetContextMisuse(context);
    CHECK(mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, s_second / 2, NULL, 0, NULL) == mtwn_success,
          "updated");
    CHECK(s_probe.calls == 1, "the easing called back in");
    CHECK(s_probe.refused == 37, "every call back in refused");
    CHECK(s_probe.cleared == 11, "its outputs cleared, as a failure's are");
    CHECK(mtwnGetContextMisuse(context) == before + 38 + 11,
          "each counted, the destroy and the outputs' calls too");
    CHECK(s_probe.context == context, "not grown");
    mtwnVec4 value;
    CHECK(mtwnTarget_GetValue(context, s_probe.target, &value) == mtwn_success && value.x == 0.5f,
          "the transition ran on, unpaused");
    CHECK(mtwnTarget_IsValid(context, s_probe.spare) &&
              mtwnPlayer_IsValid(context, s_probe.player) &&
              mtwnClip_IsValid(context, s_probe.clip),
          "nothing destroyed");
    CHECK(mtwnTarget_SetBase(context, s_probe.spare, (mtwnVec4){2.0f, 0, 0, 0}) == mtwn_success,
          "callable again after");
    mtwnDestroyContext(context);
}

// An allocator calling back in, armed once the context is made.
typedef struct Calling
{
    mtwnContext* context;
    mtwnResult inAlloc;
    mtwnResult inFree;
    uint32_t allocs;
    uint32_t frees;
} Calling;

static void* CallingAlloc(size_t size, size_t alignment, void* user)
{
    Calling* calling = user;
    if (calling->context != NULL)
    {
        calling->inAlloc = mtwnContext_Update(calling->context, 0, NULL, 0, NULL);
        calling->allocs += 1;
    }
    (void)alignment;
    return malloc(size);
}

static void CallingFree(void* memory, size_t size, size_t alignment, void* user)
{
    (void)size;
    (void)alignment;
    Calling* calling = user;
    if (calling->context != NULL)
    {
        calling->inFree = mtwnContext_SetReducedMotion(calling->context, true);
        calling->frees += 1;
    }
    free(memory);
}

static void TestAllocator(void)
{
    Calling calling = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    def.allocator = (mtwnAllocator){CallingAlloc, CallingFree, &calling};
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    calling.context = context;
    mtwnContextDef grown = def;
    grown.targetCapacity = def.targetCapacity * 2;
    CHECK(mtwnGrowContext(&context, &grown) == mtwn_success, "grown");
    CHECK(calling.allocs == 1 && calling.inAlloc == mtwn_errorBusy,
          "an update from the allocation refused");
    CHECK(calling.frees == 1 && calling.inFree == mtwn_errorBusy,
          "a change from freeing the old block refused");
    CHECK(mtwnGetContextMisuse(context) == 1, "the allocation's refusal counted, moved along");
    CHECK(!mtwnContext_IsReducedMotion(context), "nothing changed");
    calling.context = context;
    calling.inFree = mtwn_success;
    mtwnDestroyContext(context);
    CHECK(calling.frees == 2 && calling.inFree == mtwn_errorBusy,
          "a change from freeing the destroyed context refused");
}

// A task system calling back in, then running each range at once.
static mtwnContext* s_tasksContext;
static uint32_t s_enqueued;
static uint32_t s_finished;
static uint32_t s_taskRefused;

static void* CallingEnqueue(mtwnTaskFn* task, int32_t itemCount, int32_t minRange,
                            void* taskContext, void* userContext)
{
    (void)minRange;
    (void)userContext;
    s_enqueued += 1;
    mtwnTargetId none = {0, 0};
    s_taskRefused += mtwnDestroyTarget(s_tasksContext, none) == mtwn_errorBusy;
    task(0, itemCount, taskContext);
    return &s_enqueued;
}

static void CallingFinish(void* userTask, void* userContext)
{
    (void)userTask;
    (void)userContext;
    s_finished += 1;
    s_taskRefused += mtwnContext_Update(s_tasksContext, 0, NULL, 0, NULL) == mtwn_errorBusy;
}

static void TestTasks(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = CallingEnqueue;
    def.finishTask = CallingFinish;
    def.playerCapacity = 64;
    def.targetCapacity = 64;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    s_tasksContext = context;
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
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a track");
    for (int p = 0; p < 32; ++p)
    {
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding binding = mtwnBind(0, target);
        mtwnPlayerDef def2 = mtwnDefaultPlayerDef();
        def2.clip = clip;
        def2.bindings = &binding;
        def2.bindingCount = 1;
        mtwnPlayerId player;
        CHECK(mtwnCreatePlayer(context, &def2, &player) == mtwn_success, "a player");
    }
    uint64_t before = mtwnGetContextMisuse(context);
    CHECK(mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, s_second / 4, NULL, 0, NULL) == mtwn_success,
          "updated");
    CHECK(s_enqueued > 0 && s_finished > 0, "the clips' players ran on the tasks");
    CHECK(s_taskRefused == s_enqueued + s_finished, "every call back in refused");
    CHECK(mtwnGetContextMisuse(context) == before + s_taskRefused, "each counted");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestHostEasing();
    TestAllocator();
    TestTasks();
    return s_failures == 0 ? 0 : 1;
}
