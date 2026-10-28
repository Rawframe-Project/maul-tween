// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Events: a played clip's
// records, counted and ordered by the time of their first crossing; an
// hour's stall over an endless repeat in one record; backwards; jumping and
// sweeping seeks; cancel; transitions; a full queue; two players' order.

#include "test_harness.h"

#include "maul-tween/easing.h"
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
    mtwnBinding binding = mtwnBind(0, w->target);
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
    CHECK(mtwnContext_Update(w->context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) ==
              mtwn_success,
          "updated");
}

static uint32_t Read(World* w, mtwnEvent* events, uint32_t capacity)
{
    uint32_t n = 0;
    CHECK(mtwnContext_ReadEvents(w->context, events, capacity, &n, NULL) == mtwn_success, "read");
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
    mtwnNode label = 0;
    CHECK(mtwnClip_AddLabel(w.context, w.clip, MTWN_ROOT_NODE, At(1.0), 4, &label) == mtwn_success,
          "a label at 1, which gives nothing");
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

// A context of no events makes none and skips the sweeps:
// a mark and a track's iterations and ends crossed, nothing
// recorded or dropped, each frame's value that of a twin with a queue.
static void TestNoQueue(void)
{
    World twins[2] = {MakeWorld(0), MakeWorld(64)};
    for (int k = 0; k < 2; ++k)
    {
        (void)AddTrack(&twins[k], 0.5, 1.0, 3.0);
        mtwnNode mark = 0;
        CHECK(mtwnClip_AddMark(twins[k].context, twins[k].clip, MTWN_ROOT_NODE, At(2.0), 9,
                               &mark) == mtwn_success &&
                  mtwnClip_Finish(twins[k].context, twins[k].clip) == mtwn_success,
              "a mark at 2, finished");
        (void)Play(&twins[k], true, 1.0);
    }
    uint32_t differ = 0;
    for (int f = 0; f <= 50; ++f)
    {
        mtwnVec4 values[2];
        for (int k = 0; k < 2; ++k)
        {
            Update(&twins[k], 0.1 * f);
            CHECK(mtwnTarget_GetValue(twins[k].context, twins[k].target, &values[k]) ==
                      mtwn_success,
                  "read");
        }
        differ += values[0].x != values[1].x;
    }
    CHECK(differ == 0, "every frame's value as with a queue");
    uint64_t dropped = 7;
    uint32_t count = 7;
    CHECK(mtwnContext_ReadEvents(twins[0].context, NULL, 0, &count, &dropped) == mtwn_success &&
              count == 0 && dropped == 0 && mtwnContext_GetEventCount(twins[0].context) == 0,
          "nothing recorded, nothing dropped");
    CHECK(mtwnContext_GetEventCount(twins[1].context) > 0, "its twin's queue holds records");
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
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
    CHECK(mtwnPlayer_Seek(w.context, p, 1.0) == mtwn_success && Read(&w, e, 16) == 1 &&
              e[0].kind == mtwn_eventSeek && e[0].backwards,
          "a backward player's seek, backwards");
    mtwnDestroyContext(w.context);
}

// Iteration records at the time an iteration's start is reached, played
// backwards by the player or by the node's own rate: made at that time,
// they name the iteration left, which the target still shows there.
static void TestIterationLeftBackwards(void)
{
    static const double s_rates[2][2] = {{-1.0, 1.0}, {1.0, -1.0}};
    for (int i = 0; i < 2; ++i)
    {
        World w = MakeWorld(16);
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 1.0;
        keys[1].value.x = 1.0f;
        keys[0].easing = keys[1].easing = mtwnLinearEasing();
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 1.0;
        track.timing.iterations = 3.0;
        track.timing.playbackRate = s_rates[i][1];
        mtwnNode node = 0;
        CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
        CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
        (void)Play(&w, true, s_rates[i][0]);
        mtwnEvent e[16];
        Update(&w, 0.0);
        (void)Read(&w, e, 16);
        Update(&w, 1.0);
        uint32_t n = Read(&w, e, 16);
        mtwnVec4 value = {0};
        CHECK(mtwnTarget_GetValue(w.context, w.target, &value) == mtwn_success, "read");
        CHECK(n == 1 && Is(&e[0], mtwn_eventIteration, node, 1.0, i == 0) &&
                  e[0].firstIteration == 2.0 && value.x == 0.0f,
              "at the start left: the iteration left, shown at its start");
        Update(&w, 1.25);
        CHECK(Read(&w, e, 16) == 0 &&
                  mtwnTarget_GetValue(w.context, w.target, &value) == mtwn_success &&
                  value.x == 0.75f,
              "and the one before it after, with no second record");
        mtwnDestroyContext(w.context);
    }
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

static void TestRewindAndRates(void)
{
    World w = MakeWorld(16);
    mtwnNode mark = 0;
    AddTrack(&w, 0.0, 4.0, 1.0);
    CHECK(mtwnClip_AddMark(w.context, w.clip, MTWN_ROOT_NODE, At(1.0), 1, &mark) == mtwn_success,
          "a mark at 1");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, true, 2.0);
    mtwnEvent e[16];
    Update(&w, 0.0);
    Read(&w, e, 16);
    // The rate set now waits for the next update; the old one ran till then.
    CHECK(mtwnPlayer_SetRate(w.context, p, 1.0) == mtwn_success, "slower, pending");
    Update(&w, 1.0);
    uint32_t n = Read(&w, e, 16);
    CHECK(n == 1 && e[0].kind == mtwn_eventMark && fabs(e[0].hostTime - 0.5) <= 1e-12,
          "the mark crossed at 0.5, at the rate that ran");
    Update(&w, 10.0);
    Read(&w, e, 16);
    CHECK(mtwnPlayer_Play(w.context, p) == mtwn_success, "played again from its end");
    Update(&w, 11.0);
    n = Read(&w, e, 16);
    CHECK(n == 2 && e[0].kind == mtwn_eventStart && !e[0].backwards &&
              e[0].node == MTWN_ROOT_NODE && e[1].kind == mtwn_eventStart && !e[1].backwards,
          "rewound: it starts afresh, nothing crossed back");
    mtwnDestroyContext(w.context);
}

// A player made at rate 0 sweeps its first time forwards, as one at a
// positive rate does: given rate 1 later, it does not turn, so what lay at
// its first time is not crossed again.
static void TestRateZeroFirst(void)
{
    World w = MakeWorld(16);
    AddTrack(&w, 0.0, 4.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, true, 0.0);
    mtwnEvent e[16];
    Update(&w, 0.0);
    CHECK(Read(&w, e, 16) == 2 && e[0].kind == mtwn_eventStart && !e[0].backwards,
          "held at 0, the clip and its track start");
    CHECK(mtwnPlayer_SetRate(w.context, p, 1.0) == mtwn_success, "rate 1, pending");
    Update(&w, 1.0);
    Update(&w, 2.0);
    uint32_t n = Read(&w, e, 16);
    bool again = false;
    for (uint32_t i = 0; i < n; ++i)
    {
        again = again || e[i].kind == mtwn_eventStart;
    }
    CHECK(!again, "moving on, nothing at 0 starts again");
    mtwnDestroyContext(w.context);
}

// Counts as large as a record holds: 2^64 - 2048 crossings, the largest
// double below 2^64, exactly; 2^65 and 2^64 at UINT64_MAX.
static void TestCountAtTheTop(void)
{
    World w = MakeWorld(16);
    mtwnNode track = AddTrack(&w, 0.0, 0x1p-60, 0x1p67);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, true, 1.0);
    CHECK(mtwnPlayer_Pause(w.context, p) == mtwn_success, "paused");
    mtwnEvent e[16];
    Update(&w, 0.0);
    Read(&w, e, 16);
    CHECK(mtwnPlayer_Sweep(w.context, p, 16.0 - 0x1p-49) == mtwn_success, "swept");
    uint32_t n = Read(&w, e, 16);
    CHECK(n == 1 && e[0].kind == mtwn_eventIteration && e[0].node == track &&
              e[0].count == 18446744073709549568u,
          "2^64 - 2048 iterations, exactly");
    CHECK(mtwnPlayer_Sweep(w.context, p, 48.0) == mtwn_success, "swept on");
    n = Read(&w, e, 16);
    CHECK(n == 1 && e[0].kind == mtwn_eventIteration && e[0].count == UINT64_MAX,
          "2^65 more, as many as a record holds");
    CHECK(mtwnPlayer_Sweep(w.context, p, 64.0) == mtwn_success, "swept to 64");
    n = Read(&w, e, 16);
    CHECK(n == 1 && e[0].count == UINT64_MAX, "2^64 exactly, one too many");
    mtwnDestroyContext(w.context);
}

