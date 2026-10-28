// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Refusals the headers name that no module's suite reaches: every call given a missing context, the
// makers' and conversions' null outputs and unknown modes, defs with one of a pair of functions,
// capacities past the limit, a context grown smaller, a spring not made by the library, a missing
// output or cookie when adding to a clip, and a node that is not a track.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static void TestMissingContext(void)
{
    const mtwnClipId noClip = {0, 0};
    const mtwnPlayerId noPlayer = {0, 0};
    const mtwnTargetId noTarget = {0, 0};
    mtwnContext* c = NULL;
    uint32_t refused = 0;
    refused += mtwnDestroyClip(c, noClip) == mtwn_errorInvalid;
    refused += mtwnClip_Seal(c, noClip) == mtwn_errorInvalid;
    refused += mtwnContext_SetReducedMotion(c, true) == mtwn_errorInvalid;
    refused += mtwnContext_Update(c, 0, NULL, 0, NULL) == mtwn_errorInvalid;
    refused += mtwnPlayer_Play(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Pause(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Reverse(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Finish(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Cancel(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Seek(c, noPlayer, 0.25) == mtwn_errorInvalid;
    refused += mtwnPlayer_SetRate(c, noPlayer, 2.0) == mtwn_errorInvalid;
    refused += mtwnPlayer_SetWeight(c, noPlayer, 0.5f) == mtwn_errorInvalid;
    refused += mtwnDestroyPlayer(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnDestroyTarget(c, noTarget) == mtwn_errorInvalid;
    refused += mtwnTarget_SetBase(c, noTarget, (mtwnVec4){0}) == mtwn_errorInvalid;
    CHECK(refused == 15, "every call refuses a missing context");
    CHECK(mtwnContext_GetEventCount(NULL) == 0 && mtwnContext_GetDroppedEvents(NULL) == 0,
          "and its counts read 0");
    CHECK(!mtwnTarget_IsValid(NULL, noTarget) && !mtwnPlayer_IsValid(NULL, noPlayer) &&
              !mtwnClip_IsValid(NULL, noClip),
          "and names nothing alive");
}

static void TestMakers(void)
{
    mtwnEasing easing;
    CHECK(mtwnMakePennerEasing(mtwn_easingSine, mtwn_easeIn, NULL) == mtwn_errorInvalid &&
              mtwnMakePennerEasing(mtwn_easingSine, (mtwnEaseMode)9, &easing) == mtwn_errorInvalid,
          "a Penner easing: no output, an unknown mode");
    CHECK(mtwnMakeBackEasing(mtwn_easeOut, 1.7, NULL) == mtwn_errorInvalid &&
              mtwnMakeBackEasing((mtwnEaseMode)9, 1.7, &easing) == mtwn_errorInvalid,
          "a back easing");
    CHECK(mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, NULL) == mtwn_errorInvalid &&
              mtwnMakeElasticEasing((mtwnEaseMode)9, 1.0, 0.3, &easing) == mtwn_errorInvalid,
          "an elastic easing");
    CHECK(mtwnMakeHostEasing((mtwnEaseMode)9, NULL, NULL, &easing) == mtwn_errorInvalid,
          "a host easing");
    double coordinates[4] = {0.5, 0.5, 0.5, 1.0};
    mtwnColor color = {0.5f, 0.5f, 0.5f, 1.0f};
    CHECK(mtwnConvertColorToSpace(color, mtwn_colorSrgb, NULL) == mtwn_errorInvalid &&
              mtwnConvertColorToSpace(color, (mtwnColorSpace)99, coordinates) == mtwn_errorInvalid,
          "a color's coordinates: no output, an unknown space");
    CHECK(mtwnConvertColorFromSpace(NULL, mtwn_colorSrgb, &color) == mtwn_errorInvalid &&
              mtwnConvertColorFromSpace(coordinates, mtwn_colorSrgb, NULL) == mtwn_errorInvalid &&
              mtwnConvertColorFromSpace(coordinates, (mtwnColorSpace)99, &color) ==
                  mtwn_errorInvalid,
          "a color from coordinates: none, no output, an unknown space");
}

static void TestContextDefs(void)
{
    mtwnContext* context = NULL;
    mtwnContextDef def = mtwnDefaultContextDef();
    def.finishTask = NULL;
    def.enqueueTask = (mtwnEnqueueTaskFn*)(uintptr_t)1;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "a task system half given");
    def = mtwnDefaultContextDef();
    def.clipCapacity = MTWN_MAX_COUNT + 1u;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorLimit, "clips past the limit");
    def = mtwnDefaultContextDef();
    def.eventCapacity = MTWN_MAX_COUNT + 1u;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorLimit, "events past the limit");
    def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnContextDef smaller = def;
    smaller.eventCapacity = def.eventCapacity - 1u;
    CHECK(mtwnGrowContext(&context, &smaller) == mtwn_errorInvalid, "grown to fewer events");
    smaller = def;
    smaller.bindingCapacity = def.bindingCapacity - 1u;
    CHECK(mtwnGrowContext(&context, &smaller) == mtwn_errorInvalid, "grown to fewer bindings");
    mtwnContext* none = NULL;
    CHECK(mtwnGrowContext(NULL, &def) == mtwn_errorInvalid &&
              mtwnGrowContext(&none, &def) == mtwn_errorInvalid,
          "grown from no context");
    mtwnDestroyContext(context);
}

static void TestClipsAndPlayers(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.slotCapacity = MTWN_MAX_COUNT + 1u;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_errorLimit, "slots past the limit");
    clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    CHECK(mtwnClip_AddTrack(context, clip, &track, NULL) == mtwn_errorInvalid,
          "a track with nowhere to give its node");
    mtwnNode orphan = 0;
    track.parent = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &orphan) == mtwn_errorInvalid,
          "a track under node 0, no timeline");
    track.parent = MTWN_ROOT_NODE;
    mtwnNode node = 0;
    track.cookie = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "a track def without its cookie");
    CHECK(mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, at, 1, NULL) == mtwn_errorInvalid &&
              mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, at, 1, NULL) == mtwn_errorInvalid,
          "a label and a mark with nowhere to give their nodes");
    mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
    CHECK(mtwnClip_AddTimeline(context, clip, &timeline, &node) == mtwn_success, "a timeline");
    // The output is cleared before the def is read: given the def's own
    // parent, that parent is 0 when read.
    timeline.parent = node;
    CHECK(mtwnClip_AddTimeline(context, clip, &timeline, &timeline.parent) == mtwn_errorInvalid &&
              timeline.parent == 0,
          "a timeline's node given into its def's parent");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.parent = node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &track.parent) == mtwn_errorInvalid &&
              track.parent == 0,
          "a track's too");
    track.parent = node;
    mtwnNode added = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &added) == mtwn_success,
          "the same track given a node of its own");
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
    mtwnClipTrack read;
    CHECK(mtwnClip_GetTrack(context, clip, node, &read) == mtwn_errorInvalid &&
              mtwnClip_GetTrack(context, clip, 99, &read) == mtwn_errorInvalid,
          "a timeline and a node past the clip are not tracks");
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindingCount = 1;
    mtwnPlayerId player;
    CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_errorInvalid,
          "a binding count with no bindings");
    playerDef.bindingCount = 0;
    CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success, "a player");
    mtwnDestroyContext(context);
}

