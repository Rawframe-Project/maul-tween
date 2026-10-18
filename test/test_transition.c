// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Transitions and the update (records mtwn-0004, mtwn-0007, mtwn-0029):
// each kind against the values layer, the start at the next update, the
// delay, velocity over the last millisecond, interruption from the value
// reached, the commit at the end, the changed report, refusals, and the
// same bits under a host's flush-to-zero and rounding toward zero.

#include "context_internal.h"
#include "fpenv.h"
#include "test_harness.h"

#include "maul-tween/context.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static const uint64_t s_second = 1000000000u;

static mtwnContext* Make(uint32_t targets, uint32_t players)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.targetCapacity = targets;
    def.playerCapacity = players;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "made");
    return context;
}

static mtwnTargetId Target(mtwnContext* context, mtwnValueKind kind, mtwnVec4 value)
{
    mtwnTargetId id = {0, 0};
    CHECK(mtwnCreateTarget(context, kind, value, &id) == mtwn_success, "a target");
    return id;
}

static mtwnTransitionDef Linear(mtwnTargetId target, mtwnVec4 value, double duration)
{
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = target;
    def.value = value;
    def.duration = duration;
    def.easing = mtwnLinearEasing();
    return def;
}

static mtwnPlayerId Start(mtwnContext* context, const mtwnTransitionDef* def)
{
    mtwnPlayerId id = {0, 0};
    CHECK(mtwnStartTransition(context, def, &id) == mtwn_success, "started");
    return id;
}

static void Update(mtwnContext* context, uint64_t timeNs)
{
    uint32_t total = 0;
    CHECK(mtwnUpdate(context, timeNs, NULL, 0, &total) == mtwn_success, "updated");
}

static mtwnTargetState State(mtwnContext* context, mtwnTargetId target)
{
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(context, target, &state) == mtwn_success, "read");
    return state;
}

static void TestScalar(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId x = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    mtwnTransitionDef def = Linear(x, (mtwnVec4){10.0f, 0, 0, 0}, 1.0);
    mtwnPlayerId player = Start(context, &def);
    CHECK(State(context, x).value.x == 0.0f, "nothing moves before an update");
    uint64_t t0 = 5 * s_second;
    Update(context, t0);
    CHECK(State(context, x).value.x == 0.0f, "the first update is its start");
    Update(context, t0 + s_second / 4);
    mtwnTargetState state = State(context, x);
    CHECK(state.value.x == 2.5f && state.base.x == 0.0f, "a quarter: 2.5, the base untouched");
    CHECK(fabsf(state.velocity.x - 10.0f) <= 1e-3f, "10 a second");
    mtwnTargetId changed[2];
    uint32_t total = 0;
    CHECK(mtwnUpdate(context, t0 + s_second / 2, changed, 2, &total) == mtwn_success &&
              total == 1 && changed[0].index1 == x.index1 && changed[0].generation == x.generation,
          "reported as changed");
    CHECK(mtwnUpdate(context, t0 + s_second / 2, changed, 2, &total) == mtwn_success && total == 0,
          "and not again while it holds");
    Update(context, t0 + s_second);
    state = State(context, x);
    CHECK(state.value.x == 10.0f && state.base.x == 10.0f && state.velocity.x == 0.0f,
          "the end committed as the base, at rest");
    CHECK(!mtwnPlayer_IsValid(context, player), "and the transition freed");
    CHECK(context->players.free == player.index1, "its slot back in the pool, first to reuse");
    mtwnDestroyContext(context);
}

static void TestDelayAndEasing(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId x = Target(context, mtwn_valueScalar, (mtwnVec4){1.0f, 0, 0, 0});
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = x;
    def.value = (mtwnVec4){3.0f, 0, 0, 0};
    def.delay = 0.5;
    def.duration = 2.0;
    Start(context, &def);
    Update(context, 0);
    Update(context, s_second / 2);
    CHECK(State(context, x).value.x == 1.0f && State(context, x).velocity.x == 0.0f,
          "the start holds through the delay");
    Update(context, s_second / 2 + s_second);
    double eased = mtwnEase(&def.easing, 0.5, false);
    CHECK(State(context, x).value.x == (float)(0.5 * 1.0 + 0.5 * 3.0) ||
              State(context, x).value.x == (float)((1.0 - eased) * 1.0 + eased * 3.0),
          "CSS's ease at the middle");
    CHECK(State(context, x).value.x == (float)((1.0 - eased) * 1.0 + eased * 3.0), "exactly");
    // The velocity is the curve's over its last millisecond.
    double before = mtwnEase(&def.easing, (1.0 - 0.001) / 2.0, false);
    float expected =
        (float)((((1.0 - eased) + eased * 3.0) - ((1.0 - before) + before * 3.0)) / 0.001);
    CHECK(fabsf(State(context, x).velocity.x - expected) <= 1e-4f * fabsf(expected),
          "velocity over the last millisecond");
    mtwnDestroyContext(context);
}

static void TestInterruption(void)
{
    mtwnContext* context = Make(4, 1);
    mtwnTargetId x = Target(context, mtwn_valueVec2, (mtwnVec4){0});
    mtwnTransitionDef def = Linear(x, (mtwnVec4){10.0f, -10.0f, 0, 0}, 1.0);
    mtwnPlayerId first = Start(context, &def);
    Update(context, 0);
    Update(context, s_second / 2);
    def.value = (mtwnVec4){0.0f, 0.0f, 0, 0};
    mtwnPlayerId second = Start(context, &def);
    CHECK(!mtwnPlayer_IsValid(context, first) && mtwnPlayer_IsValid(context, second),
          "the new transition replaces the old, in a pool of one");
    CHECK(State(context, x).value.x == 5.0f && State(context, x).value.y == -5.0f,
          "the value reached holds until the next update");
    Update(context, s_second / 2);
    CHECK(State(context, x).value.x == 5.0f, "and the new one starts from it");
    Update(context, s_second);
    CHECK(State(context, x).value.x == 2.5f && State(context, x).value.y == -2.5f, "halfway back");
    CHECK(mtwnDestroyPlayer(context, second) == mtwn_success, "stopped");
    CHECK(State(context, x).value.x == 0.0f && State(context, x).velocity.x == 0.0f,
          "a stopped transition leaves the base");
    CHECK(mtwnDestroyPlayer(context, second) == mtwn_errorStale, "stale after");
    mtwnDestroyContext(context);
}

