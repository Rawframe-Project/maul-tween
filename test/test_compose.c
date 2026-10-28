// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Composition: replace and add with weights per kind,
// numbers, integers, colors premultiplied, rotations multiplied and
// slerped, discrete values stepping; and players layered in composite
// order, an add layer starting from the base each update, faded by its
// weight, committed with what lies under it.

#include "compose.h"
#include "test_harness.h"

#include "maul-tween/player.h"
#include "maul-tween/transition.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static const double s_pi = 3.14159265358979323846;

static mtwnMoving Still(float x, float y, float z, float w)
{
    return (mtwnMoving){{x, y, z, w}, {0.0f, 0.0f, 0.0f, 0.0f}};
}

static bool Near(double a, double b)
{
    return fabs(a - b) <= 1e-6;
}

static void TestKinds(void)
{
    mtwnMoving under = {{1.0f, 2.0f, 0, 0}, {1.0f, 0, 0, 0}};
    mtwnMoving effect = {{3.0f, 5.0f, 0, 0}, {2.0f, 0, 0, 0}};
    mtwnMoving r = mtwnCompose(mtwn_valueVec2, mtwn_compositeAdd, 1.0, under, effect);
    CHECK(r.value.x == 4.0f && r.value.y == 7.0f && r.velocity.x == 3.0f,
          "add sums, velocities too");
    r = mtwnCompose(mtwn_valueVec2, mtwn_compositeReplace, 0.25, under, effect);
    CHECK(r.value.x == 1.5f && r.value.y == 2.75f && r.velocity.x == 1.25f,
          "replace at a quarter: a quarter of the way");
    r = mtwnCompose(mtwn_valueVec2, mtwn_compositeAdd, 0.5, under, effect);
    CHECK(r.value.x == 2.5f && r.value.y == 4.5f, "add at half: half the effect added");
    r = mtwnCompose(mtwn_valueVec2, mtwn_compositeAdd, 0.0, under, effect);
    CHECK(r.value.x == 1.0f && r.velocity.x == 1.0f, "no weight: the underlying exactly");
    // An integer composes unrounded and rounds once.
    r = mtwnCompose(mtwn_valueInteger, mtwn_compositeReplace, 0.5, Still(1, 0, 0, 0),
                    Still(2.2f, 0, 0, 0));
    CHECK(r.value.x == 2.0f, "1 toward 2.2 by half is 1.6, rounded to 2");
    // Colors premultiplied: alpha clamped at 1.
    r = mtwnCompose(mtwn_valueColor, mtwn_compositeAdd, 1.0, Still(0.2f, 0, 0, 1.0f),
                    Still(0.3f, 0.4f, 0, 0.5f));
    CHECK(Near((double)r.value.x, 0.35) && Near((double)r.value.y, 0.2) && r.value.w == 1.0f,
          "colors add premultiplied");
    r = mtwnCompose(mtwn_valueColor, mtwn_compositeReplace, 0.5, Still(1.0f, 0, 0, 1.0f),
                    Still(0, 0, 1.0f, 0.0f));
    CHECK(Near((double)r.value.x, 1.0) && Near((double)r.value.z, 0.0) &&
              Near((double)r.value.w, 0.5),
          "toward a clear color: the color stays, the alpha halves");
    // Rotations: a quarter turn about z twice is a half turn.
    float s = (float)sin(s_pi / 4.0);
    float c = (float)cos(s_pi / 4.0);
    mtwnMoving quarter = Still(0, 0, s, c);
    r = mtwnCompose(mtwn_valueRotation, mtwn_compositeAdd, 1.0, quarter, quarter);
    CHECK(Near((double)r.value.z, 1.0) && Near((double)r.value.w, 0.0), "rotations multiply");
    r = mtwnCompose(mtwn_valueRotation, mtwn_compositeReplace, 0.5, Still(0, 0, 0, 1.0f), quarter);
    CHECK(Near((double)r.value.z, sin(s_pi / 8.0)) && Near((double)r.value.w, cos(s_pi / 8.0)),
          "and mix by slerp: half of a quarter turn");
    // About x, then about y in x's frame: (1/2, 1/2, 1/2, 1/2), the
    // order fixing z's sign.
    r = mtwnCompose(mtwn_valueRotation, mtwn_compositeAdd, 1.0, Still(s, 0, 0, c),
                    Still(0, s, 0, c));
    CHECK(Near((double)r.value.x, 0.5) && Near((double)r.value.y, 0.5) &&
              Near((double)r.value.z, 0.5) && Near((double)r.value.w, 0.5),
          "the underlying times the effect, in that order");
    // Add by a weight: the slerp from no rotation toward the effect, then
    // multiplied in. A quarter turn about z plus a half turn at half
    // weight is a half turn; at full weight three quarters.
    mtwnMoving halfTurn = Still(0, 0, 1.0f, 0);
    r = mtwnCompose(mtwn_valueRotation, mtwn_compositeAdd, 0.5, quarter, halfTurn);
    CHECK(Near(fabs((double)r.value.z), 1.0) && Near((double)r.value.w, 0.0),
          "a quarter turn plus half of a half turn: a half turn");
    r = mtwnCompose(mtwn_valueRotation, mtwn_compositeAdd, 0.0, quarter, halfTurn);
    CHECK(r.value.z == quarter.value.z && r.value.w == quarter.value.w,
          "at weight 0, the underlying");
    r = mtwnCompose(mtwn_valueRotation, mtwn_compositeAdd, 1.0, quarter, halfTurn);
    double sign = r.value.z < 0.0f ? -1.0 : 1.0;
    CHECK(Near(sign * (double)r.value.z, sin(3.0 * s_pi / 8.0 * 2.0)) &&
              Near(sign * (double)r.value.w, cos(3.0 * s_pi / 8.0 * 2.0)),
          "at weight 1, three quarters of a turn");
    // Colors mixed to alpha 0 have channels 0.
    r = mtwnCompose(mtwn_valueColor, mtwn_compositeReplace, 1.0, Still(1.0f, 1.0f, 1.0f, 1.0f),
                    Still(0.8f, 0.4f, 0.2f, 0.0f));
    CHECK(r.value.x == 0.0f && r.value.y == 0.0f && r.value.z == 0.0f && r.value.w == 0.0f,
          "a clear color's channels are 0");
    // A discrete value is not additive and steps at half.
    r = mtwnCompose(mtwn_valueDiscrete, mtwn_compositeAdd, 1.0, Still(3, 0, 0, 0),
                    Still(4, 0, 0, 0));
    CHECK(r.value.x == 4.0f, "a discrete value replaces under add");
    CHECK(mtwnCompose(mtwn_valueDiscrete, mtwn_compositeReplace, 0.4, Still(3, 0, 0, 0),
                      Still(4, 0, 0, 0))
                      .value.x == 3.0f &&
              mtwnCompose(mtwn_valueDiscrete, mtwn_compositeReplace, 0.5, Still(3, 0, 0, 0),
                          Still(4, 0, 0, 0))
                      .value.x == 4.0f,
          "and steps at half its weight");
}