static void TestSameTime(void)
{
    World w = MakeWorld(16);
    mtwnNode mark = 0;
    AddTrack(&w, 0.0, 4.0, 1.0);
    CHECK(mtwnClip_AddMark(w.context, w.clip, MTWN_ROOT_NODE, At(1.0), 1, &mark) == mtwn_success,
          "a mark at 1");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId first = Play(&w, true, 1.0);
    mtwnEvent e[16];
    Update(&w, 0.0);
    Read(&w, e, 16);
    mtwnPlayerId second = Play(&w, true, 1.0);
    // At 1 the first crosses its mark as the second starts.
    Update(&w, 1.0);
    uint32_t n = Read(&w, e, 16);
    CHECK(n == 3 && e[0].player.index1 == first.index1 && e[0].kind == mtwn_eventMark &&
              e[1].player.index1 == second.index1 && e[1].kind == mtwn_eventStart,
          "one time: the first player's records first, whatever their place");
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
    CHECK(mtwnContext_ReadEvents(w.context, NULL, 1, &n, NULL) == mtwn_errorInvalid &&
              mtwnContext_ReadEvents(w.context, e, 2, NULL, NULL) == mtwn_errorInvalid &&
              mtwnContext_ReadEvents(NULL, e, 2, &n, NULL) == mtwn_errorInvalid,
          "refusals");
    CHECK(mtwnContext_ReadEvents(w.context, NULL, 0, &n, NULL) == mtwn_success && n == 0,
          "none asked");
    mtwnDestroyContext(w.context);
}

static mtwnNode Endless(World* w, mtwnNode parent, double duration)
{
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.parent = parent;
    group.autoDuration = false;
    group.timing.duration = duration;
    group.timing.iterations = HUGE_VAL;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTimeline(w->context, w->clip, &group, &node) == mtwn_success, "endless");
    return node;
}

static void TestOverflow(void)
{
    // Root > a 1 s endless repeat > a 0.25 s endless repeat, a mark in each,
    // a 50 ms loop inside: an hour's sweep makes seven records (the outer
    // iterations, the inner one restarting and iterating, the marks, the
    // loop restarting and iterating), counted, into a queue of four.
    World w = MakeWorld(4);
    mtwnNode outer = Endless(&w, MTWN_ROOT_NODE, 1.0);
    mtwnNode inner = Endless(&w, outer, 0.25);
    mtwnNode outerMark = 0;
    mtwnNode innerMark = 0;
    CHECK(mtwnClip_AddMark(w.context, w.clip, outer, At(0.5), 1, &outerMark) == mtwn_success &&
              mtwnClip_AddMark(w.context, w.clip, inner, At(0.1), 2, &innerMark) == mtwn_success,
          "marks");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.parent = inner;
    track.timing.duration = 0.05;
    track.timing.iterations = HUGE_VAL;
    mtwnNode loop = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &loop) == mtwn_success, "a loop");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w, true, 1.0);
    Update(&w, 0.0);
    mtwnEvent e[8];
    uint32_t n = 0;
    uint64_t dropped = 0;
    CHECK(mtwnContext_ReadEvents(w.context, e, 8, &n, &dropped) == mtwn_success && dropped == 0,
          "the starts fit");
    Update(&w, 3600.0);
    CHECK(mtwnContext_GetEventCount(w.context) == 4, "the queue full");
    CHECK(mtwnContext_ReadEvents(w.context, e, 8, &n, &dropped) == mtwn_success && n == 4 &&
              dropped == 3 && mtwnContext_GetDroppedEvents(w.context) == 3,
          "three past its capacity: dropped, and reported at the read");
    bool sorted = true;
    for (uint32_t i = 1; i < n; ++i)
    {
        sorted = sorted && e[i - 1].hostTime <= e[i].hostTime;
    }
    CHECK(sorted, "the first four made are kept, sorted");
    CHECK(e[0].kind == mtwn_eventIteration && e[0].node == inner && e[0].count == 14400 &&
              e[1].kind == mtwn_eventMark && e[1].node == outerMark && e[1].count == 3600,
          "the inner repeat's iterations counted four times a second, the outer mark once");
    CHECK(mtwnContext_ReadEvents(w.context, e, 8, &n, &dropped) == mtwn_success && n == 0 &&
              dropped == 0,
          "the drops reported once");
    (void)loop;
    (void)innerMark;
    mtwnDestroyContext(w.context);
}

static void TestIterationStart(void)
{
    // One iteration from half way through: the iteration changes half
    // way, an iteration record, though its count is 1.
    World w = MakeWorld(16);
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    track.timing.iterationStart = 0.5;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w, true, 1.0);
    Update(&w, 0.0);
    mtwnEvent e[8];
    (void)Read(&w, e, 8);
    Update(&w, 0.25);
    CHECK(Read(&w, e, 8) == 0, "nothing before its iteration changes");
    Update(&w, 0.75);
    CHECK(Read(&w, e, 8) == 1 && Is(&e[0], mtwn_eventIteration, node, 0.5, false) &&
              e[0].firstIteration == 1.0,
          "its iteration 1 begins half way");
    mtwnDestroyContext(w.context);
}

