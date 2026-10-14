// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Clips played into targets (records mtwn-0012, mtwn-0013, mtwn-0036):
// values and velocities by the tree's times, fills, a finished player
// committing what it fills forward and freed, later players over earlier
// ones, targets no player writes showing their bases.

#include "test_harness.h"

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

// A track from a to b over seconds, linear, on a slot, in a parent.
static mtwnNode AddTrack(World* w, mtwnNode parent, uint32_t slot, mtwnValueKind kind, float a,
                         float b, double seconds, mtwnFill fill, double start)
{
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value.x = a;
    keys[1].offset = 1.0;
    keys[1].value.x = b;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.track.kind = kind;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.parent = parent;
    track.slot = slot;
    track.position = At(start);
    track.timing.duration = seconds;
    track.timing.fill = fill;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(w->context, w->clip, &track, &node) == mtwn_success, "a track");
    return node;
}

static World MakeWorld(void)
{
    World w = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &w.context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(w.context, &clipDef, &w.clip) == mtwn_success, "a clip");
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScalar, (mtwnVec4){0}, &w.target) == mtwn_success,
          "a target");
    return w;
}

static mtwnPlayerId Play(World* w, mtwnTargetId target, bool keep, double rate)
{
    mtwnBinding binding = {0, 0, target};
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

static uint32_t Update(World* w, double seconds)
{
    uint32_t total = 0;
    mtwnTargetId changed[8];
    CHECK(mtwnUpdate(w->context, (uint64_t)llround(seconds * 1e9), changed, 8, &total) ==
              mtwn_success,
          "updated");
    return total;
}

static mtwnTargetState Read(World* w, mtwnTargetId target)
{
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(w->context, target, &state) == mtwn_success, "read");
    return state;
}

static bool Near(double a, double b, double tolerance)
{
    return fabs(a - b) <= tolerance;
}

static void TestValues(void)
{
    World w = MakeWorld();
    AddTrack(&w, MTWN_ROOT_NODE, 0, mtwn_valueScalar, 0.0f, 2.0f, 2.0, mtwn_fillNone, 0.5);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, w.target, true, 1.0);
    Update(&w, 10.0);
    CHECK(Read(&w, w.target).value.x == 0.0f, "before its start, no effect: the base");
    CHECK(Update(&w, 11.5) == 1, "the target reported changed");
    mtwnTargetState s = Read(&w, w.target);
    CHECK(Near((double)s.value.x, 1.0, 1e-6) && Near((double)s.velocity.x, 1.0, 1e-3),
          "1 s into a 2 s track from 0 to 2: 1, moving at 1 per second");
    CHECK(Update(&w, 11.5) == 0, "unchanged, unreported");
    CHECK(mtwnPlayer_Pause(w.context, p) == mtwn_success, "paused");
    Update(&w, 12.0);
    s = Read(&w, w.target);
    CHECK(Near((double)s.value.x, 1.0, 1e-6) && s.velocity.x == 0.0f, "held, at rest");
    CHECK(mtwnPlayer_Play(w.context, p) == mtwn_success, "resumed");
    Update(&w, 13.0);
    Update(&w, 14.0);
    CHECK(Read(&w, w.target).value.x == 0.0f, "past its end without fill: the base again");
    CHECK(mtwnPlayer_SetRate(w.context, p, -2.0) == mtwn_success &&
              mtwnPlayer_Seek(w.context, p, 2.0) == mtwn_success,
          "backwards at 2 from 2");
    Update(&w, 14.25);
    s = Read(&w, w.target);
    CHECK(Near((double)s.value.x, 1.0, 1e-6) && Near((double)s.velocity.x, -2.0, 1e-3),
          "backwards: 1.5 is the track's 1, falling at 2 per second");
    CHECK(mtwnDestroyPlayer(w.context, p) == mtwn_success && Read(&w, w.target).value.x == 0.0f,
          "a destroyed player's target shows its base");
    mtwnDestroyContext(w.context);
}

