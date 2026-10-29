// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Players: Web Animations
// Level 1's procedures on the host's clock: the pending play resolved at
// the next update, pause, auto-rewind, the finished state, finish,
// reverse, rate changes keeping the current time, seeks, cancel; and the
// bindings' runs, the clip held busy, the creation order.

#include "test_harness.h"

#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <float.h>
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

// A context of a def with one sealed clip: a 2 s scalar track on slot 0,
// a root mark 3 at 0.75 and label 3 at 1.25; endless when asked.
static Fixture MakeFrom(const mtwnContextDef* def, bool endless)
{
    Fixture f = {0};
    CHECK(mtwnCreateContext(def, &f.context) == mtwn_success, "a context");
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
    // A mark sharing the label's key, before it: seeks find the label.
    mtwnPosition at = {0.75, 0, mtwn_positionAbsolute};
    CHECK(mtwnClip_AddMark(f.context, f.clip, MTWN_ROOT_NODE, at, 3, &node) == mtwn_success,
          "a mark");
    at.offset = 1.25;
    CHECK(mtwnClip_AddLabel(f.context, f.clip, MTWN_ROOT_NODE, at, 3, &node) == mtwn_success,
          "a label");
    CHECK(mtwnClip_Seal(f.context, f.clip) == mtwn_success, "sealed");
    CHECK(mtwnCreateTarget(f.context, mtwn_valueScalar, (mtwnVec4){0}, &f.target) == mtwn_success,
          "a target");
    return f;
}

// The same in a default context of so many bindings.
static Fixture Make(bool endless, uint32_t bindings)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.bindingCapacity = bindings;
    return MakeFrom(&def, endless);
}

