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

#include "maul-tween/event.h"
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

// A context of a def with one finished clip: a 2 s scalar track on slot 0,
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
    mtwnPosition at = {0.75, 0, mtwn_positionAbsolute, {0}};
    CHECK(mtwnClip_AddMark(f.context, f.clip, MTWN_ROOT_NODE, at, 3, &node) == mtwn_success,
          "a mark");
    at.offset = 1.25;
    CHECK(mtwnClip_AddLabel(f.context, f.clip, MTWN_ROOT_NODE, at, 3, &node) == mtwn_success,
          "a label");
    CHECK(mtwnClip_Finish(f.context, f.clip) == mtwn_success, "finished");
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
    mtwnBinding binding = mtwnBind(0, f->target);
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
    mtwnBinding bindings[2] = {mtwnBind(0, f.target), mtwnBind(0, f.target)};
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
    def = mtwnDefaultPlayerDef();
    def.clip = open;
    def.version = MTWN_ABI_VERSION + 1u;
    uint64_t misuse = mtwnGetContextMisuse(f.context);
    CHECK(mtwnCreatePlayer(f.context, &def, &p) == mtwn_errorVersion &&
              mtwnGetContextMisuse(f.context) == misuse + 1,
          "another version, a misuse");
    mtwnDestroyContext(f.context);
}