static void TestKinds(void)
{
    mtwnContext* context = Make(8, 8);
    // An integer: ties away from zero.
    mtwnTargetId count = Target(context, mtwn_valueInteger, (mtwnVec4){0});
    mtwnTransitionDef def = Linear(count, (mtwnVec4){3.0f, 0, 0, 0}, 1.0);
    Start(context, &def);
    // A discrete value flips at 1/2.
    mtwnTargetId flag = Target(context, mtwn_valueDiscrete, (mtwnVec4){4.0f, 0, 0, 0});
    def = Linear(flag, (mtwnVec4){9.0f, 0, 0, 0}, 1.0);
    Start(context, &def);
    // An angle the shorter way, across 0.
    double pi = 3.141592653589793;
    mtwnTargetId angle =
        Target(context, mtwn_valueAngle, (mtwnVec4){(float)(350 * pi / 180), 0, 0, 0});
    def = Linear(angle, (mtwnVec4){(float)(10 * pi / 180), 0, 0, 0}, 1.0);
    Start(context, &def);
    // A color in Oklab.
    mtwnColor redColor = {1.0f, 0.0f, 0.0f, 1.0f};
    mtwnColor blueColor = {0.0f, 0.0f, 1.0f, 1.0f};
    mtwnTargetId color = Target(context, mtwn_valueColor, (mtwnVec4){1.0f, 0.0f, 0.0f, 1.0f});
    def = Linear(color, (mtwnVec4){0.0f, 0.0f, 1.0f, 1.0f}, 1.0);
    Start(context, &def);
    // A rotation, a quarter turn about z.
    float s = (float)sin(pi / 4);
    float c = (float)cos(pi / 4);
    mtwnTargetId rotation = Target(context, mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f});
    def = Linear(rotation, (mtwnVec4){0, 0, s, c}, 1.0);
    Start(context, &def);
    Update(context, 0);
    Update(context, s_second / 2);
    CHECK(State(context, count).value.x == 2.0f, "1.5 rounds to 2");
    CHECK(fabsf(State(context, count).velocity.x - 3.0f) <= 1e-3f,
          "an integer's velocity is its unrounded curve's");
    CHECK(State(context, flag).value.x == 9.0f && State(context, flag).velocity.x == 0.0f,
          "a discrete value flips at 1/2, at rest");
    double reached = (double)State(context, angle).value.x;
    CHECK(State(context, angle).value.x == (float)mtwnMixAngle((double)(float)(350 * pi / 180),
                                                               (double)(float)(10 * pi / 180),
                                                               mtwn_angleShorter, 0.5) &&
              fabs(sin(reached)) < 1e-6 && cos(reached) > 0.0,
          "the shorter way through 0");
    mtwnColorMix mix;
    CHECK(mtwnMakeColorMix(redColor, blueColor, mtwn_colorOklab, mtwn_hueShorter, &mix) ==
              mtwn_success,
          "a mix");
    mtwnColor want = mtwnMixColor(&mix, 0.5);
    mtwnVec4 got = State(context, color).value;
    CHECK(got.x == want.red && got.y == want.green && got.z == want.blue && got.w == want.alpha,
          "the color as the values layer mixes it");
    mtwnRotationMix turn;
    CHECK(mtwnMakeRotationMix((mtwnQuat){0, 0, 0, 1.0f}, (mtwnQuat){0, 0, s, c},
                              mtwn_rotationShortest, &turn) == mtwn_success,
          "a rotation mix");
    mtwnQuat q = mtwnMixRotation(&turn, 0.5);
    got = State(context, rotation).value;
    CHECK(got.x == q.x && got.y == q.y && got.z == q.z && got.w == q.w, "the rotation by slerp");
    Update(context, s_second);
    CHECK(State(context, count).value.x == 3.0f && State(context, flag).value.x == 9.0f &&
              State(context, rotation).base.z == s,
          "every kind ends exactly on its end value");
    mtwnDestroyContext(context);
}

// A scale eased past the floats' range: its logarithm overshoots so far
// that both samples its speed is taken from are infinite (from fuzz_api,
// a NaN velocity before). Held at the largest float, it is still.
static void TestScaleOvershootPastFloats(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId z = Target(context, mtwn_valueScale, (mtwnVec4){1e-38f, 0, 0, 0});
    mtwnTransitionDef def = Linear(z, (mtwnVec4){3e38f, 0, 0, 0}, 1.0);
    CHECK(mtwnMakeBackEasing(mtwn_easeOut, 100.0, &def.easing) == mtwn_success, "a back out");
    Start(context, &def);
    int infinite = 0;
    int held = 0;
    for (int k = 0; k <= 60; ++k)
    {
        Update(context, (uint64_t)k * s_second / 60u);
        mtwnTargetState state = State(context, z);
        infinite += !isfinite(state.value.x) || !isfinite(state.velocity.x);
        held += state.value.x == 0x1.fffffep127f && state.velocity.x == 0.0f;
    }
    CHECK(infinite == 0, "finite throughout");
    CHECK(held > 0, "past the range, held at the largest float and still");
    mtwnDestroyContext(context);
}

static bool NearQuat(mtwnVec4 got, double x, double y, double z, double w)
{
    return fabs((double)got.x - x) <= 1e-6 && fabs((double)got.y - y) <= 1e-6 &&
           fabs((double)got.z - z) <= 1e-6 && fabs((double)got.w - w) <= 1e-6;
}

// Rotations from and to exactly a half turn (w == 0) turn the right way,
// none snapping to its end (PrimeTween 1.4.8 fixed one that did).
static void TestHalfTurns(void)
{
    mtwnContext* context = Make(8, 8);
    const mtwnVec4 halfY = {0, 1.0f, 0, 0};
    const mtwnVec4 still = {0, 0, 0, 1.0f};
    mtwnTargetId fromHalf = Target(context, mtwn_valueRotation, halfY);
    mtwnTargetId between = Target(context, mtwn_valueRotation, halfY);
    mtwnTargetId negated = Target(context, mtwn_valueRotation, halfY);
    mtwnTargetId toHalf = Target(context, mtwn_valueRotation, still);
    mtwnTransitionDef def = Linear(fromHalf, still, 1.0);
    Start(context, &def);
    def = Linear(between, (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    Start(context, &def);
    def = Linear(negated, (mtwnVec4){0, -1.0f, 0, 0}, 1.0);
    Start(context, &def);
    def = Linear(toHalf, halfY, 1.0);
    Start(context, &def);
    Update(context, 0);
    Update(context, s_second / 2);
    double r = sqrt(0.5);
    CHECK(NearQuat(State(context, fromHalf).value, 0, r, 0, r) &&
              NearQuat(State(context, toHalf).value, 0, r, 0, r),
          "a quarter turn about y half way, from a half turn and to one");
    CHECK(NearQuat(State(context, between).value, r, r, 0, 0),
          "between half turns about y and x: about their bisector");
    mtwnVec4 held = State(context, negated).value;
    CHECK(fabs(fabs((double)held.y) - 1.0) <= 1e-6, "to -q: the same rotation, held");
    Update(context, s_second);
    CHECK(State(context, fromHalf).value.w == 1.0f && State(context, toHalf).value.y == 1.0f &&
              State(context, between).value.x == 1.0f,
          "each ends on its end value");
    mtwnDestroyContext(context);
}

static void TestExactness(void)
{
    mtwnContext* context = Make(8, 8);
    // An easing at 1 before the end gives the end exactly, and the end
    // that only commits the base is reported.
    mtwnTargetId x = Target(context, mtwn_valueScalar, (mtwnVec4){0.1f, 0, 0, 0});
    mtwnTransitionDef def = Linear(x, (mtwnVec4){0.7f, 0, 0, 0}, 1.0);
    CHECK(mtwnMakeSteps(1, mtwn_jumpStart, &def.easing) == mtwn_success, "steps(1, start)");
    Start(context, &def);
    Update(context, 0);
    Update(context, s_second / 2);
    CHECK(State(context, x).value.x == 0.7f, "the end value exactly, mid-way");
    mtwnTargetId far = Target(context, mtwn_valueVec2, (mtwnVec4){1e30f, -1e-30f, 0, 0});
    def.target = far;
    def.value = (mtwnVec4){1e-30f, 1e30f, 0, 0};
    Start(context, &def);
    mtwnTargetId changed[1];
    uint32_t total = 0;
    Update(context, s_second / 2 + 1);
    Update(context, s_second / 2 + 2);
    CHECK(State(context, far).value.x == 1e-30f && State(context, far).value.y == 1e30f,
          "even between ends thirty orders apart");
    CHECK(mtwnUpdate(context, s_second, changed, 1, &total) == mtwn_success && total == 1 &&
              State(context, x).base.x == 0.7f,
          "the commit of the base alone is a change");
    // Inside the first millisecond velocity spans from the start.
    mtwnTargetId y = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    def = Linear(y, (mtwnVec4){1.0f, 0, 0, 0}, 0.002);
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &def.easing) == mtwn_success, "quad in");
    Start(context, &def);
    Update(context, 2 * s_second);
    Update(context, 2 * s_second + 500000);
    CHECK(State(context, y).value.x == 0.0625f &&
              fabsf(State(context, y).velocity.x - 125.0f) <= 1e-3f,
          "0.0625 in half a millisecond: 125 a second");
    // Shorter than a millisecond: the velocity is from the start over the
    // time elapsed, never infinite.
    mtwnTargetId quick = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    def = Linear(quick, (mtwnVec4){1.0f, 0, 0, 0}, 0.0005);
    Start(context, &def);
    Update(context, 2 * s_second + 600000);
    Update(context, 2 * s_second + 850000);
    CHECK(State(context, quick).value.x == 0.5f &&
              fabsf(State(context, quick).velocity.x - 2000.0f) <= 1e-2f,
          "half of it in 0.25 ms: 2000 a second");
    // Negative ties round away from zero.
    mtwnTargetId n = Target(context, mtwn_valueInteger, (mtwnVec4){0});
    def = Linear(n, (mtwnVec4){-3.0f, 0, 0, 0}, 1.0);
    Start(context, &def);
    Update(context, 3 * s_second);
    Update(context, 3 * s_second + s_second / 2);
    CHECK(State(context, n).value.x == -2.0f, "-1.5 rounds to -2");
    // A slot reused leaves nothing of the target before it.
    CHECK(fabsf(State(context, n).velocity.x + 3.0f) <= 1e-3f, "moving");
    CHECK(mtwnDestroyTarget(context, n) == mtwn_success, "destroyed while moving");
    mtwnTargetId fresh = Target(context, mtwn_valueScalar, (mtwnVec4){5.0f, 0, 0, 0});
    CHECK(fresh.index1 == n.index1 && State(context, fresh).velocity.x == 0.0f,
          "its slot's next target starts at rest");
    mtwnDestroyContext(context);
}

