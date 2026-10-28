// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A card's entrance as a clip: its backdrop fades in, then its title and
// body slide up one after another, staggered by positions, with a mark
// where a sound should play. A player shows it while the host reads the
// events each frame; reversed, it hides the card again, the events
// coming the other way. Returns 0 when every event came once each way,
// in order, and the card ends hidden.

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    // The card's three parts, each a binding slot.
    BACKDROP = 0,
    TITLE = 1,
    BODY = 2,
    // The mark's key.
    CHIME = 7
};

// A frame at 60 Hz, in nanoseconds.
static const uint64_t s_frame = 16666667u;

static int s_failed;

static void Expect(int ok, const char* what)
{
    if (!ok)
    {
        printf("FAIL: %s\n", what);
        s_failed = 1;
    }
}

static mtwnPosition After(mtwnPositionKind kind, double offset)
{
    mtwnPosition position;
    memset(&position, 0, sizeof position);
    position.kind = kind;
    position.offset = offset;
    return position;
}

// A track of two keyframes on a slot: from a value to another, eased.
static mtwnNode Track(mtwnContext* context, mtwnClipId clip, uint32_t slot, mtwnValueKind kind,
                      mtwnVec4 from, mtwnVec4 to, double seconds, mtwnPosition at)
{
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value = from;
    keys[1].value = to;
    keys[1].offset = 1.0;
    Expect(mtwnMakeCubicBezierEasing(0.2, 0.0, 0.0, 1.0, &keys[0].easing) == mtwn_success,
           "an easing");
    keys[1].easing = keys[0].easing;
    mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
    def.slot = slot;
    def.track.kind = kind;
    def.position = at;
    def.keyframes = keys;
    def.keyframeCount = 2;
    def.timing.duration = seconds;
    // Each part holds its first value before it starts and its last
    // after, whichever way the clip plays.
    def.timing.fill = mtwn_fillBoth;
    mtwnNode node = 0;
    Expect(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_success, "a track");
    return node;
}

static const char* KindName(mtwnEventKind kind)
{
    switch (kind)
    {
    case mtwn_eventStart:
        return "start";
    case mtwn_eventEnd:
        return "end";
    case mtwn_eventMark:
        return "mark";
    default:
        return "other";
    }
}

// Reads the frame's events; counts the chime and the ends of the body.
static void ReadEvents(mtwnContext* context, mtwnNode body, uint32_t* chimes, uint32_t* bodyEnds,
                       uint32_t* bodyStarts)
{
    mtwnEvent events[16];
    uint32_t count = 0;
    uint64_t dropped = 0;
    Expect(mtwnContext_ReadEvents(context, events, 16, &count, &dropped) == mtwn_success &&
               dropped == 0,
           "the events read");
    for (uint32_t i = 0; i < count; ++i)
    {
        const mtwnEvent* e = &events[i];
        printf("  %.3f s: %s of node %u%s\n", e->hostTime, KindName(e->kind), e->node,
               e->backwards ? ", backwards" : "");
        *chimes += e->kind == mtwn_eventMark && e->key == CHIME ? 1u : 0u;
        *bodyEnds += e->node == body && e->kind == mtwn_eventEnd ? 1u : 0u;
        *bodyStarts += e->node == body && e->kind == mtwn_eventStart ? 1u : 0u;
    }
}

int main(void)
{
    mtwnContext* context = NULL;
    mtwnContextDef def = mtwnDefaultContextDef();
    Expect(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    Expect(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    if (s_failed)
    {
        return 1;
    }
    // The backdrop fades in over 0.25 s; the title slides 24 units up
    // 0.1 s after it starts; the body follows the title's end, 0.15 s
    // before it.
    Track(context, clip, BACKDROP, mtwn_valueScalar, (mtwnVec4){0}, (mtwnVec4){1.0f, 0, 0, 0}, 0.25,
          After(mtwn_positionAbsolute, 0.0));
    Track(context, clip, TITLE, mtwn_valueVec2, (mtwnVec4){0, 24.0f, 0, 0}, (mtwnVec4){0}, 0.4,
          After(mtwn_positionWithPrevious, 0.1));
    mtwnNode body = Track(context, clip, BODY, mtwn_valueVec2, (mtwnVec4){0, 24.0f, 0, 0},
                          (mtwnVec4){0}, 0.4, After(mtwn_positionAfterPrevious, -0.15));
    mtwnNode chime = 0;
    Expect(mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, After(mtwn_positionAbsolute, 0.3), CHIME,
                            &chime) == mtwn_success,
           "a mark for the sound");
    Expect(mtwnClip_Seal(context, clip) == mtwn_success, "the clip sealed");
    double seconds = 0.0;
    Expect(mtwnClip_GetDuration(context, clip, &seconds) == mtwn_success, "its duration");
    printf("the entrance lasts %.3f s\n", seconds);

    mtwnTargetId parts[3];
    mtwnBinding bindings[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        Expect(mtwnCreateTarget(context, i == BACKDROP ? mtwn_valueScalar : mtwn_valueVec2,
                                (mtwnVec4){0}, &parts[i]) == mtwn_success,
               "a part");
        bindings[i] = mtwnBind(i, parts[i]);
    }
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindings = bindings;
    playerDef.bindingCount = 3;
    // Kept when finished, so it can play the card away again.
    playerDef.keep = true;
    mtwnPlayerId player;
    Expect(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success, "a player");

    uint32_t chimes = 0;
    uint32_t bodyEnds = 0;
    uint32_t bodyStarts = 0;
    printf("showing:\n");
    uint32_t frame = 0;
    for (; frame < 60; ++frame)
    {
        Expect(mtwnContext_Update(context, frame * s_frame, NULL, 0, NULL) == mtwn_success,
               "an update");
        ReadEvents(context, body, &chimes, &bodyEnds, &bodyStarts);
    }
    mtwnTargetState shown;
    Expect(mtwnTarget_GetState(context, parts[BODY], &shown) == mtwn_success &&
               shown.value.y == 0.0f,
           "the body in place");
    Expect(chimes == 1 && bodyEnds == 1 && bodyStarts == 1, "the chime and the body's ends once");

    // Hiding: the same clip backwards, a little faster.
    Expect(mtwnPlayer_SetRate(context, player, -1.5) == mtwn_success, "reversed");
    Expect(mtwnPlayer_Play(context, player) == mtwn_success, "played back");
    printf("hiding:\n");
    for (; frame < 120; ++frame)
    {
        Expect(mtwnContext_Update(context, frame * s_frame, NULL, 0, NULL) == mtwn_success,
               "an update");
        ReadEvents(context, body, &chimes, &bodyEnds, &bodyStarts);
    }
    mtwnTargetState backdrop;
    mtwnTargetState title;
    Expect(mtwnTarget_GetState(context, parts[BACKDROP], &backdrop) == mtwn_success &&
               mtwnTarget_GetState(context, parts[TITLE], &title) == mtwn_success,
           "read");
    Expect(backdrop.value.x == 0.0f && title.value.y == 24.0f, "the card hidden again");
    // Backwards, the body's end is crossed first, a start record, and its
    // start last, an end record: one more of each.
    Expect(chimes == 2 && bodyEnds == 2 && bodyStarts == 2, "each crossed once the other way");
    mtwnDestroyContext(context);
    return s_failed;
}
