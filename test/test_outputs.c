// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Outputs on failure: each output filled with bytes of
// 0xAB, each call made to fail. A call on a context clears its outputs, a
// missing context's failure too; a call without one leaves them as they
// were; arrays rewritten in place are as they were where the header says
// so. The busy refusals are test_reentry's.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
#include "maul-tween/player.h"
#include "maul-tween/sample.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static bool AllBytes(const void* bytes, size_t size, unsigned char value)
{
    const unsigned char* b = bytes;
    for (size_t i = 0; i < size; ++i)
    {
        if (b[i] != value)
        {
            return false;
        }
    }
    return true;
}

// Filled with 0xAB, then the call refused and the output cleared.
#define CLEARED(out, call, what)                                                                   \
    do                                                                                             \
    {                                                                                              \
        memset(&(out), 0xAB, sizeof(out));                                                         \
        CHECK((call) != mtwn_success && AllBytes(&(out), sizeof(out), 0), what);                   \
    } while (0)

// Filled with 0xAB, then the call refused and the output as it was.
#define UNCHANGED(out, call, what)                                                                 \
    do                                                                                             \
    {                                                                                              \
        memset(&(out), 0xAB, sizeof(out));                                                         \
        CHECK((call) != mtwn_success && AllBytes(&(out), sizeof(out), 0xAB), what);                \
    } while (0)

static void TestOnContext(void)
{
    const mtwnTargetId noTarget = {0, 0};
    const mtwnPlayerId noPlayer = {0, 0};
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CLEARED(context, mtwnCreateContext(NULL, &context), "a context: NULL");
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    // A missing context and a refused argument alike.
    for (int missing = 0; missing < 2; ++missing)
    {
        mtwnContext* c = missing != 0 ? NULL : context;
        mtwnTargetId target;
        CLEARED(target, mtwnCreateTarget(c, (mtwnValueKind)99, (mtwnVec4){0}, &target),
                "a target: the null id");
        mtwnVec4 value;
        CLEARED(value, mtwnTarget_GetValue(c, noTarget, &value), "a value: zero");
        mtwnTargetState targetState;
        CLEARED(targetState, mtwnTarget_GetState(c, noTarget, &targetState), "a state: zero");
        mtwnPlayerId player;
        CLEARED(player, mtwnStartTransition(c, NULL, &player), "a transition: the null id");
        double seconds;
        CLEARED(seconds, mtwnMeasureTransition(c, NULL, &seconds), "a distance: 0");
        mtwnClipId clip;
        CLEARED(clip, mtwnCreateClip(c, NULL, &clip), "a clip: the null id");
        uint64_t bytes;
        CLEARED(bytes, mtwnGetClipBytes(c, NULL, &bytes), "a clip's bytes: 0");
        mtwnClipId noClip = {0, 0};
        mtwnNode node;
        CLEARED(node, mtwnClip_AddTrack(c, noClip, NULL, &node), "a track: 0");
        CLEARED(node, mtwnClip_AddTimeline(c, noClip, NULL, &node), "a timeline: 0");
        CLEARED(node, mtwnClip_AddLabel(c, noClip, MTWN_ROOT_NODE, at, 1, &node), "a label: 0");
        CLEARED(node, mtwnClip_AddMark(c, noClip, MTWN_ROOT_NODE, at, 1, &node), "a mark: 0");
        mtwnClipTrack track;
        CLEARED(track, mtwnClip_GetTrack(c, noClip, 1, &track), "a clip's track: zero");
        mtwnClipNodeInfo info;
        CLEARED(info, mtwnClip_GetNode(c, noClip, MTWN_ROOT_NODE, &info), "a node: zero");
        CLEARED(seconds, mtwnClip_FindLabel(c, noClip, MTWN_ROOT_NODE, 1, &seconds),
                "a label's time: 0");
        CLEARED(seconds, mtwnClip_GetDuration(c, noClip, &seconds), "a duration: 0");
        CLEARED(player, mtwnCreatePlayer(c, NULL, &player), "a player: the null id");
        mtwnPlayerState playerState;
        CLEARED(playerState, mtwnPlayer_GetState(c, noPlayer, &playerState), "its state: zero");
        uint32_t count;
        CLEARED(count, mtwnReadEvents(c, NULL, 1, &count, NULL), "events taken: 0");
        uint64_t dropped;
        CLEARED(dropped, mtwnReadEvents(c, NULL, 1, &count, &dropped), "events dropped: 0");
    }
    // An update back in time changes nothing; its total is 0, its report
    // unwritten.
    CHECK(mtwnUpdate(context, 1000, NULL, 0, NULL) == mtwn_success, "updated");
    mtwnTargetId changed[2];
    memset(changed, 0xAB, sizeof changed);
    uint32_t total = 7;
    CHECK(mtwnUpdate(context, 10, changed, 2, &total) == mtwn_errorInvalid && total == 0 &&
              AllBytes(changed, sizeof changed, 0xAB),
          "an update back in time: 0 changed, nothing written");
    mtwnTargetId target;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
          "a target");
    mtwnVec4 values[2];
    memset(values, 0xAB, sizeof values);
    CHECK(mtwnReadTargets(context, NULL, 2, values) == mtwn_errorInvalid &&
              AllBytes(values, sizeof values, 0xAB),
          "targets read with no ids: nothing written");
    mtwnDestroyContext(context);
}