static mtwnPlayerId Player(Fixture* f, bool play, double rate)
{
    mtwnBinding binding = mtwnMakeBinding(0, f->target);
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
    CHECK(mtwnContext_Update(f->context, (uint64_t)(seconds * (double)s_second), NULL, 0, NULL) ==
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

// A pause at a rate other than 1 and a play again: the player resumes from
// the time held, at its rate, never ahead (Motion 14.1.0 fixed animations
// jumping ahead there).
static void TestPauseAtRate(void)
{
    Fixture f = Make(false, 64);
    mtwnPlayerId p = Player(&f, true, 2.0);
    Update(&f, 0.0);
    Update(&f, 0.25);
    CHECK(Near(State(&f, p).currentTime, 0.5), "a quarter second at 2");
    CHECK(mtwnPlayer_Pause(f.context, p) == mtwn_success, "paused");
    Update(&f, 1.0);
    CHECK(Near(State(&f, p).currentTime, 0.5), "held");
    CHECK(mtwnPlayer_Play(f.context, p) == mtwn_success, "played");
    Update(&f, 5.0);
    CHECK(Near(State(&f, p).currentTime, 0.5), "from the time held, after a long pause");
    Update(&f, 5.25);
    CHECK(Near(State(&f, p).currentTime, 1.0), "at 2 again");
    // A rate set and a pause before the next update: the rate is taken at
    // the pause, and runs from the time held.
    CHECK(mtwnPlayer_SetRate(f.context, p, 3.0) == mtwn_success &&
              mtwnPlayer_Pause(f.context, p) == mtwn_success && State(&f, p).rate == 3.0,
          "a rate pending at a pause, taken");
    Update(&f, 5.5);
    CHECK(mtwnPlayer_Play(f.context, p) == mtwn_success, "played");
    Update(&f, 6.0);
    Update(&f, 6.1);
    CHECK(Near(State(&f, p).currentTime, 1.3), "from 1 at 3");
    // Backwards: the same.
    CHECK(mtwnPlayer_SetRate(f.context, p, -2.0) == mtwn_success, "-2");
    Update(&f, 6.2);
    CHECK(Near(State(&f, p).currentTime, 1.6), "the old rate to the update");
    CHECK(mtwnPlayer_Pause(f.context, p) == mtwn_success, "paused");
    Update(&f, 7.0);
    CHECK(mtwnPlayer_Play(f.context, p) == mtwn_success, "played");
    Update(&f, 8.0);
    Update(&f, 8.1);
    CHECK(Near(State(&f, p).currentTime, 1.4) && State(&f, p).rate == -2.0,
          "from the time held, backwards at 2");
    mtwnDestroyContext(f.context);
}

static void TestSeeks(void)
{
    Fixture f = Make(false, 64);
    mtwnPlayerId p = Player(&f, true, 1.0);
    Update(&f, 1.0);
    Update(&f, 1.5);
    CHECK(mtwnPlayer_Seek(f.context, p, 3.0) == mtwn_success && State(&f, p).currentTime == 3.0,
          "a running player sought past its end is there");
    Update(&f, 2.0);
    CHECK(State(&f, p).currentTime == 3.0 && State(&f, p).state == mtwn_playFinished,
          "and stays, its previous time past the end");
    CHECK(mtwnPlayer_Seek(f.context, p, 0.5) == mtwn_success, "back into range");
    Update(&f, 2.25);
    mtwnPlayerState s = State(&f, p);
    CHECK(Near(s.currentTime, 0.75) && s.state == mtwn_playRunning,
          "a finished player sought into range runs from there");
    CHECK(mtwnPlayer_SetRate(f.context, p, -1.0) == mtwn_success, "backwards");
    Update(&f, 2.5);
    CHECK(mtwnPlayer_Seek(f.context, p, -1.0) == mtwn_success && State(&f, p).currentTime == -1.0,
          "sought before 0");
    Update(&f, 3.0);
    CHECK(State(&f, p).currentTime == -1.0, "and stays there backwards");
    CHECK(mtwnPlayer_Pause(f.context, p) == mtwn_success &&
              mtwnPlayer_Seek(f.context, p, 1.0) == mtwn_success,
          "paused, then sought");
    Update(&f, 4.0);
    s = State(&f, p);
    CHECK(s.state == mtwn_playPaused && s.currentTime == 1.0, "a paused player stays paused");
    mtwnPlayerId back = Player(&f, false, -1.0);
    CHECK(mtwnPlayer_Pause(f.context, back) == mtwn_success && State(&f, back).currentTime == 2.0,
          "an idle player paused backwards holds its end");
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
    CHECK(mtwnPlayer_SeekProgress(f.context, p, 0.5) == mtwn_errorInvalid, "no progress of it");
    mtwnDestroyContext(f.context);
}

// Past the end: a finish holds the player at its end, not at the time a
// seek left it past it (Level 1 sets the current time to the end); and a
// player at rate 0 there is not finished (Level 1 finishes only a moving
// one).
static void TestPastTheEnd(void)
{
    Fixture f = Make(false, 64);
    mtwnPlayerId p = Player(&f, true, 1.0);
    Update(&f, 1.0);
    CHECK(mtwnPlayer_Seek(f.context, p, 5.0) == mtwn_success, "sought past the end");
    Update(&f, 1.5);
    CHECK(mtwnPlayer_Finish(f.context, p) == mtwn_success && State(&f, p).currentTime == 2.0,
          "finished at the end, 2");
    mtwnPlayerId held = Player(&f, true, 1.0);
    Update(&f, 2.0);
    CHECK(mtwnPlayer_SetRate(f.context, held, 0.0) == mtwn_success &&
              mtwnPlayer_Seek(f.context, held, 5.0) == mtwn_success,
          "at rate 0, past the end");
    Update(&f, 2.5);
    mtwnPlayerState s = State(&f, held);
    CHECK(s.currentTime == 5.0 && s.state != mtwn_playFinished, "held there, not finished");
    // At rate 0 a play at 0 rewinds nowhere: 0 is inside, not before it.
    mtwnPlayerId still = Player(&f, false, 0.0);
    CHECK(mtwnPlayer_Seek(f.context, still, 0.0) == mtwn_success &&
              mtwnPlayer_Play(f.context, still) == mtwn_success &&
              State(&f, still).currentTime == 0.0,
          "played at 0 at rate 0: still 0");
    // A seek holds an idle player, and one playing at rate 0, at its time
    // (Level 1 sets the hold time with no start time or no rate).
    mtwnPlayerId idle = Player(&f, false, 1.0);
    mtwnPlayerId zero = Player(&f, true, 0.0);
    Update(&f, 3.0);
    CHECK(mtwnPlayer_Seek(f.context, idle, 1.0) == mtwn_success &&
              mtwnPlayer_Seek(f.context, zero, 1.0) == mtwn_success,
          "both sought to 1");
    Update(&f, 3.5);
    CHECK(State(&f, idle).currentTime == 1.0 && State(&f, zero).currentTime == 1.0,
          "both still at 1");
    // A pause at rate 0 keeps the time it holds.
    mtwnPlayerId slowed = Player(&f, true, 1.0);
    Update(&f, 4.0);
    Update(&f, 4.5);
    CHECK(mtwnPlayer_SetRate(f.context, slowed, 0.0) == mtwn_success, "stopped at the update");
    Update(&f, 5.0);
    double at = State(&f, slowed).currentTime;
    CHECK(mtwnPlayer_Pause(f.context, slowed) == mtwn_success, "paused");
    Update(&f, 5.5);
    CHECK(at == 1.0 && State(&f, slowed).currentTime == at, "paused where it stopped, at 1");
    mtwnDestroyContext(f.context);
    // A rate change of a player sought past its end before it began keeps
    // the time it holds: backwards at half speed for a sixteenth of a
    // second from 3.
    f = Make(false, 64);
    mtwnPlayerId sought = Player(&f, true, 1.0);
    CHECK(mtwnPlayer_Seek(f.context, sought, 3.0) == mtwn_success, "sought past the end first");
    Update(&f, 0.125);
    CHECK(mtwnPlayer_SetRate(f.context, sought, -0.5) == mtwn_success, "backwards at half");
    Update(&f, 0.1875);
    CHECK(Near(State(&f, sought).currentTime, 2.96875), "from the time it held");
    // A paused player finished stays at its end through the next update.
    mtwnPlayerId paused = Player(&f, true, 1.5);
    Update(&f, 0.25);
    Update(&f, 0.75);
    CHECK(mtwnPlayer_Pause(f.context, paused) == mtwn_success &&
              mtwnPlayer_Finish(f.context, paused) == mtwn_success,
          "paused, then finished");
    Update(&f, 1.0);
    mtwnPlayerState finished = State(&f, paused);
    CHECK(finished.currentTime == 2.0 && finished.state == mtwn_playFinished,
          "at its end, finished");
    mtwnDestroyContext(f.context);
}

// Bindings are a player's own: a clip with no slots needs none, so its
// player is made with every binding taken; and destroying a player shows
// only its own targets' bases, not those of the player after it.
static void TestOwnBindings(void)
{
    Fixture f = Make(false, 1);
    mtwnPlayerId first = Player(&f, true, 1.0);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId marks;
    CHECK(mtwnCreateClip(f.context, &clipDef, &marks) == mtwn_success, "a clip");
    mtwnPosition at = {0.5, 0, mtwn_positionAbsolute};
    mtwnNode node;
    CHECK(mtwnClip_AddMark(f.context, marks, MTWN_ROOT_NODE, at, 1, &node) == mtwn_success &&
              mtwnClip_Seal(f.context, marks) == mtwn_success,
          "a clip of a mark alone");
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = marks;
    mtwnPlayerId alone;
    CHECK(mtwnCreatePlayer(f.context, &def, &alone) == mtwn_success,
          "its player made, the one binding taken");
    (void)first;
    mtwnDestroyContext(f.context);
    f = Make(false, 64);
    first = Player(&f, true, 1.0);
    mtwnTargetId other;
    CHECK(mtwnCreateTarget(f.context, mtwn_valueScalar, (mtwnVec4){0}, &other) == mtwn_success,
          "another target");
    mtwnBinding binding = mtwnMakeBinding(0, other);
    def = mtwnDefaultPlayerDef();
    def.clip = f.clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.keep = true;
    mtwnPlayerId second;
    CHECK(mtwnCreatePlayer(f.context, &def, &second) == mtwn_success, "a second player");
    Update(&f, 1.0);
    Update(&f, 2.0);
    CHECK(mtwnDestroyPlayer(f.context, first) == mtwn_success, "the first destroyed");
    mtwnVec4 value;
    CHECK(mtwnTarget_GetValue(f.context, other, &value) == mtwn_success && value.x == 0.5f,
          "the second's target as it was");
    mtwnDestroyContext(f.context);
    // A player refused for the full pool gives back no bindings it never
    // took: the one player's binding still writes its target.
    mtwnContextDef small = mtwnDefaultContextDef();
    small.playerCapacity = 1;
    f = MakeFrom(&small, false);
    first = Player(&f, true, 1.0);
    clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(f.context, &clipDef, &marks) == mtwn_success &&
              mtwnClip_AddMark(f.context, marks, MTWN_ROOT_NODE, at, 1, &node) == mtwn_success &&
              mtwnClip_Seal(f.context, marks) == mtwn_success,
          "a clip of a mark alone");
    def = mtwnDefaultPlayerDef();
    def.clip = marks;
    CHECK(mtwnCreatePlayer(f.context, &def, &alone) == mtwn_errorCapacity, "the pool full");
    Update(&f, 1.0);
    Update(&f, 2.0);
    CHECK(mtwnTarget_GetValue(f.context, f.target, &value) == mtwn_success && value.x == 0.5f,
          "the one player's target written");
    mtwnDestroyContext(f.context);
}

// A slot past a full clip's last is refused whatever lies after the slots:
// a clip of 16 slots, its root's time from the last sample ending in the
// byte a scalar's kind would have.
static void TestSlotPastFull(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.slotCapacity = 16;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip of 16 slots");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    int added = 0;
    for (uint32_t slot = 0; slot < 16; ++slot)
    {
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.slot = slot;
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 1.0;
        mtwnNode node;
        added += mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success;
    }
    CHECK(added == 16 && mtwnClip_Seal(context, clip) == mtwn_success, "every slot used");
    mtwnTargetId target;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
          "a target");
    mtwnBinding binding = mtwnMakeBinding(0, target);
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.keep = true;
    def.play = false;
    mtwnPlayerId player;
    CHECK(mtwnCreatePlayer(context, &def, &player) == mtwn_success &&
              mtwnPlayer_Seek(context, player, 0x1.0000000000001p-1) == mtwn_success &&
              mtwnContext_Update(context, 1000000u, NULL, 0, NULL) == mtwn_success,
          "sampled at a time ending in the byte 1");
    binding = mtwnMakeBinding(16, target);
    mtwnPlayerId past;
    CHECK(mtwnCreatePlayer(context, &def, &past) == mtwn_errorInvalid, "slot 16 refused");
    mtwnDestroyContext(context);
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
    CHECK(mtwnPlayer_Seek(f.context, p, HUGE_VAL) == mtwn_errorLimit &&
              mtwnPlayer_Seek(f.context, p, -HUGE_VAL) == mtwn_errorLimit,
          "an infinite time either way past MTWN_SEEK_LIMIT");
    CHECK(mtwnPlayer_Sweep(f.context, p, (double)NAN) == mtwn_errorInvalid &&
              mtwnPlayer_Sweep(f.context, p, HUGE_VAL) == mtwn_errorLimit &&
              mtwnPlayer_Sweep(f.context, p, -HUGE_VAL) == mtwn_errorLimit,
          "a sweep to NaN invalid, to infinity either way past the limit");
    CHECK(mtwnPlayer_SeekProgress(f.context, p, (double)NAN) == mtwn_errorInvalid &&
              mtwnPlayer_SeekProgress(f.context, p, HUGE_VAL) == mtwn_errorLimit &&
              mtwnPlayer_SeekProgress(f.context, p, -HUGE_VAL) == mtwn_errorLimit,
          "a progress of NaN invalid, of infinity either way past the limit");
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
    mtwnBinding bindings[2] = {mtwnMakeBinding(0, f.target), mtwnMakeBinding(0, f.target)};
    def.bindings = bindings;
    def.bindingCount = 2;
    mtwnPlayerId p;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorInvalid && p.index1 == 0,
          "a slot bound twice");
    bindings[1].slot = 1;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorInvalid, "a slot the clip lacks");
    def.bindingCount = 0;
    def.rate = HUGE_VAL;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorLimit, "an infinite rate");
    def.rate = 1.0;
    mtwnPlayerId players[4];
    for (int i = 0; i < 4; ++i)
    {
        CHECK(mtwnCreatePlayer(f.context, &def, &players[i]) == mtwn_success,
              "unbound slots: one binding each");
    }
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorCapacity, "the bindings full");
    mtwnClipId busy = f.clip;
    CHECK(mtwnDestroyClip(f.context, busy) == mtwn_errorBusy, "the clip busy");
    for (int i = 0; i < 4; ++i)
    {
        CHECK(mtwnDestroyPlayer(f.context, players[i]) == mtwn_success, "freed");
    }
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_success, "the bindings back");
    CHECK(mtwnDestroyPlayer(f.context, p) == mtwn_success, "freed");
    CHECK(mtwnDestroyClip(f.context, busy) == mtwn_success, "the clip free");
    // A clip not sealed, or gone.
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