typedef struct World
{
    mtwnContext* context;
    mtwnTargetId target;
} World;

static World MakeWorld(float base)
{
    World w = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    def.clipStorageBytes = 1u << 20;
    CHECK(mtwnCreateContext(&def, &w.context) == mtwn_success, "a context");
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScalar, (mtwnVec4){base, 0, 0, 0}, &w.target) ==
              mtwn_success,
          "a target");
    return w;
}

// A finished clip of a 10 s scalar track from a to b.
static mtwnClipId Clip(World* w, float a, float b, mtwnFill fill)
{
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(w->context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value.x = a;
    keys[1].offset = 1.0;
    keys[1].value.x = b;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 10.0;
    track.timing.fill = fill;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w->context, clip, &track, &node) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(w->context, clip) == mtwn_success, "finished");
    return clip;
}

static mtwnPlayerId Layer(World* w, mtwnClipId clip, mtwnComposite composite, float weight,
                          bool keep)
{
    mtwnBinding binding = mtwnBind(0, w->target);
    binding.composite = composite;
    binding.weight = weight;
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.keep = keep;
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnCreatePlayer(w->context, &def, &id) == mtwn_success, "a player");
    return id;
}

static float Value(World* w)
{
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(w->context, w->target, &state) == mtwn_success, "read");
    return state.value.x;
}