static void TestEnd(void)
{
    World w = MakeWorld();
    AddTrack(&w, MTWN_ROOT_NODE, 0, mtwn_valueScalar, 0.0f, 2.0f, 1.0, mtwn_fillForwards, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, w.target, false, 1.0);
    Update(&w, 1.0);
    Update(&w, 3.0);
    mtwnTargetState s = Read(&w, w.target);
    CHECK(s.base.x == 2.0f && s.value.x == 2.0f && s.velocity.x == 0.0f,
          "at its end it fills forward: committed as the base");
    CHECK(!mtwnPlayer_IsValid(w.context, p), "and freed");
    CHECK(mtwnDestroyClip(w.context, w.clip) == mtwn_success, "the clip no longer busy");
    // Without fill, nothing to commit.
    CHECK(mtwnCreateClip(w.context, &(mtwnClipDef){0}, &w.clip) == mtwn_errorInvalid, "no def");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(w.context, &clipDef, &w.clip) == mtwn_success, "a clip");
    AddTrack(&w, MTWN_ROOT_NODE, 0, mtwn_valueScalar, 5.0f, 6.0f, 1.0, mtwn_fillNone, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    p = Play(&w, w.target, false, 1.0);
    Update(&w, 4.0);
    Update(&w, 4.5);
    CHECK(Near((double)Read(&w, w.target).value.x, 5.5, 1e-6), "playing");
    Update(&w, 6.0);
    s = Read(&w, w.target);
    CHECK(s.base.x == 2.0f && s.value.x == 2.0f && !mtwnPlayer_IsValid(w.context, p),
          "ended without fill: the base kept, shown, and the player freed");
    mtwnDestroyContext(w.context);
}

static void TestTree(void)
{
    World w = MakeWorld();
    // A timeline at 1, its iterations reversed, holding a track from 0 to
    // 4 over 4 s: played from its end back to its start.
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.position = At(1.0);
    group.timing.direction = mtwn_directionReverse;
    group.timing.fill = mtwn_fillForwards;
    mtwnNode reversed = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &group, &reversed) == mtwn_success, "reversed");
    AddTrack(&w, reversed, 0, mtwn_valueScalar, 0.0f, 4.0f, 4.0, mtwn_fillBoth, 0.0);
    // A second slot: an integer, in a timeline at twice the rate.
    group = mtwnDefaultClipTimelineDef();
    group.timing.playbackRate = 2.0;
    mtwnNode fast = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &group, &fast) == mtwn_success, "fast");
    AddTrack(&w, fast, 1, mtwn_valueInteger, 0.0f, 10.0f, 10.0, mtwn_fillNone, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnTargetId count;
    CHECK(mtwnCreateTarget(w.context, mtwn_valueInteger, (mtwnVec4){0}, &count) == mtwn_success,
          "an integer target");
    mtwnBinding bindings[2] = {{0, 0, w.target}, {1, 0, count}};
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = w.clip;
    def.bindings = bindings;
    def.bindingCount = 2;
    def.keep = true;
    mtwnPlayerId p;
    CHECK(mtwnCreatePlayer(w.context, &def, &p) == mtwn_success, "a player of both");
    Update(&w, 0.0);
    Update(&w, 2.0);
    mtwnTargetState s = Read(&w, w.target);
    CHECK(Near((double)s.value.x, 3.0, 1e-6) && Near((double)s.velocity.x, -1.0, 1e-3),
          "1 s into the reversed timeline: the track's 3, falling");
    s = Read(&w, count);
    CHECK(s.value.x == 4.0f && Near((double)s.velocity.x, 2.0, 1e-3),
          "2 s at twice the rate: 4, its velocity the unrounded curve's");
    Update(&w, 2.26);
    CHECK(Read(&w, count).value.x == 5.0f, "4.52 rounds to 5");
    Update(&w, 6.0);
    CHECK(Read(&w, w.target).value.x == 0.0f && Read(&w, count).value.x == 0.0f,
          "past the reversed end, filled at the start; the integer's track over, its base");
    // Wrong bindings.
    mtwnBinding wrong = {1, 0, w.target};
    def.bindings = &wrong;
    def.bindingCount = 1;
    mtwnPlayerId q;
    CHECK(mtwnCreatePlayer(w.context, &def, &q) == mtwn_errorInvalid,
          "a scalar on an integer slot");
    CHECK(mtwnDestroyTarget(w.context, count) == mtwn_success, "a target destroyed");
    wrong = (mtwnBinding){1, 0, count};
    CHECK(mtwnCreatePlayer(w.context, &def, &q) == mtwn_errorInvalid, "a stale target");
    mtwnTargetId reused;
    CHECK(mtwnCreateTarget(w.context, mtwn_valueInteger, (mtwnVec4){42.0f, 0, 0, 0}, &reused) ==
                  mtwn_success &&
              reused.index1 == count.index1,
          "its slot reused");
    Update(&w, 7.0);
    CHECK(mtwnPlayer_IsValid(w.context, p) && Read(&w, reused).value.x == 42.0f,
          "a player whose target went plays on, and not into the new one");
    mtwnDestroyContext(w.context);
}

