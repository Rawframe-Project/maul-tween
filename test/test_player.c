// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Players (records mtwn-0012, mtwn-0020, mtwn-0035): Web Animations
// Level 1's procedures on the host's clock: the pending play resolved at
// the next update, pause, auto-rewind, the finished state, finish,
// reverse, rate changes keeping the current time, seeks, cancel; and the
// bindings' runs, the clip held busy, the creation order.

#include "context_internal.h"
#include "runs.h"
#include "test_harness.h"

#include "maul-tween/player.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static const uint64_t s_second = 1000000000u;

typedef struct Fixture
{
    mtwnContext* context;
    mtwnClipId clip;
    mtwnTargetId target;
} Fixture;

// A context with one finished clip: a 2 s scalar track on slot 0, a root
// label 3 at 1.25; endless when asked.
static Fixture Make(bool endless, uint32_t bindings)
{
    Fixture f = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    def.bindingCapacity = bindings;
    CHECK(mtwnCreateContext(&def, &f.context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(f.context, &clipDef, &f.clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 2.0;
    track.timing.iterations = endless ? HUGE_VAL : 1.0;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(f.context, f.clip, &track, &node) == mtwn_success, "a track");
    mtwnPosition at = {1.25, 0, mtwn_positionAbsolute, {0}};
    CHECK(mtwnClip_AddLabel(f.context, f.clip, MTWN_ROOT_NODE, at, 3, &node) == mtwn_success,
          "a label");
    CHECK(mtwnClip_Finish(f.context, f.clip) == mtwn_success, "finished");
    CHECK(mtwnCreateTarget(f.context, mtwn_valueScalar, (mtwnVec4){0}, &f.target) == mtwn_success,
          "a target");
    return f;
}

static mtwnPlayerId Player(Fixture* f, bool play, double rate)
{
    mtwnBinding binding = {0, 0, f->target};
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = f->clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.play = play;
    def.rate = rate;
    def.keep = true;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(f->context, &def, &id) == mtwn_success, "a player");
    return id;
}

static void Update(Fixture* f, double seconds)
{
    CHECK(mtwnUpdate(f->context, (uint64_t)(seconds * (double)s_second), NULL, 0, NULL) ==
              mtwn_success,
          "updated");
}

static mtwnPlayerState State(Fixture* f, mtwnPlayerId player)
{
    mtwnPlayerState state;
    CHECK(mtwnPlayer_GetState(f->context, player, &state) == mtwn_success, "read");
    return state;
}

static bool Near(double a, double b)
{
    return fabs(a - b) <= 1e-9;
}

static void TestPlayPause(void)
{
    Fixture f = Make(false, 64);
    mtwnPlayerId p = Player(&f, true, 1.0);
    mtwnPlayerState s = State(&f, p);
    CHECK(s.pending && s.currentTime == 0.0 && s.state == mtwn_playRunning && s.duration == 2.0,
          "playing waits for the next update, at 0");
    Update(&f, 10.0);
    CHECK(!State(&f, p).pending && State(&f, p).currentTime == 0.0, "which is its start");
    Update(&f, 11.0);
    CHECK(Near(State(&f, p).currentTime, 1.0), "a second later, 1");
    CHECK(mtwnPlayer_Pause(f.context, p) == mtwn_success, "paused");
    Update(&f, 12.0);
    s = State(&f, p);
    CHECK(Near(s.currentTime, 1.0) && s.state == mtwn_playPaused, "held at the last update's");
    CHECK(mtwnPlayer_Pause(f.context, p) == mtwn_success, "pausing again changes nothing");
    CHECK(mtwnPlayer_Play(f.context, p) == mtwn_success && State(&f, p).pending, "resumed");
    Update(&f, 15.0);
    Update(&f, 15.5);
    CHECK(Near(State(&f, p).currentTime, 1.5), "from where it was held");
    // Natural finish: the hold is the end, and stays.
    Update(&f, 20.0);
    s = State(&f, p);
    CHECK(s.state == mtwn_playFinished && s.currentTime == 2.0, "finished at its end");
    Update(&f, 30.0);
    CHECK(State(&f, p).currentTime == 2.0, "and stays there");
    // Playing a finished player rewinds.
    CHECK(mtwnPlayer_Play(f.context, p) == mtwn_success && State(&f, p).currentTime == 0.0,
          "auto-rewind to 0");
    Update(&f, 31.0);
    Update(&f, 31.25);
    CHECK(Near(State(&f, p).currentTime, 0.25), "and plays again");
    mtwnDestroyContext(f.context);
}

static void TestFinishReverse(void)
{
    Fixture f = Make(false, 64);
    mtwnPlayerId p = Player(&f, true, 1.0);
    Update(&f, 1.0);
    Update(&f, 1.5);
    CHECK(mtwnPlayer_Finish(f.context, p) == mtwn_success, "finished by hand");
    mtwnPlayerState s = State(&f, p);
    CHECK(s.state == mtwn_playFinished && s.currentTime == 2.0, "at the end");
    CHECK(mtwnPlayer_Reverse(f.context, p) == mtwn_success, "reversed");
    s = State(&f, p);
    CHECK(s.rate == -1.0 && s.currentTime == 2.0 && s.state == mtwn_playRunning,
          "backwards from the end, the end kept");
    Update(&f, 2.0);
    Update(&f, 2.5);
    CHECK(Near(State(&f, p).currentTime, 1.5), "half a second back");
    Update(&f, 5.0);
    s = State(&f, p);
    CHECK(s.state == mtwn_playFinished && s.currentTime == 0.0, "finished at 0 backwards");
    CHECK(mtwnPlayer_Finish(f.context, p) == mtwn_success && State(&f, p).currentTime == 0.0,
          "finish backwards is 0");
    // A rate change keeps the current time.
    CHECK(mtwnPlayer_SetRate(f.context, p, 1.0) == mtwn_success &&
              mtwnPlayer_Seek(f.context, p, 0.5) == mtwn_success,
          "forwards at 0.5");
    Update(&f, 6.0);
    CHECK(Near(State(&f, p).currentTime, 1.5), "running from the seek");
    CHECK(mtwnPlayer_SetRate(f.context, p, 4.0) == mtwn_success &&
              Near(State(&f, p).currentTime, 1.5) && State(&f, p).rate == 4.0 &&
              State(&f, p).pending,
          "a rate of 4 pending, the time kept");
    Update(&f, 6.1);
    CHECK(Near(State(&f, p).currentTime, 1.6) && !State(&f, p).pending,
          "the old rate ran until the update, as Level 1's seamless update does");
    Update(&f, 6.15);
    CHECK(Near(State(&f, p).currentTime, 1.8), "four times as fast after it");
    mtwnDestroyContext(f.context);
    // An endless clip cannot be played back from its end.
    f = Make(true, 64);
    p = Player(&f, false, 1.0);
    CHECK(State(&f, p).state == mtwn_playIdle && isnan(State(&f, p).currentTime), "idle");
    CHECK(mtwnPlayer_Reverse(f.context, p) == mtwn_errorState, "refused");
    CHECK(State(&f, p).rate == 1.0 && State(&f, p).state == mtwn_playIdle, "and nothing changed");
    CHECK(mtwnPlayer_Finish(f.context, p) == mtwn_errorState, "no end to finish at");
    CHECK(mtwnPlayer_SeekProgress(f.context, p, 0.5) == mtwn_errorState, "no progress of it");
    mtwnDestroyContext(f.context);
}

static void TestSeekCancel(void)
{
    Fixture f = Make(false, 64);
    mtwnPlayerId p = Player(&f, false, 1.0);
    CHECK(mtwnPlayer_Pause(f.context, p) == mtwn_success && State(&f, p).state == mtwn_playPaused &&
              State(&f, p).currentTime == 0.0,
          "pausing an idle player holds it at 0");
    CHECK(mtwnPlayer_Seek(f.context, p, 0.5) == mtwn_success && State(&f, p).currentTime == 0.5,
          "seek by time");
    CHECK(mtwnPlayer_SeekProgress(f.context, p, 0.75) == mtwn_success &&
              State(&f, p).currentTime == 1.5,
          "by progress");
    CHECK(mtwnPlayer_SeekLabel(f.context, p, 3) == mtwn_success && State(&f, p).currentTime == 1.25,
          "at a label");
    CHECK(mtwnPlayer_SeekLabel(f.context, p, 4) == mtwn_errorInvalid, "no such label");
    CHECK(mtwnPlayer_Seek(f.context, p, (double)NAN) == mtwn_errorInvalid, "a NaN time");
    CHECK(mtwnPlayer_Cancel(f.context, p) == mtwn_success && State(&f, p).state == mtwn_playIdle &&
              isnan(State(&f, p).currentTime),
          "canceled: idle");
    CHECK(mtwnPlayer_IsValid(f.context, p), "and kept");
    CHECK(mtwnPlayer_SetRate(f.context, p, 0.0) == mtwn_success &&
              mtwnPlayer_Play(f.context, p) == mtwn_success,
          "played at rate 0");
    Update(&f, 3.0);
    Update(&f, 4.0);
    CHECK(State(&f, p).currentTime == 0.0, "which holds");
    CHECK(mtwnPlayer_Finish(f.context, p) == mtwn_errorState, "no end at rate 0");
    mtwnDestroyContext(f.context);
}

static void TestBindings(void)
{
    Fixture f = Make(false, 4);
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = f.clip;
    mtwnBinding bindings[2] = {{0, 0, f.target}, {0, 0, f.target}};
    def.bindings = bindings;
    def.bindingCount = 2;
    mtwnPlayerId p;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorInvalid && p.index1 == 0,
          "a slot bound twice");
    bindings[1].slot = 1;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorInvalid, "a slot the clip lacks");
    def.bindingCount = 0;
    def.rate = HUGE_VAL;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorInvalid, "an infinite rate");
    def.rate = 1.0;
    mtwnPlayerId players[4];
    for (int i = 0; i < 4; ++i)
    {
        CHECK(mtwnCreatePlayer(f.context, &def, &players[i]) == mtwn_success,
              "unbound slots: one binding each");
    }
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorLimit, "the bindings full");
    mtwnClipId busy = f.clip;
    CHECK(mtwnDestroyClip(f.context, busy) == mtwn_errorBusy, "the clip busy");
    for (int i = 0; i < 4; ++i)
    {
        CHECK(mtwnDestroyPlayer(f.context, players[i]) == mtwn_success, "freed");
    }
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_success, "the bindings back");
    CHECK(mtwnDestroyPlayer(f.context, p) == mtwn_success, "freed");
    CHECK(mtwnDestroyClip(f.context, busy) == mtwn_success, "the clip free");
    // A clip not finished, or gone.
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId open;
    CHECK(mtwnCreateClip(f.context, &clipDef, &open) == mtwn_success, "an open clip");
    def.clip = open;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorState, "not finished");
    def.clip = busy;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorStale, "a destroyed clip");
    def.cookie = 0;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorInvalid, "no cookie");
    mtwnDestroyContext(f.context);
}