static mtwnTransitionDef SpringTo(mtwnTargetId target, mtwnVec4 value)
{
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = target;
    def.value = value;
    def.spec = mtwn_transitionSpring;
    return def;
}

static void TestSpring(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId x = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    mtwnTransitionDef def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    mtwnPlayerId player = Start(context, &def);
    double settle = mtwnSpringSettlingTime(&def.spring, -1.0, 0.0, def.restOffset, def.restSpeed);
    CHECK(settle > 0.3 && settle < 2.0, "a critical spring settles within a second or so");
    Update(context, 0);
    double worst = 0.0;
    for (int k = 1; k * 0.01 < settle; ++k)
    {
        uint64_t time = (uint64_t)k * 10000000u;
        Update(context, time);
        double offset = 0.0;
        double velocity = 0.0;
        mtwnSampleSpring(&def.spring, -1.0, 0.0, (double)time / 1e9, &offset, &velocity);
        mtwnTargetState state = State(context, x);
        double e = fabs((double)state.value.x - (1.0 + offset)) +
                   fabs((double)state.velocity.x - velocity);
        worst = e > worst ? e : worst;
    }
    CHECK(worst <= 1e-6, "the spring's value and its own velocity, to a float's rounding");
    CHECK(mtwnPlayer_IsValid(context, player), "alive before it settles");
    Update(context, (uint64_t)(settle * 1e9) + 1);
    CHECK(!mtwnPlayer_IsValid(context, player) && State(context, x).base.x == 1.0f &&
              State(context, x).velocity.x == 0.0f,
          "at its settling time it snaps to the end, at rest");
    mtwnDestroyContext(context);
}

static void TestHandover(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId x = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    mtwnTransitionDef def = Linear(x, (mtwnVec4){10.0f, 0, 0, 0}, 1.0);
    Start(context, &def);
    Update(context, 0);
    Update(context, s_second / 2);
    float velocity = State(context, x).velocity.x;
    CHECK(fabsf(velocity - 10.0f) <= 1e-3f, "moving at 10 a second");
    mtwnTransitionDef back = SpringTo(x, (mtwnVec4){0.0f, 0, 0, 0});
    Start(context, &back);
    Update(context, s_second / 2);
    Update(context, s_second / 2 + 1000000);
    double offset = 0.0;
    double speed = 0.0;
    mtwnSampleSpring(&back.spring, 5.0, (double)velocity, 0.001, &offset, &speed);
    CHECK(fabs((double)State(context, x).value.x - offset) <= 1e-6 &&
              fabs((double)State(context, x).velocity.x - speed) <= 1e-5,
          "the spring takes the velocity the easing left");
    // A given velocity, as a fling gives it.
    back.velocitySource = mtwn_velocityGiven;
    back.velocity = (mtwnVec4){-40.0f, 0, 0, 0};
    float at = State(context, x).value.x;
    Start(context, &back);
    Update(context, s_second);
    Update(context, s_second + 1000000);
    mtwnSampleSpring(&back.spring, (double)at, -40.0, 0.001, &offset, &speed);
    CHECK(fabs((double)State(context, x).value.x - offset) <= 1e-6, "or the one given");
    mtwnDestroyContext(context);
}

static void TestDecay(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId x = Target(context, mtwn_valueVec2, (mtwnVec4){100.0f, -3.0f, 0, 0});
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = x;
    def.spec = mtwn_transitionDecay;
    def.velocitySource = mtwn_velocityGiven;
    def.velocity = (mtwnVec4){2000.0f, -50.0f, 0, 0};
    mtwnPlayerId player = Start(context, &def);
    double stop = mtwnDecayStopTime(&def.decay, 2000.0, def.restSpeed);
    CHECK(stop > mtwnDecayStopTime(&def.decay, -50.0, def.restSpeed),
          "the faster channel stops last");
    Update(context, 0);
    Update(context, s_second / 4);
    double position = 0.0;
    double velocity = 0.0;
    mtwnSampleDecay(&def.decay, 100.0, 2000.0, 0.25, &position, &velocity);
    CHECK(State(context, x).value.x == (float)position &&
              State(context, x).velocity.x == (float)velocity,
          "the decay's position and velocity");
    Update(context, (uint64_t)(stop * 1e9) + 1);
    mtwnSampleDecay(&def.decay, 100.0, 2000.0, stop, &position, NULL);
    CHECK(!mtwnPlayer_IsValid(context, player) && State(context, x).base.x == (float)position &&
              State(context, x).velocity.x == 0.0f,
          "it ends where it stopped, committed");
    // An integer coasts to a whole number; an angle takes no turn.
    mtwnTargetId n = Target(context, mtwn_valueInteger, (mtwnVec4){0});
    def.target = n;
    def.velocity = (mtwnVec4){7.0f, 0, 0, 0};
    Start(context, &def);
    Update(context, 100 * s_second);
    Update(context, 200 * s_second);
    float end = State(context, n).base.x;
    CHECK(end == roundf(end) && end > 1.0f, "an integer stops on a whole number");
    mtwnTargetId angle = Target(context, mtwn_valueAngle, (mtwnVec4){1.0f, 0, 0, 0});
    def.target = angle;
    def.angleMethod = mtwn_angleLonger;
    def.velocity = (mtwnVec4){0.0f, 0, 0, 0};
    Start(context, &def);
    Update(context, 300 * s_second);
    CHECK(State(context, angle).base.x == 1.0f, "a decay at rest stays, with no turn added");
    mtwnDestroyContext(context);
}