static void TestRuns(void)
{
    mtwnBound entries[10];
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

// A player refused for want of a player's slot gives back the bindings
// it took first: grown to two players, the context makes two.
static void TestFullPoolKeepsBindings(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.playerCapacity = 1;
    def.bindingCapacity = 2;
    Fixture f = MakeFrom(&def, false);
    mtwnPlayerId first = Player(&f, true, 1.0);
    mtwnBinding binding = mtwnBind(0, f.target);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = f.clip;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    mtwnPlayerId refused = {0, 0};
    CHECK(mtwnCreatePlayer(f.context, &playerDef, &refused) == mtwn_errorLimit &&
              refused.index1 == 0,
          "no player's slot left");
    def.playerCapacity = 2;
    CHECK(mtwnGrowContext(&f.context, &def) == mtwn_success, "grown to two players");
    mtwnPlayerId second = {0, 0};
    CHECK(mtwnCreatePlayer(f.context, &playerDef, &second) == mtwn_success &&
              mtwnPlayer_IsValid(f.context, first),
          "the bindings the refused player took are there again");
    mtwnDestroyContext(f.context);
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

// Whether the chain of clips' players holds exactly the live ones, each
// linked back to the one before it.
static bool ChainHolds(const mtwnContext* context, const mtwnPlayerId* players, const bool* live,
                       uint32_t count)
{
    uint32_t seen = 0;
    uint32_t previous = 0;
    for (uint32_t i = context->firstClipPlayer; i != 0 && seen <= count;
         i = mtwnPlayerAt(context, i)->play.nextClip)
    {
        const mtwnPlayerSlot* slot = mtwnPlayerAt(context, i);
        bool known = false;
        for (uint32_t k = 0; k < count; ++k)
        {
            known = known || (live[k] && players[k].index1 == i);
        }
        if (!known || slot->type != MTWN_PLAYER_CLIP || slot->play.previousClip != previous)
        {
            return false;
        }
        previous = i;
        ++seen;
    }
    uint32_t alive = 0;
    for (uint32_t k = 0; k < count; ++k)
    {
        alive += live[k] ? 1u : 0u;
    }
    return seen == alive;
}

static void TestClipChain(void)
{
    // Clips' players among transitions, freed from the chain's middle, its
    // head and its end in turn, and made again: the chain the update walks
    // holds the live ones only, its links both ways right.
    Fixture f = Make(false, 64);
    enum
    {
        COUNT = 5
    };
    mtwnPlayerId players[COUNT];
    bool live[COUNT];
    mtwnTargetId others[COUNT];
    for (uint32_t k = 0; k < COUNT; ++k)
    {
        players[k] = Player(&f, true, 1.0);
        live[k] = true;
        CHECK(mtwnCreateTarget(f.context, mtwn_valueScalar, (mtwnVec4){0}, &others[k]) ==
                  mtwn_success,
              "a target");
        mtwnTransitionDef def = mtwnDefaultTransitionDef();
        def.target = others[k];
        def.value = (mtwnVec4){1.0f, 0, 0, 0};
        mtwnPlayerId transition;
        CHECK(mtwnStartTransition(f.context, &def, &transition) == mtwn_success, "a transition");
    }
    CHECK(ChainHolds(f.context, players, live, COUNT), "all five chained");
    static const uint32_t order[COUNT] = {2, 4, 0, 3, 1};
    for (uint32_t k = 0; k < COUNT; ++k)
    {
        CHECK(mtwnDestroyPlayer(f.context, players[order[k]]) == mtwn_success, "one freed");
        live[order[k]] = false;
        CHECK(ChainHolds(f.context, players, live, COUNT), "the chain after it");
        Update(&f, 0.1 * (double)(k + 1));
    }
    CHECK(f.context->firstClipPlayer == 0, "none left");
    players[0] = Player(&f, true, 1.0);
    live[0] = true;
    CHECK(ChainHolds(f.context, players, live, COUNT), "one again");
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
    CHECK(mtwnReadEvents(f.context, events, 16, &count, NULL) == mtwn_success && count > 0,
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
// installed (record mtwn-0047): transitions alone update as before, and a
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

// The procedures' edges at a rate of 0 and at the ends (Level 1), found
// by mutants.
static void TestEdges(void)
{
    // A binding to the slot past the last is refused.
    Fixture f = Make(false, 8);
    mtwnBinding past = mtwnBind(1, f.target);
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
// unchanged (record mtwn-0046).
static void TestRateLimit(void)
{
    Fixture f = Make(false, 8);
    mtwnPlayerId p = Player(&f, true, 1.0);
    CHECK(mtwnUpdate(f.context, 0, NULL, 0, NULL) == mtwn_success, "started");
    uint64_t misuse = mtwnGetContextMisuse(f.context);
    CHECK(mtwnPlayer_SetRate(f.context, p, MTWN_RATE_LIMIT) == mtwn_success &&
              mtwnPlayer_SetRate(f.context, p, -MTWN_RATE_LIMIT) == mtwn_success,
          "the limit itself, either way");
    CHECK(mtwnPlayer_SetRate(f.context, p, 2.0) == mtwn_success, "back to 2");
    mtwnPlayerState before;
    CHECK(mtwnPlayer_GetState(f.context, p, &before) == mtwn_success, "read");
    CHECK(mtwnPlayer_SetRate(f.context, p, nextafter(MTWN_RATE_LIMIT, HUGE_VAL)) ==
                  mtwn_errorInvalid &&
              mtwnPlayer_SetRate(f.context, p, -1e300) == mtwn_errorInvalid &&
              mtwnPlayer_SetRate(f.context, p, (double)NAN) == mtwn_errorInvalid &&
              mtwnPlayer_SetRate(f.context, p, HUGE_VAL) == mtwn_errorInvalid,
          "past the limit, NaN and infinity refused");
    mtwnPlayerState after;
    CHECK(mtwnPlayer_GetState(f.context, p, &after) == mtwn_success && after.rate == before.rate &&
              after.pending == before.pending && mtwnGetContextMisuse(f.context) == misuse + 4,
          "the player unchanged, each counted as misuse");
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = f.clip;
    def.rate = 1e300;
    mtwnPlayerId refused;
    CHECK(mtwnCreatePlayer(f.context, &def, &refused) == mtwn_errorInvalid && refused.index1 == 0,
          "a def past the limit refused");
    def.rate = -MTWN_RATE_LIMIT;
    CHECK(mtwnCreatePlayer(f.context, &def, &refused) == mtwn_success, "a def at the limit");
    mtwnDestroyContext(f.context);
}

int main(void)
{
    TestRateLimit();
    TestPendingRates();
    TestEdges();
    TestAfterLastPlayer();
    TestFirstSweepAtRest();
    TestPlayPause();
    TestPauseAtRate();
    TestSeeks();
    TestFinishReverse();
    TestSeekCancel();
    TestBindings();
    TestRuns();
    TestFullPoolKeepsBindings();
    TestOrder();
    TestClipChain();
    return s_failures == 0 ? 0 : 1;
}
