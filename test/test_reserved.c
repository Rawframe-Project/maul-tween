// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reserved bytes: an input a call takes is refused when
// any reserved byte of it, or of a part of it, is not 0, one misuse
// counted where a context takes it; with them 0 it is taken. What the
// library writes, outputs and made values alike, has them 0 though the
// buffer held other bytes.
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Each byte of a field set to 1 in turn, the call made, the byte put
// back: counts the tries and the refusals.
#define REFUSED_EACH(field, call)                                                                  \
    for (size_t at_ = 0; at_ < sizeof(field); ++at_)                                               \
    {                                                                                              \
        ((uint8_t*)&(field))[at_] = 1u;                                                            \
        refused += (call) == mtwn_errorInvalid;                                                    \
        tried += 1;                                                                                \
        ((uint8_t*)&(field))[at_] = 0u;                                                            \
    }

static mtwnKeyframe Key(double offset, float x)
{
    mtwnKeyframe k;
    memset(&k, 0, sizeof k);
    k.offset = offset;
    k.value = (mtwnVec4){x, 0, 0, 0};
    k.easing = mtwnLinearEasing();
    return k;
}

static mtwnPosition At(double offset)
{
    mtwnPosition position;
    memset(&position, 0, sizeof position);
    position.kind = mtwn_positionAbsolute;
    position.offset = offset;
    return position;
}

static void TestDefs(void)
{
    uint32_t tried = 0;
    uint32_t refused = 0;
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    REFUSED_EACH(contextDef.reserved, mtwnCreateContext(&contextDef, &context));
    CHECK(tried == 4 && refused == tried, "a context def");
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "taken with them 0");

    tried = refused = 0;
    uint64_t misuse = mtwnGetContextMisuse(context);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    REFUSED_EACH(clipDef.reserved, mtwnCreateClip(context, &clipDef, &clip));
    CHECK(refused == tried && mtwnGetContextMisuse(context) == misuse + tried, "a clip def");
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip with them 0");

    tried = refused = 0;
    misuse = mtwnGetContextMisuse(context);
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.position = At(0.0);
    mtwnNode node = 0;
    REFUSED_EACH(track.reserved, mtwnClip_AddTrack(context, clip, &track, &node));
    REFUSED_EACH(track.timing.reserved, mtwnClip_AddTrack(context, clip, &track, &node));
    REFUSED_EACH(track.timing.easing.reserved, mtwnClip_AddTrack(context, clip, &track, &node));
    REFUSED_EACH(track.track.reserved, mtwnClip_AddTrack(context, clip, &track, &node));
    REFUSED_EACH(track.position.reserved, mtwnClip_AddTrack(context, clip, &track, &node));
    REFUSED_EACH(keys[0].reserved, mtwnClip_AddTrack(context, clip, &track, &node));
    REFUSED_EACH(keys[1].easing.reserved, mtwnClip_AddTrack(context, clip, &track, &node));
    CHECK(refused == tried && mtwnGetContextMisuse(context) == misuse + tried,
          "a clip track def, its timing, easing, track, position and keyframes");
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track with them 0");

    tried = refused = 0;
    misuse = mtwnGetContextMisuse(context);
    mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
    timeline.position = At(0.0);
    timeline.timing.duration = 1.0;
    REFUSED_EACH(timeline.reserved, mtwnClip_AddTimeline(context, clip, &timeline, &node));
    REFUSED_EACH(timeline.timing.reserved, mtwnClip_AddTimeline(context, clip, &timeline, &node));
    REFUSED_EACH(timeline.timing.easing.reserved,
                 mtwnClip_AddTimeline(context, clip, &timeline, &node));
    REFUSED_EACH(timeline.position.reserved, mtwnClip_AddTimeline(context, clip, &timeline, &node));
    mtwnPosition at = At(0.5);
    REFUSED_EACH(at.reserved, mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, at, 3, &node));
    REFUSED_EACH(at.reserved, mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, at, 7, &node));
    CHECK(refused == tried && mtwnGetContextMisuse(context) == misuse + tried,
          "a timeline def, its timing and position; a mark's and a label's position");
    CHECK(mtwnClip_AddTimeline(context, clip, &timeline, &node) == mtwn_success &&
              mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, at, 3, &node) == mtwn_success &&
              mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, at, 7, &node) == mtwn_success,
          "each with them 0");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "the clip finished");

    tried = refused = 0;
    misuse = mtwnGetContextMisuse(context);
    mtwnTargetId target;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
          "a target");
    mtwnBinding binding = mtwnBind(0, target);
    mtwnPlayerDef player = mtwnDefaultPlayerDef();
    player.clip = clip;
    player.bindings = &binding;
    player.bindingCount = 1;
    mtwnPlayerId id;
    REFUSED_EACH(player.reserved, mtwnCreatePlayer(context, &player, &id));
    REFUSED_EACH(binding.reserved, mtwnCreatePlayer(context, &player, &id));
    CHECK(refused == tried && mtwnGetContextMisuse(context) == misuse + tried,
          "a player def and its binding");
    CHECK(mtwnCreatePlayer(context, &player, &id) == mtwn_success, "a player with them 0");

    tried = refused = 0;
    misuse = mtwnGetContextMisuse(context);
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    transition.target = target;
    transition.value = (mtwnVec4){1.0f, 0, 0, 0};
    transition.duration = 0.5;
    double distance = 0.0;
    REFUSED_EACH(transition.easing.reserved, mtwnStartTransition(context, &transition, &id));
    REFUSED_EACH(transition.spring.reserved, mtwnStartTransition(context, &transition, &id));
    REFUSED_EACH(transition.easing.reserved,
                 mtwnMeasureTransition(context, &transition, &distance));
    REFUSED_EACH(transition.spring.reserved,
                 mtwnMeasureTransition(context, &transition, &distance));
    CHECK(refused == tried && mtwnGetContextMisuse(context) == misuse + tried,
          "a transition def's easing and spring, started or measured");
    CHECK(mtwnMeasureTransition(context, &transition, &distance) == mtwn_success &&
              mtwnStartTransition(context, &transition, &id) == mtwn_success,
          "a transition with them 0");
    mtwnDestroyContext(context);
}