// The determinism scenario's nesting: a timeline of 1.5 s, 4 iterations
// alternating, and in it one of 0.4 s repeating without end from a
// quarter way. In that, tracks with their iterations at their start and
// end: 0.3 s, 3 iterations, which never ends within the 0.4 s (0, 2);
// 0.2 s, 2 iterations, which ends where it repeats (0, 1); 0.1 s, 2
// iterations, after 0.1 s (0, 1); and of no length, 3 iterations, after
// 0.05 s, in its after phase at its start too (2, 2).
enum
{
    NESTED_TRACKS = 4
};

static const double s_nestedTiming[NESTED_TRACKS][3] = {
    {0.3, 3.0, 0.0}, {0.2, 2.0, 0.0}, {0.1, 2.0, 0.1}, {0.0, 3.0, 0.05}};
static const double s_nestedIterations[NESTED_TRACKS][2] = {{0, 2}, {0, 1}, {0, 1}, {2, 2}};

static void NestedTracks(World* w, mtwnNode tracks[NESTED_TRACKS])
{
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = 1.5;
    group.timing.iterations = 4.0;
    group.timing.direction = mtwn_directionAlternate;
    mtwnNode outer = 0;
    CHECK(mtwnClip_AddTimeline(w->context, w->clip, &group, &outer) == mtwn_success, "outer");
    group.parent = outer;
    group.timing.duration = 0.4;
    group.timing.iterations = HUGE_VAL;
    group.timing.direction = mtwn_directionNormal;
    group.timing.iterationStart = 0.25;
    mtwnNode inner = 0;
    CHECK(mtwnClip_AddTimeline(w->context, w->clip, &group, &inner) == mtwn_success, "inner");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = inner;
    track.keyframes = keys;
    track.keyframeCount = 2;
    for (int k = 0; k < NESTED_TRACKS; ++k)
    {
        track.timing.duration = s_nestedTiming[k][0];
        track.timing.iterations = s_nestedTiming[k][1];
        track.timing.delay = s_nestedTiming[k][2];
        CHECK(mtwnClip_AddTrack(w->context, w->clip, &track, &tracks[k]) == mtwn_success,
              "a track");
    }
    CHECK(mtwnClip_Finish(w->context, w->clip) == mtwn_success, "finished");
}

// A track in a repeating timeline in another: the record of one crossing
// has one iteration, first and last alike. Searched apart, the last can
// land a double before the first, in the iteration before.
static void TestOneCrossingIterations(void)
{
    World w = MakeWorld(64);
    mtwnNode tracks[NESTED_TRACKS];
    NestedTracks(&w, tracks);
    Play(&w, true, 1.0);
    uint32_t ones = 0;
    uint32_t apart = 0;
    for (int frame = 0; frame <= 600; ++frame)
    {
        Update(&w, frame * 0.0113);
        mtwnEvent e[64];
        uint32_t n = Read(&w, e, 64);
        for (uint32_t i = 0; i < n; ++i)
        {
            ones += e[i].count == 1 ? 1u : 0u;
            apart += e[i].count == 1 && e[i].firstIteration != e[i].lastIteration ? 1u : 0u;
        }
    }
    if (apart != 0)
    {
        printf("  %u of %u single crossings with two iterations\n", apart, ones);
    }
    CHECK(ones > 40 && apart == 0, "a single crossing's iterations alike");
    mtwnDestroyContext(w.context);
}

// The iteration a nested track's record of its active start or end should
// have, or -1 for another record.
static double EdgeIteration(const mtwnEvent* e, const mtwnNode tracks[NESTED_TRACKS])
{
    // A start crossed backwards is an end record, an end a start.
    bool start = (e->kind == mtwn_eventStart) != e->backwards;
    bool edge = e->kind == mtwn_eventStart || e->kind == mtwn_eventEnd;
    for (int k = 0; k < NESTED_TRACKS; ++k)
    {
        if (edge && e->node == tracks[k])
        {
            return s_nestedIterations[k][start ? 0 : 1];
        }
    }
    return -1.0;
}

// A start or end record has the node's iteration at its active start or
// end, at every crossing, played in short frames and in frames of 0.7 s,
// which cross each several times. Where the inner timeline repeats, the
// iteration is not looked up on its far side, where an end would have
// the first and a start crossed backwards the second.
static void TestEdgeIterations(void)
{
    static const double frames[2] = {0.0113, 0.7};
    uint32_t edges = 0;
    uint32_t wrong = 0;
    for (int f = 0; f < 2; ++f)
    {
        World w = MakeWorld(64);
        mtwnNode tracks[NESTED_TRACKS];
        NestedTracks(&w, tracks);
        Play(&w, true, 1.0);
        for (double t = 0.0; t <= 6.5; t += frames[f])
        {
            Update(&w, t);
            mtwnEvent e[64];
            uint32_t n = Read(&w, e, 64);
            for (uint32_t i = 0; i < n; ++i)
            {
                double expected = EdgeIteration(&e[i], tracks);
                edges += expected >= 0.0 ? 1u : 0u;
                wrong += expected >= 0.0 &&
                                 (e[i].firstIteration != expected || e[i].lastIteration != expected)
                             ? 1u
                             : 0u;
            }
        }
        mtwnDestroyContext(w.context);
    }
    if (wrong != 0)
    {
        printf("  %u of %u starts and ends in another iteration\n", wrong, edges);
    }
    CHECK(edges > 80 && wrong == 0, "starts and ends in their own iterations");
}

// A timeline of the root's with a track at its start, played in frames
// of 1/64 s, the timeline's delay and length on them; returns the track.
static mtwnNode HeldEdges(World* w, double delay, double iterations)
{
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = 0.46875;
    group.timing.iterations = iterations;
    group.timing.delay = delay;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w->context, w->clip, &group, &timeline) == mtwn_success,
          "a timeline");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = timeline;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 0.25;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(w->context, w->clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w->context, w->clip) == mtwn_success, "finished");
    return node;
}

// The count of a node's records of a kind and way over frames of 1/64 s.
static uint64_t CountOverFrames(World* w, mtwnNode node, mtwnEventKind kind, bool backwards,
                                int frames)
{
    uint64_t n = 0;
    for (int f = 0; f <= frames; ++f)
    {
        Update(w, f / 64.0);
        mtwnEvent e[16];
        uint32_t read = Read(w, e, 16);
        for (uint32_t i = 0; i < read; ++i)
        {
            n += e[i].node == node && e[i].kind == kind && e[i].backwards == backwards ? e[i].count
                                                                                       : 0u;
        }
    }
    return n;
}

