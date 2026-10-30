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
    // Calls made back in, those refused with mtwn_errorBusy, the creations
    // whose ids the refusal cleared, and the calls whose arrays and counts
    // it left.
    uint32_t calls;
    uint32_t refused;
    uint32_t cleared;
    uint32_t left;
} Probe;

static Probe s_probe;

// Calls every function taking the context non-const, each with an
// argument it refuses as invalid or stale (a null id or output, a time
// gone by), so none can change the context and none returns
// mtwn_errorBusy of itself: that status is the busy refusal alone. Counts
// those: 28, the functions returning a status. Busy, it ends with the
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
    r += mtwnClip_Seal(c, noClip) == mtwn_errorBusy;
    mtwnContext* grown = c;
    r += mtwnContext_Grow(&grown, NULL) == mtwn_errorBusy;
    // Reduced motion as it is: the one call that succeeds when not busy.
    r += mtwnContext_EnableReducedMotion(c, false) == mtwn_errorBusy;
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
    r += mtwnDestroyPlayer(c, noPlayer) == mtwn_errorBusy;
    r += mtwnCreateTarget(c, mtwn_valueScalar, (mtwnVec4){0}, NULL) == mtwn_errorBusy;
    r += mtwnDestroyTarget(c, noTarget) == mtwn_errorBusy;
    r += mtwnTarget_SetBase(c, noTarget, (mtwnVec4){0}) == mtwn_errorBusy;
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    r += mtwnStartTransition(c, &transition, NULL) == mtwn_errorBusy;
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
        cleared += (call) == mtwn_errorBusy && AllBytes(&(out), sizeof(out), 0);                   \
    } while (0)

static bool AllBytes(const void* bytes, size_t size, unsigned char value)
{
    const unsigned char* b = bytes;
    for (size_t i = 0; i < size; ++i)
    {
        if (b[i] != value)
        {
            return false;
        }
    }
    return true;
}

// The creations, made busy: each refused and the id of what it would make
// the null id, as on any failure. Counts those: 8.
static uint32_t ClearedWhenBusy(mtwnContext* c)
{
    uint32_t cleared = 0;
    const mtwnClipId noClip = {0, 0};
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    mtwnTargetId target;
    BUSY_CLEARS(target, mtwnCreateTarget(c, mtwn_valueScalar, (mtwnVec4){0}, &target));
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
    return cleared;
}

// The calls filling an array, made busy: each refused, its array and its
// counts left as they were, since the call in progress may be filling or
// have written them. Counts those: 2.
static uint32_t LeftWhenBusy(mtwnContext* c)
{
    uint32_t left = 0;
    mtwnTargetId changed[2];
    memset(changed, 0xAB, sizeof changed);
    uint32_t total = 7;
    left += mtwnContext_Update(c, 0, changed, 2, &total) == mtwn_errorBusy && total == 7 &&
            AllBytes(changed, sizeof changed, 0xAB);
    mtwnEvent events[2];
    memset(events, 0xAB, sizeof events);
    uint32_t count = 7;
    uint64_t dropped = 7;
    left += mtwnContext_ReadEvents(c, events, 2, &count, &dropped) == mtwn_errorBusy &&
            count == 7 && dropped == 7 && AllBytes(events, sizeof events, 0xAB);
    return left;
}

// A host easing, the identity, calling back in on its first call.
static double CallingIn(void* unused, double progress)
{
    (void)unused;
    if (s_probe.calls == 0)
    {
        s_probe.cleared = ClearedWhenBusy(s_probe.context);
        s_probe.left = LeftWhenBusy(s_probe.context);
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
              mtwnContext_EnableReducedMotion(context, false) == mtwn_success,
          "not busy, none of the calls returns a state error, the last left too");
    uint64_t before = mtwnContext_GetMisuse(context);
    CHECK(mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, s_second / 2, NULL, 0, NULL) == mtwn_success,
          "updated");
    CHECK(s_probe.calls == 1, "the easing called back in");
    CHECK(s_probe.refused == 28, "every call back in refused");
    CHECK(s_probe.cleared == 8, "a creation's id the null id, as on any failure");
    CHECK(s_probe.left == 2, "its arrays and counts left, as the call in progress may hold them");
    CHECK(mtwnContext_GetMisuse(context) == before + 29 + 8 + 2,
          "each counted, the destroy and the creations' and arrays' calls too");
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