static void TestValues(void)
{
    uint32_t tried = 0;
    uint32_t refused = 0;
    mtwnTiming timing = mtwnDefaultTiming();
    timing.duration = 1.0;
    REFUSED_EACH(timing.reserved, mtwnValidateTiming(&timing));
    REFUSED_EACH(timing.easing.reserved, mtwnValidateTiming(&timing));
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnPath path;
    REFUSED_EACH(keys[1].reserved, mtwnMakePath(keys, 2, 1, 0.5, NULL, &path));
    REFUSED_EACH(keys[0].easing.reserved, mtwnMakePath(keys, 2, 1, 0.5, NULL, &path));
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueScalar);
    mtwnTrack track;
    REFUSED_EACH(def.reserved, mtwnMakeTrack(&def, keys, 2, NULL, NULL, &track));
    CHECK(refused == tried, "a timing, its easing, keyframes and a track def");
    CHECK(mtwnValidateTiming(&timing) == mtwn_success &&
              mtwnMakePath(keys, 2, 1, 0.5, NULL, &path) == mtwn_success &&
              mtwnMakeTrack(&def, keys, 2, NULL, NULL, &track) == mtwn_success,
          "each with them 0");
}

// Whether every byte of a field is 0.
static bool Clear(const void* field, size_t size)
{
    const uint8_t* bytes = (const uint8_t*)field;
    uint8_t any = 0;
    for (size_t i = 0; i < size; ++i)
    {
        any |= bytes[i];
    }
    return any == 0;
}

#define FILLED(x)    memset(&(x), 0xFF, sizeof(x))
#define CLEAR(field) Clear(&(field), sizeof(field))

static double Identity(void* unused, double progress)
{
    (void)unused;
    return progress;
}

