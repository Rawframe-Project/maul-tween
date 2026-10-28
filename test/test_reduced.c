// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reduced motion: transitions of every spec and running
// clips' players with a finite end finish at the next update, their
// delays skipped and their records at that update's time, landing where
// they would have; essential ones play on, a player with an infinite end
// plays on, a paused one is left alone, and turning it off reaches what
// has not finished.

#include "test_harness.h"

#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct World
{
    mtwnContext* context;
    mtwnTargetId target;
} World;

static World MakeWorld(void)
{
    World w = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &w.context) == mtwn_success, "a context");
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScalar, (mtwnVec4){0}, &w.target) == mtwn_success,
          "a target");
    return w;
}

static void Update(World* w, double seconds)
{
    uint32_t total = 0;
    CHECK(mtwnUpdate(w->context, (uint64_t)llround(seconds * 1e9), NULL, 0, &total) == mtwn_success,
          "updated");
}

static mtwnTargetState Read(World* w)
{
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(w->context, w->target, &state) == mtwn_success, "read");
    return state;
}

// The records waiting, at most 16.
static uint32_t Events(World* w, mtwnEvent events[16])
{
    uint32_t count = 0;
    CHECK(mtwnReadEvents(w->context, events, 16, &count, NULL) == mtwn_success, "the records");
    return count;
}

static mtwnPlayerId Transition(World* w, mtwnTransitionDef def)
{
    def.target = w->target;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnStartTransition(w->context, &def, &id) == mtwn_success, "a transition");
    return id;
}

static mtwnTransitionDef Eased(float to, double delay, double duration)
{
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.value = (mtwnVec4){to, 0, 0, 0};
    def.easing = mtwnLinearEasing();
    def.delay = delay;
    def.duration = duration;
    return def;
}

static void TestSetting(void)
{
    World w = MakeWorld();
    CHECK(!mtwnContext_IsReducedMotion(w.context), "off when made");
    CHECK(mtwnContext_SetReducedMotion(w.context, true) == mtwn_success &&
              mtwnContext_IsReducedMotion(w.context),
          "on");
    CHECK(mtwnContext_SetReducedMotion(w.context, false) == mtwn_success &&
              !mtwnContext_IsReducedMotion(w.context),
          "off");
    CHECK(mtwnContext_SetReducedMotion(NULL, true) == mtwn_errorInvalid, "a null context");
    CHECK(!mtwnContext_IsReducedMotion(NULL), "a null context is not reduced");
    mtwnDestroyContext(w.context);
}

static void TestEasedTransition(void)
{
    World w = MakeWorld();
    CHECK(mtwnContext_SetReducedMotion(w.context, true) == mtwn_success, "reduced");
    Update(&w, 1.0);
    mtwnPlayerId id = Transition(&w, Eased(5.0f, 0.5, 2.0));
    Update(&w, 1.25);
    mtwnTargetState state = Read(&w);
    CHECK(state.value.x == 5.0f && state.base.x == 5.0f && state.velocity.x == 0.0f,
          "at its end, committed, at rest");
    CHECK(!mtwnPlayer_IsValid(w.context, id), "freed");
    mtwnEvent events[16];
    uint32_t count = Events(&w, events);
    CHECK(count == 2 && events[0].kind == mtwn_eventStart && events[1].kind == mtwn_eventEnd &&
              events[0].hostTime == 1.25 && events[1].hostTime == 1.25 && events[1].clipTime == 2.0,
          "its start and end, its delay skipped, at the update's time");
    // Essential: plays on.
    mtwnTransitionDef fade = Eased(9.0f, 0.0, 2.0);
    fade.essential = true;
    Transition(&w, fade);
    Update(&w, 1.5);
    Update(&w, 2.5);
    CHECK(fabs((double)Read(&w).value.x - 7.0) < 1e-5, "an essential one plays on");
    mtwnDestroyContext(w.context);
}