static void TestMakers(void)
{
    mtwnEasing easing;
    UNCHANGED(easing, mtwnMakePowerEasing(mtwn_easeIn, -1.0, &easing), "a power easing");
    UNCHANGED(easing, mtwnMakePennerEasing((mtwnEasingKind)99, mtwn_easeIn, &easing),
              "a Penner easing");
    UNCHANGED(easing, mtwnMakeBackEasing(mtwn_easeOut, (double)NAN, &easing), "a back easing");
    UNCHANGED(easing, mtwnMakeElasticEasing(mtwn_easeOut, (double)NAN, 0.3, &easing), "an elastic");
    UNCHANGED(easing, mtwnMakeHostEasing(mtwn_easeIn, NULL, NULL, &easing), "a host easing");
    UNCHANGED(easing, mtwnMakeCubicBezierEasing(2.0, 0.0, 0.5, 1.0, &easing), "a cubic bezier");
    UNCHANGED(easing, mtwnMakeStepsEasing(0, mtwn_jumpEnd, &easing), "steps");
    UNCHANGED(easing, mtwnMakeCssEasing((mtwnCssKeyword)99, &easing), "a CSS keyword");
    mtwnLinearPoint points[3] = {
        {(double)NAN, 0.0}, {(double)NAN, 0.5}, {(double)NAN, (double)INFINITY}};
    mtwnLinearPoint given[3];
    memcpy(given, points, sizeof points);
    UNCHANGED(easing, mtwnMakeLinearPointsEasing(points, 3, &easing), "linear()");
    CHECK(memcmp(points, given, sizeof points) == 0, "its points as they were");
    mtwnSpring spring;
    UNCHANGED(spring, mtwnMakeSpring(-1.0, 1.0, 1.0, &spring), "a spring");
    UNCHANGED(spring, mtwnMakeSpringFromFrequency(0.0, 1.0, &spring), "a spring by frequency");
    UNCHANGED(spring, mtwnMakeSpringFromDuration(-1.0, 0.0, &spring), "a spring by duration");
    UNCHANGED(easing, mtwnMakeSpringEasing(NULL, 1.0, 0.0, mtwn_easeIn, &easing),
              "a spring easing");
    mtwnDecay decay;
    UNCHANGED(decay, mtwnMakeDecay(-1.0, &decay), "a decay");
    UNCHANGED(decay, mtwnMakeDecayFromDeceleration(2.0, &decay), "a decay by deceleration");
    UNCHANGED(decay, mtwnMakeDecayToRest(0.0, 0.0, 1.0, &decay), "a decay to rest");
    mtwnBoundedDecay bounded;
    UNCHANGED(bounded, mtwnMakeBoundedDecay(NULL, NULL, 0.0, 1.0, &bounded), "a bounded decay");
    mtwnColorMix colorMix;
    UNCHANGED(colorMix,
              mtwnMakeColorMix((mtwnColor){NAN, 0, 0, 1}, (mtwnColor){0, 0, 0, 1}, mtwn_colorSrgb,
                               mtwn_hueShorter, &colorMix),
              "a color mix");
    double coordinates[4];
    UNCHANGED(coordinates, mtwnColorToSpace((mtwnColor){NAN, 0, 0, 1}, mtwn_colorSrgb, coordinates),
              "a color's coordinates");
    mtwnColor color;
    double far[4] = {(double)NAN, 0.0, 0.0, 1.0};
    UNCHANGED(color, mtwnColorFromSpace(far, mtwn_colorSrgb, &color), "a color");
    mtwnRotationMix rotationMix;
    UNCHANGED(rotationMix,
              mtwnMakeRotationMix((mtwnQuat){0, 0, 0, 0}, (mtwnQuat){0, 0, 0, 1},
                                  mtwn_rotationShortest, &rotationMix),
              "a rotation mix");
    mtwnScaleMix scaleMix;
    UNCHANGED(scaleMix, mtwnMakeScaleMix(-1.0, 1.0, &scaleMix), "a scale mix");
    mtwnZoomMix zoomMix;
    UNCHANGED(zoomMix,
              mtwnMakeZoomMix((mtwnZoomView){0, 0, -1.0}, (mtwnZoomView){0, 0, 1.0}, 1.4, &zoomMix),
              "a zoom mix");
    mtwnPunch punch;
    UNCHANGED(punch, mtwnMakePunch(-1.0, 1.0, 1.0, &punch), "a punch");
    mtwnShake shake;
    UNCHANGED(shake, mtwnMakeShake(-1.0, 1.0, 1.0, 1, &shake), "a shake");
    float samples[4];
    UNCHANGED(samples, mtwnSampleEasing(NULL, 4, samples), "an easing's samples");
    double offsets[4];
    UNCHANGED(offsets, mtwnComputeStagger(NULL, 4, offsets), "a stagger's offsets");
    uint32_t fitted;
    UNCHANGED(fitted, mtwnFitLinearEasing(NULL, 0.01, NULL, 0, &fitted), "a fit's count");
}

