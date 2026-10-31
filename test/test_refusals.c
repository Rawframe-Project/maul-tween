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
#include "maul-tween/path.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
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
    refused += mtwnContext_Update(c, 0, NULL, 0, NULL) == mtwn_errorInvalid;
    refused += mtwnClipPlayer_Play(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnClipPlayer_Pause(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnClipPlayer_Reverse(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnClipPlayer_Finish(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnClipPlayer_Cancel(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnClipPlayer_Seek(c, noPlayer, 0.25) == mtwn_errorInvalid;
    refused += mtwnClipPlayer_SetRate(c, noPlayer, 2.0) == mtwn_errorInvalid;
    refused += mtwnClipPlayer_SetWeight(c, noPlayer, 0.5f) == mtwn_errorInvalid;
    refused += mtwnDestroyPlayer(c, noPlayer) == mtwn_errorInvalid;
    refused += mtwnDestroyTarget(c, noTarget) == mtwn_errorInvalid;
    refused += mtwnTarget_SetBase(c, noTarget, (mtwnVec4){0}) == mtwn_errorInvalid;
    CHECK(refused == 14, "every call refuses a missing context");
    // The switch returns nothing: without a context it changes nothing.
    mtwnContext_EnableReducedMotion(c, true);
    CHECK(!mtwnContext_IsReducedMotionEnabled(c), "nor switches it");
    CHECK(mtwnContext_GetEventCount(NULL) == 0 && mtwnContext_GetEventsDropped(NULL) == 0 &&
              mtwnContext_GetMisuse(NULL) == 0 && mtwnContext_GetSerialAdvanceCount(NULL) == 0 &&
              mtwnContext_GetMemoryBytes(NULL) == 0,
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
    def.enqueueTask = (mtwnEnqueueTaskFunction*)(uintptr_t)1;
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
    CHECK(mtwnContext_Grow(&context, &smaller) == mtwn_errorInvalid, "grown to fewer events");
    smaller = def;
    smaller.bindingCapacity = def.bindingCapacity - 1u;
    CHECK(mtwnContext_Grow(&context, &smaller) == mtwn_errorInvalid, "grown to fewer bindings");
    mtwnContext* none = NULL;
    CHECK(mtwnContext_Grow(NULL, &def) == mtwn_errorInvalid &&
              mtwnContext_Grow(&none, &def) == mtwn_errorInvalid,
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
    keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
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
    mtwnClipPlayerDef playerDef = mtwnDefaultClipPlayerDef();
    playerDef.clip = clip;
    playerDef.bindingCount = 1;
    mtwnPlayerId player;
    CHECK(mtwnCreateClipPlayer(context, &playerDef, &player) == mtwn_errorInvalid,
          "a binding count with no bindings");
    playerDef.bindingCount = 0;
    CHECK(mtwnCreateClipPlayer(context, &playerDef, &player) == mtwn_success, "a player");
    mtwnDestroyContext(context);
}

// A clip of one scalar track on slot 0, sealed.
static mtwnClipId OneTrack(mtwnContext* context)
{
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
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
    mtwnBinding bindings[2] = {mtwnMakeBinding(0, scalar), mtwnMakeBinding(0, scalar)};
    mtwnClipPlayerDef def = mtwnDefaultClipPlayerDef();
    def.clip = clip;
    def.bindings = bindings;
    def.bindingCount = 1;
    mtwnPlayerId player;
    bindings[0].target = dead;
    CHECK(mtwnCreateClipPlayer(context, &def, &player) == mtwn_errorInvalid, "a destroyed target");
    bindings[0].target = never;
    CHECK(mtwnCreateClipPlayer(context, &def, &player) == mtwn_errorInvalid, "a target never made");
    bindings[0].target = color;
    CHECK(mtwnCreateClipPlayer(context, &def, &player) == mtwn_errorInvalid,
          "a target of another kind");
    bindings[0] = mtwnMakeBinding(1, scalar);
    CHECK(mtwnCreateClipPlayer(context, &def, &player) == mtwn_errorInvalid,
          "a slot the clip lacks");
    bindings[0] = mtwnMakeBinding(0, scalar);
    def.bindingCount = 2;
    CHECK(mtwnCreateClipPlayer(context, &def, &player) == mtwn_errorInvalid, "a slot bound twice");
    def.bindingCount = 1;
    CHECK(mtwnCreateClipPlayer(context, &def, &player) == mtwn_success,
          "bound once, live, its kind");
    CHECK(mtwnClipPlayer_GetState(context, player, NULL) == mtwn_errorInvalid,
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

// Every kind, mode and choice is an int32_t: a negative one is refused,
// passed by value or in a def, and one a host wrote into a made struct is
// refused or read within its tables, never indexed below them.
static void TestNegativeKinds(void)
{
    const int32_t negatives[2] = {-1, INT32_MIN};
    uint32_t refused = 0;
    uint32_t calls = 0;
    for (int k = 0; k < 2; ++k)
    {
        int32_t n = negatives[k];
        mtwnEasing easing;
        refused += mtwnMakePowerEasing(n, 2.0, &easing) == mtwn_errorInvalid;
        refused += mtwnMakePennerEasing(n, mtwn_easeIn, &easing) == mtwn_errorInvalid;
        refused += mtwnMakePennerEasing(mtwn_easingSine, n, &easing) == mtwn_errorInvalid;
        refused += mtwnMakeBackEasing(n, 1.7, &easing) == mtwn_errorInvalid;
        refused += mtwnMakeElasticEasing(n, 1.0, 0.3, &easing) == mtwn_errorInvalid;
        refused += mtwnMakeStepsEasing(2, n, &easing) == mtwn_errorInvalid;
        refused += mtwnMakeCssEasing(n, &easing) == mtwn_errorInvalid;
        mtwnSpring spring;
        CHECK(mtwnMakeSpringFromDuration(0.5, 0.0, &spring) == mtwn_success, "a spring");
        refused += mtwnMakeSpringEasing(&spring, 1.0, 0.0, n, &easing) == mtwn_errorInvalid;
        easing = mtwnMakeLinearEasing();
        refused += mtwnSetEaseMode(&easing, n) == mtwn_errorInvalid;
        calls += 9;
        double eased = 0.0;
        mtwnEasing changed = mtwnMakeLinearEasing();
        changed.kind = n;
        refused += mtwnSampleEasing(&changed, 0.5, false, &eased) == mtwn_errorInvalid;
        changed = mtwnMakeLinearEasing();
        changed.mode = n;
        refused += mtwnSampleEasing(&changed, 0.5, false, &eased) == mtwn_errorInvalid;
        calls += 2;
        mtwnColor gray = {0.5f, 0.5f, 0.5f, 1.0f};
        mtwnColorMix colorMix;
        double coordinates[4] = {0.5, 0.0, 0.0, 1.0};
        refused += mtwnMakeColorMix(gray, gray, n, mtwn_hueShorter, &colorMix) == mtwn_errorInvalid;
        refused += mtwnMakeColorMix(gray, gray, mtwn_colorOklch, n, &colorMix) == mtwn_errorInvalid;
        refused += mtwnConvertColorToSpace(gray, n, coordinates) == mtwn_errorInvalid;
        refused += mtwnConvertColorFromSpace(coordinates, n, &gray) == mtwn_errorInvalid;
        CHECK(mtwnMakeColorMix(gray, gray, mtwn_colorOklab, mtwn_hueShorter, &colorMix) ==
                  mtwn_success,
              "a color mix");
        colorMix.space = n;
        refused += mtwnMixColor(&colorMix, 0.5, &gray) == mtwn_errorInvalid;
        double angle = 0.0;
        refused += mtwnMixAngle(0.0, 1.0, n, 0.5, &angle) == mtwn_errorInvalid;
        mtwnQuat identity = {0.0f, 0.0f, 0.0f, 1.0f};
        mtwnRotationMix rotationMix;
        refused += mtwnMakeRotationMix(identity, identity, n, &rotationMix) == mtwn_errorInvalid;
        calls += 7;
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 1.0;
        keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
        keys[0].kind = n;
        mtwnPath path;
        refused += mtwnMakePath(keys, 2, 1, 0.5, NULL, &path) == mtwn_errorInvalid;
        mtwnTrack track;
        refused +=
            mtwnMakeTrack(keys, 2, &(mtwnTrackDef){0}, NULL, NULL, &track) == mtwn_errorInvalid;
        keys[0].kind = mtwn_segmentLinear;
        mtwnTrackDef trackDef = mtwnDefaultTrackDef(n);
        refused += mtwnMakeTrack(keys, 2, &trackDef, NULL, NULL, &track) == mtwn_errorInvalid;
        int32_t* choices[3] = {&trackDef.colorSpace, &trackDef.angleMethod, &trackDef.rotationPath};
        mtwnValueKind kinds[3] = {mtwn_valueColor, mtwn_valueAngle, mtwn_valueRotation};
        for (int c = 0; c < 3; ++c)
        {
            trackDef = mtwnDefaultTrackDef(kinds[c]);
            *choices[c] = n;
            mtwnSegmentMix mixes[1];
            refused += mtwnMakeTrack(keys, 2, &trackDef, NULL, mixes, &track) == mtwn_errorInvalid;
        }
        calls += 6;
        // Made, then changed by the host: refused or read within its
        // tables, which the sanitizers check.
        CHECK(mtwnMakePath(keys, 2, 1, 0.5, NULL, &path) == mtwn_success, "a path");
        keys[0].kind = n;
        double value[4];
        (void)mtwnSamplePath(&path, 0.25, value, NULL);
        keys[0].kind = mtwn_segmentLinear;
        trackDef = mtwnDefaultTrackDef(mtwn_valueScalar);
        CHECK(mtwnMakeTrack(keys, 2, &trackDef, NULL, NULL, &track) == mtwn_success, "a track");
        track.kind = n;
        mtwnVec4 sampled;
        refused += mtwnSampleTrack(&track, 0.25, &sampled) == mtwn_errorInvalid;
        calls += 1;
        int32_t* timingChoices[3];
        mtwnTiming timing = mtwnDefaultTiming();
        timingChoices[0] = &timing.fill;
        timingChoices[1] = &timing.direction;
        timingChoices[2] = &timing.alternateEasing;
        for (int c = 0; c < 3; ++c)
        {
            timing = mtwnDefaultTiming();
            *timingChoices[c] = n;
            refused += mtwnValidateTiming(&timing) == mtwn_errorInvalid;
        }
        mtwnStagger stagger = mtwnDefaultStagger();
        int32_t* staggerChoices[3] = {&stagger.origin, &stagger.axis, &stagger.spacingKind};
        double offsets[4];
        for (int c = 0; c < 3; ++c)
        {
            stagger = mtwnDefaultStagger();
            *staggerChoices[c] = n;
            refused += mtwnComputeStagger(&stagger, 4, offsets) == mtwn_errorInvalid;
        }
        mtwnRoughDef rough;
        memset(&rough, 0, sizeof rough);
        rough.shape = mtwnMakeLinearEasing();
        rough.taper = n;
        mtwnLinearPoint points[4];
        refused += mtwnMakeRoughEasing(&rough, points, 4, &easing) == mtwn_errorInvalid;
        calls += 7;
    }
    CHECK(refused == calls, "every negative kind refused without a context");

    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId target;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
          "a target");
    // Each choice on a target of the kind that reads it: the others' are
    // not read.
    mtwnTargetId color;
    mtwnTargetId angle;
    mtwnTargetId rotation;
    CHECK(mtwnCreateTarget(context, mtwn_valueColor, (mtwnVec4){0.5f, 0.5f, 0.5f, 1.0f}, &color) ==
                  mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueAngle, (mtwnVec4){0}, &angle) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueRotation, (mtwnVec4){0.0f, 0.0f, 0.0f, 1.0f},
                               &rotation) == mtwn_success,
          "a color, an angle and a rotation target");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    refused = 0;
    calls = 0;
    for (int k = 0; k < 2; ++k)
    {
        int32_t n = negatives[k];
        mtwnTargetId made;
        refused += mtwnCreateTarget(context, n, (mtwnVec4){0}, &made) == mtwn_errorInvalid;
        mtwnTransitionDef def = mtwnDefaultTransitionDef();
        int32_t* transitionChoices[6] = {&def.kind,      &def.velocitySource, &def.colorSpace,
                                         &def.hueMethod, &def.angleMethod,    &def.rotationPath};
        for (int c = 0; c < 6; ++c)
        {
            def = mtwnDefaultTransitionDef();
            mtwnTargetId on[6] = {target, target, color, color, angle, rotation};
            mtwnVec4 to[6] = {{1.0f, 0, 0, 0},          {1.0f, 0, 0, 0}, {0.2f, 0.2f, 0.2f, 1.0f},
                              {0.2f, 0.2f, 0.2f, 1.0f}, {1.0f, 0, 0, 0}, {0.0f, 0.0f, 0.6f, 0.8f}};
            def.target = on[c];
            def.value = to[c];
            *transitionChoices[c] = n;
            mtwnPlayerId player;
            refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
        }
        def = mtwnDefaultTransitionDef();
        def.target = angle;
        def.angleMethod = n;
        double distance = 0.0;
        refused += mtwnContext_MeasureTransition(context, &def, &distance) == mtwn_errorInvalid;
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 1.0;
        keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
        mtwnClipTrackDef trackDef = mtwnDefaultClipTrackDef();
        trackDef.keyframes = keys;
        trackDef.keyframeCount = 2;
        trackDef.position.kind = n;
        mtwnNode node;
        refused += mtwnClip_AddTrack(context, clip, &trackDef, &node) == mtwn_errorInvalid;
        trackDef = mtwnDefaultClipTrackDef();
        trackDef.keyframes = keys;
        trackDef.keyframeCount = 2;
        trackDef.timing.fill = n;
        refused += mtwnClip_AddTrack(context, clip, &trackDef, &node) == mtwn_errorInvalid;
        mtwnPosition at = {0.5, 0, n};
        refused +=
            mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, at, 1, &node) == mtwn_errorInvalid;
        mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
        timeline.position.kind = n;
        refused += mtwnClip_AddTimeline(context, clip, &timeline, &node) == mtwn_errorInvalid;
        calls += 12;
    }
    CHECK(refused == calls, "every negative kind refused on a context");
    mtwnClipId one = OneTrack(context);
    for (int k = 0; k < 2; ++k)
    {
        mtwnBinding binding = mtwnMakeBinding(0, target);
        binding.composite = negatives[k];
        mtwnClipPlayerDef play = mtwnDefaultClipPlayerDef();
        play.clip = one;
        play.bindings = &binding;
        play.bindingCount = 1;
        mtwnPlayerId player;
        CHECK(mtwnCreateClipPlayer(context, &play, &player) == mtwn_errorInvalid,
              "a negative composite refused");
    }
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
    TestNegativeKinds();
    return s_failures == 0 ? 0 : 1;
}