// A context of no bindings has no free run; grown to four, it holds four
// one-slot players and refuses a fifth.
static void TestGrowFromNoBindings(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.bindingCapacity = 0;
    Fixture f = MakeFrom(&def, false);
    def.bindingCapacity = 4;
    CHECK(mtwnContext_Grow(&f.context, &def) == mtwn_success, "grown to four bindings");
    for (int k = 0; k < 4; ++k)
    {
        (void)Player(&f, true, 1.0);
    }
    mtwnBinding binding = mtwnMakeBinding(0, f.target);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = f.clip;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    mtwnPlayerId refused = {0, 0};
    CHECK(mtwnCreatePlayer(f.context, &playerDef, &refused) == mtwn_errorCapacity,
          "a fifth refused");
    mtwnDestroyContext(f.context);
}

// A player refused for want of a player's slot gives back the bindings
// it took first: grown to two players, the context makes two.
static void TestFullPoolKeepsBindings(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.playerCapacity = 1;
    def.bindingCapacity = 2;
    Fixture f = MakeFrom(&def, false);
    mtwnPlayerId first = Player(&f, true, 1.0);
    mtwnBinding binding = mtwnMakeBinding(0, f.target);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = f.clip;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    mtwnPlayerId refused = {0, 0};
    CHECK(mtwnCreatePlayer(f.context, &playerDef, &refused) == mtwn_errorCapacity &&
              refused.index1 == 0,
          "no player's slot left");
    def.playerCapacity = 2;
    CHECK(mtwnContext_Grow(&f.context, &def) == mtwn_success, "grown to two players");
    mtwnPlayerId second = {0, 0};
    CHECK(mtwnCreatePlayer(f.context, &playerDef, &second) == mtwn_success &&
              mtwnPlayer_IsValid(f.context, first),
          "the bindings the refused player took are there again");
    mtwnDestroyContext(f.context);
}