// What the readers gave a host easing that called them in the middle of an
// update.
typedef struct Reading
{
    mtwnContext* context;
    mtwnTargetId target;
    mtwnClipId clip;
    mtwnNode track;
    mtwnPlayerId player;
    mtwnTransitionDef transition;
    uint32_t calls;
    uint32_t read;
    bool stateRefused;
    uint64_t misuse;
} Reading;

static Reading s_reading;

// Calls every function that reads the context, each taking it const, on
// live objects. Counts those that read: 9, all but mtwnTarget_GetState,
// whose velocity may run the host's code again and is refused.
static uint32_t ReadEverything(const mtwnContext* c)
{
    uint32_t r = 0;
    mtwnVec4 value;
    r += mtwnTarget_GetValue(c, s_reading.target, &value) == mtwn_success;
    mtwnTargetState state;
    memset(&state, 0xAB, sizeof state);
    s_reading.stateRefused = mtwnTarget_GetState(c, s_reading.target, &state) == mtwn_errorBusy &&
                             AllBytes(&state, sizeof state, 0xAB);
    r += mtwnContext_ReadTargets(c, &s_reading.target, 1, &value) == mtwn_success;
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    uint64_t bytes = 0;
    r += mtwnContext_GetClipBytes(c, &clipDef, &bytes) == mtwn_success && bytes > 0;
    double distance = 0.0;
    r += mtwnContext_MeasureTransition(c, &s_reading.transition, &distance) == mtwn_success &&
         distance > 0.0;
    mtwnClipTrack track;
    r += mtwnClip_GetTrack(c, s_reading.clip, s_reading.track, &track) == mtwn_success;
    mtwnClipNodeInfo info;
    r += mtwnClip_GetNode(c, s_reading.clip, MTWN_ROOT_NODE, &info) == mtwn_success;
    double seconds;
    r += mtwnClip_FindLabel(c, s_reading.clip, MTWN_ROOT_NODE, 1, &seconds) == mtwn_success;
    r += mtwnClip_GetDuration(c, s_reading.clip, &seconds) == mtwn_success;
    mtwnPlayerState playerState;
    r += mtwnPlayer_GetState(c, s_reading.player, &playerState) == mtwn_success;
    return r;
}

// A host easing, the identity, reading the context on its first call.
static double ReadingIn(void* unused, double progress)
{
    (void)unused;
    if (s_reading.calls == 0)
    {
        // Counted first, so a read that ran this easing again would not
        // read again.
        s_reading.calls = 1;
        uint64_t before = mtwnContext_GetMisuse(s_reading.context);
        s_reading.read = ReadEverything(s_reading.context);
        s_reading.misuse = mtwnContext_GetMisuse(s_reading.context) - before;
    }
    return progress;
}

