// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Transitions' decays bounded and landing (record mtwn-0063): a bounded
// decay plays as mtwnSampleBoundedDecay per channel and ends on the bound
// it reached; a landing plays each channel at its own rate and ends on
// the def's value; scales in their logarithm; angles land after their
// turns; and the refusals.

#include "test_harness.h"

#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static const uint64_t s_second = 1000000000u;
static const double s_degree = 3.14159265358979323846 / 180.0;

static mtwnContext* Make(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
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

static void Update(mtwnContext* context, uint64_t timeNs)
{
    CHECK(mtwnUpdate(context, timeNs, NULL, 0, NULL) == mtwn_success, "updated");
}

static mtwnTargetState State(mtwnContext* context, mtwnTargetId target)
{
    mtwnTargetState state;
    CHECK(mtwnTarget_GetState(context, target, &state) == mtwn_success, "read");
    return state;
}

// A fling: a decay from the given velocity, iOS's deceleration, Motion's
// bounce.
static mtwnTransitionDef Fling(mtwnTargetId target, mtwnVec4 velocity)
{
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = target;
    def.spec = mtwn_transitionDecay;
    def.velocitySource = mtwn_velocityGiven;
    def.velocity = velocity;
    CHECK(mtwnMakeSpring(500.0, 10.0, 1.0, &def.spring) == mtwn_success, "a bounce");
    def.restOffset = 0.5;
    def.restSpeed = 1.0;
    return def;
}

static void TestBounded(void)
{
    mtwnContext* context = Make();
    // x flung past its upper bound, y coasting within: each its own way.
    mtwnTargetId t = Target(context, mtwn_valueVec2, (mtwnVec4){40.0f, 300.0f, 0, 0});
    mtwnTransitionDef def = Fling(t, (mtwnVec4){1500.0f, -200.0f, 0, 0});
    def.bounded = true;
    def.lower = (mtwnVec4){0.0f, 0.0f, 0, 0};
    def.upper = (mtwnVec4){600.0f, 600.0f, 0, 0};
    mtwnPlayerId player = {0, 0};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "a bounded fling");
    Update(context, 0);
    mtwnBoundedDecay x;
    mtwnBoundedDecay y;
    CHECK(mtwnMakeBoundedDecay(&def.decay, &def.spring, 0.0, 600.0, &x) == mtwn_success &&
              mtwnMakeBoundedDecay(&def.decay, &def.spring, 0.0, 600.0, &y) == mtwn_success,
          "its channels alone");
    int differ = 0;
    for (int k = 1; k <= 60; ++k)
    {
        double s = k / 60.0;
        Update(context, (uint64_t)k * s_second / 60u);
        double px = 0.0;
        double py = 0.0;
        mtwnSampleBoundedDecay(&x, 40.0, 1500.0, s, &px, NULL);
        mtwnSampleBoundedDecay(&y, 300.0, -200.0, s, &py, NULL);
        mtwnTargetState state = State(context, t);
        differ += state.value.x != (float)px || state.value.y != (float)py;
    }
    CHECK(differ == 0, "each channel its bounded decay, sampled as alone");
    double stop = mtwnBoundedDecayStopTime(&x, 40.0, 1500.0, 0.5, 1.0);
    Update(context, (uint64_t)(stop * 1e9) + s_second);
    mtwnTargetState end = State(context, t);
    double rest = 0.0;
    mtwnSampleDecay(&def.decay, 300.0, -200.0,
                    mtwnDecayStopTime(&def.decay, -200.0, 1.0) > stop
                        ? mtwnDecayStopTime(&def.decay, -200.0, 1.0)
                        : stop,
                    &rest, NULL);
    CHECK(!mtwnPlayer_IsValid(context, player) && end.base.x == 600.0f &&
              fabsf(end.base.y - (float)rest) <= 1e-3f,
          "ends on the bound it reached, the other where it stopped");
    // Flung down through the lower bound from within: the bounce there.
    mtwnTargetId w = Target(context, mtwn_valueScalar, (mtwnVec4){100.0f, 0, 0, 0});
    def = Fling(w, (mtwnVec4){-1500.0f, 0, 0, 0});
    def.bounded = true;
    def.lower = (mtwnVec4){0.0f, 0, 0, 0};
    def.upper = (mtwnVec4){600.0f, 0, 0, 0};
    uint64_t down = (uint64_t)(stop * 1e9) + s_second;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "flung down");
    Update(context, down);
    mtwnBoundedDecay low;
    CHECK(mtwnMakeBoundedDecay(&def.decay, &def.spring, 0.0, 600.0, &low) == mtwn_success,
          "its decay");
    int off = 0;
    for (int k = 1; k <= 30; ++k)
    {
        Update(context, down + (uint64_t)k * s_second / 30u);
        double pw = 0.0;
        mtwnSampleBoundedDecay(&low, 100.0, -1500.0, k / 30.0, &pw, NULL);
        // Its clock seconds in: within the float's rounding of the decay
        // sampled alone.
        off += fabs((double)State(context, w).value.x - pw) > 1e-6 * fmax(1.0, fabs(pw));
    }
    CHECK(off == 0, "the lower bound's crossing and bounce");
    // Dragged past the lower bound and let go: the bounce from there.
    mtwnTargetId u = Target(context, mtwn_valueScalar, (mtwnVec4){-30.0f, 0, 0, 0});
    def = Fling(u, (mtwnVec4){-100.0f, 0, 0, 0});
    def.bounded = true;
    def.lower = (mtwnVec4){0.0f, 0, 0, 0};
    def.upper = (mtwnVec4){(float)INFINITY, 0, 0, 0};
    uint64_t at = (uint64_t)(stop * 1e9) + 2 * s_second;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "a fling from past one");
    Update(context, at);
    Update(context, at + s_second / 10u);
    double offset = 0.0;
    mtwnSampleSpring(&def.spring, -30.0, -100.0, 0.1, &offset, NULL);
    CHECK(State(context, u).value.x == (float)offset, "the bounce from the start");
    Update(context, at + 10u * s_second);
    CHECK(State(context, u).base.x == 0.0f, "ending on the bound");
    mtwnDestroyContext(context);
}

