// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Refusals the headers name that no other test reached (item 108's
// branch audit): every call given a missing context, the makers' and
// conversions' null outputs and unknown modes, defs with one of a pair of
// functions, capacities past the limit, a context grown smaller, a spring
// not made by the library, a missing output or cookie when adding to a
// clip, and a node that is not a track.

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
    refused += mtwnClip_Finish(c, noClip) == mtwn_errorInvalid;
    refused += mtwnContext_SetReducedMotion(c, true) == mtwn_errorInvalid;
    refused += mtwnUpdate(c, 0, NULL, 0, NULL) == mtwn_errorInvalid;
    refused += mtwnPlayer_Play(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Pause(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Reverse(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Finish(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Cancel(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnPlayer_Seek(c, noPlayer, 0.25) == mtwn_errorInvalid;
    refused += mtwnPlayer_SetRate(c, noPlayer, 2.0) == mtwn_errorInvalid;
    refused += mtwnPlayer_SetWeight(c, noPlayer, 0.5) == mtwn_errorInvalid;
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
    CHECK(mtwnColorToSpace(color, mtwn_colorSrgb, NULL) == mtwn_errorInvalid &&
              mtwnColorToSpace(color, (mtwnColorSpace)99, coordinates) == mtwn_errorInvalid,
          "a color's coordinates: no output, an unknown space");
    CHECK(mtwnColorFromSpace(NULL, mtwn_colorSrgb, &color) == mtwn_errorInvalid &&
              mtwnColorFromSpace(coordinates, mtwn_colorSrgb, NULL) == mtwn_errorInvalid &&
              mtwnColorFromSpace(coordinates, (mtwnColorSpace)99, &color) == mtwn_errorInvalid,
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
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "clips past the limit");
    def = mtwnDefaultContextDef();
    def.eventCapacity = MTWN_MAX_COUNT + 1u;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "events past the limit");
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
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_errorInvalid, "slots past the limit");
    clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    CHECK(mtwnClip_AddTrack(context, clip, &track, NULL) == mtwn_errorInvalid,
          "a track with nowhere to give its node");
    mtwnNode node = 0;
    track.cookie = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "a track def without its cookie");
    CHECK(mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, at, 1, NULL) == mtwn_errorInvalid &&
              mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, at, 1, NULL) == mtwn_errorInvalid,
          "a label and a mark with nowhere to give their nodes");
    mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
    CHECK(mtwnClip_AddTimeline(context, clip, &timeline, &node) == mtwn_success, "a timeline");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
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
    def.spec = mtwn_transitionSpring;
    memset(&def.spring, 0xAB, sizeof def.spring);
    mtwnPlayerId player;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid,
          "a spring not made by the library");
    def = mtwnDefaultTransitionDef();
    def.target = target;
    def.value = (mtwnVec4){1.0f, 0, 0, 0};
    def.spec = mtwn_transitionSpring;
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
    TestTransitions();
    return s_failures == 0 ? 0 : 1;
}