// The readers take the context const, so a call back in is not refused:
// it reads the context as the call in progress has left it so far.
static void TestReadsWhileBusy(void)
{
    memset(&s_reading, 0, sizeof s_reading);
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &s_reading.context) == mtwn_success, "a context");
    mtwnContext* context = s_reading.context;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &s_reading.target) ==
              mtwn_success,
          "a target");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(context, &clipDef, &s_reading.clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2] = {0};
    keys[1].value = (mtwnVec4){1.0f, 0, 0, 0};
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
    mtwnClipTrackDef trackDef = mtwnDefaultClipTrackDef();
    trackDef.keyframes = keys;
    trackDef.keyframeCount = 2;
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    mtwnNode label;
    CHECK(mtwnClip_AddTrack(context, s_reading.clip, &trackDef, &s_reading.track) == mtwn_success &&
              mtwnClip_AddLabel(context, s_reading.clip, MTWN_ROOT_NODE, at, 1, &label) ==
                  mtwn_success &&
              mtwnClip_Seal(context, s_reading.clip) == mtwn_success,
          "a sealed clip of a track and a label");
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = s_reading.clip;
    CHECK(mtwnCreatePlayer(context, &playerDef, &s_reading.player) == mtwn_success, "its player");
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    transition.target = s_reading.target;
    transition.value = (mtwnVec4){1.0f, 0, 0, 0};
    transition.duration = 1.0;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, ReadingIn, NULL, &transition.easing) == mtwn_success,
          "a host easing");
    s_reading.transition = transition;
    mtwnPlayerId moving;
    CHECK(mtwnStartTransition(context, &transition, &moving) == mtwn_success, "started");
    uint64_t before = mtwnContext_GetMisuse(context);
    CHECK(mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, s_second / 2, NULL, 0, NULL) == mtwn_success,
          "updated");
    CHECK(s_reading.calls == 1, "the easing read the context");
    CHECK(s_reading.read == 9, "every other reader read it in the middle of the update");
    CHECK(s_reading.stateRefused, "a target's state refused, its output left");
    CHECK(s_reading.misuse == 1 && mtwnContext_GetMisuse(context) == before + 1,
          "that refusal alone counted as misuse");
    mtwnDestroyContext(context);
}

// A target read whose velocity samples a host easing: that easing,
// calling back in to change the context, is refused, as during an update.
typedef struct Changing
{
    mtwnContext* context;
    mtwnTargetId spare;
    bool armed;
    uint32_t calls;
    mtwnResult changed;
} Changing;

static Changing s_changing;

static double ChangingIn(void* unused, double progress)
{
    (void)unused;
    if (s_changing.armed)
    {
        s_changing.changed =
            mtwnTarget_SetBase(s_changing.context, s_changing.spare, (mtwnVec4){1.0f, 0, 0, 0});
        s_changing.calls += 1;
    }
    return progress;
}

static void TestStateReadMarksBusy(void)
{
    memset(&s_changing, 0, sizeof s_changing);
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &s_changing.context) == mtwn_success, "a context");
    mtwnContext* context = s_changing.context;
    mtwnTargetId target;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &s_changing.spare) ==
                  mtwn_success,
          "targets");
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    transition.target = target;
    transition.value = (mtwnVec4){1.0f, 0, 0, 0};
    transition.duration = 1.0;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, ChangingIn, NULL, &transition.easing) == mtwn_success,
          "a host easing");
    mtwnPlayerId moving;
    CHECK(mtwnStartTransition(context, &transition, &moving) == mtwn_success &&
              mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, s_second / 2, NULL, 0, NULL) == mtwn_success,
          "eased half way");
    uint64_t before = mtwnContext_GetMisuse(context);
    s_changing.armed = true;
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(context, target, &state) == mtwn_success && state.velocity.x > 0.0f,
          "its state read, a velocity on the way");
    s_changing.armed = false;
    CHECK(s_changing.calls > 0 && s_changing.changed == mtwn_errorBusy,
          "the easing the velocity sampled refused a change");
    CHECK(mtwnContext_GetMisuse(context) == before + s_changing.calls, "each refusal counted");
    CHECK(mtwnTarget_SetBase(context, s_changing.spare, (mtwnVec4){2.0f, 0, 0, 0}) == mtwn_success,
          "not busy after the read");
    mtwnDestroyContext(context);
}

// The readers of targets and players called from a clip's host easing:
// refused while the update's groups run on the task system, which may be
// writing what they read, and reading on the calling thread otherwise. The
// task system runs every group inline, within enqueueTask on the calling
// thread, which must find the context grouping as a worker would.
typedef struct Grouped
{
    mtwnContext* context;
    mtwnTargetId target;
    mtwnPlayerId player;
    mtwnTransitionDef transition;
    uint32_t calls;
    uint32_t refused;
    uint32_t read;
    bool split;
} Grouped;

static Grouped s_grouped;

static void* RunGroupsAtOnce(mtwnTaskFunction* task, int32_t itemCount, int32_t minRange,
                             void* taskContext, void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext);
    return NULL;
}