static void TestLanding(void)
{
    mtwnContext* context = Make();
    // Snapped to (640, 0) from (40, 300): each channel at its own rate.
    mtwnTargetId t = Target(context, mtwn_valueVec2, (mtwnVec4){40.0f, 300.0f, 0, 0});
    mtwnTransitionDef def = Fling(t, (mtwnVec4){1500.0f, -900.0f, 0, 0});
    def.lands = true;
    def.value = (mtwnVec4){640.0f, 0.0f, 0, 0};
    mtwnPlayerId player = {0, 0};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "a landing");
    Update(context, 0);
    Update(context, s_second / 4u);
    mtwnDecay x;
    mtwnDecay y;
    CHECK(mtwnMakeDecayToRest(40.0, 1500.0, 640.0, &x) == mtwn_success &&
              mtwnMakeDecayToRest(300.0, -900.0, 0.0, &y) == mtwn_success,
          "its channels' landings");
    double px = 0.0;
    double py = 0.0;
    mtwnSampleDecay(&x, 40.0, 1500.0, 0.25, &px, NULL);
    mtwnSampleDecay(&y, 300.0, -900.0, 0.25, &py, NULL);
    mtwnTargetState state = State(context, t);
    CHECK(state.value.x == (float)px && state.value.y == (float)py, "each its own decay");
    Update(context, 60u * s_second);
    state = State(context, t);
    CHECK(!mtwnPlayer_IsValid(context, player) && state.base.x == 640.0f && state.base.y == 0.0f,
          "ending on the value exactly");
    // A scale lands in its logarithm: 1 to 4 at 3 a second is a rate of
    // 3 / ln 4.
    mtwnTargetId z = Target(context, mtwn_valueScale, (mtwnVec4){1.0f, 0, 0, 0});
    def = Fling(z, (mtwnVec4){3.0f, 0, 0, 0});
    def.lands = true;
    def.value = (mtwnVec4){4.0f, 0, 0, 0};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "a zoom landing");
    Update(context, 61u * s_second);
    Update(context, 61u * s_second + s_second / 2u);
    mtwnDecay log4;
    CHECK(mtwnMakeDecay(3.0 / log(4.0), &log4) == mtwn_success, "its rate");
    double position = 0.0;
    mtwnSampleDecay(&log4, 0.0, 3.0, 0.5, &position, NULL);
    CHECK(fabsf(State(context, z).value.x - (float)exp(position)) <= 1e-6f, "in the logarithm");
    Update(context, 200u * s_second);
    CHECK(State(context, z).base.x == 4.0f, "ending on 4");
    // An angle lands after the method's turns: 350 degrees to 10 the
    // shorter way is 370.
    mtwnTargetId a =
        Target(context, mtwn_valueAngle, (mtwnVec4){(float)(350.0 * s_degree), 0, 0, 0});
    def = Fling(a, (mtwnVec4){1.0f, 0, 0, 0});
    def.lands = true;
    def.value = (mtwnVec4){(float)(10.0 * s_degree), 0, 0, 0};
    def.restOffset = 1e-3;
    def.restSpeed = 1e-3;
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "an angle landing");
    Update(context, 201u * s_second);
    Update(context, 300u * s_second);
    float want = (float)mtwnMixAngle(350.0 * s_degree, (double)(float)(10.0 * s_degree),
                                     mtwn_angleShorter, 1.0);
    CHECK(State(context, a).base.x == want && want > 6.2f, "ending on 370 degrees");
    mtwnDestroyContext(context);
}