static void TestPathSprings(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId color = Target(context, mtwn_valueColor, (mtwnVec4){1.0f, 0.0f, 0.0f, 1.0f});
    mtwnTransitionDef def = Linear(color, (mtwnVec4){0.0f, 0.0f, 1.0f, 1.0f}, 1.0);
    def.colorSpace = mtwn_colorSrgb;
    Start(context, &def);
    Update(context, 0);
    Update(context, s_second / 2);
    mtwnVec4 v = State(context, color).velocity;
    mtwnVec4 at = State(context, color).value;
    CHECK(fabsf(v.x + 1.0f) <= 1e-3f && fabsf(v.z - 1.0f) <= 1e-3f, "moving red to blue");
    mtwnTransitionDef spring = SpringTo(color, (mtwnVec4){0.0f, 1.0f, 1.0f, 1.0f});
    spring.colorSpace = mtwn_colorSrgb;
    Start(context, &spring);
    uint32_t index1 = mtwnTargetAt(context, color.index1)->header.link;
    // The velocity projected on cyan minus the point reached: 2/3.
    double d[4] = {0.0 - (double)at.x, 1.0 - (double)at.y, 1.0 - (double)at.z, 1.0 - (double)at.w};
    double along =
        (double)v.x * d[0] + (double)v.y * d[1] + (double)v.z * d[2] + (double)v.w * d[3];
    double length = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] + d[3] * d[3];
    CHECK(fabs(mtwnPlayerAt(context, index1)->velocity[0] - along / length) <= 1e-12 &&
              fabs(along / length - 2.0 / 3.0) <= 1e-3,
          "a path takes the velocity along it");
    Update(context, s_second / 2);
    Update(context, s_second);
    double offset = 0.0;
    mtwnSampleSpring(&spring.spring, -1.0, along / length, 0.5, &offset, NULL);
    mtwnColorMix mix;
    CHECK(mtwnMakeColorMix((mtwnColor){at.x, at.y, at.z, at.w}, (mtwnColor){0.0f, 1.0f, 1.0f, 1.0f},
                           mtwn_colorSrgb, mtwn_hueShorter, &mix) == mtwn_success,
          "the mix");
    mtwnColor want = mtwnMixColor(&mix, 1.0 + offset);
    mtwnVec4 got = State(context, color).value;
    CHECK(got.x == want.red && got.y == want.green && got.z == want.blue,
          "the color at the spring's progress along its path");
    Update(context, 20 * s_second);
    CHECK(State(context, color).base.y == 1.0f && State(context, color).base.z == 1.0f &&
              State(context, color).base.x == 0.0f,
          "and it ends exactly on cyan");
    // An undamped spring never settles: it runs until replaced or stopped.
    mtwnTargetId rotation = Target(context, mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f});
    mtwnTransitionDef forever = SpringTo(rotation, (mtwnVec4){0, 0, 1.0f, 0});
    CHECK(mtwnMakeSpringFromFrequency(1.0, 0.0, &forever.spring) == mtwn_success, "undamped");
    mtwnPlayerId player = Start(context, &forever);
    Update(context, 1000 * s_second);
    CHECK(mtwnPlayer_IsValid(context, player), "still swinging after 1000 seconds");
    mtwnDestroyContext(context);
}

// A scale (record mtwn-0059): eased along its mix, sprung along its mix's
// progress with the start velocity as a rate over the logarithm's
// distance, decayed in its logarithm; zero, negative and an end past a
// float's range refused.
static void TestScales(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId s = Target(context, mtwn_valueScale, (mtwnVec4){1.0f, 0, 0, 0});
    mtwnTransitionDef def = Linear(s, (mtwnVec4){100.0f, 0, 0, 0}, 1.0);
    Start(context, &def);
    Update(context, 0);
    Update(context, s_second / 2);
    mtwnScaleMix mix;
    CHECK(mtwnMakeScaleMix(1.0, 100.0, &mix) == mtwn_success, "the mix");
    mtwnTargetState state = State(context, s);
    CHECK(state.value.x == (float)mtwnMixScale(&mix, 0.5), "halfway from 1 to 100 is 10");
    // d/dt 100^t at 1/2 is 10 ln 100, measured over the last millisecond:
    // a backward difference, under it by about 10 ln(100)^2 / 2000, 0.11.
    CHECK(fabs((double)state.velocity.x - 10.0 * log(100.0)) <= 0.15, "its velocity, 46");
    // A spring from there to 4 takes that velocity as a rate in the
    // logarithm over log(4 / 10).
    mtwnTransitionDef spring = SpringTo(s, (mtwnVec4){4.0f, 0, 0, 0});
    Start(context, &spring);
    uint32_t index1 = mtwnTargetAt(context, s.index1)->header.link;
    double from = (double)state.value.x;
    double progress = (double)state.velocity.x / from / (log(4.0) - log(from));
    CHECK(fabs(mtwnPlayerAt(context, index1)->velocity[0] - progress) <= 1e-12 * fabs(progress),
          "the start velocity as progress along the logarithm");
    Update(context, s_second / 2);
    Update(context, s_second);
    double offset = 0.0;
    mtwnSampleSpring(&spring.spring, -1.0, mtwnPlayerAt(context, index1)->velocity[0], 0.5, &offset,
                     NULL);
    CHECK(mtwnMakeScaleMix(from, 4.0, &mix) == mtwn_success, "the spring's mix");
    CHECK(State(context, s).value.x == (float)mtwnMixScale(&mix, 1.0 + offset),
          "the scale at the spring's progress along its mix");
    Update(context, 30 * s_second);
    CHECK(State(context, s).base.x == 4.0f, "it ends exactly on 4");
    // A spring to the value it holds, given a velocity: progress along a
    // logarithm's distance of 0 has no rate to take it, so it starts still.
    mtwnTransitionDef still = SpringTo(s, (mtwnVec4){4.0f, 0, 0, 0});
    still.velocitySource = mtwn_velocityGiven;
    still.velocity = (mtwnVec4){3.0f, 0, 0, 0};
    Start(context, &still);
    CHECK(mtwnPlayerAt(context, mtwnTargetAt(context, s.index1)->header.link)->velocity[0] == 0.0,
          "a spring to its own scale starts still");
    // A decay: 3 a second at 1 is a rate of 3 in the logarithm.
    mtwnTargetId z = Target(context, mtwn_valueScale, (mtwnVec4){2.0f, 0, 0, 0});
    mtwnTransitionDef coast = mtwnDefaultTransitionDef();
    coast.target = z;
    coast.spec = mtwn_transitionDecay;
    coast.velocitySource = mtwn_velocityGiven;
    coast.velocity = (mtwnVec4){6.0f, 0, 0, 0};
    mtwnPlayerId player = Start(context, &coast);
    Update(context, 40 * s_second);
    Update(context, 40 * s_second + s_second / 4);
    double position = 0.0;
    double rate = 0.0;
    mtwnSampleDecay(&coast.decay, log(2.0), 3.0, 0.25, &position, &rate);
    CHECK(fabsf(State(context, z).value.x - (float)exp(position)) <=
                  1e-6f * State(context, z).value.x &&
              fabs((double)State(context, z).velocity.x - exp(position) * rate) <=
                  1e-5 * exp(position) * rate,
          "the decay in the logarithm, its velocity the value times the rate");
    double stop = mtwnDecayStopTime(&coast.decay, 3.0, coast.restSpeed);
    Update(context, 40 * s_second + (uint64_t)(stop * 1e9) + 1);
    mtwnSampleDecay(&coast.decay, log(2.0), 3.0, stop, &position, NULL);
    CHECK(!mtwnPlayer_IsValid(context, player) &&
              fabsf(State(context, z).base.x - (float)exp(position)) <=
                  1e-6f * (float)exp(position),
          "it ends where its logarithm stopped");
    // An easing far past 1 takes the scale past a float's range: it holds
    // at the largest float, never infinite.
    mtwnLinearPoint points[2] = {{0.0, 0.0}, {1.0, 400.0}};
    def = Linear(s, (mtwnVec4){10.0f, 0, 0, 0}, 1.0);
    CHECK(mtwnMakeLinearEasing(points, 2, &def.easing) == mtwn_success, "linear() to 400");
    Start(context, &def);
    Update(context, 1000 * s_second);
    Update(context, 1000 * s_second + s_second / 2);
    CHECK(State(context, s).value.x == 0x1.fffffep127f, "ten to the 200th held at the largest");
    // Refusals.
    mtwnTargetId unused = {0, 0};
    CHECK(mtwnCreateTarget(context, mtwn_valueScale, (mtwnVec4){0.0f, 0, 0, 0}, &unused) ==
                  mtwn_errorInvalid &&
              mtwnCreateTarget(context, mtwn_valueScale, (mtwnVec4){-1.0f, 0, 0, 0}, &unused) ==
                  mtwn_errorInvalid,
          "a scale of 0 or below refused");
    mtwnPlayerId refused = {0, 0};
    def = Linear(s, (mtwnVec4){0.0f, 0, 0, 0}, 1.0);
    CHECK(mtwnStartTransition(context, &def, &refused) == mtwn_errorInvalid, "a transition to 0");
    coast.velocity = (mtwnVec4){1e30f, 0, 0, 0};
    CHECK(mtwnStartTransition(context, &coast, &refused) == mtwn_errorInvalid,
          "a decay stopping past a float's range");
    mtwnDestroyContext(context);
}