// A frame ending exactly where a held time starts to move crosses what
// lies at the held time: a track at the start of a timeline delayed by
// 0.25 s starts in the frame landing on 0.25 s; played backwards from
// 2.5 s, a timeline held after its end at 1.40625 s ends, backwards, in
// the frame landing there. Both were lost (found playing clips in two
// cuts of time).
static void TestHoldEdges(void)
{
    World w = MakeWorld(16);
    mtwnNode track = HeldEdges(&w, 0.25, 1.0);
    Play(&w, true, 1.0);
    CHECK(CountOverFrames(&w, track, mtwn_eventStart, false, 32) == 1,
          "the track starts as the timeline's delay ends");
    mtwnDestroyContext(w.context);
    w = MakeWorld(16);
    (void)HeldEdges(&w, 0.0, 3.0);
    mtwnPlayerId player = Play(&w, true, -1.0);
    CHECK(mtwnPlayer_Seek(w.context, player, 2.5) == mtwn_success, "sought");
    // The timeline is node 2; its end crossed backwards records a start.
    CHECK(CountOverFrames(&w, 2, mtwn_eventStart, true, 80) == 1,
          "the timeline's end crossed backwards where it was held");
    mtwnDestroyContext(w.context);
}

static void TestSharedRecords(void)
{
    // Two tracks in a row of one timing: the sweep gives the second the
    // first's records. Each has its own start, iterations and end, the
    // same but for the node.
    World w = MakeWorld(64);
    mtwnNode first = AddTrack(&w, 0.5, 1.0, 3.0);
    mtwnNode second = AddTrack(&w, 0.5, 1.0, 3.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w, true, 1.0);
    mtwnEvent all[64];
    uint32_t total = 0;
    for (int f = 0; f <= 40; ++f)
    {
        Update(&w, f * 0.1);
        total += Read(&w, all + total, 64 - total);
    }
    mtwnEvent a[16];
    mtwnEvent b[16];
    uint32_t na = 0;
    uint32_t nb = 0;
    for (uint32_t i = 0; i < total; ++i)
    {
        if (all[i].node == first && na < 16)
        {
            a[na++] = all[i];
        }
        else if (all[i].node == second && nb < 16)
        {
            b[nb++] = all[i];
        }
    }
    bool same = na == nb && na >= 4;
    for (uint32_t i = 0; same && i < na; ++i)
    {
        same = a[i].kind == b[i].kind && a[i].clipTime == b[i].clipTime &&
               a[i].count == b[i].count && a[i].firstIteration == b[i].firstIteration &&
               a[i].lastIteration == b[i].lastIteration;
    }
    CHECK(same, "each track its own records, the same but for the node");
    CHECK(na >= 1 && a[0].kind == mtwn_eventStart && a[na - 1].kind == mtwn_eventEnd,
          "from its start to its end");
    mtwnDestroyContext(w.context);
}

static void TestSettledBoundary(void)
{
    // A repeating track's time ending on, and turning at, an iteration's
    // start: its iteration is the same at both ends of each sweep, yet the
    // start is reached, so the count runs and the iteration is recorded.
    World w = MakeWorld(16);
    mtwnNode track = AddTrack(&w, 0.0, 1.0, 3.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, true, 1.0);
    CHECK(mtwnPlayer_Pause(w.context, p) == mtwn_success, "paused");
    Update(&w, 0.5);
    mtwnEvent e[8];
    (void)Read(&w, e, 8);
    CHECK(mtwnPlayer_Sweep(w.context, p, 1.5) == mtwn_success, "a sweep to 1.5");
    (void)Read(&w, e, 8);
    CHECK(mtwnPlayer_Sweep(w.context, p, 1.0) == mtwn_success, "back to 1");
    CHECK(Read(&w, e, 8) == 1 && e[0].kind == mtwn_eventIteration && e[0].node == track &&
              e[0].backwards && e[0].clipTime == 1.0 && e[0].firstIteration == 1.0,
          "the iteration's start reached backwards");
    CHECK(mtwnPlayer_Sweep(w.context, p, 1.25) == mtwn_success, "turned to 1.25");
    CHECK(Read(&w, e, 8) == 1 && e[0].kind == mtwn_eventIteration && e[0].node == track &&
              !e[0].backwards && e[0].clipTime == 1.0,
          "turning there takes the start again");
    mtwnDestroyContext(w.context);
}

// Counts the records of a kind for a node in a batch.
static uint32_t CountKind(const mtwnEvent* e, uint32_t n, mtwnEventKind kind, mtwnNode node)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        count += e[i].kind == kind && e[i].node == node ? 1u : 0u;
    }
    return count;
}

static void TestSettledReversedParent(void)
{
    // A track repeating three times in a timeline played backwards: the
    // parent's child time falls as the player's rises, so the track moves
    // backwards through its iterations. A sweep inside an iteration is
    // settled; one that reaches an iteration's start is not.
    World w = MakeWorld(16);
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = true;
    group.timing.playbackRate = -1.0;
    group.timing.fill = mtwn_fillBoth;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &group, &timeline) == mtwn_success, "a timeline");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
    def.parent = timeline;
    def.keyframes = keys;
    def.keyframeCount = 2;
    def.timing.duration = 1.0;
    def.timing.iterations = 3.0;
    mtwnNode track = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &def, &track) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, true, 1.0);
    CHECK(mtwnPlayer_Pause(w.context, p) == mtwn_success, "paused");
    Update(&w, 0.5);
    mtwnEvent e[8];
    CHECK(mtwnPlayer_Seek(w.context, p, 0.5) == mtwn_success, "at 0.5");
    (void)Read(&w, e, 8);
    // Child time 2.5 to 2.1: inside the third iteration.
    CHECK(mtwnPlayer_Sweep(w.context, p, 0.9) == mtwn_success, "a sweep to 0.9");
    uint32_t n = Read(&w, e, 8);
    CHECK(CountKind(e, n, mtwn_eventIteration, track) == 0, "settled inside an iteration");
    // Child time 2.1 to 2.0: the third iteration's start, reached.
    CHECK(mtwnPlayer_Sweep(w.context, p, 1.0) == mtwn_success, "a sweep to 1");
    n = Read(&w, e, 8);
    CHECK(CountKind(e, n, mtwn_eventIteration, track) == 1, "the iteration's start reached");
    // On from it the same way: not reached again.
    CHECK(mtwnPlayer_Sweep(w.context, p, 1.25) == mtwn_success, "a sweep to 1.25");
    n = Read(&w, e, 8);
    CHECK(CountKind(e, n, mtwn_eventIteration, track) == 0, "not again from it");
    // A seek landing exactly on a boundary (child time 1), then a sweep
    // on: the boundary is where it starts, not crossed; turning back to it
    // reaches it.
    CHECK(mtwnPlayer_Seek(w.context, p, 2.0) == mtwn_success, "at 2");
    (void)Read(&w, e, 8);
    CHECK(mtwnPlayer_Sweep(w.context, p, 2.4) == mtwn_success, "a sweep to 2.4");
    n = Read(&w, e, 8);
    CHECK(CountKind(e, n, mtwn_eventIteration, track) == 0, "a seek's boundary not crossed");
    CHECK(mtwnPlayer_Sweep(w.context, p, 2.0) == mtwn_success, "back to 2");
    n = Read(&w, e, 8);
    CHECK(CountKind(e, n, mtwn_eventIteration, track) == 1, "turning back to it, reached");
    mtwnDestroyContext(w.context);
}