static void TestOrder(void)
{
    World w = MakeWorld();
    AddTrack(&w, MTWN_ROOT_NODE, 0, mtwn_valueScalar, 1.0f, 1.0f, 1.0, mtwn_fillNone, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "the first clip");
    mtwnClipId first = w.clip;
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(w.context, &clipDef, &w.clip) == mtwn_success, "a clip");
    AddTrack(&w, MTWN_ROOT_NODE, 0, mtwn_valueScalar, 2.0f, 2.0f, 1.0, mtwn_fillNone, 1.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "the second, from 1 s");
    mtwnClipId second = w.clip;
    w.clip = first;
    mtwnPlayerId a = Play(&w, w.target, true, 1.0);
    w.clip = second;
    mtwnPlayerId b = Play(&w, w.target, true, 1.0);
    Update(&w, 0.0);
    Update(&w, 0.5);
    CHECK(Read(&w, w.target).value.x == 1.0f, "the later player has no effect yet");
    Update(&w, 1.0);
    CHECK(Read(&w, w.target).value.x == 2.0f, "then the later wins");
    Update(&w, 1.5);
    CHECK(Read(&w, w.target).value.x == 2.0f, "and alone");
    CHECK(mtwnPlayer_Cancel(w.context, b) == mtwn_success, "the later canceled");
    Update(&w, 1.75);
    CHECK(Read(&w, w.target).value.x == 0.0f, "neither has effect: the base");
    // A transition before a clip's player that has no effect keeps its value.
    CHECK(mtwnDestroyPlayer(w.context, a) == mtwn_success &&
              mtwnDestroyPlayer(w.context, b) == mtwn_success,
          "both gone");
    mtwnTransitionDef move = mtwnDefaultTransitionDef();
    move.target = w.target;
    move.value = (mtwnVec4){8.0f, 0, 0, 0};
    move.easing = mtwnLinearEasing();
    move.duration = 10.0;
    mtwnPlayerId t;
    CHECK(mtwnStartTransition(w.context, &move, &t) == mtwn_success, "a transition");
    b = Play(&w, w.target, true, 1.0);
    Update(&w, 2.0);
    Update(&w, 2.5);
    CHECK(Near((double)Read(&w, w.target).value.x, 0.4, 1e-6),
          "the clip's player before its start leaves the transition's value");
    mtwnDestroyContext(w.context);
}

static void TestEndlessTimeline(void)
{
    World w = MakeWorld();
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    mtwnNode endless = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &group, &endless) == mtwn_success, "a timeline");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.parent = endless;
    track.timing.duration = 1.0;
    track.timing.iterations = HUGE_VAL;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w.context, w.clip, &track, &node) == mtwn_success, "a loop");
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, w.target, true, 1.0);
    Update(&w, 0.0);
    Update(&w, 100.25);
    CHECK(Near((double)Read(&w, w.target).value.x, 0.25, 1e-6) && mtwnPlayer_IsValid(w.context, p),
          "an endless timeline passes its active time on");
    mtwnDestroyContext(w.context);
}