// A transition's distance for a duration set by speed (record
// mtwn-0060), in each kind's own measure; nothing started or changed.
static void TestMeasure(void)
{
    mtwnContext* context = Make(8, 4);
    double d = 0.0;
    mtwnTransitionDef def =
        Linear(Target(context, mtwn_valueVec2, (mtwnVec4){0}), (mtwnVec4){3.0f, 4.0f, 0, 0}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && d == 5.0, "a vector: 5");
    def = Linear(Target(context, mtwn_valueInteger, (mtwnVec4){2.0f, 0, 0, 0}),
                 (mtwnVec4){-1.0f, 0, 0, 0}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && d == 3.0, "an integer: 3");
    const double deg = 3.14159265358979323846 / 180.0;
    def = Linear(Target(context, mtwn_valueAngle, (mtwnVec4){(float)(350 * deg), 0, 0, 0}),
                 (mtwnVec4){(float)(10 * deg), 0, 0, 0}, 1.0);
    def.angleMethod = mtwn_angleShorter;
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && fabs(d - 20 * deg) <= 1e-6,
          "an angle the shorter way: 20 degrees");
    def.angleMethod = mtwn_angleLonger;
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && fabs(d - 340 * deg) <= 1e-6,
          "the longer way: 340");
    // A quarter turn about z, and the other way round.
    float s = (float)sin(3.14159265358979323846 / 4.0);
    def = Linear(Target(context, mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f}),
                 (mtwnVec4){0, 0, s, s}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && fabs(d - 90 * deg) <= 1e-6,
          "a rotation: the angle it turns, 90 degrees");
    def.rotationPath = mtwn_rotationLongest;
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && fabs(d - 270 * deg) <= 1e-6,
          "the longest way: 270");
    def = Linear(Target(context, mtwn_valueScale, (mtwnVec4){2.0f, 0, 0, 0}),
                 (mtwnVec4){8.0f, 0, 0, 0}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && fabs(d - log(4.0)) <= 1e-15,
          "a scale: ln 4");
    // A color by deltaEOK with alpha: a fade alone has a length.
    mtwnTargetId red = Target(context, mtwn_valueColor, (mtwnVec4){1.0f, 0, 0, 1.0f});
    def = Linear(red, (mtwnVec4){1.0f, 0, 0, 0.0f}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && fabs(d - 1.0) <= 1e-12,
          "a fade: its alpha, 1");
    def = Linear(red, (mtwnVec4){0, 0, 1.0f, 1.0f}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_success && fabs(d - 0.537) <= 1e-3,
          "red to blue: deltaEOK 0.537");
    // The recipe: at 2 a second, within 0.15 and 1 second.
    def.duration = fmin(fmax(d / 2.0, 0.15), 1.0);
    CHECK(def.duration > 0.26 && def.duration < 0.28, "a duration from it");
    // Refusals; nothing started.
    def =
        Linear(Target(context, mtwn_valueDiscrete, (mtwnVec4){0}), (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_errorInvalid && d == 0.0,
          "a discrete value has no distance");
    mtwnTargetId gone = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    CHECK(mtwnDestroyTarget(context, gone) == mtwn_success, "destroyed");
    def = Linear(gone, (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_errorStale, "a stale target");
    def = Linear(red, (mtwnVec4){1.0f, 0, 0, 2.0f}, 1.0);
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_errorInvalid, "an alpha over 1");
    def.cookie = 0;
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_errorInvalid &&
              mtwnMeasureTransition(context, NULL, &d) == mtwn_errorInvalid &&
              mtwnMeasureTransition(NULL, &def, &d) == mtwn_errorInvalid,
          "no cookie, no def, no context");
    def = Linear(red, (mtwnVec4){0, 0, 1.0f, 1.0f}, 1.0);
    def.version += 1u;
    CHECK(mtwnMeasureTransition(context, &def, &d) == mtwn_errorVersion, "another version");
    def.version -= 1u;
    CHECK(mtwnMeasureTransition(context, &def, NULL) == mtwn_errorInvalid, "nowhere to put it");
    CHECK(mtwnTargetAt(context, red.index1)->header.link == 0 &&
              State(context, red).value.x == 1.0f,
          "nothing started or changed");
    mtwnDestroyContext(context);
}

static void TestAngleTurns(void)
{
    mtwnContext* context = Make(4, 4);
    mtwnTargetId angle = Target(context, mtwn_valueAngle, (mtwnVec4){1.0f, 0, 0, 0});
    mtwnTransitionDef def = Linear(angle, (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    def.angleMethod = mtwn_angleLonger;
    Start(context, &def);
    Update(context, 0);
    Update(context, s_second - 1);
    float near = State(context, angle).value.x;
    Update(context, s_second);
    float turned = (float)mtwnMixAngle(1.0, 1.0, mtwn_angleLonger, 1.0);
    CHECK(State(context, angle).base.x == turned && fabsf(near - turned) <= 1e-6f,
          "the end moved by its turn is what is committed: no jump of a turn");
    mtwnDestroyContext(context);
}

static void TestSpecRefusals(void)
{
    mtwnContext* context = Make(8, 8);
    mtwnTargetId flag = Target(context, mtwn_valueDiscrete, (mtwnVec4){0});
    mtwnTargetId color = Target(context, mtwn_valueColor, (mtwnVec4){0, 0, 0, 1.0f});
    mtwnTargetId x = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    mtwnPlayerId id;
    mtwnTransitionDef def = SpringTo(flag, (mtwnVec4){1.0f, 0, 0, 0});
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a discrete spring");
    def = SpringTo(color, (mtwnVec4){0, 0, 0, 1.0f});
    def.spec = mtwn_transitionDecay;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a color decay");
    def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    def.spring = (mtwnSpring){{0.0}, 0, 0};
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a spring not made");
    def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    def.restOffset = 0.0;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "no rest offset");
    def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    def.restSpeed = (double)NAN;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a NaN rest speed");
    def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    def.restOffset = HUGE_VAL;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "an infinite rest offset");
    def = SpringTo(x, (mtwnVec4){NAN, 0, 0, 0});
    def.spec = mtwn_transitionDecay;
    def.restSpeed = 0.0;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a decay never at rest");
    def.restSpeed = 0.01;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_success, "a decay reads no end value");
    def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    def.spec = mtwn_transitionDecay;
    def.decay.rate = 0.0;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a decay not made");
    def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    def.velocitySource = mtwn_velocityGiven;
    def.velocity = (mtwnVec4){INFINITY, 0, 0, 0};
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "an infinite velocity");
    def.velocity = (mtwnVec4){0, NAN, 0, 0};
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_success,
          "channels past the kind's are not read");
    def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    def.velocitySource = 2;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a velocity source");
    def = SpringTo(x, (mtwnVec4){1.0f, 0, 0, 0});
    def.spec = 3;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a spec");
    mtwnDestroyContext(context);
}