static void Update(World* w, double seconds)
{
    CHECK(mtwnUpdate(w->context, (uint64_t)llround(seconds * 1e9), NULL, 0, NULL) == mtwn_success,
          "updated");
}

static void TestLayers(void)
{
    World w = MakeWorld(3.0f);
    mtwnPlayerId wave =
        Layer(&w, Clip(&w, 0.0f, 1.0f, mtwn_fillNone), mtwn_compositeAdd, 1.0f, true);
    Update(&w, 0.0);
    Update(&w, 5.0);
    CHECK(Near((double)Value(&w), 3.5), "an add layer alone: the base plus its value");
    Update(&w, 6.0);
    CHECK(Near((double)Value(&w), 3.6), "from the base each update, not its last value");
    mtwnPlayerId move =
        Layer(&w, Clip(&w, 0.0f, 10.0f, mtwn_fillNone), mtwn_compositeReplace, 1.0f, true);
    Update(&w, 6.0);
    CHECK(Near((double)Value(&w), 0.0), "a later replace covers it, at its start");
    CHECK(mtwnDestroyPlayer(w.context, move) == mtwn_success, "gone");
    move = Layer(&w, Clip(&w, 0.0f, 10.0f, mtwn_fillNone), mtwn_compositeReplace, 1.0f, true);
    mtwnPlayerId again =
        Layer(&w, Clip(&w, 0.0f, 1.0f, mtwn_fillNone), mtwn_compositeAdd, 0.5f, true);
    Update(&w, 7.0);
    Update(&w, 9.0);
    CHECK(Near((double)Value(&w), 2.0 + 0.1), "the replace under a half-weight add layer");
    CHECK(mtwnPlayer_SetWeight(w.context, again, 0.0f) == mtwn_success, "faded out");
    Update(&w, 9.0);
    CHECK(Near((double)Value(&w), 2.0), "the add layer has no effect at weight 0");
    CHECK(mtwnPlayer_SetWeight(w.context, again, 1.5f) == mtwn_errorInvalid &&
              mtwnPlayer_SetWeight(w.context, again, NAN) == mtwn_errorInvalid,
          "weights outside [0, 1] refused");
    CHECK(mtwnDestroyPlayer(w.context, wave) == mtwn_success, "the first layer gone");
    (void)move;
    mtwnDestroyContext(w.context);
}

static void TestCommitAndRefusals(void)
{
    World w = MakeWorld(3.0f);
    mtwnPlayerId p =
        Layer(&w, Clip(&w, 0.0f, 2.0f, mtwn_fillForwards), mtwn_compositeAdd, 1.0f, false);
    Update(&w, 0.0);
    Update(&w, 20.0);
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(w.context, w.target, &state) == mtwn_success &&
              state.base.x == 5.0f && state.value.x == 5.0f && !mtwnPlayer_IsValid(w.context, p),
          "an add layer that ends commits the base plus its end");
    mtwnClipId clip = Clip(&w, 0.0f, 1.0f, mtwn_fillNone);
    mtwnBinding binding = mtwnBind(0, w.target);
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    mtwnPlayerId q;
    binding.weight = 1.5f;
    CHECK(mtwnCreatePlayer(w.context, &def, &q) == mtwn_errorInvalid, "a binding weight past 1");
    binding.weight = 1.0f;
    binding.composite = 2;
    CHECK(mtwnCreatePlayer(w.context, &def, &q) == mtwn_errorInvalid, "an unknown composite");
    binding.composite = mtwn_compositeAdd;
    def.weight = -0.5f;
    CHECK(mtwnCreatePlayer(w.context, &def, &q) == mtwn_errorInvalid, "a negative player weight");
    def.weight = 0.25f;
    CHECK(mtwnCreatePlayer(w.context, &def, &q) == mtwn_success, "a quarter-weight player");
    Update(&w, 20.0);
    Update(&w, 28.0);
    CHECK(Near((double)Value(&w), 5.0 + 0.25 * 0.8), "its weight scales its bindings'");
    mtwnTransitionDef move = mtwnDefaultTransitionDef();
    move.target = w.target;
    move.value = (mtwnVec4){1.0f, 0, 0, 0};
    mtwnPlayerId t;
    CHECK(mtwnStartTransition(w.context, &move, &t) == mtwn_success &&
              mtwnPlayer_SetWeight(w.context, t, 0.5f) == mtwn_errorInvalid,
          "a transition has no weight");
    mtwnDestroyContext(w.context);
}