static void TestSpringAndDecay(void)
{
    World w = MakeWorld();
    Update(&w, 0.0);
    mtwnTransitionDef spring = Eased(3.0f, 0.0, 0.0);
    spring.kind = mtwn_transitionSpring;
    Transition(&w, spring);
    Update(&w, 0.1);
    CHECK(Read(&w).value.x != 3.0f, "a spring on its way");
    CHECK(mtwnContext_SetReducedMotion(w.context, true) == mtwn_success, "reduced on the way");
    Update(&w, 0.2);
    CHECK(Read(&w).value.x == 3.0f && Read(&w).base.x == 3.0f, "a running spring lands");
    // A decay lands where it would have come to rest.
    World u = MakeWorld();
    mtwnTransitionDef fling = mtwnDefaultTransitionDef();
    fling.kind = mtwn_transitionDecay;
    fling.velocitySource = mtwn_velocityGiven;
    fling.velocity = (mtwnVec4){100.0f, 0, 0, 0};
    Update(&u, 0.0);
    Transition(&u, fling);
    for (int i = 1; i <= 2000 &&
                    mtwnTarget_GetState(u.context, u.target, &(mtwnTargetState){0}) == mtwn_success;
         ++i)
    {
        Update(&u, i * 0.01);
    }
    float rest = Read(&u).base.x;
    CHECK(rest > 0.0f, "an unreduced decay coasts and rests");
    CHECK(mtwnContext_SetReducedMotion(w.context, true) == mtwn_success, "still reduced");
    Transition(&w, fling);
    Update(&w, 0.3);
    CHECK(Read(&w).value.x == 3.0f + rest && Read(&w).base.x == 3.0f + rest,
          "a reduced decay lands at its rest at once");
    mtwnDestroyContext(u.context);
    mtwnDestroyContext(w.context);
}

// An undamped spring has no end time and never ends by itself. Ended by
// reduced motion, its end record gives the time it had run.
static void TestEndless(void)
{
    World w = MakeWorld();
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScalar, (mtwnVec4){0}, &w.target) == mtwn_success,
          "a target below its end");
    Update(&w, 1.0);
    mtwnTransitionDef spring = Eased(1.0f, 0.0, 0.0);
    spring.kind = mtwn_transitionSpring;
    CHECK(mtwnMakeSpring(100.0, 0.0, 1.0, &spring.spring) == mtwn_success, "an undamped spring");
    mtwnPlayerId id = Transition(&w, spring);
    Update(&w, 1.5);
    CHECK(mtwnPlayer_IsValid(w.context, id), "running");
    mtwnEvent events[16];
    CHECK(Events(&w, events) == 1 && events[0].kind == mtwn_eventStart, "begun at 1.5");
    CHECK(mtwnContext_SetReducedMotion(w.context, true) == mtwn_success, "reduced");
    Update(&w, 1.6);
    CHECK(!mtwnPlayer_IsValid(w.context, id), "ended");
    uint32_t count = Events(&w, events);
    CHECK(count == 1 && events[0].kind == mtwn_eventEnd && events[0].hostTime == 1.6 &&
              fabs(events[0].clipTime - 0.1) <= 1e-12,
          "its end record 0.1 s in, as far as it ran");
    mtwnDestroyContext(w.context);
    // Ended in its delay, it ran none.
    World d = MakeWorld();
    CHECK(mtwnCreateTarget(d.context, mtwn_valueScalar, (mtwnVec4){0}, &d.target) == mtwn_success,
          "a target below its end");
    Update(&d, 1.0);
    spring.delay = 1.0;
    id = Transition(&d, spring);
    Update(&d, 1.5);
    CHECK(mtwnContext_SetReducedMotion(d.context, true) == mtwn_success, "reduced");
    Update(&d, 1.6);
    count = Events(&d, events);
    CHECK(!mtwnPlayer_IsValid(d.context, id) && count == 2 && events[1].kind == mtwn_eventEnd &&
              events[1].clipTime == 0.0,
          "ended in its delay: its end record 0 s in");
    mtwnDestroyContext(d.context);
}