static void TestRefusals(void)
{
    mtwnContext* context = Make(4, 1);
    mtwnTargetId x = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    mtwnTargetId y = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    mtwnTransitionDef def = Linear(x, (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    mtwnPlayerId id;
    CHECK(mtwnStartTransition(NULL, &def, &id) == mtwn_errorInvalid, "no context");
    CHECK(mtwnStartTransition(context, NULL, &id) == mtwn_errorInvalid, "no def");
    CHECK(mtwnStartTransition(context, &def, NULL) == mtwn_errorInvalid, "no out");
    def.cookie = 0;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid && id.index1 == 0,
          "no cookie");
    def = Linear(x, (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    def.version = MTWN_ABI_VERSION + 1;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorVersion, "another version");
    def = Linear(x, (mtwnVec4){NAN, 0, 0, 0}, 1.0);
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a NaN end");
    def = Linear(x, (mtwnVec4){1.0f, 0, 0, 0}, -1.0);
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a negative duration");
    def = Linear(x, (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    def.delay = HUGE_VAL;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "an infinite delay");
    def = Linear((mtwnTargetId){9, 1}, (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorStale, "a stale target");
    uint64_t misuse = mtwnContext_GetMisuseCount(context);
    CHECK(misuse == 8, "each refusal with a context counted");
    def = Linear(x, (mtwnVec4){1.0f, 0, 0, 0}, 1.0);
    mtwnPlayerId first = Start(context, &def);
    def.target = y;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorLimit && id.index1 == 0,
          "the pool full");
    CHECK(mtwnContext_GetMisuseCount(context) == misuse, "a full pool is not misuse");
    CHECK(mtwnDestroyTarget(context, x) == mtwn_success && !mtwnPlayer_IsValid(context, first),
          "destroying a target ends its transition");
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_success, "its slot free again");
    mtwnTargetId color = {0, 0};
    mtwnDestroyContext(context);
    context = Make(4, 4);
    color = Target(context, mtwn_valueColor, (mtwnVec4){0, 0, 0, 1.0f});
    def = Linear(color, (mtwnVec4){0, 0, 0, 2.0f}, 1.0);
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "alpha past 1");
    def = Linear(color, (mtwnVec4){0, 0, 0, 1.0f}, 1.0);
    def.colorSpace = 9;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid, "a color space");
    mtwnTargetId rotation = Target(context, mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f});
    def = Linear(rotation, (mtwnVec4){0, 0, 0, 1.0f}, 1.0);
    def.rotationPath = mtwn_rotationLongest;
    CHECK(mtwnStartTransition(context, &def, &id) == mtwn_errorInvalid,
          "the long way between equal rotations");
    mtwnDestroyContext(context);
}

static void TestUpdateReport(void)
{
    mtwnContext* context = Make(8, 8);
    mtwnTargetId ids[5];
    for (int i = 0; i < 5; ++i)
    {
        ids[i] = Target(context, mtwn_valueScalar, (mtwnVec4){0});
    }
    CHECK(mtwnTarget_SetBase(context, ids[3], (mtwnVec4){1.0f, 0, 0, 0}) == mtwn_success &&
              mtwnTarget_SetBase(context, ids[1], (mtwnVec4){1.0f, 0, 0, 0}) == mtwn_success &&
              mtwnTarget_SetBase(context, ids[3], (mtwnVec4){2.0f, 0, 0, 0}) == mtwn_success,
          "two set, one twice");
    mtwnTransitionDef def = Linear(ids[4], (mtwnVec4){1.0f, 0, 0, 0}, 0.0);
    Start(context, &def);
    CHECK(mtwnDestroyTarget(context, ids[1]) == mtwn_success, "one destroyed");
    mtwnTargetId changed[1];
    uint32_t total = 0;
    CHECK(mtwnUpdate(context, 10, changed, 1, &total) == mtwn_success && total == 2 &&
              changed[0].index1 == ids[3].index1,
          "in the order they changed, once each, the destroyed one gone, the rest counted");
    CHECK(State(context, ids[4]).value.x == 1.0f && State(context, ids[4]).base.x == 1.0f,
          "a zero duration ends at its first update");
    CHECK(mtwnUpdate(context, 10, NULL, 0, NULL) == mtwn_success, "no report wanted");
    CHECK(mtwnUpdate(context, 9, NULL, 0, &total) == mtwn_errorInvalid, "time going back");
    CHECK(mtwnUpdate(context, 11, NULL, 1, &total) == mtwn_errorInvalid, "a capacity, no array");
    CHECK(mtwnUpdate(NULL, 11, NULL, 0, &total) == mtwn_errorInvalid, "no context");
    CHECK(mtwnContext_GetMisuseCount(context) == 2, "the two with a context counted");
    // Setting the base under a transition changes the base only.
    def = Linear(ids[0], (mtwnVec4){4.0f, 0, 0, 0}, 1.0);
    Start(context, &def);
    Update(context, 20);
    CHECK(mtwnTarget_SetBase(context, ids[0], (mtwnVec4){7.0f, 0, 0, 0}) == mtwn_success &&
              State(context, ids[0]).value.x == 0.0f && State(context, ids[0]).base.x == 7.0f,
          "the transition still writes the value");
    Update(context, 20 + s_second);
    CHECK(State(context, ids[0]).base.x == 4.0f, "and its end becomes the base");
    mtwnDestroyContext(context);
}

#if defined(__x86_64__) || defined(_M_X64)
// Flush-to-zero, denormals-are-zero, round toward zero, exceptions masked.
static const uint64_t s_hostSetting = 0x1f80u | (1u << 15) | (1u << 6) | (3u << 13);
#elif defined(__aarch64__) || defined(_M_ARM64)
// Flush-to-zero and round toward zero.
static const uint64_t s_hostSetting = (1ull << 24) | (3ull << 22);
#else
static const uint64_t s_hostSetting = 0;
#endif

// A scenario's every value, hashed by FNV-1a over their bits; the host's
// control register is checked after every call.
static uint64_t Scenario(uint64_t host, int* restoredOut)
{
    uint64_t hash = 0xcbf29ce484222325u;
    mtwnContext* context = Make(8, 8);
    double pi = 3.141592653589793;
    mtwnTargetId ids[4] = {
        Target(context, mtwn_valueScalar, (mtwnVec4){0x1p-120f, 0, 0, 0}),
        Target(context, mtwn_valueColor, (mtwnVec4){0.9f, 0.1f, 0.0f, 1.0f}),
        Target(context, mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f}),
        Target(context, mtwn_valueVec3, (mtwnVec4){1.0f, 2.0f, 3.0f, 0}),
    };
    mtwnVec4 ends[4] = {{0x1p-140f, 0, 0, 0},
                        {0.1f, 0.3f, 0.9f, 0.5f},
                        {(float)sin(pi / 3), 0, 0, (float)cos(pi / 3)},
                        {-1.0f, 1e-30f, 3.3f, 0}};
    // The defs are made first: their default spring and decay compute,
    // and arithmetic in the host's mode would set its status flags.
    mtwnTransitionDef defs[4];
    for (int i = 0; i < 4; ++i)
    {
        defs[i] = mtwnDefaultTransitionDef();
        defs[i].target = ids[i];
        defs[i].value = ends[i];
        defs[i].duration = 0.8;
        defs[i].colorSpace = mtwn_colorOklch;
    }
    uint64_t saved = mtwnFloatControlRead();
    int restored = 1;
    mtwnFloatControlWrite(host);
    for (int i = 0; i < 4; ++i)
    {
        mtwnPlayerId id;
        restored &= mtwnStartTransition(context, &defs[i], &id) == mtwn_success &&
                    mtwnFloatControlRead() == host;
    }
    for (int k = 0; k <= 60; ++k)
    {
        restored &= mtwnUpdate(context, (uint64_t)k * 16666667u, NULL, 0, NULL) == mtwn_success &&
                    mtwnFloatControlRead() == host;
        for (int i = 0; i < 4; ++i)
        {
            mtwnTargetState state;
            restored &= mtwnTarget_GetState(context, ids[i], &state) == mtwn_success;
            unsigned char bytes[32];
            memcpy(bytes, &state.value, 16);
            memcpy(bytes + 16, &state.velocity, 16);
            for (int b = 0; b < 32; ++b)
            {
                hash = (hash ^ bytes[b]) * 0x100000001b3u;
            }
        }
    }
    mtwnFloatControlWrite(saved);
    mtwnDestroyContext(context);
    *restoredOut = restored;
    return hash;
}

