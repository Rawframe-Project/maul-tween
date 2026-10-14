// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Events (records mtwn-0004, mtwn-0013, mtwn-0038): a played clip's
// records, counted and ordered by the time of their first crossing; an
// hour's stall over an endless repeat in one record; backwards; jumping and
// sweeping seeks; cancel; transitions; a full queue; two players' order.

#include "test_harness.h"

#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct World
{
    mtwnContext* context;
    mtwnClipId clip;
    mtwnTargetId target;
} World;

static mtwnPosition At(double offset)
{
    mtwnPosition position;
    memset(&position, 0, sizeof position);
    position.offset = offset;
    return position;
}

static World MakeWorld(uint32_t events)
{
    World w = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    def.eventCapacity = events;
    CHECK(mtwnCreateContext(&def, &w.context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(w.context, &clipDef, &w.clip) == mtwn_success, "a clip");
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScalar, (mtwnVec4){0}, &w.target) == mtwn_success,
          "a target");
    return w;
}

static mtwnNode AddTrack(World* w, double start, double seconds, double iterations)
{
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.position = At(start);
    track.timing.duration = seconds;
    track.timing.iterations = iterations;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(w->context, w->clip, &track, &node) == mtwn_success, "a track");
    return node;
}

static mtwnPlayerId Play(World* w, bool keep, double rate)
{
    mtwnBinding binding = {0, 0, w->target};
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = w->clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.keep = keep;
    def.rate = rate;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(w->context, &def, &id) == mtwn_success, "a player");
    return id;
}

static void Update(World* w, double seconds)
{
    CHECK(mtwnUpdate(w->context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) == mtwn_success,
          "updated");
}

static uint32_t Read(World* w, mtwnEvent* events, uint32_t capacity)
{
    uint32_t n = 0;
    CHECK(mtwnReadEvents(w->context, events, capacity, &n) == mtwn_success, "read");
    return n;
}

static bool Is(const mtwnEvent* e, mtwnEventKind kind, mtwnNode node, double host, bool backwards)
{
    return e->kind == kind && e->node == node && fabs(e->hostTime - host) <= 1e-9 &&
           e->backwards == backwards;
}

static void TestPlayed(void)
{
    World w = MakeWorld(64);
    mtwnNode track = AddTrack(&w, 0.5, 1.0, 3.0);
    mtwnNode mark = 0;
    CHECK(mtwnClip_AddMark(w.context, w.clip, MTWN_ROOT_NODE, At(2.0), 9, &mark) == mtwn_success,
          "a mark at 2");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, false, 1.0);
    mtwnEvent e[16];
    Update(&w, 0.0);
    CHECK(Read(&w, e, 16) == 1 && Is(&e[0], mtwn_eventStart, MTWN_ROOT_NODE, 0.0, false) &&
              e[0].player.index1 == p.index1 && e[0].count == 1,
          "the clip starts as the player does");
    Update(&w, 1.0);
    CHECK(Read(&w, e, 16) == 1 && Is(&e[0], mtwn_eventStart, track, 0.5, false) &&
              fabs(e[0].clipTime - 0.5) <= 1e-15 && e[0].firstIteration == 0.0,
          "the track starts at 0.5, between the updates");
    Update(&w, 10.0);
    uint32_t n = Read(&w, e, 16);
    CHECK(n == 4, "four records");
    CHECK(Is(&e[0], mtwn_eventIteration, track, 1.5, false) && e[0].count == 2 &&
              e[0].firstIteration == 1.0 && e[0].lastIteration == 2.0,
          "two iterations in one record, from 1.5");
    CHECK(Is(&e[1], mtwn_eventMark, mark, 2.0, false) && e[1].key == 9, "the mark at 2");
    CHECK(Is(&e[2], mtwn_eventEnd, MTWN_ROOT_NODE, 3.5, false) &&
              Is(&e[3], mtwn_eventEnd, track, 3.5, false),
          "the ends at 3.5, the clip's order");
    CHECK(!mtwnPlayer_IsValid(w.context, p), "and the player gone");
    mtwnDestroyContext(w.context);
}