// Players compose in the order they were made, not by their slots: of two
// replacing players on one target, the later made writes it, even in the
// slot of a player freed before the other was made.
static void TestOrder(void)
{
    Fixture f = Make(false, 64);
    mtwnPlayerId first = Player(&f, true, 0.0);
    mtwnPlayerId earlier = Player(&f, true, 0.0);
    CHECK(mtwnDestroyPlayer(f.context, first) == mtwn_success, "the first freed");
    mtwnPlayerId later = Player(&f, true, 0.0);
    CHECK(later.index1 == first.index1 && later.index1 < earlier.index1,
          "the later made in the earlier slot");
    CHECK(mtwnPlayer_Seek(f.context, earlier, 0.5) == mtwn_success &&
              mtwnPlayer_Seek(f.context, later, 1.5) == mtwn_success,
          "held at a quarter and three quarters of the track");
    Update(&f, 1.0);
    mtwnTargetState target;
    CHECK(mtwnTarget_GetState(f.context, f.target, &target) == mtwn_success &&
              target.value.x == 0.75f,
          "the later made writes last");
    CHECK(mtwnDestroyPlayer(f.context, later) == mtwn_success, "the later freed");
    Update(&f, 1.5);
    CHECK(mtwnTarget_GetState(f.context, f.target, &target) == mtwn_success &&
              target.value.x == 0.25f,
          "then the earlier writes it");
    mtwnTargetId other;
    CHECK(mtwnCreateTarget(f.context, mtwn_valueScalar, (mtwnVec4){0}, &other) == mtwn_success,
          "another target");
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = other;
    def.value = (mtwnVec4){1.0f, 0, 0, 0};
    mtwnPlayerId transition;
    CHECK(mtwnStartTransition(f.context, &def, &transition) == mtwn_success, "a transition");
    CHECK(mtwnPlayer_Pause(f.context, transition) == mtwn_errorInvalid,
          "transitions take no controls");
    mtwnPlayerState s;
    CHECK(mtwnPlayer_GetState(f.context, transition, &s) == mtwn_errorInvalid,
          "nor report a state");
    CHECK(mtwnPlayer_Play(f.context, (mtwnPlayerId){9, 9}) == mtwn_errorStale, "a stale player");
    mtwnDestroyContext(f.context);
}