static void TestFloatEnvironment(void)
{
    int restored = 0;
    uint64_t host = mtwnFloatControlRead();
    uint64_t plain = Scenario(host, &restored);
    CHECK(restored, "the default environment kept");
    int hostileRestored = 0;
    uint64_t hostile = Scenario(s_hostSetting == 0 ? host : s_hostSetting, &hostileRestored);
    CHECK(hostileRestored, "the host's flush-to-zero and rounding left as it set them");
    CHECK(plain == hostile, "the same bits under the host's flush-to-zero and rounding");
}

static void TestReplacedVelocity(void)
{
    // An eased transition's velocity is computed when read; replaced
    // before the next update, the target keeps the one it had.
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId x;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success,
          "a target");
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = x;
    def.value = (mtwnVec4){10.0f, 0, 0, 0};
    def.duration = 1.0;
    CHECK(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &def.easing) == mtwn_success, "quad out");
    mtwnPlayerId first;
    CHECK(mtwnStartTransition(context, &def, &first) == mtwn_success, "a transition");
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success, "started");
    CHECK(mtwnUpdate(context, s_second / 4, NULL, 0, NULL) == mtwn_success, "a quarter in");
    mtwnTargetState before;
    CHECK(mtwnTarget_GetState(context, x, &before) == mtwn_success && before.velocity.x > 14.0f,
          "moving at about 15 a second");
    // Replaced by a spring to elsewhere, which inherits that velocity.
    def.spec = mtwn_transitionSpring;
    def.value = (mtwnVec4){-5.0f, 0, 0, 0};
    CHECK(mtwnMakeSpringFromDuration(0.5, 0.0, &def.spring) == mtwn_success, "a spring");
    mtwnPlayerId second;
    CHECK(mtwnStartTransition(context, &def, &second) == mtwn_success, "replaced");
    mtwnTargetState after;
    CHECK(mtwnTarget_GetState(context, x, &after) == mtwn_success, "read");
    CHECK(after.velocity.x == before.velocity.x && after.value.x == before.value.x,
          "the velocity it had, until the next update");
    CHECK(mtwnPlayerAt(context, second.index1)->velocity[0] == (double)before.velocity.x,
          "and the spring starts from it");
    mtwnDestroyContext(context);
}

// The update's integer powers, by every mode, give the easing's values: a
// transition of a number from 0 to 1 over a second, at sixteenths.
static void TestPowerModes(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    uint32_t wrong = 0;
    uint64_t now = 0;
    for (uint32_t mode = mtwn_easeIn; mode <= mtwn_easeOutIn; ++mode)
    {
        for (uint32_t n = 2; n <= 5; ++n)
        {
            mtwnTargetId x;
            CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success,
                  "a target");
            mtwnTransitionDef def = mtwnDefaultTransitionDef();
            def.target = x;
            def.value = (mtwnVec4){1.0f, 0, 0, 0};
            def.duration = 1.0;
            CHECK(mtwnMakePowerEasing((mtwnEaseMode)mode, (double)n, &def.easing) == mtwn_success,
                  "a power");
            mtwnPlayerId player;
            CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success &&
                      mtwnUpdate(context, now, NULL, 0, NULL) == mtwn_success,
                  "started");
            for (uint32_t k = 1; k < 16; ++k)
            {
                CHECK(mtwnUpdate(context, now + k * (s_second / 16), NULL, 0, NULL) == mtwn_success,
                      "an update");
                mtwnTargetState state;
                CHECK(mtwnTarget_GetState(context, x, &state) == mtwn_success, "read");
                double expected = mtwnEase(&def.easing, k / 16.0, false);
                wrong += fabs((double)state.value.x - expected) <= 1e-6 ? 0u : 1u;
            }
            now += 2 * s_second;
            CHECK(mtwnUpdate(context, now, NULL, 0, NULL) == mtwn_success, "ended");
        }
    }
    CHECK(wrong == 0, "every mode's power as the easing gives it");
    mtwnDestroyContext(context);
}

static void TestStartVelocity(void)
{
    // A spring started with a velocity reports it at its first update,
    // so a second replacement before the next one inherits it too.
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId x;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success,
          "a target");
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = x;
    def.value = (mtwnVec4){1.0f, 0, 0, 0};
    def.spec = mtwn_transitionSpring;
    def.velocity = (mtwnVec4){5.0f, 0, 0, 0};
    def.velocitySource = mtwn_velocityGiven;
    CHECK(mtwnMakeSpringFromDuration(0.5, 0.0, &def.spring) == mtwn_success, "a spring");
    mtwnPlayerId player;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "flung at 5 a second");
    CHECK(mtwnUpdate(context, s_second, NULL, 0, NULL) == mtwn_success, "its first update");
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(context, x, &state) == mtwn_success && state.value.x == 0.0f &&
              state.velocity.x == 5.0f,
          "at its start, moving at 5");
    def.velocitySource = mtwn_velocityInherit;
    def.value = (mtwnVec4){2.0f, 0, 0, 0};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "replaced at once");
    CHECK(mtwnPlayerAt(context, player.index1)->velocity[0] == 5.0,
          "the replacement keeps the fling");
    // A delay holds the start at rest until it ends.
    def.delay = 0.5;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "a delayed one");
    CHECK(mtwnUpdate(context, s_second + s_second / 4, NULL, 0, NULL) == mtwn_success,
          "in its delay");
    CHECK(mtwnTarget_GetState(context, x, &state) == mtwn_success && state.velocity.x == 0.0f,
          "at rest in the delay");
    mtwnDestroyContext(context);
}

// Starts an eased transition of x to a value over a duration, CSS's
// reversal asked for or not; returns its slot.
static mtwnPlayerSlot* Toward(mtwnContext* context, mtwnTargetId x, float value, double seconds,
                              const mtwnEasing* easing, bool reverses)
{
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = x;
    def.value = (mtwnVec4){value, 0, 0, 0};
    def.duration = seconds;
    def.easing = *easing;
    def.reverses = reverses;
    mtwnPlayerId player;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "a transition");
    return mtwnPlayerAt(context, player.index1);
}