static void TestBoundaryPlay(void)
{
    // From a random clip that once lost a record: a timeline sized by its
    // track, three iterations after a delay, the track's end its
    // iteration's end. A player running backwards starts at the clip's
    // end, where the timeline's overall progress rounds to 3 while still
    // active: its iteration reads 3 at child time 0, yet the place is the
    // third iteration's end, where the track ends. Its end is passed,
    // backwards: a start record.
    World w = MakeWorld(16);
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.position = At(0.30538823604583737);
    group.autoDuration = true;
    group.timing.delay = 0.19598060846328735;
    group.timing.iterations = 3.0;
    group.timing.fill = mtwn_fillBoth;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &group, &timeline) == mtwn_success, "a timeline");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = timeline;
    track.position = At(0.51963862180709841);
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 0.64215481877326963;
    track.timing.iterations = 3.0;
    track.timing.direction = mtwn_directionAlternateReverse;
    track.timing.fill = mtwn_fillBackwards;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    // Played at a host time of 10.192 s, as the random clip was: the play
    // resolves its start time there, and its first time lands one ulp
    // short of the clip's end.
    mtwnBinding binding = mtwnBind(0, w.target);
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = w.clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.keep = true;
    def.play = false;
    def.rate = -1.6416786193847657;
    mtwnPlayerId p = {0, 0};
    CHECK(mtwnCreatePlayer(w.context, &def, &p) == mtwn_success, "a player");
    Update(&w, 10.179);
    CHECK(mtwnPlayer_Play(w.context, p) == mtwn_success, "played from its start, so its end");
    Update(&w, 10.192);
    mtwnEvent e[8];
    uint32_t n = Read(&w, e, 8);
    bool started = false;
    for (uint32_t i = 0; i < n; ++i)
    {
        started = started || (e[i].kind == mtwn_eventStart && e[i].node == node && e[i].backwards);
    }
    CHECK(started, "the track's end passed backwards at the boundary");
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

// One clip played to the same time by three paths of frames: each
// record's time may be another double where its count flips
//  but lies within 1e-9 s of the others', and two boundaries
// 1e-12 s apart, thousands of doubles, keep their order.
static void TestPaths(void)
{
    double ends[3] = {0.0, 0.0, 0.0};
    double starts[3] = {0.0, 0.0, 0.0};
    bool ordered = true;
    for (int path = 0; path < 3; ++path)
    {
        World w = MakeWorld(64);
        // The later boundary's track first, so its record is made first
        // and only the order by time puts the earlier one ahead.
        mtwnNode second = AddTrack(&w, 1.0 + 1e-12, 0.5, 1.0);
        mtwnNode first = AddTrack(&w, 0.0, 1.0, 1.0);
        CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
        mtwnPlayerId p = Play(&w, true, 1.0);
        mtwnEvent all[64];
        uint32_t n = 0;
        if (path == 0)
        {
            // Played in steps of 16 ms.
            for (int f = 0; f <= 125; ++f)
            {
                Update(&w, f * 0.016);
                n += Read(&w, all + n, 64 - n);
            }
        }
        else if (path == 1)
        {
            // Two updates: the whole time in one sweep.
            Update(&w, 0.0);
            Update(&w, 2.0);
            n = Read(&w, all, 64);
        }
        else
        {
            // Scrubbed by uneven sweeps.
            Update(&w, 0.0);
            CHECK(mtwnPlayer_Pause(w.context, p) == mtwn_success, "paused");
            const double to[4] = {0.37, 0.999999999999, 1.3, 2.0};
            for (int k = 0; k < 4; ++k)
            {
                CHECK(mtwnPlayer_Sweep(w.context, p, to[k]) == mtwn_success, "a sweep");
                n += Read(&w, all + n, 64 - n);
            }
        }
        int end = -1;
        int start = -1;
        for (uint32_t i = 0; i < n; ++i)
        {
            end = all[i].kind == mtwn_eventEnd && all[i].node == first ? (int)i : end;
            start = all[i].kind == mtwn_eventStart && all[i].node == second ? (int)i : start;
        }
        CHECK(end >= 0 && start >= 0, "both records made");
        if (end >= 0 && start >= 0)
        {
            ordered = ordered && end < start;
            ends[path] = all[end].clipTime;
            starts[path] = all[start].clipTime;
        }
        mtwnDestroyContext(w.context);
    }
    CHECK(ordered, "boundaries 1e-12 s apart keep their order on every path");
    bool near = true;
    for (int path = 1; path < 3; ++path)
    {
        near = near && fabs(ends[path] - ends[0]) <= 1e-9 && fabs(starts[path] - starts[0]) <= 1e-9;
    }
    CHECK(near, "a record's time within 1e-9 s whatever the path");
}

// A timeline eased by steps that jump at their start: from its first
// instant its child time is a quarter in, so a mark before it is never
// crossed, and one at 0.3 is when the second step lands on a half (a
// timeline's iteration starts in its active phase, not its before one).
static void TestJumpStartMarks(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    CHECK(mtwnMakeStepsEasing(4, mtwn_jumpStart, &group.timing.easing) == mtwn_success, "steps");
    group.timing.duration = 1.0;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &group, &timeline) == mtwn_success, "a timeline");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    track.parent = timeline;
    mtwnNode node = 0;
    mtwnNode before = 0;
    mtwnNode after = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success &&
              mtwnClip_AddMark(w.context, w.clip, timeline, At(0.1), 1, &before) == mtwn_success &&
              mtwnClip_AddMark(w.context, w.clip, timeline, At(0.3), 2, &after) == mtwn_success &&
              mtwnClip_Finish(w.context, w.clip) == mtwn_success,
          "two marks");
    (void)Play(&w, true, 1.0);
    Update(&w, 0.0);
    Update(&w, 0.7);
    mtwnEvent events[16];
    uint32_t n = Read(&w, events, 16);
    int marks = 0;
    bool early = false;
    for (uint32_t i = 0; i < n; ++i)
    {
        marks += events[i].kind == mtwn_eventMark;
        early = early || (events[i].kind == mtwn_eventMark && events[i].node == before);
        if (events[i].kind == mtwn_eventMark && events[i].node == after)
        {
            CHECK(fabs(events[i].clipTime - 0.25) <= 1e-9, "the mark at 0.3 as the step lands");
        }
    }
    CHECK(marks == 1 && !early, "the mark before the first step never crossed");
    mtwnDestroyContext(w.context);
}

