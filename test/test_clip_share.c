// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Tracks in a row share one timing result only when they would compute
// the same one: each track below differs from the one before it in one
// way (its start, its duration, its easing's exponent, its parent), one
// sharing track follows an unbound one, and every target is checked
// against the values layer's own timing and sample of its track. Host
// easings are called as often in a row as apart, and players paused at
// rates of 0 and -0 each get what they get alone.

#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/clip.h"
#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"

#include <math.h>
#include <string.h>

enum
{
    TRACKS = 8,
    UNBOUND = 6
};

static mtwnPosition At(double offset)
{
    mtwnPosition position;
    memset(&position, 0, sizeof position);
    position.offset = offset;
    return position;
}

static mtwnNode AddTrack(mtwnContext* context, mtwnClipId clip, mtwnNode parent, double start,
                         double duration, double exponent, uint32_t slot)
{
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 10.0f + (float)slot;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = parent;
    track.slot = slot;
    track.position = At(start);
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = duration;
    CHECK(mtwnMakePowerEasing(mtwn_easeOut, exponent, &track.timing.easing) == mtwn_success,
          "an easing");
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
    return node;
}

// The time a node's parent gives it at a player's time: the root's is
// the player's, a timeline's its transformed progress times its duration.
static double ParentTime(mtwnContext* context, mtwnClipId clip, mtwnNode parent, double t)
{
    if (parent == MTWN_ROOT_NODE)
    {
        return t;
    }
    mtwnClipNodeInfo info;
    CHECK(mtwnClip_GetNode(context, clip, parent, &info) == mtwn_success, "a timeline");
    mtwnTimingResult result;
    mtwnComputeTiming(&info.timing, t - info.start, false, &result);
    return result.transformedProgress * info.timing.duration;
}

// What a track's target holds at a player's time, by the values layer:
// its sample at its progress, or 0 (the base) with no effect.
static float Expected(mtwnContext* context, mtwnClipId clip, mtwnNode node, double t)
{
    mtwnClipTrack track;
    CHECK(mtwnClip_GetTrack(context, clip, node, &track) == mtwn_success, "the track");
    double local = ParentTime(context, clip, track.parent, t) - track.start;
    mtwnTimingResult result;
    mtwnComputeTiming(&track.timing, local, false, &result);
    if (isnan(result.transformedProgress))
    {
        return 0.0f;
    }
    return SampleTrack(&track.track, result.transformedProgress).x;
}

static void TestDiffering(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.timing.delay = 0.3;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &timeline) == mtwn_success, "a timeline");
    mtwnNode nodes[TRACKS];
    // Each differs from the one before in one way; 7 matches the unbound 6.
    nodes[0] = AddTrack(context, clip, MTWN_ROOT_NODE, 0.0, 1.0, 2.5, 0);
    nodes[1] = AddTrack(context, clip, MTWN_ROOT_NODE, 0.25, 1.0, 2.5, 1);
    // Exponents not whole, which an easing keeps in its parameters alone.
    nodes[2] = AddTrack(context, clip, MTWN_ROOT_NODE, 0.25, 2.0, 2.5, 2);
    nodes[3] = AddTrack(context, clip, MTWN_ROOT_NODE, 0.25, 2.0, 3.5, 3);
    nodes[4] = AddTrack(context, clip, timeline, 0.25, 2.0, 3.5, 4);
    nodes[5] = AddTrack(context, clip, MTWN_ROOT_NODE, 0.0, 1.5, 2.0, 5);
    nodes[6] = AddTrack(context, clip, MTWN_ROOT_NODE, 0.0, 0.5, 4.0, UNBOUND);
    nodes[7] = AddTrack(context, clip, MTWN_ROOT_NODE, 0.0, 0.5, 4.0, 7);
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnTargetId targets[TRACKS];
    mtwnBinding bindings[TRACKS - 1];
    uint32_t bound = 0;
    for (uint32_t i = 0; i < TRACKS; ++i)
    {
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &targets[i]) ==
                  mtwn_success,
              "a target");
        if (i != UNBOUND)
        {
            bindings[bound++] = mtwnBind(i, targets[i]);
        }
    }
    mtwnPlayerDef player = mtwnDefaultPlayerDef();
    player.clip = clip;
    player.bindings = bindings;
    player.bindingCount = bound;
    player.keep = true;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(context, &player, &id) == mtwn_success, "a player");
    static const double s_times[] = {0.0, 0.1, 0.3, 0.45, 0.7, 1.1, 1.6, 2.4};
    int wrong = 0;
    for (size_t k = 0; k < sizeof s_times / sizeof s_times[0]; ++k)
    {
        CHECK(mtwnUpdate(context, (uint64_t)llround(s_times[k] * 1e9), NULL, 0, NULL) ==
                  mtwn_success,
              "an update");
        for (uint32_t i = 0; i < TRACKS; ++i)
        {
            if (i == UNBOUND)
            {
                continue;
            }
            mtwnVec4 value = {0};
            CHECK(mtwnTarget_GetValue(context, targets[i], &value) == mtwn_success, "a value");
            float expected = Expected(context, clip, nodes[i], s_times[k]);
            if (fabsf(value.x - expected) > 1e-5f)
            {
                printf("  track %u at %g: %.9g, expected %.9g\n", i, s_times[k], (double)value.x,
                       (double)expected);
                ++wrong;
            }
        }
    }
    CHECK(wrong == 0, "every track as the values layer samples it, whatever it shares");
    mtwnDestroyContext(context);
}