static void TestStall(void)
{
    World w = MakeWorld(16);
    mtwnNode track = AddTrack(&w, 0.0, 0.01, HUGE_VAL);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w, true, 1.0);
    Update(&w, 0.0);
    mtwnEvent e[16];
    Read(&w, e, 16);
    Update(&w, 3600.0);
    uint32_t n = Read(&w, e, 16);
    CHECK(n == 1 && e[0].kind == mtwn_eventIteration && e[0].node == track &&
              e[0].count >= 359999 && e[0].count <= 360000 && e[0].firstIteration == 1.0,
          "an hour over a 10 ms endless repeat: one record, counted");
    mtwnDestroyContext(w.context);
}

static void TestBackwards(void)
{
    World w = MakeWorld(16);
    mtwnNode track = AddTrack(&w, 1.0, 1.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, true, -1.0);
    Update(&w, 0.0);
    mtwnEvent e[16];
    uint32_t n = Read(&w, e, 16);
    CHECK(n == 2 && Is(&e[0], mtwn_eventStart, MTWN_ROOT_NODE, 0.0, true) &&
              Is(&e[1], mtwn_eventStart, track, 0.0, true) && e[1].clipTime == 2.0,
          "played backwards from the end: both start, going back");
    Update(&w, 1.5);
    n = Read(&w, e, 16);
    CHECK(n == 1 && Is(&e[0], mtwn_eventEnd, track, 1.0, true),
          "the track ends at its start, going back");
    Update(&w, 5.0);
    n = Read(&w, e, 16);
    CHECK(n == 1 && Is(&e[0], mtwn_eventEnd, MTWN_ROOT_NODE, 2.0, true) &&
              mtwnPlayer_IsValid(w.context, p),
          "and the clip at 0, kept");
    mtwnDestroyContext(w.context);
}

static void TestSeeks(void)
{
    World w = MakeWorld(16);
    mtwnNode track = AddTrack(&w, 1.0, 1.0, 1.0);
    mtwnNode mark = 0;
    CHECK(mtwnClip_AddMark(w.context, w.clip, MTWN_ROOT_NODE, At(3.0), 1, &mark) == mtwn_success,
          "a mark at 3");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, true, 1.0);
    Update(&w, 0.0);
    Update(&w, 0.5);
    mtwnEvent e[16];
    Read(&w, e, 16);
    CHECK(mtwnPlayer_Seek(w.context, p, 2.5) == mtwn_success, "a seek");
    uint32_t n = Read(&w, e, 16);
    CHECK(n == 1 && Is(&e[0], mtwn_eventSeek, MTWN_ROOT_NODE, 0.5, false) && e[0].clipTime == 2.5,
          "a seek record at once");
    Update(&w, 0.75);
    CHECK(Read(&w, e, 16) == 0, "the track it jumped over gave nothing");
    CHECK(mtwnPlayer_Sweep(w.context, p, 0.0) == mtwn_success, "a sweep back to 0");
    n = Read(&w, e, 16);
    CHECK(n == 4 && e[0].kind == mtwn_eventStart && e[0].node == MTWN_ROOT_NODE && e[0].backwards &&
              e[1].kind == mtwn_eventStart && e[1].node == track && e[1].backwards &&
              e[2].kind == mtwn_eventEnd && e[2].node == track && e[3].kind == mtwn_eventEnd &&
              e[3].node == MTWN_ROOT_NODE && e[3].clipTime == 0.0,
          "back from past the end: the clip's end, the track back, the clip's start");
    CHECK(mtwnPlayer_Finish(w.context, p) == mtwn_success, "finished by hand");
    n = Read(&w, e, 16);
    CHECK(n == 4 && e[0].node == MTWN_ROOT_NODE && e[0].kind == mtwn_eventStart &&
              e[3].kind == mtwn_eventEnd && e[3].node == track,
          "finishing crosses what lies before the end");
    CHECK(mtwnPlayer_Cancel(w.context, p) == mtwn_success, "canceled");
    n = Read(&w, e, 16);
    CHECK(n == 1 && e[0].kind == mtwn_eventCancel, "a cancel record");
    CHECK(mtwnPlayer_Cancel(w.context, p) == mtwn_success && Read(&w, e, 16) == 0,
          "an idle player's cancel makes none");
    mtwnDestroyContext(w.context);
}