static void TestTurnedOff(void)
{
    World w = MakeWorld();
    Update(&w, 0.0);
    CHECK(mtwnContext_SetReducedMotion(w.context, true) == mtwn_success, "reduced");
    CHECK(mtwnContext_SetReducedMotion(w.context, false) == mtwn_success, "and not");
    Transition(&w, Eased(4.0f, 0.0, 1.0));
    Update(&w, 0.0);
    Update(&w, 0.5);
    CHECK(fabs((double)Read(&w).value.x - 2.0) < 1e-6, "off, it moves");
    mtwnDestroyContext(w.context);
}

// A clip of one track from 0 to 1 over a second, a mark at a half, the
// track repeated by iterations.
static mtwnClipId MakeClip(World* w, double iterations)
{
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(w->context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    track.timing.iterations = iterations;
    track.timing.fill = mtwn_fillForwards;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(w->context, clip, &track, &node) == mtwn_success, "a track");
    mtwnPosition half;
    memset(&half, 0, sizeof half);
    half.offset = 0.5;
    CHECK(mtwnClip_AddMark(w->context, clip, 1, half, 7, &node) == mtwn_success, "a mark");
    CHECK(mtwnClip_Finish(w->context, clip) == mtwn_success, "finished");
    return clip;
}

static mtwnPlayerId Play(World* w, mtwnClipId clip, bool essential)
{
    mtwnBinding binding = mtwnBind(0, w->target);
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.keep = true;
    def.essential = essential;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(w->context, &def, &id) == mtwn_success, "a player");
    return id;
}

static void TestPlayers(void)
{
    World w = MakeWorld();
    mtwnClipId clip = MakeClip(&w, 1.0);
    CHECK(mtwnContext_SetReducedMotion(w.context, true) == mtwn_success, "reduced");
    Update(&w, 2.0);
    mtwnPlayerId p = Play(&w, clip, false);
    Update(&w, 2.25);
    mtwnPlayerState state;
    CHECK(mtwnPlayer_GetState(w.context, p, &state) == mtwn_success &&
              state.state == mtwn_playFinished && state.currentTime == 1.0,
          "finished at its first update");
    CHECK(Read(&w).value.x == 1.0f, "at its end");
    mtwnEvent events[16];
    uint32_t count = Events(&w, events);
    uint32_t marks = 0;
    bool atUpdate = count > 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        marks += events[i].kind == mtwn_eventMark && events[i].key == 7;
        atUpdate = atUpdate && events[i].hostTime == 2.25;
    }
    CHECK(marks == 1 && atUpdate, "its mark crossed once, every record at the update's time");
    CHECK(count >= 3 && events[0].kind == mtwn_eventStart &&
              events[count - 1].kind == mtwn_eventEnd,
          "its start first and its end last");

    // Essential: plays on.
    World e = MakeWorld();
    CHECK(mtwnContext_SetReducedMotion(e.context, true) == mtwn_success, "reduced");
    mtwnClipId fade = MakeClip(&e, 1.0);
    Update(&e, 0.0);
    Play(&e, fade, true);
    Update(&e, 0.0);
    Update(&e, 0.25);
    CHECK(fabs((double)Read(&e).value.x - 0.25) < 1e-6, "an essential player plays on");

    // An infinite end cannot finish: it plays on.
    World f = MakeWorld();
    mtwnClipId loop = MakeClip(&f, HUGE_VAL);
    Update(&f, 0.0);
    mtwnPlayerId l = Play(&f, loop, false);
    Update(&f, 0.0);
    CHECK(mtwnContext_SetReducedMotion(f.context, true) == mtwn_success, "reduced");
    Update(&f, 1.5);
    CHECK(fabs((double)Read(&f).value.x - 0.5) < 1e-6 &&
              mtwnPlayer_GetState(f.context, l, &state) == mtwn_success &&
              state.state == mtwn_playRunning,
          "a loop plays on");

    // Paused: left alone, then finishes once played.
    World g = MakeWorld();
    mtwnClipId held = MakeClip(&g, 1.0);
    Update(&g, 0.0);
    mtwnPlayerId h = Play(&g, held, false);
    Update(&g, 0.0);
    Update(&g, 0.4);
    CHECK(mtwnPlayer_Pause(g.context, h) == mtwn_success, "paused");
    CHECK(mtwnContext_SetReducedMotion(g.context, true) == mtwn_success, "reduced");
    Update(&g, 0.8);
    CHECK(mtwnPlayer_GetState(g.context, h, &state) == mtwn_success &&
              state.state == mtwn_playPaused && fabs(state.currentTime - 0.4) < 1e-9,
          "a paused player left where it is");
    CHECK(mtwnPlayer_Play(g.context, h) == mtwn_success, "played");
    Update(&g, 0.9);
    CHECK(mtwnPlayer_GetState(g.context, h, &state) == mtwn_success &&
              state.state == mtwn_playFinished && Read(&g).value.x == 1.0f,
          "played, it finishes");
    // Backwards: to 0.
    CHECK(mtwnPlayer_Reverse(g.context, h) == mtwn_success, "reversed");
    Update(&g, 1.0);
    CHECK(mtwnPlayer_GetState(g.context, h, &state) == mtwn_success && state.currentTime == 0.0,
          "played backwards, it finishes at 0");
    mtwnDestroyContext(g.context);
    mtwnDestroyContext(f.context);
    mtwnDestroyContext(e.context);
    mtwnDestroyContext(w.context);
}