// A timeline at 0 holding a track from 5 to 6 at 1 for 1 s without fill on
// slot 0 and one of 4 s on slot 1, which sizes it to 4.
static mtwnNode Boundary(World* w, mtwnNode parent, mtwnClipTimelineDef group)
{
    group.parent = parent;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(w->context, w->clip, &group, &timeline) == mtwn_success,
          "a timeline");
    AddTrack(w, timeline, 0, mtwn_valueScalar, 5.0f, 6.0f, 1.0, mtwn_fillNone, 1.0);
    AddTrack(w, timeline, 1, mtwn_valueScalar, 0.0f, 1.0f, 4.0, mtwn_fillNone, 0.0);
    return timeline;
}

// The value a paused player of the world's clip shows at a time and rate.
static float ShownAt(World* w, double seconds, double rate)
{
    mtwnPlayerId p = Play(w, w->target, true, rate);
    CHECK(mtwnPlayer_Pause(w->context, p) == mtwn_success &&
              mtwnPlayer_Seek(w->context, p, seconds) == mtwn_success,
          "held at the time");
    Update(w, 100.0);
    float value = Read(w, w->target).value.x;
    CHECK(mtwnDestroyPlayer(w->context, p) == mtwn_success, "gone");
    return value;
}

static void TestBoundaries(void)
{
    // At a track's start exactly: active going forwards, before it going
    // backwards (Level 1's phases), a paused player keeping its direction.
    World w = MakeWorld();
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    Boundary(&w, MTWN_ROOT_NODE, group);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    CHECK(ShownAt(&w, 1.0, 1.0) == 5.0f, "forwards at the start: its first value");
    CHECK(ShownAt(&w, 1.0, -1.0) == 0.0f, "backwards at the start: before it, the base");
    mtwnDestroyContext(w.context);
    // A timeline played at rate -1 runs its children backwards: at 3 of 4
    // its child time is 1, going back.
    w = MakeWorld();
    group.timing.playbackRate = -1.0;
    Boundary(&w, MTWN_ROOT_NODE, group);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    CHECK(ShownAt(&w, 3.0, 1.0) == 0.0f, "a negative rate turns time back");
    CHECK(ShownAt(&w, 3.0, -1.0) == 5.0f, "and a backward player forwards again");
    mtwnDestroyContext(w.context);
    // A reversed timeline passes its direction to a normal one inside it.
    w = MakeWorld();
    mtwnClipTimelineDef outer = mtwnDefaultClipTimelineDef();
    outer.timing.direction = mtwn_directionReverse;
    mtwnNode reversed = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &outer, &reversed) == mtwn_success, "reversed");
    Boundary(&w, reversed, mtwnDefaultClipTimelineDef());
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    CHECK(ShownAt(&w, 3.0, 1.0) == 0.0f, "backwards two levels down");
    mtwnDestroyContext(w.context);
}