static void TestTransitions(void)
{
    World w = MakeWorld(16);
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = w.target;
    def.value = (mtwnVec4){1.0f, 0, 0, 0};
    def.easing = mtwnLinearEasing();
    def.duration = 1.0;
    def.delay = 0.5;
    mtwnPlayerId t;
    CHECK(mtwnStartTransition(w.context, &def, &t) == mtwn_success, "a transition");
    mtwnEvent e[16];
    Update(&w, 10.0);
    CHECK(Read(&w, e, 16) == 0, "in its delay, nothing");
    Update(&w, 12.0);
    uint32_t n = Read(&w, e, 16);
    CHECK(n == 2 && Is(&e[0], mtwn_eventStart, 0, 10.5, false) &&
              Is(&e[1], mtwn_eventEnd, 0, 11.5, false) && e[1].player.index1 == t.index1,
          "its start after the delay and its end, both in one update");
    CHECK(mtwnStartTransition(w.context, &def, &t) == mtwn_success, "another");
    mtwnPlayerId second;
    CHECK(mtwnStartTransition(w.context, &def, &second) == mtwn_success, "replacing it");
    n = Read(&w, e, 16);
    CHECK(n == 1 && e[0].kind == mtwn_eventCancel && e[0].player.index1 == t.index1,
          "the replaced one canceled");
    mtwnDestroyContext(w.context);
}

static void TestQueue(void)
{
    World w = MakeWorld(2);
    AddTrack(&w, 0.5, 1.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w, true, 1.0);
    Update(&w, 0.0);
    Update(&w, 5.0);
    CHECK(mtwnContext_GetEventCount(w.context) == 2 && mtwnContext_GetDroppedEvents(w.context) == 2,
          "four records into two places: two dropped");
    mtwnEvent e[2];
    CHECK(Read(&w, e, 1) == 1 && e[0].node == MTWN_ROOT_NODE &&
              mtwnContext_GetEventCount(w.context) == 1,
          "read one at a time, the oldest first");
    uint32_t n = 0;
    CHECK(mtwnReadEvents(w.context, NULL, 1, &n) == mtwn_errorInvalid &&
              mtwnReadEvents(w.context, e, 2, NULL) == mtwn_errorInvalid &&
              mtwnReadEvents(NULL, e, 2, &n) == mtwn_errorInvalid,
          "refusals");
    CHECK(mtwnReadEvents(w.context, NULL, 0, &n) == mtwn_success && n == 0, "none asked");
    mtwnDestroyContext(w.context);
}

static void TestOrder(void)
{
    World w = MakeWorld(16);
    mtwnNode mark = 0;
    AddTrack(&w, 0.0, 4.0, 1.0);
    CHECK(mtwnClip_AddMark(w.context, w.clip, MTWN_ROOT_NODE, At(2.0), 1, &mark) == mtwn_success,
          "a mark at 2");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId slow = Play(&w, true, 1.0);
    mtwnPlayerId fast = Play(&w, true, 2.0);
    Update(&w, 0.0);
    mtwnEvent e[16];
    Read(&w, e, 16);
    Update(&w, 3.0);
    uint32_t n = Read(&w, e, 16);
    CHECK(n >= 2 && e[0].kind == mtwn_eventMark && e[0].player.index1 == fast.index1 &&
              fabs(e[0].hostTime - 1.0) <= 1e-9 && e[1].kind == mtwn_eventMark &&
              e[1].player.index1 == slow.index1,
          "by time across players: the faster one's mark first");
    mtwnDestroyContext(w.context);
}

int main(void)
{
    TestPlayed();
    TestStall();
    TestBackwards();
    TestSeeks();
    TestTransitions();
    TestQueue();
    TestOrder();
    return s_failures == 0 ? 0 : 1;
}