static void TestPathsAndTracks(void)
{
    mtwnKeyframe keys[3];
    memset(keys, 0, sizeof keys);
    for (int i = 0; i < 3; ++i)
    {
        keys[i].offset = 0.5 * i;
        keys[i].easing = mtwnLinearEasing();
    }
    keys[2].value.x = NAN;
    double lengths[3];
    mtwnPath path;
    memset(lengths, 0xAB, sizeof lengths);
    UNCHANGED(path, mtwnMakePath(keys, 3, 1, 0.5, lengths, &path), "a path");
    CHECK(AllBytes(lengths, sizeof lengths, 0xAB), "its lengths unwritten");
    // Rotations: the first segment's mix made, the second refused (the
    // longest way between equal rotations); nothing written.
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueRotation);
    def.rotationPath = mtwn_rotationLongest;
    keys[0].value = (mtwnVec4){0, 0, 0, 1.0f};
    keys[1].value = (mtwnVec4){0, 0, 1.0f, 0};
    keys[2].value = keys[1].value;
    mtwnKeyframe given[3];
    memcpy(given, keys, sizeof keys);
    mtwnSegmentMix mixes[2];
    mtwnTrack track;
    memset(mixes, 0xAB, sizeof mixes);
    UNCHANGED(track, mtwnMakeTrack(&def, keys, 3, NULL, mixes, &track), "a track");
    CHECK(AllBytes(mixes, sizeof mixes, 0xAB), "its mixes unwritten");
    CHECK(memcmp(keys, given, sizeof keys) == 0, "its keyframes as they were");
}