// The players that play on though not essential: counted by the update
// and flagged in their state, so the host can find them.
static void TestPlayingOn(void)
{
    World w = MakeWorld();
    mtwnClipId endless = MakeClip(&w, HUGE_VAL);
    mtwnClipId once = MakeClip(&w, 1.0);
    mtwnPlayerId loop = Play(&w, endless, false);
    mtwnPlayerId kept = Play(&w, endless, true);
    mtwnPlayerId ends = Play(&w, once, false);
    Update(&w, 0.0);
    CHECK(mtwnContext_GetPlayingOnCount(w.context) == 0, "none while motion is full");
    CHECK(mtwnContext_SetReducedMotion(w.context, true) == mtwn_success, "reduced");
    Update(&w, 0.1);
    CHECK(mtwnContext_GetPlayingOnCount(w.context) == 1, "one plays on though not essential");
    mtwnPlayerState state;
    CHECK(mtwnPlayer_GetState(w.context, loop, &state) == mtwn_success && state.playsOnReduced &&
              state.state == mtwn_playRunning,
          "the endless one flagged");
    CHECK(mtwnPlayer_GetState(w.context, kept, &state) == mtwn_success && !state.playsOnReduced,
          "an essential one not flagged");
    CHECK(mtwnPlayer_GetState(w.context, ends, &state) == mtwn_success && !state.playsOnReduced &&
              state.state == mtwn_playFinished,
          "one with an end finished, not flagged");
    // The host stops it: none left.
    CHECK(mtwnPlayer_Pause(w.context, loop) == mtwn_success, "paused");
    Update(&w, 0.2);
    CHECK(mtwnContext_GetPlayingOnCount(w.context) == 0 &&
              mtwnPlayer_GetState(w.context, loop, &state) == mtwn_success && !state.playsOnReduced,
          "a paused one no longer plays on");
    CHECK(mtwnContext_GetPlayingOnCount(NULL) == 0, "0 for no context");
    mtwnDestroyContext(w.context);
}

int main(void)
{
    TestSetting();
    TestEasedTransition();
    TestSpringAndDecay();
    TestEndless();
    TestTurnedOff();
    TestPlayers();
    TestPlayingOn();
    return s_failures == 0 ? 0 : 1;
}