// A host easing that tells 0 from -0, as a host's function may.
static double SignAware(void* context, double progress)
{
    (void)context;
    return signbit(progress) ? 0.5 : progress;
}

// The values a paused player's target holds, its clip's track eased by
// SignAware: each rate's player made in turn in one context, every
// player's target read after two updates.
static void HeldValues(const double* rates, uint32_t count, float* valuesOut)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
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
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, SignAware, NULL, &track.timing.easing) == mtwn_success,
          "a host easing");
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a clip of one track");
    mtwnTargetId targets[2];
    for (uint32_t i = 0; i < count; ++i)
    {
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &targets[i]) ==
                  mtwn_success,
              "a target");
        mtwnBinding binding = mtwnBind(0, targets[i]);
        mtwnPlayerDef player = mtwnDefaultPlayerDef();
        player.clip = clip;
        player.bindings = &binding;
        player.bindingCount = 1;
        player.rate = rates[i];
        mtwnPlayerId id = {0, 0};
        CHECK(mtwnCreatePlayer(context, &player, &id) == mtwn_success, "a player");
    }
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, 400000000u, NULL, 0, NULL) == mtwn_success,
          "the updates");
    for (uint32_t i = 0; i < count; ++i)
    {
        mtwnVec4 value = {0};
        CHECK(mtwnTarget_GetValue(context, targets[i], &value) == mtwn_success, "a value");
        valuesOut[i] = value.x;
    }
    mtwnDestroyContext(context);
}

// Rates of 0 and -0 are equal, but a player's time is its rate times
// the time since it started, 0 or -0. A player shares another's results
// only when its time and rate match the other's bits, so each gets what
// it gets alone; today the time reaches an easing as 0 either way (an
// iteration's progress is a difference), and this holds if that changes.
static void TestSignedZero(void)
{
    static const double s_rates[2] = {0.0, -0.0};
    float together[2];
    float alone[2];
    HeldValues(s_rates, 2, together);
    HeldValues(&s_rates[0], 1, &alone[0]);
    HeldValues(&s_rates[1], 1, &alone[1]);
    if (together[0] != alone[0] || together[1] != alone[1])
    {
        printf("  values %g %g, alone %g %g\n", (double)together[0], (double)together[1],
               (double)alone[0], (double)alone[1]);
    }
    CHECK(together[0] == alone[0] && together[1] == alone[1],
          "each paused player's value as it is alone");
}

static int s_calls;

static double Counted(void* context, double progress)
{
    (void)context;
    ++s_calls;
    return progress * progress;
}

// A player of three tracks with one host easing: the middle one of
// another easing when apart. Returns the host function's calls in an
// update once it runs.
static int HostCalls(bool apart)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnEasing host;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, Counted, NULL, &host) == mtwn_success, "a host easing");
    mtwnBinding bindings[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 1.0;
        keys[1].value.x = 1.0f;
        keys[0].easing = keys[1].easing = mtwnLinearEasing();
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.slot = i;
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 1.0;
        track.timing.easing = apart && i == 1 ? mtwnLinearEasing() : host;
        mtwnNode node = 0;
        CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        bindings[i] = mtwnBind(i, target);
    }
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnPlayerDef player = mtwnDefaultPlayerDef();
    player.clip = clip;
    player.bindings = bindings;
    player.bindingCount = 3;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(context, &player, &id) == mtwn_success, "a player");
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success, "the first update");
    s_calls = 0;
    CHECK(mtwnUpdate(context, 400000000u, NULL, 0, NULL) == mtwn_success, "an update");
    int calls = s_calls;
    mtwnDestroyContext(context);
    return calls;
}