// The samplers of springs and decays: a spring, decay or bounded decay
// no maker produced, or an argument outside its range, refused with every
// output as it was.
static void TestSamplers(void)
{
    mtwnEasing easing;
    CHECK(mtwnMakeCssEasing(mtwn_cssEase, &easing) == mtwn_success, "an easing");
    double eased;
    UNCHANGED(eased, mtwnEase(&easing, (double)NAN, false, &eased), "an easing at NaN");
    UNCHANGED(eased, mtwnEaseCubicBezier(&easing, HUGE_VAL, &eased), "a bezier at infinity");
    mtwnEasing forgedEasing = easing;
    forgedEasing.parameters[0] = 2.0;
    bool monotonic;
    UNCHANGED(monotonic, mtwnIsEasingMonotonic(&forgedEasing, &monotonic),
              "a forged easing's monotonicity");
    mtwnSpring spring;
    CHECK(mtwnMakeSpringFromDuration(0.5, 0.2, &spring) == mtwn_success, "a spring");
    mtwnSpring forged = spring;
    forged.parameters[2] *= 2.0;
    mtwnSpringMotion motion;
    UNCHANGED(motion, mtwnSpringMotionAt(&forged, 0.1, &motion), "a forged spring's motion");
    UNCHANGED(motion, mtwnSpringMotionAt(&spring, -1.0, &motion), "a motion before the start");
    double values[2];
    memset(values, 0xAB, sizeof values);
    CHECK(mtwnSampleSpring(&spring, (double)NAN, 0.0, 0.1, &values[0], &values[1]) ==
                  mtwn_errorInvalid &&
              AllBytes(values, sizeof values, 0xAB),
          "a spring from a NaN start");
    double seconds;
    UNCHANGED(seconds, mtwnSpringSettlingTime(&spring, 1.0, 0.0, 0.0, 1e-3, &seconds),
              "a settling time to no tolerance");
    mtwnDecay decay;
    CHECK(mtwnMakeDecay(2.0, &decay) == mtwn_success, "a decay");
    mtwnDecay still = {0.0};
    memset(values, 0xAB, sizeof values);
    CHECK(mtwnSampleDecay(&still, 0.0, 1.0, 0.1, &values[0], &values[1]) == mtwn_errorInvalid &&
              AllBytes(values, sizeof values, 0xAB),
          "a decay of rate 0");
    UNCHANGED(seconds, mtwnDecayStopTime(&decay, 1.0, 0.0, &seconds), "a stop at speed 0");
    UNCHANGED(seconds, mtwnDecayCrossingTime(&decay, 0.0, 1.0, (double)INFINITY, &seconds),
              "a crossing of infinity");
    mtwnBoundedDecay bounded;
    CHECK(mtwnMakeBoundedDecay(&decay, &spring, 0.0, 1.0, &bounded) == mtwn_success,
          "a bounded decay");
    mtwnBoundedDecay crossed = bounded;
    crossed.lower = 2.0;
    memset(values, 0xAB, sizeof values);
    CHECK(mtwnSampleBoundedDecay(&crossed, 0.5, 1.0, 0.1, &values[0], &values[1]) ==
                  mtwn_errorInvalid &&
              AllBytes(values, sizeof values, 0xAB),
          "bounds crossed");
    UNCHANGED(seconds, mtwnBoundedDecayStopTime(&bounded, 0.5, 1.0, 0.5, (double)NAN, &seconds),
              "a stop at a NaN speed");
}

int main(void)
{
    TestOnContext();
    TestMakers();
    TestPathsAndTracks();
    TestSamplers();
    return s_failures == 0 ? 0 : 1;
}