// A clip of one scalar track on slot 0, sealed.
static mtwnClipId OneTrack(mtwnContext* context)
{
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
    mtwnNode node = 0;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success &&
              mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Seal(context, clip) == mtwn_success,
          "a clip of one track");
    return clip;
}

static void TestBindings(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnClipId clip = OneTrack(context);
    mtwnTargetId scalar = {0, 0};
    mtwnTargetId color = {0, 0};
    mtwnTargetId dead = {0, 0};
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &scalar) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueColor, (mtwnVec4){0, 0, 0, 1.0f}, &color) ==
                  mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &dead) == mtwn_success &&
              mtwnDestroyTarget(context, dead) == mtwn_success,
          "targets, one destroyed");
    mtwnTargetId never = scalar;
    never.index1 += 50;
    mtwnBinding bindings[2] = {mtwnBind(0, scalar), mtwnBind(0, scalar)};
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = bindings;
    def.bindingCount = 1;
    mtwnPlayerId player;
    bindings[0].target = dead;
    CHECK(mtwnCreatePlayer(context, &def, &player) == mtwn_errorInvalid, "a destroyed target");
    bindings[0].target = never;
    CHECK(mtwnCreatePlayer(context, &def, &player) == mtwn_errorInvalid, "a target never made");
    bindings[0].target = color;
    CHECK(mtwnCreatePlayer(context, &def, &player) == mtwn_errorInvalid,
          "a target of another kind");
    bindings[0] = mtwnBind(1, scalar);
    CHECK(mtwnCreatePlayer(context, &def, &player) == mtwn_errorInvalid, "a slot the clip lacks");
    bindings[0] = mtwnBind(0, scalar);
    def.bindingCount = 2;
    CHECK(mtwnCreatePlayer(context, &def, &player) == mtwn_errorInvalid, "a slot bound twice");
    def.bindingCount = 1;
    CHECK(mtwnCreatePlayer(context, &def, &player) == mtwn_success, "bound once, live, its kind");
    CHECK(mtwnPlayer_GetState(context, player, NULL) == mtwn_errorInvalid,
          "its state with nowhere to give it");
    mtwnDestroyContext(context);
}

static void TestTransitions(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId target;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
          "a target");
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = target;
    def.value = (mtwnVec4){1.0f, 0, 0, 0};
    def.kind = mtwn_transitionSpring;
    memset(&def.spring, 0xAB, sizeof def.spring);
    mtwnPlayerId player;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid,
          "a spring not made by the library");
    def = mtwnDefaultTransitionDef();
    def.target = target;
    def.value = (mtwnVec4){1.0f, 0, 0, 0};
    def.kind = mtwn_transitionSpring;
    def.restOffset = 0.0;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid,
          "a rest offset not above 0");
    def.restOffset = (double)NAN;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid, "nor NaN");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestMissingContext();
    TestMakers();
    TestContextDefs();
    TestClipsAndPlayers();
    TestBindings();
    TestTransitions();
    return s_failures == 0 ? 0 : 1;
}