static void TestRuns(void)
{
    mtwnTargetId entries[10];
    mtwnRuns runs = mtwnMakeRuns(entries, 10);
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t d = 0;
    CHECK(mtwnTakeRun(&runs, 3, &a) && mtwnTakeRun(&runs, 3, &b) && mtwnTakeRun(&runs, 4, &c) &&
              a == 0 && b == 3 && c == 6,
          "first fit, in order");
    CHECK(!mtwnTakeRun(&runs, 1, &d), "full");
    mtwnGiveRun(&runs, a, 3);
    mtwnGiveRun(&runs, c, 4);
    CHECK(!mtwnTakeRun(&runs, 5, &d), "two holes of 3 and 4, none of 5");
    mtwnGiveRun(&runs, b, 3);
    CHECK(mtwnTakeRun(&runs, 10, &d) && d == 0, "coalesced into one run of 10");
    mtwnGiveRun(&runs, 0, 10);
    CHECK(mtwnTakeRun(&runs, 2, &a) && mtwnTakeRun(&runs, 2, &b) && mtwnTakeRun(&runs, 2, &c),
          "three of 2");
    mtwnGiveRun(&runs, b, 2);
    CHECK(mtwnTakeRun(&runs, 1, &d) && d == b, "the hole reused first");
    CHECK(mtwnTakeRun(&runs, 1, &d) && d == b + 1, "and its rest");
    CHECK(mtwnTakeRun(&runs, 4, &d) && d == 6, "then the tail");
}