static void TestHostEasing(void)
{
    // Apart, two host-eased tracks; in a row, three. Each is called as
    // often per track either way.
    int apart = HostCalls(true);
    int together = HostCalls(false);
    CHECK(apart > 0 && together * 2 == apart * 3,
          "a host easing called for every track, sharing none of them");
}

// Players in step share their samples: one host-eased
// track played by four and by eight players in step, the eight call the
// host function no more than the four, 4 times (a check that took every
// track as unknown sampled each of them: 6 calls and 14).
static int InStepCalls(int players)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnEasing host;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, Counted, NULL, &host) == mtwn_success, "a host easing");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    track.timing.easing = host;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a host-eased track");
    for (int p = 0; p < players; ++p)
    {
        mtwnTargetId target;
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
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success, "the first update");
    s_calls = 0;
    CHECK(mtwnUpdate(context, 400000000u, NULL, 0, NULL) == mtwn_success, "an update");
    int calls = s_calls;
    mtwnDestroyContext(context);
    return calls;
}

static void TestInStep(void)
{
    int four = InStepCalls(4);
    CHECK(four > 0 && InStepCalls(8) == four, "eight players in step sample as four do");
}

// Four players of a clip held at rate 0 start in step, the later ones
// taking the records the earlier made; set going, each records its
// start once.
static void TestHeldInStep(void)
{
    mtwnContext* context = NULL;
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    (void)AddTrack(context, clip, MTWN_ROOT_NODE, 0.0, 1.0, 2.0, 0);
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnPlayerId players[4];
    for (int i = 0; i < 4; ++i)
    {
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding binding = mtwnBind(0, target);
        mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = &binding;
        playerDef.bindingCount = 1;
        playerDef.keep = true;
        playerDef.rate = 0.0;
        CHECK(mtwnCreatePlayer(context, &playerDef, &players[i]) == mtwn_success, "a player");
    }
    uint32_t starts[4] = {0, 0, 0, 0};
    for (int k = 0; k < 4; ++k)
    {
        if (k == 1)
        {
            for (int i = 0; i < 4; ++i)
            {
                CHECK(mtwnPlayer_SetRate(context, players[i], 1.0) == mtwn_success, "going");
            }
        }
        CHECK(mtwnUpdate(context, (uint64_t)k * 250000000u, NULL, 0, NULL) == mtwn_success,
              "an update");
        mtwnEvent events[64];
        uint32_t n = 0;
        CHECK(mtwnReadEvents(context, events, 64, &n, NULL) == mtwn_success, "read");
        for (uint32_t e = 0; e < n; ++e)
        {
            for (int i = 0; i < 4; ++i)
            {
                starts[i] += events[e].kind == mtwn_eventStart &&
                                     events[e].node == MTWN_ROOT_NODE &&
                                     events[e].player.index1 == players[i].index1
                                 ? 1u
                                 : 0u;
            }
        }
    }
    CHECK(starts[0] == 1 && starts[1] == 1 && starts[2] == 1 && starts[3] == 1,
          "each player's start once");
    mtwnDestroyContext(context);
}

// Players at their end turn back only when they came there moving: one that
// seeked there sweeps on from its point, so it is not in step with those
// turning, which enter their clip again. The first of players in step makes
// no key, so the seeked one is the third, between two turning.
static void TestTurnAfterSeek(void)
{
    mtwnContext* context = NULL;
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    (void)AddTrack(context, clip, MTWN_ROOT_NODE, 0.0, 1.0, 2.0, 0);
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnPlayerId players[4];
    for (int i = 0; i < 4; ++i)
    {
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding binding = mtwnBind(0, target);
        mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = &binding;
        playerDef.bindingCount = 1;
        playerDef.keep = true;
        CHECK(mtwnCreatePlayer(context, &playerDef, &players[i]) == mtwn_success, "a player");
    }
    uint32_t starts[4] = {0, 0, 0, 0};
    for (int k = 0; k < 4; ++k)
    {
        if (k == 3)
        {
            CHECK(mtwnPlayer_Seek(context, players[2], 1.0) == mtwn_success, "a seek");
            for (int i = 0; i < 4; ++i)
            {
                CHECK(mtwnPlayer_SetRate(context, players[i], -1.0) == mtwn_success, "back");
            }
        }
        CHECK(mtwnUpdate(context, (uint64_t)k * 500000000u, NULL, 0, NULL) == mtwn_success,
              "an update");
        mtwnEvent events[64];
        uint32_t n = 0;
        CHECK(mtwnReadEvents(context, events, 64, &n, NULL) == mtwn_success, "read");
        for (uint32_t e = 0; e < n; ++e)
        {
            for (int i = 0; i < 4 && k == 3; ++i)
            {
                starts[i] += events[e].kind == mtwn_eventStart &&
                                     events[e].player.index1 == players[i].index1
                                 ? 1u
                                 : 0u;
            }
        }
    }
    CHECK(starts[0] == 2 && starts[1] == 2 && starts[2] == 0 && starts[3] == 2,
          "the turning players enter again");
    mtwnDestroyContext(context);
}