// A player finished by reduced motion with a rate change pending, then
// reversed: no time ran for it since, so its run starts at the update
// after the reversal, not at the one it finished in.
static void TestReverseAfterReducedFinish(void)
{
    World w = MakeWorld(64);
    (void)AddTrack(&w, 0.0, 1.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, true, 1.0);
    mtwnEvent e[16];
    Update(&w, 0.0);
    Update(&w, 0.1);
    CHECK(mtwnPlayer_SetRate(w.context, p, 1.5) == mtwn_success &&
              mtwnContext_SetReducedMotion(w.context, true) == mtwn_success,
          "a rate pending, reduced motion");
    Update(&w, 0.25);
    (void)Read(&w, e, 16);
    CHECK(mtwnContext_SetReducedMotion(w.context, false) == mtwn_success &&
              mtwnPlayer_Reverse(w.context, p) == mtwn_success,
          "reversed");
    Update(&w, 0.5);
    CHECK(Read(&w, e, 16) == 2 && Is(&e[0], mtwn_eventStart, MTWN_ROOT_NODE, 0.5, true),
          "the reversed run starts at the update after it");
    mtwnDestroyContext(w.context);
}

// A timeline of duration 0 holds its children's time at 0: a timeline
// placed later in it never starts, nor does a track in that one.
static void TestUnderAZeroTimeline(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef outer = mtwnDefaultClipTimelineDef();
    outer.parent = MTWN_ROOT_NODE;
    outer.position = At(0.1);
    outer.autoDuration = false;
    outer.timing.duration = 0.0;
    mtwnNode zero = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &outer, &zero) == mtwn_success, "a timeline");
    mtwnClipTimelineDef inner = mtwnDefaultClipTimelineDef();
    inner.parent = zero;
    inner.position = At(0.2);
    inner.autoDuration = false;
    inner.timing.duration = 1.0;
    mtwnNode later = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &inner, &later) == mtwn_success, "another");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = later;
    track.position = At(0.1);
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 0.5;
    mtwnNode held = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &held) == mtwn_success, "a track in it");
    (void)AddTrack(&w, 0.0, 2.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    (void)Play(&w, false, 1.0);
    uint32_t found = 0;
    for (int k = 0; k <= 6; ++k)
    {
        Update(&w, 0.5 * k);
        mtwnEvent e[16];
        uint32_t n = Read(&w, e, 16);
        for (uint32_t i = 0; i < n; ++i)
        {
            found += e[i].node == later || e[i].node == held ? 1u : 0u;
        }
    }
    CHECK(found == 0, "no record of what lies past a zero timeline's time");
    mtwnDestroyContext(w.context);
}

// An endless timeline from 0.25, a track and a mark at its start: all
// three are crossed there.
static void TestEndlessTimelineStart(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef endless = mtwnDefaultClipTimelineDef();
    endless.parent = MTWN_ROOT_NODE;
    endless.position = At(0.25);
    endless.autoDuration = false;
    endless.timing.duration = HUGE_VAL;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &endless, &timeline) == mtwn_success,
          "an endless timeline");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = timeline;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 0.5;
    mtwnNode inner = 0;
    mtwnNode mark = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &inner) == mtwn_success &&
              mtwnClip_AddMark(w.context, w.clip, timeline, At(0.0), 3, &mark) == mtwn_success &&
              mtwnClip_Finish(w.context, w.clip) == mtwn_success,
          "a track and a mark at its start");
    (void)Play(&w, true, 1.0);
    Update(&w, 0.0);
    mtwnEvent e[16];
    (void)Read(&w, e, 16);
    Update(&w, 0.5);
    uint32_t n = Read(&w, e, 16);
    uint32_t found = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        found += Is(&e[i], mtwn_eventStart, timeline, 0.25, false) ? 1u : 0u;
        found += Is(&e[i], mtwn_eventStart, inner, 0.25, false) ? 1u : 0u;
        found += Is(&e[i], mtwn_eventMark, mark, 0.25, false) && e[i].key == 3 ? 1u : 0u;
    }
    CHECK(found == 3, "the timeline's start, the track's and the mark, at 0.25");
    mtwnDestroyContext(w.context);
}

// Zero timelines nested three deep, the innermost placed past its
// parent's time: a mark at its start is never crossed.
static void TestUnderZeroTimelines(void)
{
    World w = MakeWorld(64);
    const double at[3] = {0.1, 0.02, 0.25};
    mtwnNode parent = MTWN_ROOT_NODE;
    for (int depth = 0; depth < 3; ++depth)
    {
        mtwnClipTimelineDef zero = mtwnDefaultClipTimelineDef();
        zero.parent = parent;
        zero.position = At(at[depth]);
        zero.autoDuration = false;
        zero.timing.duration = 0.0;
        CHECK(mtwnClip_AddTimeline(w.context, w.clip, &zero, &parent) == mtwn_success,
              "a zero timeline");
    }
    mtwnNode mark = 0;
    CHECK(mtwnClip_AddMark(w.context, w.clip, parent, At(0.0), 5, &mark) == mtwn_success,
          "a mark at the innermost's start");
    (void)AddTrack(&w, 0.0, 2.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    (void)Play(&w, false, 1.0);
    uint32_t found = 0;
    for (int k = 0; k <= 6; ++k)
    {
        Update(&w, 0.5 * k);
        mtwnEvent e[16];
        uint32_t n = Read(&w, e, 16);
        for (uint32_t i = 0; i < n; ++i)
        {
            found += e[i].node == mark ? 1u : 0u;
        }
    }
    CHECK(found == 0, "no record of a mark past the zero timelines' time");
    mtwnDestroyContext(w.context);
}

// A mark in a zero timeline of two iterations, crossed going forwards:
// all its iterations pass at once, so the mark is in the last.
static void TestZeroTimelineIteration(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef zero = mtwnDefaultClipTimelineDef();
    zero.parent = MTWN_ROOT_NODE;
    zero.position = At(0.1);
    zero.autoDuration = false;
    zero.timing.duration = 0.0;
    zero.timing.iterations = 2.0;
    mtwnNode timeline = 0;
    mtwnNode mark = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &zero, &timeline) == mtwn_success &&
              mtwnClip_AddMark(w.context, w.clip, timeline, At(0.0), 4, &mark) == mtwn_success,
          "a zero timeline of two iterations, a mark in it");
    (void)AddTrack(&w, 0.0, 1.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    (void)Play(&w, false, 1.0);
    Update(&w, 0.0);
    mtwnEvent e[16];
    (void)Read(&w, e, 16);
    Update(&w, 0.5);
    uint32_t n = Read(&w, e, 16);
    uint32_t found = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        found += Is(&e[i], mtwn_eventMark, mark, 0.1, false) && e[i].firstIteration == 1.0 &&
                         e[i].lastIteration == 1.0
                     ? 1u
                     : 0u;
    }
    CHECK(found == 1, "the mark at 0.1, in the last iteration");
    mtwnDestroyContext(w.context);
}