// A player made at a rate of 0 runs forwards from no time: its first
// update records the starts of what begins at 0, not ends.
static void TestFirstSweepAtRest(void)
{
    Fixture f = Make(false, 8);
    (void)Player(&f, true, 0.0);
    Update(&f, 0.0);
    mtwnEvent events[16];
    uint32_t count = 0;
    CHECK(mtwnContext_ReadEvents(f.context, events, 16, &count, NULL) == mtwn_success && count > 0,
          "records");
    uint32_t starts = 0;
    uint32_t ends = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        starts += events[i].kind == mtwn_eventStart ? 1u : 0u;
        ends += events[i].kind == mtwn_eventEnd ? 1u : 0u;
    }
    CHECK(starts >= 2 && ends == 0, "the root's and the track's starts, no ends");
    mtwnDestroyContext(f.context);
}

// After the last clip's player goes, the clips' part of an update stays
// installed: transitions alone update as before, and a
// target a player wrote shows its base.
static void TestAfterLastPlayer(void)
{
    Fixture f = Make(false, 8);
    mtwnPlayerId p = Player(&f, true, 1.0);
    Update(&f, 0.0);
    Update(&f, 1.0);
    mtwnPlayerState state;
    CHECK(mtwnPlayer_GetState(f.context, p, &state) == mtwn_success, "played");
    CHECK(mtwnDestroyPlayer(f.context, p) == mtwn_success, "the last player gone");
    mtwnTargetId x;
    CHECK(mtwnCreateTarget(f.context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success,
          "a target for transitions");
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = x;
    def.value = (mtwnVec4){1.0f, 0, 0, 0};
    def.duration = 1.0;
    def.easing = mtwnLinearEasing();
    mtwnPlayerId transition;
    CHECK(mtwnStartTransition(f.context, &def, &transition) == mtwn_success, "a transition");
    Update(&f, 1.5);
    Update(&f, 2.0);
    mtwnTargetState moved;
    mtwnTargetState written;
    CHECK(mtwnTarget_GetState(f.context, x, &moved) == mtwn_success && moved.value.x == 0.5f &&
              mtwnTarget_GetState(f.context, f.target, &written) == mtwn_success &&
              written.value.x == written.base.x,
          "the transition half way; the player's target at its base");
    Update(&f, 3.0);
    CHECK(mtwnTarget_GetState(f.context, x, &moved) == mtwn_success && moved.value.x == 1.0f &&
              !mtwnPlayer_IsValid(f.context, transition),
          "ended and gone");
    mtwnDestroyContext(f.context);
}

// The procedures' edges at a rate of 0 and at the ends (Level 1).
static void TestEdges(void)
{
    // A binding to the slot past the last is refused.
    Fixture f = Make(false, 8);
    mtwnBinding past = mtwnMakeBinding(1, f.target);
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = f.clip;
    def.bindings = &past;
    def.bindingCount = 1;
    mtwnPlayerId refused;
    CHECK(mtwnCreatePlayer(f.context, &def, &refused) == mtwn_errorInvalid,
          "a binding past the clip's slots refused");
    // At a rate of 0 a player is never finished, at 0 or anywhere.
    mtwnPlayerId still = Player(&f, true, 0.0);
    Update(&f, 0.0);
    mtwnPlayerState state;
    CHECK(mtwnPlayer_GetState(f.context, still, &state) == mtwn_success &&
              state.state == mtwn_playRunning && state.currentTime == 0.0,
          "running at 0, held");
    // Paused at 0.5, its rate set to 0 and played: the time stays.
    mtwnPlayerId held = Player(&f, true, 1.0);
    Update(&f, 0.5);
    Update(&f, 1.0);
    CHECK(mtwnPlayer_Pause(f.context, held) == mtwn_success &&
              mtwnPlayer_SetRate(f.context, held, 0.0) == mtwn_success &&
              mtwnPlayer_Play(f.context, held) == mtwn_success,
          "paused, rate 0, played");
    Update(&f, 1.5);
    Update(&f, 2.5);
    CHECK(mtwnPlayer_GetState(f.context, held, &state) == mtwn_success && state.currentTime == 0.5,
          "a play at rate 0 keeps the time held");
    // Backwards and finished at 0, a play rewinds to the end.
    mtwnPlayerId back = Player(&f, true, -1.0);
    Update(&f, 3.0);
    Update(&f, 6.0);
    CHECK(mtwnPlayer_GetState(f.context, back, &state) == mtwn_success &&
              state.currentTime == 0.0 && state.state == mtwn_playFinished,
          "finished backwards at 0");
    CHECK(mtwnPlayer_Play(f.context, back) == mtwn_success &&
              mtwnPlayer_GetState(f.context, back, &state) == mtwn_success &&
              state.currentTime == state.duration,
          "played again from the end");
    mtwnDestroyContext(f.context);
    // Idle at a rate of 0 on an endless clip, a pause seeks to 0.
    f = Make(true, 8);
    mtwnPlayerId idle = Player(&f, false, 0.0);
    CHECK(mtwnPlayer_Pause(f.context, idle) == mtwn_success &&
              mtwnPlayer_GetState(f.context, idle, &state) == mtwn_success &&
              state.state == mtwn_playPaused && state.currentTime == 0.0,
          "an idle endless player at rate 0 paused at 0");
    mtwnDestroyContext(f.context);
}

// A running player's pending rate is applied by its next update, the old
// rate running until then; by a finish, at once (Level 1).
static void TestPendingRates(void)
{
    Fixture f = Make(false, 8);
    mtwnPlayerId p = Player(&f, true, 1.0);
    Update(&f, 0.0);
    Update(&f, 0.5);
    CHECK(mtwnPlayer_SetRate(f.context, p, 0.0) == mtwn_success, "a rate of 0, pending");
    mtwnPlayerState state;
    Update(&f, 0.75);
    CHECK(mtwnPlayer_GetState(f.context, p, &state) == mtwn_success && state.rate == 0.0 &&
              state.currentTime == 0.75,
          "the old rate ran to the update, the time held there");
    Update(&f, 1.5);
    CHECK(mtwnPlayer_GetState(f.context, p, &state) == mtwn_success && state.currentTime == 0.75 &&
              state.state == mtwn_playRunning,
          "held, running");
    CHECK(mtwnPlayer_SetRate(f.context, p, 2.0) == mtwn_success, "a rate of 2, pending");
    Update(&f, 2.0);
    CHECK(mtwnPlayer_GetState(f.context, p, &state) == mtwn_success && state.currentTime == 0.75 &&
              state.rate == 2.0,
          "moving again from the time held");
    CHECK(mtwnPlayer_SetRate(f.context, p, 4.0) == mtwn_success &&
              mtwnPlayer_Finish(f.context, p) == mtwn_success &&
              mtwnPlayer_GetState(f.context, p, &state) == mtwn_success && state.rate == 4.0 &&
              state.currentTime == state.duration && state.state == mtwn_playFinished,
          "a finish applies the pending rate at once, at the end");
    mtwnDestroyContext(f.context);
}

// A rate past MTWN_RATE_LIMIT in size is refused as misuse, the player
// unchanged.
static void TestRateLimit(void)
{
    Fixture f = Make(false, 8);
    mtwnPlayerId p = Player(&f, true, 1.0);
    CHECK(mtwnContext_Update(f.context, 0, NULL, 0, NULL) == mtwn_success, "started");
    uint64_t misuse = mtwnContext_GetMisuse(f.context);
    CHECK(mtwnPlayer_SetRate(f.context, p, MTWN_RATE_LIMIT) == mtwn_success &&
              mtwnPlayer_SetRate(f.context, p, -MTWN_RATE_LIMIT) == mtwn_success,
          "the limit itself, either way");
    CHECK(mtwnPlayer_SetRate(f.context, p, 2.0) == mtwn_success, "back to 2");
    mtwnPlayerState before;
    CHECK(mtwnPlayer_GetState(f.context, p, &before) == mtwn_success, "read");
    CHECK(mtwnPlayer_SetRate(f.context, p, nextafter(MTWN_RATE_LIMIT, HUGE_VAL)) ==
                  mtwn_errorLimit &&
              mtwnPlayer_SetRate(f.context, p, -1e300) == mtwn_errorLimit &&
              mtwnPlayer_SetRate(f.context, p, (double)NAN) == mtwn_errorInvalid &&
              mtwnPlayer_SetRate(f.context, p, HUGE_VAL) == mtwn_errorLimit,
          "past the limit, NaN and infinity refused");
    mtwnPlayerState after;
    CHECK(mtwnPlayer_GetState(f.context, p, &after) == mtwn_success && after.rate == before.rate &&
              after.pending == before.pending && mtwnContext_GetMisuse(f.context) == misuse + 4,
          "the player unchanged, each counted as misuse");
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = f.clip;
    def.rate = 1e300;
    mtwnPlayerId refused;
    CHECK(mtwnCreatePlayer(f.context, &def, &refused) == mtwn_errorLimit && refused.index1 == 0,
          "a def past the limit refused");
    def.rate = -MTWN_RATE_LIMIT;
    CHECK(mtwnCreatePlayer(f.context, &def, &refused) == mtwn_success, "a def at the limit");
    // A rate other than 0 below MTWN_MIN_RATE would overflow the start
    // time (MTWN_SEEK_LIMIT): 5e-324 took a player at 1 s to its end.
    CHECK(MTWN_MIN_RATE == 0x1p-20 && MTWN_MIN_RATE * MTWN_RATE_LIMIT == 1.0,
          "the floor is 2^-20, the limit's inverse");
    misuse = mtwnContext_GetMisuse(f.context);
    CHECK(mtwnPlayer_SetRate(f.context, p, MTWN_MIN_RATE) == mtwn_success &&
              mtwnPlayer_SetRate(f.context, p, -MTWN_MIN_RATE) == mtwn_success &&
              mtwnPlayer_SetRate(f.context, p, 0.0) == mtwn_success,
          "the smallest rate, either way, and 0");
    CHECK(mtwnPlayer_SetRate(f.context, p, nextafter(MTWN_MIN_RATE, 0.0)) == mtwn_errorLimit &&
              mtwnPlayer_SetRate(f.context, p, -0x1p-1074) == mtwn_errorLimit &&
              mtwnPlayer_SetRate(f.context, p, 1e-300) == mtwn_errorLimit,
          "below it refused");
    def.rate = 0x1p-1074;
    CHECK(mtwnCreatePlayer(f.context, &def, &refused) == mtwn_errorLimit &&
              mtwnContext_GetMisuse(f.context) == misuse + 4,
          "a def below it too, each counted as misuse");
    mtwnDestroyContext(f.context);
}

// A seek's time is at most MTWN_SEEK_LIMIT in size, a share of the
// duration and a label's start included: past it the start time would
// overflow, as a progress of 1e308 took a player to an infinite time.
static void TestSeekLimit(void)
{
    Fixture f = Make(false, 8);
    mtwnPlayerId p = Player(&f, true, 1.0);
    Update(&f, 1.0);
    Update(&f, 2.0);
    uint64_t misuse = mtwnContext_GetMisuse(f.context);
    CHECK(mtwnPlayer_SeekProgress(f.context, p, 1e308) == mtwn_errorLimit &&
              mtwnPlayer_SeekProgress(f.context, p, DBL_MAX) == mtwn_errorLimit &&
              mtwnPlayer_SeekProgress(f.context, p, -1e308) == mtwn_errorLimit &&
              mtwnPlayer_Seek(f.context, p, nextafter(MTWN_SEEK_LIMIT, HUGE_VAL)) ==
                  mtwn_errorLimit &&
              mtwnPlayer_Seek(f.context, p, -1e308) == mtwn_errorLimit &&
              mtwnPlayer_Sweep(f.context, p, 1e308) == mtwn_errorLimit,
          "a progress of 1e308 or DBL_MAX, and times past the limit, refused");
    CHECK(State(&f, p).currentTime == 1.0 && mtwnContext_GetMisuse(f.context) == misuse + 6,
          "the player where it was, each counted as misuse");
    // At the smallest rate the limit's time and the start time stay finite.
    CHECK(mtwnPlayer_SetRate(f.context, p, MTWN_MIN_RATE) == mtwn_success, "slowed");
    Update(&f, 2.5);
    CHECK(mtwnPlayer_Seek(f.context, p, -MTWN_SEEK_LIMIT) == mtwn_success &&
              State(&f, p).currentTime == -MTWN_SEEK_LIMIT,
          "a seek to the limit's negative, at the smallest rate");
    Update(&f, 3.5);
    mtwnPlayerState state = State(&f, p);
    CHECK(state.currentTime == -MTWN_SEEK_LIMIT && state.state == mtwn_playRunning,
          "still there, running, after an update");
    CHECK(mtwnPlayer_SeekProgress(f.context, p, 0.5) == mtwn_success, "back to 1 s");
    Update(&f, 4.5);
    CHECK(Near(State(&f, p).currentTime, 1.0 + MTWN_MIN_RATE),
          "a second on at the smallest rate, not to the end");
    // A label of the root past the limit is refused too.
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId far;
    mtwnNode node;
    mtwnPosition at = {1e308, 0, mtwn_positionAbsolute};
    CHECK(mtwnCreateClip(f.context, &clipDef, &far) == mtwn_success &&
              mtwnClip_AddLabel(f.context, far, MTWN_ROOT_NODE, at, 1, &node) == mtwn_success,
          "a label at 1e308");
    at.offset = MTWN_SEEK_LIMIT;
    CHECK(mtwnClip_AddLabel(f.context, far, MTWN_ROOT_NODE, at, 2, &node) == mtwn_success &&
              mtwnClip_Seal(f.context, far) == mtwn_success,
          "and one at the limit");
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = far;
    def.play = false;
    mtwnPlayerId q;
    CHECK(mtwnCreatePlayer(f.context, &def, &q) == mtwn_success &&
              mtwnPlayer_SeekLabel(f.context, q, 1) == mtwn_errorLimit &&
              mtwnPlayer_SeekLabel(f.context, q, 2) == mtwn_success &&
              State(&f, q).currentTime == MTWN_SEEK_LIMIT,
          "a seek to the far label refused, to the one at the limit taken");
    mtwnDestroyContext(f.context);
}

// A label seek takes a label of the root: one of a timeline inside it is
// not the clip's, though its key matches.
static void TestSeekLabelOfRoot(void)
{
    Fixture f = Make(false, 64);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.position = (mtwnPosition){1.0, 0, mtwn_positionAbsolute};
    mtwnNode timeline;
    mtwnNode node;
    mtwnPosition at = {0.5, 0, mtwn_positionAbsolute};
    CHECK(mtwnCreateClip(f.context, &clipDef, &clip) == mtwn_success &&
              mtwnClip_AddTimeline(f.context, clip, &group, &timeline) == mtwn_success &&
              mtwnClip_AddLabel(f.context, clip, timeline, at, 5, &node) == mtwn_success,
          "a label keyed 5 in a timeline at 1 s");
    at.offset = 0.25;
    CHECK(mtwnClip_AddLabel(f.context, clip, MTWN_ROOT_NODE, at, 6, &node) == mtwn_success &&
              mtwnClip_Seal(f.context, clip) == mtwn_success,
          "and one keyed 6 in the root");
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.play = false;
    mtwnPlayerId p;
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_success &&
              mtwnPlayer_SeekLabel(f.context, p, 5) == mtwn_errorInvalid &&
              mtwnPlayer_SeekLabel(f.context, p, 6) == mtwn_success &&
              State(&f, p).currentTime == 0.25,
          "the timeline's label refused, the root's taken");
    mtwnDestroyContext(f.context);
}