// A clip of exactly the shared node capacity: the players in step keep
// its tracks' results in all of it and no further, so the records kept
// for the fourth (the third's) are the first's to the bit.
static void TestFullSharedNodes(void)
{
    mtwnContext* context = NULL;
    mtwnContextDef def = mtwnDefaultContextDef();
    def.sharedNodeCapacity = 3;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    (void)AddTrack(context, clip, MTWN_ROOT_NODE, 0.05, 1.0, 2.0, 0);
    (void)AddTrack(context, clip, MTWN_ROOT_NODE, 0.07, 1.0, 2.0, 1);
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnPlayerId players[4];
    for (int i = 0; i < 4; ++i)
    {
        mtwnTargetId target;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
              "a target");
        mtwnBinding binding = mtwnBind(0, target);
        mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = &binding;
        playerDef.bindingCount = 1;
        playerDef.keep = true;
        CHECK(mtwnCreatePlayer(context, &playerDef, &players[i]) == mtwn_success, "a player");
    }
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success, "started");
    mtwnEvent events[64];
    uint32_t n = 0;
    CHECK(mtwnReadEvents(context, events, 64, &n, NULL) == mtwn_success, "read");
    CHECK(mtwnUpdate(context, 100000000u, NULL, 0, NULL) == mtwn_success, "a tenth of a second");
    CHECK(mtwnReadEvents(context, events, 64, &n, NULL) == mtwn_success, "read");
    double first[8];
    double fourth[8];
    uint32_t firstCount = 0;
    uint32_t fourthCount = 0;
    for (uint32_t e = 0; e < n; ++e)
    {
        if (events[e].player.index1 == players[0].index1 && firstCount < 8)
        {
            first[firstCount++] = events[e].hostTime;
        }
        if (events[e].player.index1 == players[3].index1 && fourthCount < 8)
        {
            fourth[fourthCount++] = events[e].hostTime;
        }
    }
    bool same = firstCount == 2 && fourthCount == 2;
    for (uint32_t e = 0; same && e < firstCount; ++e)
    {
        same = memcmp(&first[e], &fourth[e], sizeof first[e]) == 0;
    }
    CHECK(same, "the fourth's two starts at the first's host times, to the bit");
    mtwnDestroyContext(context);
}

// Players in step that bind different slots: the third keeps only the
// tracks it binds, so the fourth, binding one more, samples its clip
// itself and its second target shows that track's value.
static void TestInStepMoreSlots(void)
{
    mtwnContext* context = NULL;
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    (void)AddTrack(context, clip, MTWN_ROOT_NODE, 0.0, 1.0, 2.0, 0);
    mtwnNode second = AddTrack(context, clip, MTWN_ROOT_NODE, 0.0, 1.0, 2.0, 1);
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnTargetId last = {0, 0};
    for (int i = 0; i < 4; ++i)
    {
        mtwnBinding bindings[2];
        for (uint32_t b = 0; b < 2; ++b)
        {
            mtwnTargetId target;
            CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) ==
                      mtwn_success,
                  "a target");
            bindings[b] = mtwnBind(b, target);
            last = target;
        }
        mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = bindings;
        playerDef.bindingCount = i == 3 ? 2 : 1;
        playerDef.keep = true;
        mtwnPlayerId player;
        CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success, "a player");
    }
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success, "started");
    CHECK(mtwnUpdate(context, 500000000u, NULL, 0, NULL) == mtwn_success, "half way");
    mtwnVec4 value;
    CHECK(mtwnTarget_GetValue(context, last, &value) == mtwn_success, "read");
    CHECK(value.x == Expected(context, clip, second, 0.5) && value.x > 0.0f,
          "the fourth's second target at its track's value");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestInStep();
    TestDiffering();
    TestHostEasing();
    TestSignedZero();
    TestHeldInStep();
    TestTurnAfterSeek();
    TestFullSharedNodes();
    TestInStepMoreSlots();
    return s_failures == 0 ? 0 : 1;
}