static void WaitForNone(void* userTask, void* userContext)
{
    (void)userTask;
    (void)userContext;
}

// The four readers that read while the context is busy, outputs of 0xAB: how many were refused as
// busy with their outputs left, and how many read.
static double ReadingInGroups(void* unused, double progress)
{
    (void)unused;
    const mtwnContext* c = s_grouped.context;
    if (c == NULL)
    {
        return progress;
    }
    s_grouped.calls += 1;
    mtwnVec4 value;
    mtwnVec4 values[1];
    mtwnPlayerState playerState;
    double distance;
    memset(&value, 0xAB, sizeof value);
    memset(values, 0xAB, sizeof values);
    memset(&playerState, 0xAB, sizeof playerState);
    memset(&distance, 0xAB, sizeof distance);
    mtwnResult r[4] = {
        mtwnTarget_GetValue(c, s_grouped.target, &value),
        mtwnContext_ReadTargets(c, &s_grouped.target, 1, values),
        mtwnPlayer_GetState(c, s_grouped.player, &playerState),
        mtwnContext_MeasureTransition(c, &s_grouped.transition, &distance),
    };
    uint32_t busy = 0;
    uint32_t read = 0;
    for (int i = 0; i < 4; ++i)
    {
        busy += r[i] == mtwn_errorBusy;
        read += r[i] == mtwn_success;
    }
    bool left = AllBytes(&value, sizeof value, 0xAB) && AllBytes(values, sizeof values, 0xAB) &&
                AllBytes(&playerState, sizeof playerState, 0xAB) &&
                AllBytes(&distance, sizeof distance, 0xAB);
    s_grouped.refused += busy == 4 && left ? 4 : 0;
    s_grouped.read += read == 4 ? 4 : 0;
    // All four refused or all four read, never a mix.
    s_grouped.split = s_grouped.split || (busy != 4 && read != 4);
    return progress;
}

static void TestReadsInGroups(void)
{
    memset(&s_grouped, 0, sizeof s_grouped);
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = RunGroupsAtOnce;
    def.finishTask = WaitForNone;
    def.playerCapacity = 64;
    def.targetCapacity = 64;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context with a task system");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2] = {0};
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, ReadingInGroups, NULL, &keys[0].easing) == mtwn_success,
          "a host easing");
    keys[1].easing = keys[0].easing;
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Seal(context, clip) == mtwn_success,
          "a track");
    for (int p = 0; p < 32; ++p)
    {
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding binding = mtwnMakeBinding(0, target);
        mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = &binding;
        playerDef.bindingCount = 1;
        CHECK(mtwnCreatePlayer(context, &playerDef, &s_grouped.player) == mtwn_success, "a player");
        s_grouped.target = target;
    }
    s_grouped.transition = mtwnDefaultTransitionDef();
    s_grouped.transition.target = s_grouped.target;
    s_grouped.transition.value = (mtwnVec4){1.0f, 0, 0, 0};
    uint64_t before = mtwnContext_GetMisuse(context);
    s_grouped.context = context;
    CHECK(mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, s_second / 4, NULL, 0, NULL) == mtwn_success,
          "updated");
    s_grouped.context = NULL;
    CHECK(s_grouped.calls > 0 && !s_grouped.split,
          "every reader of a call refused, or every one read");
    CHECK(s_grouped.refused > 0, "refused in the groups, outputs left");
    CHECK(s_grouped.read > 0, "read on the calling thread");
    CHECK(s_grouped.refused + s_grouped.read == 4 * s_grouped.calls, "nothing else");
    CHECK(mtwnContext_GetMisuse(context) == before + s_grouped.refused, "each refusal counted");
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
        calling->inFree = mtwnContext_EnableReducedMotion(calling->context, true);
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
    CHECK(mtwnContext_Grow(&context, &grown) == mtwn_success, "grown");
    CHECK(calling.allocs == 1 && calling.inAlloc == mtwn_errorBusy,
          "an update from the allocation refused");
    CHECK(calling.frees == 1 && calling.inFree == mtwn_errorBusy,
          "a change from freeing the old block refused");
    CHECK(mtwnContext_GetMisuse(context) == 1, "the allocation's refusal counted, moved along");
    CHECK(!mtwnContext_IsReducedMotionEnabled(context), "nothing changed");
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