static void TestMade(void)
{
    mtwnEasing e = mtwnLinearEasing();
    bool easings = e.reserved == 0;
    FILLED(e);
    easings =
        easings && mtwnMakePowerEasing(mtwn_easeIn, 2.5, &e) == mtwn_success && e.reserved == 0;
    FILLED(e);
    easings = easings && mtwnMakePennerEasing(mtwn_easingSine, mtwn_easeOut, &e) == mtwn_success &&
              e.reserved == 0;
    FILLED(e);
    easings =
        easings && mtwnMakeBackEasing(mtwn_easeIn, 1.7, &e) == mtwn_success && e.reserved == 0;
    FILLED(e);
    easings = easings && mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &e) == mtwn_success &&
              e.reserved == 0;
    FILLED(e);
    easings = easings && mtwnMakeHostEasing(mtwn_easeIn, Identity, NULL, &e) == mtwn_success &&
              e.reserved == 0;
    FILLED(e);
    easings =
        easings && mtwnMakeCubicBezier(0.25, 0.1, 0.25, 1.0, &e) == mtwn_success && e.reserved == 0;
    FILLED(e);
    easings = easings && mtwnMakeCssEasing(mtwn_cssStepEnd, &e) == mtwn_success && e.reserved == 0;
    FILLED(e);
    mtwnLinearPoint points[2] = {{0.0, 0.0}, {1.0, 1.0}};
    easings = easings && mtwnMakeLinearEasing(points, 2, &e) == mtwn_success && e.reserved == 0;
    CHECK(easings, "every easing maker writes its reserved byte 0");

    mtwnSpring spring;
    FILLED(spring);
    bool springs =
        mtwnMakeSpring(100.0, 10.0, 1.0, &spring) == mtwn_success && spring.reserved == 0;
    FILLED(spring);
    springs = springs && mtwnMakeSpringFromFrequency(2.0, 0.7, &spring) == mtwn_success &&
              spring.reserved == 0;
    FILLED(spring);
    springs = springs && mtwnMakeSpringFromDuration(0.5, 0.2, &spring) == mtwn_success &&
              spring.reserved == 0;
    FILLED(e);
    springs = springs && mtwnMakeSpringEasing(&spring, 0.5, 0.0, mtwn_easeIn, &e) == mtwn_success &&
              e.reserved == 0;
    CHECK(springs, "every spring maker, and the spring easing");

    mtwnColorMix mix;
    FILLED(mix);
    mtwnColor red = {1.0f, 0.0f, 0.0f, 1.0f};
    mtwnColor blue = {0.0f, 0.0f, 1.0f, 1.0f};
    CHECK(mtwnMakeColorMix(red, blue, mtwn_colorOklch, mtwn_hueShorter, &mix) == mtwn_success &&
              CLEAR(mix.reserved),
          "a color mix");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnPath path;
    FILLED(path);
    CHECK(mtwnMakePath(keys, 2, 1, 0.5, NULL, &path) == mtwn_success && CLEAR(path.reserved),
          "a path");
    mtwnTrackDef trackDef = mtwnDefaultTrackDef(mtwn_valueScalar);
    mtwnTrack track;
    FILLED(track);
    CHECK(CLEAR(trackDef.reserved) &&
              mtwnMakeTrack(&trackDef, keys, 2, NULL, NULL, &track) == mtwn_success &&
              CLEAR(track.reserved),
          "a track def and a track");
    mtwnTiming timing = mtwnDefaultTiming();
    mtwnStagger stagger = mtwnDefaultStagger();
    CHECK(CLEAR(timing.reserved) && timing.easing.reserved == 0 && CLEAR(stagger.reserved),
          "the default timing and stagger");
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipTrackDef clipTrack = mtwnDefaultClipTrackDef();
    mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
    mtwnPlayerDef player = mtwnDefaultPlayerDef();
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    CHECK(contextDef.reserved == 0 && clipDef.reserved == 0 && CLEAR(clipTrack.reserved) &&
              CLEAR(clipTrack.track.reserved) && CLEAR(clipTrack.timing.reserved) &&
              CLEAR(clipTrack.position.reserved) && CLEAR(timeline.reserved) &&
              CLEAR(timeline.position.reserved) && CLEAR(player.reserved) &&
              transition.easing.reserved == 0 && transition.spring.reserved == 0,
          "every default def");

    uint32_t tried = 0;
    uint32_t refused = 0;
    double offsets[4];
    REFUSED_EACH(stagger.reserved, mtwnComputeStagger(&stagger, 4, offsets));
    CHECK(tried > 0 && refused == tried && mtwnComputeStagger(&stagger, 4, offsets) == mtwn_success,
          "a stagger refused with a reserved byte, taken with them 0");
}

static void TestOutputs(void)
{
    mtwnTiming timing = mtwnDefaultTiming();
    timing.duration = 1.0;
    mtwnTimingResult result;
    FILLED(result);
    mtwnComputeTiming(&timing, 0.5, false, &result);
    CHECK(CLEAR(result.reserved), "a timing's result");

    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnTargetId target;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
          "a target");
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    transition.target = target;
    transition.value = (mtwnVec4){1.0f, 0, 0, 0};
    transition.duration = 0.5;
    mtwnPlayerId id;
    CHECK(mtwnStartTransition(context, &transition, &id) == mtwn_success, "a transition");
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, 250000000u, NULL, 0, NULL) == mtwn_success,
          "updated");
    mtwnTargetState targetState;
    FILLED(targetState);
    CHECK(mtwnTarget_GetState(context, target, &targetState) == mtwn_success &&
              CLEAR(targetState.reserved),
          "a target's state");
    mtwnEvent events[8];
    memset(events, 0xFF, sizeof events);
    uint32_t count = 0;
    uint64_t dropped = 0;
    bool written =
        mtwnReadEvents(context, events, 8, &count, &dropped) == mtwn_success && count > 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        written = written && CLEAR(events[i].reserved);
    }
    CHECK(written, "every event read");

    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.position = At(0.0);
    mtwnNode node = 0;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success &&
              mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a clip");
    mtwnClipNodeInfo info;
    FILLED(info);
    bool nodes = mtwnClip_GetNode(context, clip, MTWN_ROOT_NODE, &info) == mtwn_success &&
                 CLEAR(info.reserved);
    FILLED(info);
    nodes = nodes && mtwnClip_GetNode(context, clip, node, &info) == mtwn_success &&
            CLEAR(info.reserved);
    CHECK(nodes, "a clip's root and track read");
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    mtwnPlayerId clipPlayer;
    CHECK(mtwnCreatePlayer(context, &playerDef, &clipPlayer) == mtwn_success, "a clip's player");
    mtwnPlayerState playerState;
    FILLED(playerState);
    CHECK(mtwnPlayer_GetState(context, clipPlayer, &playerState) == mtwn_success &&
              CLEAR(playerState.reserved),
          "a player's state");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestDefs();
    TestValues();
    TestMade();
    TestOutputs();
    return s_failures == 0 ? 0 : 1;
}