static void TestScaleBounded(void)
{
    // A pinch flung out at a rate of 3 in the logarithm, bounded at 4.
    mtwnContext* context = Make();
    mtwnTargetId z = Target(context, mtwn_valueScale, (mtwnVec4){1.0f, 0, 0, 0});
    mtwnTransitionDef def = Fling(z, (mtwnVec4){3.0f, 0, 0, 0});
    def.bounded = true;
    def.lower = (mtwnVec4){0.5f, 0, 0, 0};
    def.upper = (mtwnVec4){4.0f, 0, 0, 0};
    def.restOffset = 1e-3;
    def.restSpeed = 1e-2;
    mtwnPlayerId player = {0, 0};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "a bounded zoom");
    Update(context, 0);
    mtwnBoundedDecay logs;
    CHECK(mtwnMakeBoundedDecay(&def.decay, &def.spring, log(0.5), log(4.0), &logs) == mtwn_success,
          "its logarithm's bounds");
    int differ = 0;
    for (int k = 1; k <= 30; ++k)
    {
        Update(context, (uint64_t)k * s_second / 30u);
        double position = 0.0;
        mtwnSampleBoundedDecay(&logs, 0.0, 3.0, k / 30.0, &position, NULL);
        float got = State(context, z).value.x;
        differ += fabsf(got - (float)exp(position)) > 1e-6f * got;
    }
    CHECK(differ == 0, "the bounded decay in the logarithm");
    Update(context, 100u * s_second);
    CHECK(State(context, z).base.x == 4.0f, "ending on 4");
    mtwnDestroyContext(context);
}

// A zoom flung far past the floats (1e12 a second in the logarithm):
// its value holds at the largest float and its velocity stays finite,
// where it was the infinite value's times its rate.
static void TestScalePastFloats(void)
{
    mtwnContext* context = Make();
    mtwnTargetId z = Target(context, mtwn_valueScale, (mtwnVec4){0.5f, 0, 0, 0});
    mtwnTransitionDef def = Fling(z, (mtwnVec4){1e12f, 0, 0, 0});
    def.bounded = true;
    def.lower = (mtwnVec4){0.875f, 0, 0, 0};
    def.upper = def.lower;
    mtwnPlayerId player = {0, 0};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success, "a fling past the floats");
    int infinite = 0;
    for (int k = 0; k <= 30; ++k)
    {
        Update(context, (uint64_t)k * s_second / 10u);
        mtwnTargetState state = State(context, z);
        infinite += !isfinite(state.value.x) || !isfinite(state.velocity.x);
    }
    CHECK(infinite == 0, "finite throughout");
    mtwnDestroyContext(context);
}

static void TestRefusals(void)
{
    mtwnContext* context = Make();
    mtwnTargetId t = Target(context, mtwn_valueScalar, (mtwnVec4){40.0f, 0, 0, 0});
    mtwnTargetId z = Target(context, mtwn_valueScale, (mtwnVec4){1.0f, 0, 0, 0});
    mtwnPlayerId player = {0, 0};
    int refused = 0;
    mtwnTransitionDef def = Fling(t, (mtwnVec4){1500.0f, 0, 0, 0});
    def.bounded = true;
    def.lands = true;
    def.value = (mtwnVec4){640.0f, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def.lands = false;
    def.lower = (mtwnVec4){10.0f, 0, 0, 0};
    def.upper = (mtwnVec4){5.0f, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def.lower = (mtwnVec4){(float)NAN, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def.lower = (mtwnVec4){0.0f, 0, 0, 0};
    def.upper = (mtwnVec4){(float)NAN, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def.lower = (mtwnVec4){(float)INFINITY, 0, 0, 0};
    def.upper = (mtwnVec4){(float)INFINITY, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def.lower = (mtwnVec4){-(float)INFINITY, 0, 0, 0};
    def.upper = (mtwnVec4){-(float)INFINITY, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def.lower = (mtwnVec4){0.0f, 0, 0, 0};
    def.upper = (mtwnVec4){600.0f, 0, 0, 0};
    def.restOffset = 0.0;
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def = Fling(z, (mtwnVec4){3.0f, 0, 0, 0});
    def.bounded = true;
    def.upper = (mtwnVec4){4.0f, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def = Fling(t, (mtwnVec4){-1500.0f, 0, 0, 0});
    def.lands = true;
    def.value = (mtwnVec4){640.0f, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    def.velocity = (mtwnVec4){0.0f, 0, 0, 0};
    refused += mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid;
    CHECK(refused == 10, "bounded and landing, bounds the wrong way round or NaN on either "
                         "side, a lower bound of infinity or an upper of -infinity, no rest "
                         "tolerance, a scale's bound of 0, a landing "
                         "behind or with no velocity refused");
    def.value = (mtwnVec4){40.0f, 0, 0, 0};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_success,
          "landing where it is with no velocity: it stays");
    def.velocity = (mtwnVec4){100.0f, 0, 0, 0};
    CHECK(mtwnStartTransition(context, &def, &player) == mtwn_errorInvalid,
          "landing where it is with a velocity: refused, no rate takes it there");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestBounded();
    TestLanding();
    TestScaleBounded();
    TestScalePastFloats();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