static void* CallingEnqueue(mtwnTaskFunction* task, int32_t itemCount, int32_t minRange,
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

// A free that reads back the clip whose block it was given, as the clip
// readers, not refused on a call back in, let it: by then the clip's id is
// stale, for mtwnDestroyClip and for the clips mtwnDestroyContext frees.
typedef struct Freeing
{
    mtwnContext* context;
    mtwnClipId clip;
    bool armed;
    uint32_t reads;
    mtwnResult read;
} Freeing;

static Freeing s_freeing;

static void* PlainAlloc(size_t size, size_t alignment, void* user)
{
    (void)alignment;
    (void)user;
    return malloc(size);
}

static void ReadingFree(void* memory, size_t size, size_t alignment, void* user)
{
    (void)size;
    (void)alignment;
    (void)user;
    free(memory);
    // Not the context's own block, which is gone with this call.
    if (s_freeing.armed && memory != (void*)s_freeing.context)
    {
        double seconds = 7.0;
        s_freeing.read = mtwnClip_GetDuration(s_freeing.context, s_freeing.clip, &seconds);
        s_freeing.reads += 1;
    }
}

static void TestFreeReadsItsClip(void)
{
    memset(&s_freeing, 0, sizeof s_freeing);
    mtwnContextDef def = mtwnDefaultContextDef();
    def.allocator = (mtwnAllocator){PlainAlloc, ReadingFree, NULL};
    CHECK(mtwnCreateContext(&def, &s_freeing.context) == mtwn_success, "a context");
    mtwnContext* context = s_freeing.context;
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(context, &clipDef, &s_freeing.clip) == mtwn_success &&
              mtwnClip_Seal(context, s_freeing.clip) == mtwn_success,
          "a sealed clip");
    s_freeing.armed = true;
    CHECK(mtwnDestroyClip(context, s_freeing.clip) == mtwn_success, "destroyed");
    s_freeing.armed = false;
    CHECK(s_freeing.reads == 1 && s_freeing.read == mtwn_errorStale,
          "its block's free reads it back stale");
    CHECK(mtwnCreateClip(context, &clipDef, &s_freeing.clip) == mtwn_success &&
              mtwnClip_Seal(context, s_freeing.clip) == mtwn_success,
          "another");
    s_freeing.reads = 0;
    s_freeing.armed = true;
    mtwnDestroyContext(context);
    s_freeing.armed = false;
    CHECK(s_freeing.reads == 1 && s_freeing.read == mtwn_errorStale,
          "the context's destruction frees it stale too");
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
    keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Seal(context, clip) == mtwn_success,
          "a track");
    for (int p = 0; p < 32; ++p)
    {
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding binding = mtwnMakeBinding(0, target);
        mtwnPlayerDef def2 = mtwnDefaultPlayerDef();
        def2.clip = clip;
        def2.bindings = &binding;
        def2.bindingCount = 1;
        mtwnPlayerId player;
        CHECK(mtwnCreatePlayer(context, &def2, &player) == mtwn_success, "a player");
    }
    uint64_t before = mtwnContext_GetMisuse(context);
    CHECK(mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, s_second / 4, NULL, 0, NULL) == mtwn_success,
          "updated");
    CHECK(s_enqueued > 0 && s_finished > 0, "the clips' players ran on the tasks");
    CHECK(s_taskRefused == s_enqueued + s_finished, "every call back in refused");
    CHECK(mtwnContext_GetMisuse(context) == before + s_taskRefused, "each counted");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestHostEasing();
    TestReadsWhileBusy();
    TestStateReadMarksBusy();
    TestReadsInGroups();
    TestAllocator();
    TestFreeReadsItsClip();
    TestTasks();
    return s_failures == 0 ? 0 : 1;
}
