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
    CHECK(context->players.alive == 0, "its slot back in the pool");
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
    uint64_t saved = mtwnFloatControlRead();
    int restored = 1;
    mtwnFloatControlWrite(host);
    for (int i = 0; i < 4; ++i)
    {
        mtwnTransitionDef def = mtwnDefaultTransitionDef();
        def.target = ids[i];
        def.value = ends[i];
        def.duration = 0.8;
        def.colorSpace = mtwn_colorOklch;
        mtwnPlayerId id;
        restored &= mtwnStartTransition(context, &def, &id) == mtwn_success &&
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

int main(void)
{
    TestScalar();
    TestDelayAndEasing();
    TestInterruption();
    TestKinds();
    TestRefusals();
    TestUpdateReport();
    TestFloatEnvironment();
    return s_failures == 0 ? 0 : 1;
}