// A timeline of no iterations, running reversed, holds its child time at
// its duration: a mark there is crossed with it, a track that ended
// earlier is not.
static void TestHeldReversedTimeline(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef held = mtwnDefaultClipTimelineDef();
    held.parent = MTWN_ROOT_NODE;
    held.position = At(0.125);
    held.autoDuration = false;
    held.timing.duration = 0.25;
    held.timing.iterations = 0.0;
    held.timing.direction = mtwn_directionAlternateReverse;
    held.timing.fill = mtwn_fillBackwards;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &held, &timeline) == mtwn_success,
          "a reversed timeline of no iterations");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = timeline;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 0.0;
    track.timing.delay = 0.125;
    track.timing.fill = mtwn_fillBoth;
    mtwnNode ended = 0;
    mtwnNode mark = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &ended) == mtwn_success &&
              mtwnClip_AddMark(w.context, w.clip, timeline, At(0.25), 4, &mark) == mtwn_success &&
              mtwnClip_Finish(w.context, w.clip) == mtwn_success,
          "a track ended before its time, a mark at it");
    (void)Play(&w, true, 1.0);
    Update(&w, 0.0);
    mtwnEvent e[16];
    (void)Read(&w, e, 16);
    Update(&w, 0.5);
    uint32_t n = Read(&w, e, 16);
    uint32_t marks = 0;
    uint32_t tracks = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        marks += Is(&e[i], mtwn_eventMark, mark, 0.125, false) ? 1u : 0u;
        tracks += e[i].node == ended ? 1u : 0u;
    }
    CHECK(marks == 1 && tracks == 0, "the mark crossed at 0.125, the track not at all");
    mtwnDestroyContext(w.context);
}

// A reversed timeline whose iterations repeat the shape of a step that
// jumps at its start, its progress eased inside an iteration as the
// timing model eases it: the track at its start records its end at each
// of the timeline's iteration starts and nothing else.
static void TestReflectedJumpStart(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef stepped = mtwnDefaultClipTimelineDef();
    stepped.parent = MTWN_ROOT_NODE;
    stepped.position = At(0.25);
    stepped.autoDuration = false;
    stepped.timing.duration = 0.25;
    stepped.timing.iterations = 3.0;
    stepped.timing.direction = mtwn_directionReverse;
    stepped.timing.fill = mtwn_fillBackwards;
    stepped.timing.alternateEasing = mtwn_alternateRepeatsShape;
    CHECK(mtwnMakeStepsEasing(1, mtwn_jumpStart, &stepped.timing.easing) == mtwn_success, "a step");
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &stepped, &timeline) == mtwn_success,
          "a reversed stepped timeline");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = timeline;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 0.25;
    mtwnNode inner = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &inner) == mtwn_success &&
              mtwnClip_Finish(w.context, w.clip) == mtwn_success,
          "a track at its start");
    (void)Play(&w, true, 1.0);
    uint32_t ends = 0;
    uint32_t others = 0;
    for (int k = 0; k <= 12; ++k)
    {
        Update(&w, 0.125 * k);
        mtwnEvent e[16];
        uint32_t n = Read(&w, e, 16);
        for (uint32_t i = 0; i < n; ++i)
        {
            if (e[i].node != inner)
            {
                continue;
            }
            bool end = e[i].kind == mtwn_eventEnd && e[i].hostTime == e[i].clipTime &&
                       (e[i].hostTime == 0.25 || e[i].hostTime == 0.5 || e[i].hostTime == 0.75);
            ends += end ? 1u : 0u;
            others += end ? 0u : 1u;
        }
    }
    CHECK(ends == 3 && others == 0, "the track's ends at 0.25, 0.5 and 0.75, nothing else");
    mtwnDestroyContext(w.context);
}

// An endless timeline of no iterations holds its children at 0, as the
// values do: a mark at its start is crossed once, when it is reached.
static void TestEndlessNoIterations(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef endless = mtwnDefaultClipTimelineDef();
    endless.parent = MTWN_ROOT_NODE;
    endless.position = At(0.25);
    endless.autoDuration = false;
    endless.timing.duration = HUGE_VAL;
    endless.timing.iterations = 0.0;
    endless.timing.fill = mtwn_fillForwards;
    mtwnNode timeline = 0;
    mtwnNode mark = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &endless, &timeline) == mtwn_success &&
              mtwnClip_AddMark(w.context, w.clip, timeline, At(0.0), 3, &mark) == mtwn_success,
          "an endless timeline of no iterations, a mark at its start");
    (void)AddTrack(&w, 0.0, 1.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    (void)Play(&w, false, 1.0);
    uint32_t found = 0;
    for (int k = 0; k <= 4; ++k)
    {
        Update(&w, 0.125 * k);
        mtwnEvent e[16];
        uint32_t n = Read(&w, e, 16);
        for (uint32_t i = 0; i < n; ++i)
        {
            found += e[i].node == mark && e[i].hostTime == 0.25 ? 1u : 0u;
        }
    }
    CHECK(found == 1, "the mark once, at 0.25");
    mtwnDestroyContext(w.context);
}

// A reversed endless timeline holds its children at infinity, of one
// iteration or none: an endless child there is active, its end at no
// time, and a track in it lies past its end, filling it. Nothing in them
// is crossed, at infinity from the start.
static void TestUnderReversedEndless(void)
{
    for (int none = 0; none < 2; ++none)
    {
        World w = MakeWorld(64);
        mtwnClipTimelineDef outer = mtwnDefaultClipTimelineDef();
        outer.parent = MTWN_ROOT_NODE;
        outer.autoDuration = false;
        outer.timing.duration = HUGE_VAL;
        outer.timing.iterations = none ? 0.0 : 1.0;
        outer.timing.direction = none ? mtwn_directionAlternateReverse : mtwn_directionReverse;
        outer.timing.fill = mtwn_fillBoth;
        mtwnClipTimelineDef inner = outer;
        inner.timing.iterations = 2.0;
        inner.timing.iterationStart = 0.25;
        inner.timing.direction = mtwn_directionAlternate;
        mtwnNode a = 0;
        mtwnNode b = 0;
        CHECK(mtwnClip_AddTimeline(w.context, w.clip, &outer, &a) == mtwn_success,
              "a reversed one");
        inner.parent = a;
        CHECK(mtwnClip_AddTimeline(w.context, w.clip, &inner, &b) == mtwn_success,
              "an endless one in it");
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 1.0;
        keys[1].value.x = 1.0f;
        keys[0].easing = keys[1].easing = mtwnLinearEasing();
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.parent = b;
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 0.5;
        track.timing.fill = mtwn_fillForwards;
        mtwnNode t = 0;
        CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &t) == mtwn_success, "a track");
        CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
        (void)Play(&w, false, 1.0);
        uint32_t inside = 0;
        for (int k = 0; k <= 4; ++k)
        {
            Update(&w, 0.125 * k);
            mtwnEvent e[16];
            uint32_t n = Read(&w, e, 16);
            for (uint32_t i = 0; i < n; ++i)
            {
                inside += e[i].node == b || e[i].node == t ? 1u : 0u;
            }
        }
        mtwnVec4 value = {0};
        CHECK(mtwnTarget_GetValue(w.context, w.target, &value) == mtwn_success, "a value");
        CHECK(inside == 0, "nothing at infinity crossed");
        CHECK(value.x == 1.0f, "the track filled at its end");
        mtwnDestroyContext(w.context);
    }
}