// A player of a clip with no tracks has no bindings: destroying it gives
// back nothing, and the bindings of the player before it stay.
static void TestNoBindingsDestroyed(void)
{
    Fixture f = Make(false, 16);
    mtwnPlayerId first = Player(&f, true, 1.0);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId marks;
    mtwnNode node;
    mtwnPosition at = {0.5, 0, mtwn_positionAbsolute};
    CHECK(mtwnCreateClip(f.context, &clipDef, &marks) == mtwn_success &&
              mtwnClip_AddMark(f.context, marks, MTWN_ROOT_NODE, at, 1, &node) == mtwn_success &&
              mtwnClip_Seal(f.context, marks) == mtwn_success,
          "a clip of a mark only");
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = marks;
    mtwnPlayerId bare = {0, 0};
    CHECK(mtwnCreatePlayer(f.context, &def, &bare) == mtwn_success &&
              mtwnDestroyPlayer(f.context, bare) == mtwn_success,
          "a player of no bindings made and destroyed");
    Update(&f, 0.0);
    Update(&f, 0.5);
    mtwnVec4 value;
    CHECK(mtwnPlayer_IsValid(f.context, first) &&
              mtwnTarget_GetValue(f.context, f.target, &value) == mtwn_success && value.x == 0.25f,
          "the first player still writes its target");
    mtwnDestroyContext(f.context);
}

int main(void)
{
    TestRateLimit();
    TestSeekLimit();
    TestSeekLabelOfRoot();
    TestPendingRates();
    TestEdges();
    TestAfterLastPlayer();
    TestFirstSweepAtRest();
    TestPlayPause();
    TestPauseAtRate();
    TestSeeks();
    TestFinishReverse();
    TestPastTheEnd();
    TestOwnBindings();
    TestSlotPastFull();
    TestSeekCancel();
    TestBindings();
    TestGrowFromNoBindings();
    TestFullPoolKeepsBindings();
    TestNoBindingsDestroyed();
    TestOrder();
    return s_failures == 0 ? 0 : 1;
}