static void TestReversal(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId x;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success,
          "a target");
    mtwnEasing linear = mtwnLinearEasing();
    // 0 to 1 over a second, left a quarter of the way: back in a quarter.
    (void)Toward(context, x, 1.0f, 1.0, &linear, true);
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success, "started");
    CHECK(mtwnUpdate(context, s_second / 4, NULL, 0, NULL) == mtwn_success, "a quarter");
    mtwnPlayerSlot* back = Toward(context, x, 0.0f, 1.0, &linear, true);
    CHECK(back->duration == 0.25 && back->shortening == 0.25 && back->reversingStart.x == 1.0f,
          "the way back shortened to a quarter");
    // Half way back, forward again: the share of the way is CSS's, from
    // the shortened reversal, 0.5 x 0.25 + 0.75.
    CHECK(mtwnUpdate(context, s_second / 4, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, s_second * 3 / 8, NULL, 0, NULL) == mtwn_success,
          "half way back");
    mtwnPlayerSlot* again = Toward(context, x, 1.0f, 1.0, &linear, true);
    CHECK(again->duration == 0.875 && again->reversingStart.x == 0.0f,
          "forward again over seven eighths");
    // Not a reversal: another end, or the reversal not asked for.
    mtwnPlayerSlot* elsewhere = Toward(context, x, 0.5f, 1.0, &linear, true);
    CHECK(elsewhere->duration == 1.0 && elsewhere->shortening == 1.0,
          "another end takes its whole duration");
    CHECK(mtwnUpdate(context, s_second / 2, NULL, 0, NULL) == mtwn_success, "moving");
    mtwnPlayerSlot* unasked = Toward(context, x, 1.0f, 1.0, &linear, false);
    CHECK(unasked->duration == 1.0, "a reversal not asked for takes its whole duration");
    mtwnDestroyContext(context);

    // The share is of the eased way: a quadratic out a quarter of the
    // time in has gone 1 - 0.75^2 of the way.
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success,
          "a target");
    mtwnEasing quadOut;
    CHECK(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &quadOut) == mtwn_success, "quad out");
    (void)Toward(context, x, 1.0f, 1.0, &quadOut, true);
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, s_second / 4, NULL, 0, NULL) == mtwn_success,
          "a quarter of the time");
    back = Toward(context, x, 0.0f, 1.0, &quadOut, true);
    CHECK(back->duration == 0.4375, "back over the share of the way gone");
    // A spring does not shorten, nor shorten the eased one after it.
    mtwnTransitionDef spring = mtwnDefaultTransitionDef();
    spring.target = x;
    spring.spec = mtwn_transitionSpring;
    spring.reverses = true;
    mtwnPlayerId sprung;
    CHECK(mtwnStartTransition(context, &spring, &sprung) == mtwn_success, "a spring");
    CHECK(mtwnUpdate(context, s_second / 2, NULL, 0, NULL) == mtwn_success, "springing");
    mtwnPlayerSlot* after = Toward(context, x, 0.0f, 1.0, &quadOut, true);
    CHECK(after->duration == 1.0, "an easing after a spring takes its whole duration");
    mtwnDestroyContext(context);
}

// The reversal's ends compare as CSS compares computed values: 0 and -0
// are the same end.
static void TestReversalSignedZero(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId x;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){-0.0f, 0, 0, 0}, &x) ==
              mtwn_success,
          "a target at -0");
    mtwnEasing linear = mtwnLinearEasing();
    (void)Toward(context, x, 1.0f, 1.0, &linear, true);
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, s_second / 4, NULL, 0, NULL) == mtwn_success,
          "a quarter in");
    mtwnPlayerSlot* back = Toward(context, x, 0.0f, 1.0, &linear, true);
    CHECK(back->duration == 0.25, "back to +0 from -0 shortened");
    mtwnDestroyContext(context);

    // A jump-start step has gone a quarter of the way at its start: CSS's
    // share is the easing's output, so the way back takes a quarter.
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success,
          "a target");
    mtwnEasing steps;
    CHECK(mtwnMakeSteps(4, mtwn_jumpStart, &steps) == mtwn_success, "steps(4, jump-start)");
    (void)Toward(context, x, 1.0f, 1.0, &steps, true);
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success, "at its start");
    back = Toward(context, x, 0.0f, 1.0, &steps, true);
    CHECK(back->duration == 0.25, "back over the step already taken");
    mtwnDestroyContext(context);

    // Every channel compares: an end differing in the last alone is not
    // the way back.
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId v;
    CHECK(mtwnCreateTarget(context, mtwn_valueVec4, (mtwnVec4){1, 2, 3, 4}, &v) == mtwn_success,
          "a vector");
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = v;
    def.value = (mtwnVec4){5, 6, 7, 8};
    def.duration = 1.0;
    def.reverses = true;
    mtwnPlayerId player;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success &&
              mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, s_second / 4, NULL, 0, NULL) == mtwn_success,
          "a quarter in");
    def.value = (mtwnVec4){1, 2, 3, 9};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success &&
              mtwnPlayerAt(context, player.index1)->duration == 1.0,
          "another end in its last channel takes its whole duration");
    mtwnDestroyContext(context);
}

static void TestReversalEdges(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId x;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){2.0f, 0, 0, 0}, &x) ==
              mtwn_success,
          "a target at 2");
    mtwnEasing linear = mtwnLinearEasing();
    // From 2 to 3 and back to 2 a quarter in: the way back ends a
    // quarter of a second later, its value committed.
    CHECK(mtwnUpdate(context, 5 * s_second, NULL, 0, NULL) == mtwn_success, "at 5 s");
    (void)Toward(context, x, 3.0f, 1.0, &linear, true);
    CHECK(mtwnUpdate(context, 5 * s_second, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, 5 * s_second + s_second / 4, NULL, 0, NULL) == mtwn_success,
          "a quarter in");
    mtwnPlayerSlot* back = Toward(context, x, 2.0f, 1.0, &linear, true);
    CHECK(back->duration == 0.25, "from 2, back over a quarter");
    mtwnTargetState state;
    CHECK(mtwnUpdate(context, 5 * s_second + s_second / 4, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, 5 * s_second + s_second / 2, NULL, 0, NULL) == mtwn_success &&
              mtwnTarget_GetState(context, x, &state) == mtwn_success && state.value.x == 2.0f &&
              state.base.x == 2.0f && x.index1 != 0 &&
              mtwnTargetAt(context, x.index1)->header.link == 0,
          "ended at its shortened end, its value committed");
    // Reversed before the other's first update, or in its delay: it had
    // gone none of the way, so the way back takes no time.
    mtwnPlayerSlot* fresh = Toward(context, x, 3.0f, 1.0, &linear, true);
    (void)fresh;
    mtwnPlayerSlot* undone = Toward(context, x, 2.0f, 1.0, &linear, true);
    CHECK(undone->duration == 0.0, "reversed before it started");
    mtwnTransitionDef late = mtwnDefaultTransitionDef();
    late.target = x;
    late.value = (mtwnVec4){3.0f, 0, 0, 0};
    late.easing = linear;
    late.delay = 0.5;
    late.reverses = true;
    mtwnPlayerId waiting;
    CHECK(mtwnUpdate(context, 6 * s_second, NULL, 0, NULL) == mtwn_success &&
              mtwnStartTransition(context, &late, &waiting) == mtwn_success &&
              mtwnUpdate(context, 6 * s_second, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, 6 * s_second + s_second / 10, NULL, 0, NULL) == mtwn_success,
          "in its delay");
    undone = Toward(context, x, 2.0f, 1.0, &linear, true);
    CHECK(undone->duration == 0.0, "reversed in its delay");
    mtwnDestroyContext(context);
    // An easing that first goes below its start (back in): the share of
    // the way is its size, |p f + 1 - f|.
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success,
          "a target");
    mtwnEasing backIn;
    CHECK(mtwnMakeBackEasing(mtwn_easeIn, 1.70158, &backIn) == mtwn_success, "back in");
    (void)Toward(context, x, 1.0f, 1.0, &backIn, true);
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, s_second / 4, NULL, 0, NULL) == mtwn_success,
          "a quarter in, below the start");
    double p = mtwnEase(&backIn, 0.25, false);
    back = Toward(context, x, 0.0f, 1.0, &backIn, true);
    CHECK(p < 0.0 && back->duration == -p, "back over the size of the share");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestReversal();
    TestPowerModes();
    TestReversalEdges();
    TestReversalSignedZero();
    TestStartVelocity();
    TestReplacedVelocity();
    TestScalar();
    TestDelayAndEasing();
    TestInterruption();
    TestKinds();
    TestHalfTurns();
    TestScaleOvershootPastFloats();
    TestExactness();
    TestSpring();
    TestHandover();
    TestDecay();
    TestPathSprings();
    TestScales();
    TestMeasure();
    TestAngleTurns();
    TestSpecRefusals();
    TestRefusals();
    TestUpdateReport();
    TestFloatEnvironment();
    return s_failures == 0 ? 0 : 1;
}
