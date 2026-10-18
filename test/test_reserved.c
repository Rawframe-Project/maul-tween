// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reserved bytes (record mtwn-0071): an input a call takes is refused when
// any reserved byte of it, or of a part of it, is not 0, one misuse
// counted where a context takes it; with them 0 it is taken.
#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/path.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

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
    uint64_t misuse = mtwnContext_GetMisuseCount(context);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    REFUSED_EACH(clipDef.reserved, mtwnCreateClip(context, &clipDef, &clip));
    CHECK(refused == tried && mtwnContext_GetMisuseCount(context) == misuse + tried, "a clip def");
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip with them 0");

    tried = refused = 0;
    misuse = mtwnContext_GetMisuseCount(context);
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
    CHECK(refused == tried && mtwnContext_GetMisuseCount(context) == misuse + tried,
          "a clip track def, its timing, easing, track, position and keyframes");
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track with them 0");

    tried = refused = 0;
    misuse = mtwnContext_GetMisuseCount(context);
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
    CHECK(refused == tried && mtwnContext_GetMisuseCount(context) == misuse + tried,
          "a timeline def, its timing and position; a mark's and a label's position");
    CHECK(mtwnClip_AddTimeline(context, clip, &timeline, &node) == mtwn_success &&
              mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, at, 3, &node) == mtwn_success &&
              mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, at, 7, &node) == mtwn_success,
          "each with them 0");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "the clip finished");

    tried = refused = 0;
    misuse = mtwnContext_GetMisuseCount(context);
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
    CHECK(refused == tried && mtwnContext_GetMisuseCount(context) == misuse + tried,
          "a player def and its binding");
    CHECK(mtwnCreatePlayer(context, &player, &id) == mtwn_success, "a player with them 0");

    tried = refused = 0;
    misuse = mtwnContext_GetMisuseCount(context);
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
    CHECK(refused == tried && mtwnContext_GetMisuseCount(context) == misuse + tried,
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

int main(void)
{
    TestDefs();
    TestValues();
    return s_failures == 0 ? 0 : 1;
}