static void TestOrder(void)
{
    Fixture f = Make(false, 64);
    mtwnTargetId other;
    CHECK(mtwnCreateTarget(f.context, mtwn_valueScalar, (mtwnVec4){0}, &other) == mtwn_success,
          "another target");
    mtwnPlayerId first = Player(&f, true, 1.0);
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = other;
    def.value = (mtwnVec4){1.0f, 0, 0, 0};
    mtwnPlayerId transition;
    CHECK(mtwnStartTransition(f.context, &def, &transition) == mtwn_success, "a transition");
    mtwnPlayerId last = Player(&f, true, 1.0);
    CHECK(mtwnDestroyPlayer(f.context, first) == mtwn_success, "the first freed");
    mtwnPlayerId again = Player(&f, true, 1.0);
    CHECK(again.index1 == first.index1, "its slot reused");
    uint32_t order[4];
    uint32_t n = 0;
    for (uint32_t i = f.context->firstPlayer; i != 0 && n < 4;
         i = mtwnPlayerAt(f.context, i)->later)
    {
        order[n++] = i;
    }
    CHECK(n == 3 && order[0] == transition.index1 && order[1] == last.index1 &&
              order[2] == again.index1 && f.context->lastPlayer == again.index1,
          "creation order, not slot order");
    CHECK(mtwnPlayer_Pause(f.context, transition) == mtwn_errorState,
          "transitions take no controls");
    mtwnPlayerState s;
    CHECK(mtwnPlayer_GetState(f.context, transition, &s) == mtwn_errorState, "nor report a state");
    CHECK(mtwnPlayer_Play(f.context, (mtwnPlayerId){9, 9}) == mtwn_errorStale, "a stale player");
    mtwnDestroyContext(f.context);
}

int main(void)
{
    TestPlayPause();
    TestFinishReverse();
    TestSeekCancel();
    TestBindings();
    TestRuns();
    TestOrder();
    return s_failures == 0 ? 0 : 1;
}