static void TestTimelineShapes(void)
{
    // A reversed timeline of infinite set duration holds its end: its
    // children are past theirs.
    World w = MakeWorld();
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = HUGE_VAL;
    group.timing.direction = mtwn_directionReverse;
    mtwnNode endless = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &group, &endless) == mtwn_success, "endless");
    AddTrack(&w, endless, 0, mtwn_valueScalar, 5.0f, 6.0f, 1.0, mtwn_fillForwards, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    CHECK(ShownAt(&w, 0.5, 1.0) == 6.0f, "the child past its end, filled");
    mtwnDestroyContext(w.context);
    // A timeline's easing shapes its children's time.
    w = MakeWorld();
    group = mtwnDefaultClipTimelineDef();
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &group.timing.easing) == mtwn_success,
          "ease in, squared");
    mtwnNode eased = 0;
    CHECK(mtwnClip_AddTimeline(w.context, w.clip, &group, &eased) == mtwn_success, "eased");
    AddTrack(&w, eased, 0, mtwn_valueScalar, 0.0f, 4.0f, 4.0, mtwn_fillNone, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    CHECK(ShownAt(&w, 2.0, 1.0) == 1.0f, "half way through: a quarter of the time");
    mtwnDestroyContext(w.context);
    // A discrete value moves by steps, at rest.
    w = MakeWorld();
    mtwnTargetId flag;
    CHECK(mtwnCreateTarget(w.context, mtwn_valueDiscrete, (mtwnVec4){0}, &flag) == mtwn_success,
          "a discrete target");
    AddTrack(&w, MTWN_ROOT_NODE, 0, mtwn_valueDiscrete, 1.0f, 2.0f, 1.0, mtwn_fillNone, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    Play(&w, flag, true, 1.0);
    bool still = true;
    bool stepped = false;
    for (int i = 0; i < 2000; ++i)
    {
        Update(&w, 0.0005 * i);
        mtwnTargetState s = Read(&w, flag);
        still = still && s.velocity.x == 0.0f;
        stepped = stepped || s.value.x == 2.0f;
    }
    CHECK(still && stepped, "it steps, at rest throughout");
    mtwnDestroyContext(w.context);
}

static void TestStaleTarget(void)
{
    World w = MakeWorld();
    AddTrack(&w, MTWN_ROOT_NODE, 0, mtwn_valueScalar, 0.0f, 2.0f, 4.0, mtwn_fillNone, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnPlayerId p = Play(&w, w.target, true, 1.0);
    Update(&w, 0.0);
    Update(&w, 1.0);
    CHECK(mtwnDestroyTarget(w.context, w.target) == mtwn_success, "its target destroyed mid-play");
    mtwnTargetId reused;
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScalar, (mtwnVec4){9.0f, 0, 0, 0}, &reused) ==
                  mtwn_success &&
              reused.index1 == w.target.index1,
          "its slot reused");
    Update(&w, 2.0);
    CHECK(Read(&w, reused).value.x == 9.0f && mtwnPlayer_IsValid(w.context, p),
          "the player writes nothing into the new target");
    mtwnDestroyContext(w.context);
}

static void TestCommitReported(void)
{
    // The committing update changes only the base of a target filled since
    // earlier: it is still reported.
    World w = MakeWorld();
    AddTrack(&w, MTWN_ROOT_NODE, 0, mtwn_valueScalar, 0.0f, 2.0f, 1.0, mtwn_fillForwards, 0.0);
    AddTrack(&w, MTWN_ROOT_NODE, 1, mtwn_valueScalar, 0.0f, 2.0f, 2.0, mtwn_fillForwards, 0.0);
    CHECK(mtwnClip_Finish(w.context, w.clip) == mtwn_success, "finished");
    mtwnTargetId other;
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScalar, (mtwnVec4){0}, &other) == mtwn_success,
          "another target");
    mtwnBinding bindings[2] = {{0, 0, w.target}, {1, 0, other}};
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = w.clip;
    def.bindings = bindings;
    def.bindingCount = 2;
    mtwnPlayerId p;
    CHECK(mtwnCreatePlayer(w.context, &def, &p) == mtwn_success, "a player");
    Update(&w, 0.0);
    Update(&w, 1.5);
    CHECK(Read(&w, w.target).value.x == 2.0f && Read(&w, w.target).base.x == 0.0f,
          "filled, not yet committed");
    mtwnTargetId changed[4];
    uint32_t total = 0;
    CHECK(mtwnUpdate(w.context, 3000000000u, changed, 4, &total) == mtwn_success, "the end");
    bool reported = false;
    for (uint32_t i = 0; i < total && i < 4; ++i)
    {
        reported = reported || changed[i].index1 == w.target.index1;
    }
    CHECK(reported && Read(&w, w.target).base.x == 2.0f, "its new base reported");
    mtwnDestroyContext(w.context);
}

int main(void)
{
    TestValues();
    TestEnd();
    TestTree();
    TestOrder();
    TestEndlessTimeline();
    TestBoundaries();
    TestTimelineShapes();
    TestStaleTarget();
    TestCommitReported();
    return s_failures == 0 ? 0 : 1;
}