// Scales compose in their logarithm: add multiplies,
// replace moves geometrically, a weight a power, velocities as rates; an
// incremental scale track multiplies by its ratio each iteration.
static void TestScales(void)
{
    mtwnMoving under = {{2.0f, 0, 0, 0}, {1.0f, 0, 0, 0}};
    mtwnMoving effect = {{8.0f, 0, 0, 0}, {4.0f, 0, 0, 0}};
    mtwnMoving r = mtwnCompose(mtwn_valueScale, mtwn_compositeAdd, 1.0, under, effect);
    CHECK(r.value.x == 16.0f && Near((double)r.velocity.x, 16.0),
          "add multiplies, rates adding: 2 at 1/2 a second and 8 at 1/2");
    r = mtwnCompose(mtwn_valueScale, mtwn_compositeAdd, 0.5, under, effect);
    CHECK(Near((double)r.value.x, 2.0 * sqrt(8.0)) &&
              Near((double)r.velocity.x, 0.75 * 2.0 * sqrt(8.0)),
          "add at half: times the effect's square root");
    r = mtwnCompose(mtwn_valueScale, mtwn_compositeReplace, 0.5, under, effect);
    CHECK(Near((double)r.value.x, 4.0) && Near((double)r.velocity.x, 2.0),
          "replace at half: the geometric mean");
    r = mtwnCompose(mtwn_valueScale, mtwn_compositeReplace, 1.0, under, effect);
    CHECK(r.value.x == 8.0f && r.velocity.x == 4.0f, "replace at full weight: the effect exactly");
    // In a clip: 1 to 100 over 10 s, two iterations moving on, added onto
    // a base of 2.
    World w = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    def.clipStorageBytes = 1u << 20;
    CHECK(mtwnCreateContext(&def, &w.context) == mtwn_success, "a context");
    CHECK(mtwnCreateTarget(w.context, mtwn_valueScale, (mtwnVec4){2.0f, 0, 0, 0}, &w.target) ==
              mtwn_success,
          "a scale target");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(w.context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value.x = 1.0f;
    keys[1].offset = 1.0;
    keys[1].value.x = 100.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.track = mtwnDefaultTrackDef(mtwn_valueScale);
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 10.0;
    track.timing.iterations = 2.0;
    track.incremental = true;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w.context, clip, &track, &node) == mtwn_success, "a scale track");
    CHECK(mtwnClip_Finish(w.context, clip) == mtwn_success, "finished");
    Layer(&w, clip, mtwn_compositeAdd, 1.0f, false);
    Update(&w, 0.0);
    Update(&w, 5.0);
    CHECK(fabs((double)Value(&w) / 20.0 - 1.0) <= 1e-6, "halfway through the first: 2 times 10");
    Update(&w, 15.0);
    CHECK(fabs((double)Value(&w) / 2000.0 - 1.0) <= 1e-6,
          "halfway through the second: 2 times 100 times 10");
    mtwnDestroyContext(w.context);
}

// Velocities past the floats held within them: a
// scale added onto another, its product past the floats, and a clip's
// track moving faster than the largest float a second.
static void TestPastFloats(void)
{
    mtwnMoving under = {{1e30f, 0, 0, 0}, {0.0f, 0, 0, 0}};
    mtwnMoving effect = {{1e30f, 0, 0, 0}, {1e30f, 0, 0, 0}};
    mtwnMoving r = mtwnCompose(mtwn_valueScale, mtwn_compositeAdd, 1.0, under, effect);
    CHECK(r.value.x == FLT_MAX && r.velocity.x == FLT_MAX, "a product past the floats");
    World w = MakeWorld(0.0f);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(w.context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[0].value.x = -3e38f;
    keys[1].offset = 1.0;
    keys[1].value.x = 3e38f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(w.context, clip, &track, &node) == mtwn_success &&
              mtwnClip_Finish(w.context, clip) == mtwn_success,
          "6e38 a second");
    Layer(&w, clip, mtwn_compositeReplace, 1.0f, false);
    Update(&w, 0.0);
    Update(&w, 0.5);
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(w.context, w.target, &state) == mtwn_success &&
              state.velocity.x == FLT_MAX,
          "its velocity the largest float");
    mtwnDestroyContext(w.context);
}