// A reversed timeline of no iterations, played back over its point from
// past it: its children hold their time either side, but the point lies
// inside the sweep, not at an end, so the timeline is crossed.
static void TestPointPassedBack(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef point = mtwnDefaultClipTimelineDef();
    point.parent = MTWN_ROOT_NODE;
    point.autoDuration = false;
    point.timing.duration = 0.375;
    point.timing.iterations = 0.0;
    point.timing.direction = mtwn_directionReverse;
    point.timing.fill = mtwn_fillForwards;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &point, &timeline) == mtwn_success,
          "a timeline of no iterations");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef inner = mtwnDefaultClipTrackDef();
    inner.parent = timeline;
    inner.keyframes = keys;
    inner.keyframeCount = 2;
    inner.timing.duration = 0.5;
    inner.timing.iterations = 0.0;
    inner.timing.fill = mtwn_fillForwards;
    mtwnNode track = 0;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &inner, &track) == mtwn_success,
          "a track of no iterations");
    mtwnNode mark = 0;
    CHECK(mtwnClip_AddMark(w.context, w.clip, timeline, At(0.125), 7, &mark) == mtwn_success,
          "a mark");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId player = Play(&w, true, 1.0);
    Update(&w, 0.125);
    CHECK(mtwnPlayer_Reverse(w.context, player) == mtwn_success, "reversed");
    Update(&w, 0.1875);
    CHECK(mtwnPlayer_SetRate(w.context, player, 1.0) == mtwn_success, "forwards");
    Update(&w, 0.5);
    CHECK(mtwnPlayer_Reverse(w.context, player) == mtwn_success, "reversed");
    Update(&w, 0.875);
    CHECK(mtwnPlayer_Reverse(w.context, player) == mtwn_success, "reversed");
    Update(&w, 1.125);
    CHECK(mtwnPlayer_SetRate(w.context, player, -0.5) == mtwn_success, "going back");
    Update(&w, 1.375);
    CHECK(mtwnPlayer_Seek(w.context, player, 0.125) == mtwn_success, "past the point");
    Update(&w, 1.4375);
    mtwnEvent e[32];
    (void)Read(&w, e, 32);
    // Going back from past the point, over it, inside one update.
    CHECK(mtwnPlayer_SetRate(w.context, player, 0.0) == mtwn_success, "held");
    Update(&w, 1.6875);
    uint32_t n = Read(&w, e, 32);
    uint32_t crossed = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        crossed += e[i].node == timeline && e[i].backwards ? 1u : 0u;
    }
    CHECK(crossed == 2, "the timeline's end and start, crossed back");
    mtwnDestroyContext(w.context);
}

// A player sought past its end is held there, so the time it ran at since
// is 0: the records of its way back after a rate change carry the update's
// host time, not one between the updates.
static void TestHeldAfterSeek(void)
{
    World w = MakeWorld(64);
    mtwnClipTimelineDef thrice = mtwnDefaultClipTimelineDef();
    thrice.parent = MTWN_ROOT_NODE;
    thrice.autoDuration = false;
    thrice.timing.duration = 0.25;
    thrice.timing.iterations = 3.0;
    thrice.timing.iterationStart = 0.75;
    thrice.timing.direction = mtwn_directionAlternateReverse;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &thrice, &timeline) == mtwn_success,
          "a timeline");
    (void)AddTrack(&w, 0.0, 0.25, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId player = Play(&w, true, 1.0);
    Update(&w, 0.125);
    Update(&w, 0.1875);
    CHECK(mtwnPlayer_Seek(w.context, player, 1.125) == mtwn_success, "past the end");
    Update(&w, 0.25);
    mtwnEvent e[16];
    (void)Read(&w, e, 16);
    CHECK(mtwnPlayer_SetRate(w.context, player, 0.5) == mtwn_success, "half the rate");
    Update(&w, 0.5);
    uint32_t n = Read(&w, e, 16);
    uint32_t atUpdate = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        atUpdate += e[i].hostTime == 0.5 ? 1u : 0u;
    }
    CHECK(n == 2 && atUpdate == n, "the root's and the timeline's at the update's host time");
    mtwnDestroyContext(w.context);
}

// A play at 0 is no rewind: a player paused at its start and played again
// records its start once.
static void TestPlayAtStart(void)
{
    World w = MakeWorld(64);
    (void)AddTrack(&w, 0.0, 1.0, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId player = Play(&w, true, 1.0);
    Update(&w, 0.0);
    CHECK(mtwnPlayer_Pause(w.context, player) == mtwn_success, "paused at 0");
    Update(&w, 0.25);
    CHECK(mtwnPlayer_Play(w.context, player) == mtwn_success, "played at 0");
    Update(&w, 0.5);
    mtwnEvent e[16];
    uint32_t n = Read(&w, e, 16);
    uint32_t starts = 0;
    for (uint32_t i = 0; i < n; ++i)
    {
        starts += e[i].node == MTWN_ROOT_NODE && e[i].kind == mtwn_eventStart ? 1u : 0u;
    }
    CHECK(starts == 1, "the start once");
    mtwnDestroyContext(w.context);
}

int main(void)
{
    TestJumpStartMarks();
    TestReverseAfterReducedFinish();
    TestUnderAZeroTimeline();
    TestEndlessTimelineStart();
    TestUnderZeroTimelines();
    TestZeroTimelineIteration();
    TestHeldReversedTimeline();
    TestReflectedJumpStart();
    TestHoldEdges();
    TestOneCrossingIterations();
    TestEdgeIterations();
    TestPlayed();
    TestNoQueue();
    TestStall();
    TestBackwards();
    TestIterationLeftBackwards();
    TestSeeks();
    TestRewindAndRates();
    TestRateZeroFirst();
    TestCountAtTheTop();
    TestSameTime();
    TestTransitions();
    TestQueue();
    TestOverflow();
    TestIterationStart();
    TestBoundaryPlay();
    TestSettledBoundary();
    TestSettledReversedParent();
    TestSharedRecords();
    TestOrder();
    TestPaths();
    TestEndlessNoIterations();
    TestUnderReversedEndless();
    TestPointPassedBack();
    TestHeldAfterSeek();
    TestPlayAtStart();
    return s_failures == 0 ? 0 : 1;
}