// What reads the target after an eased transition of numbers wrote over a
// clip's track left lazy in the same update: nothing (0), a spring started
// from its velocity (1), or an add layer composed over it (2). From
// `under`, the track is first, from 1 to 101 over 10 s; otherwise the
// target's base is the value the track gives at 1 s.
static mtwnTargetState EasedOverTrack(bool under, float base, int reader)
{
    World w = MakeWorld(under ? 1.0f : base);
    if (under)
    {
        Layer(&w, Clip(&w, 1.0f, 101.0f, mtwn_fillNone), mtwn_compositeReplace, 1.0f, false);
    }
    Update(&w, 0.0);
    Update(&w, 1.0);
    mtwnTransitionDef move = mtwnDefaultTransitionDef();
    move.target = w.target;
    move.value = (mtwnVec4){1000.0f, 0, 0, 0};
    move.duration = 2.0;
    move.easing = mtwnLinearEasing();
    mtwnPlayerId eased;
    CHECK(mtwnStartTransition(w.context, &move, &eased) == mtwn_success, "eased to 1000");
    Update(&w, 1.5);
    Update(&w, 2.0);
    if (reader == 1)
    {
        mtwnTransitionDef settle = mtwnDefaultTransitionDef();
        settle.target = w.target;
        settle.value = (mtwnVec4){500.0f, 0, 0, 0};
        settle.spec = mtwn_transitionSpring;
        mtwnPlayerId spring;
        CHECK(mtwnStartTransition(w.context, &settle, &spring) == mtwn_success,
              "a spring from its velocity");
        Update(&w, 2.25);
    }
    else if (reader == 2)
    {
        Layer(&w, Clip(&w, 0.0f, 10.0f, mtwn_fillNone), mtwn_compositeAdd, 1.0f, false);
        Update(&w, 2.25);
        Update(&w, 2.5);
    }
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(w.context, w.target, &state) == mtwn_success, "read");
    mtwnDestroyContext(w.context);
    return state;
}

// A transition written over a clip's track in one update is what the
// target holds: its velocity is the transition's, not the track's left
// lazy, for every reader. A twin with no track under it, from the same
// value, gives the same bits.
static void TestEasedOverLazyTrack(void)
{
    World probe = MakeWorld(1.0f);
    Layer(&probe, Clip(&probe, 1.0f, 101.0f, mtwn_fillNone), mtwn_compositeReplace, 1.0f, false);
    Update(&probe, 0.0);
    Update(&probe, 1.0);
    float base = Value(&probe);
    mtwnDestroyContext(probe.context);
    static const char* const s_readers[3] = {"read", "a spring started from it",
                                             "an add layer over it"};
    for (int reader = 0; reader < 3; ++reader)
    {
        mtwnTargetState over = EasedOverTrack(true, base, reader);
        mtwnTargetState alone = EasedOverTrack(false, base, reader);
        CHECK(memcmp(&over.value, &alone.value, sizeof over.value) == 0 &&
                  memcmp(&over.velocity, &alone.velocity, sizeof over.velocity) == 0,
              s_readers[reader]);
    }
    // Linear from 11 to 1000 over 2 s.
    mtwnTargetState read = EasedOverTrack(true, base, 0);
    CHECK(fabs((double)read.velocity.x - 494.5) <= 0.1, "989 over 2 s");
}

int main(void)
{
    TestKinds();
    TestLayers();
    TestCommitAndRefusals();
    TestScales();
    TestPastFloats();
    TestEasedOverLazyTrack();
    return s_failures == 0 ? 0 : 1;
}
