// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Every public call that computes, under every host test_fenv.h plays:
// each rounding mode, each with flush-to-zero and denormals-are-zero off
// and on. A case makes its inputs from constants and the library's own
// calls, does no float arithmetic of its own, and writes every result it
// gets; it runs once in the default setting and then under each host, and
// its bytes must be the same, with the host's setting as it set it after
// every case.

#include "test_fenv.h"
#include "test_harness.h"
#include "test_values.h"

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

#include <stdint.h>
#include <string.h>

typedef struct Out
{
    unsigned char bytes[1 << 15];
    size_t at;
    bool full;
} Out;

static void Put(Out* out, const void* value, size_t size)
{
    if (out->at + size > sizeof out->bytes)
    {
        out->full = true;
        return;
    }
    memcpy(out->bytes + out->at, value, size);
    out->at += size;
}

#define PUT(out, value) Put((out), &(value), sizeof(value))

// Progress values: either end, outside them, a subnormal (which
// denormals-are-zero reads as 0), and between.
static const double s_progress[] = {-0.25, 0.0,   0x1p-1030, 0.1, 0.3333333333333333,
                                    0.5,   0.625, 0.999,     1.0, 1.4};
enum
{
    s_progressCount = sizeof s_progress / sizeof s_progress[0]
};

static void EaseAll(Out* out, const mtwnEasing* easing)
{
    for (int i = 0; i < s_progressCount; ++i)
    {
        double eased = EaseAt(easing, s_progress[i], false);
        PUT(out, eased);
    }
}

static void Easings(Out* out)
{
    mtwnEasing e;
    mtwnResult r = mtwnMakePowerEasing(mtwn_easeInOut, 2.5, &e);
    PUT(out, r);
    EaseAll(out, &e);
    static const mtwnEasingKind s_penner[] = {mtwn_easingSine, mtwn_easingExpo, mtwn_easingCirc,
                                              mtwn_easingBounce};
    for (int k = 0; k < 4; ++k)
    {
        r = mtwnMakePennerEasing(s_penner[k], mtwn_easeOutIn, &e);
        PUT(out, r);
        EaseAll(out, &e);
    }
    r = mtwnMakeBackEasing(mtwn_easeOut, 1.70158, &e);
    PUT(out, r);
    EaseAll(out, &e);
    r = mtwnMakeElasticEasing(mtwn_easeInOut, 1.25, 0.3, &e);
    PUT(out, r);
    EaseAll(out, &e);
    r = mtwnMakeCubicBezier(0.68, -0.6, 0.32, 1.6, &e);
    PUT(out, r);
    EaseAll(out, &e);
    for (int i = 0; i < s_progressCount; ++i)
    {
        double eased = EaseBezierAt(&e, s_progress[i]);
        PUT(out, eased);
    }
    r = mtwnSetEaseMode(&e, mtwn_easeOut);
    PUT(out, r);
    EaseAll(out, &e);
    r = mtwnMakeCssEasing(mtwn_cssEaseInOut, &e);
    PUT(out, r);
    EaseAll(out, &e);
    bool monotonic = IsMonotonicEasing(&e);
    PUT(out, monotonic);
    static const mtwnLinearPoint s_points[4] = {
        {0.0, 0.0}, {0x1p-1040, 0.3}, {0.6, 0.2}, {1.0, 1.0}};
    mtwnLinearPoint points[4];
    memcpy(points, s_points, sizeof points);
    r = mtwnMakeLinearEasing(points, 4, &e);
    PUT(out, r);
    PUT(out, points);
    EaseAll(out, &e);
    mtwnRoughDef rough;
    memset(&rough, 0, sizeof rough);
    rough.strength = 0.2;
    rough.seed = 7;
    mtwnLinearPoint roughPoints[24] = {0};
    r = mtwnMakeRoughEasing(&rough, roughPoints, 24, &e);
    PUT(out, r);
    PUT(out, roughPoints);
    EaseAll(out, &e);
    // The samplers and fits.
    float table[17] = {0};
    r = mtwnSampleEasing(&e, 17, table);
    PUT(out, r);
    PUT(out, table);
    mtwnEasing spring;
    mtwnSpring s;
    (void)mtwnMakeSpringFromFrequency(1.5, 0.4, &s);
    r = mtwnMakeSpringEasing(&s, 1.2, 0.5, mtwn_easeIn, &spring);
    PUT(out, r);
    EaseAll(out, &spring);
    mtwnLinearPoint fit[64] = {0};
    uint32_t count = 0;
    r = mtwnFitLinearEasing(&spring, 0.001, fit, 64, &count);
    PUT(out, r);
    PUT(out, count);
    PUT(out, fit);
}

static const double s_seconds[] = {0.0, 0x1p-1060, 0.016, 0.25, 0.75, 3.0, 40.0};
enum
{
    s_secondCount = sizeof s_seconds / sizeof s_seconds[0]
};

static void Springs(Out* out)
{
    mtwnSpring springs[3];
    mtwnResult r[3] = {mtwnMakeSpring(170.0, 26.0, 1.0, &springs[0]),
                       mtwnMakeSpringFromFrequency(2.0, 1.0, &springs[1]),
                       mtwnMakeSpringFromDuration(0.6, 0.3, &springs[2])};
    PUT(out, r);
    for (int k = 0; k < 3; ++k)
    {
        for (int i = 0; i < s_secondCount; ++i)
        {
            mtwnSpringMotion motion = SpringMotionAt(&springs[k], s_seconds[i]);
            PUT(out, motion);
            double offset = 0.0;
            double velocity = 0.0;
            SampleSpring(&springs[k], -1.0, 0.3, s_seconds[i], &offset, &velocity);
            PUT(out, offset);
            PUT(out, velocity);
        }
        double settle = SpringSettlingTime(&springs[k], -1.0, 0.3, 0.001, 0.01);
        PUT(out, settle);
    }
    mtwnDecay decays[3];
    mtwnResult d[3] = {mtwnMakeDecay(3.5, &decays[0]),
                       mtwnMakeDecayFromDeceleration(0.998, &decays[1]),
                       mtwnMakeDecayToRest(10.0, 400.0, 70.0, &decays[2])};
    PUT(out, d);
    for (int k = 0; k < 3; ++k)
    {
        for (int i = 0; i < s_secondCount; ++i)
        {
            double position = 0.0;
            double velocity = 0.0;
            SampleDecay(&decays[k], 10.0, 400.0, s_seconds[i], &position, &velocity);
            PUT(out, position);
            PUT(out, velocity);
        }
        double stop = DecayStopTime(&decays[k], 400.0, 0.5);
        double cross = DecayCrossingTime(&decays[k], 10.0, 400.0, 60.0);
        PUT(out, stop);
        PUT(out, cross);
    }
    mtwnBoundedDecay bounded;
    mtwnResult b = mtwnMakeBoundedDecay(&decays[0], &springs[0], 0.0, 100.0, &bounded);
    PUT(out, b);
    for (int i = 0; i < s_secondCount; ++i)
    {
        double position = 0.0;
        double velocity = 0.0;
        SampleBoundedDecay(&bounded, 90.0, 900.0, s_seconds[i], &position, &velocity);
        PUT(out, position);
        PUT(out, velocity);
    }
    double stop = BoundedDecayStopTime(&bounded, 90.0, 900.0, 0.001, 0.5);
    double band = 0.0;
    double back = 0.0;
    mtwnResult banded[2] = {mtwnRubberBand(120.0, 400.0, 0.55, &band),
                            mtwnRubberBandInverse(80.0, 400.0, 0.55, &back)};
    PUT(out, stop);
    PUT(out, banded);
    PUT(out, band);
    PUT(out, back);
}

static void Values(Out* out)
{
    mtwnColorMix mix;
    mtwnResult r = mtwnMakeColorMix((mtwnColor){0.9f, 0x1p-130f, 0.1f, 1.0f},
                                    (mtwnColor){0.1f, 0.3f, 0.9f, 0.5f}, mtwn_colorOklch,
                                    mtwn_hueLonger, &mix);
    PUT(out, r);
    double lch[4] = {0};
    mtwnColor back = {0};
    for (int i = 0; i < s_progressCount; ++i)
    {
        mtwnColor color = {0};
        r = mtwnMixColor(&mix, s_progress[i], &color);
        PUT(out, r);
        PUT(out, color);
        r = mtwnColorToSpace(color, mtwn_colorOklch, lch);
        PUT(out, r);
        PUT(out, lch);
        r = mtwnColorFromSpace(lch, mtwn_colorOklab, &back);
        PUT(out, r);
        PUT(out, back);
        double angle = mtwnMixAngle(0.3, 5.9, mtwn_angleShorter, s_progress[i]);
        PUT(out, angle);
        int64_t integer = mtwnMixInteger(-7, 1000003, s_progress[i]);
        uint32_t discrete = mtwnMixDiscrete(2, 9, s_progress[i]);
        PUT(out, integer);
        PUT(out, discrete);
    }
    mtwnRotationMix rotation;
    r = mtwnMakeRotationMix((mtwnQuat){0.0f, 0.0f, 0.0f, 1.0f}, (mtwnQuat){0.5f, 0.5f, 0.5f, 0.5f},
                            mtwn_rotationShortest, &rotation);
    PUT(out, r);
    mtwnScaleMix scale;
    r = mtwnMakeScaleMix(0x1p-1030, 3.0, &scale);
    PUT(out, r);
    mtwnZoomMix zoom;
    r = mtwnMakeZoomMix((mtwnZoomView){0.0, 0.0, 1.0}, (mtwnZoomView){10.0, 3.0, 0.25}, 1.41,
                        &zoom);
    PUT(out, r);
    double length = 0.0;
    r = mtwnZoomLength(&zoom, &length);
    PUT(out, r);
    PUT(out, length);
    for (int i = 0; i < s_progressCount; ++i)
    {
        mtwnQuat q = {0};
        double scaled = 0.0;
        mtwnZoomView view = {0};
        mtwnResult mixed[3] = {mtwnMixRotation(&rotation, s_progress[i], &q),
                               mtwnMixScale(&scale, s_progress[i], &scaled),
                               mtwnMixZoom(&zoom, s_progress[i], &view)};
        PUT(out, mixed);
        PUT(out, q);
        PUT(out, scaled);
        PUT(out, view);
    }
    mtwnPunch punch;
    mtwnShake shake;
    mtwnResult made[2] = {mtwnMakePunch(7.0, 0.6, 2.0, &punch),
                          mtwnMakeShake(11.0, 0.8, 1.5, 42, &shake)};
    PUT(out, made);
    for (int i = 0; i < s_secondCount; ++i)
    {
        double p = 0.0;
        double sh = 0.0;
        double noise = 0.0;
        mtwnResult sampled[3] = {mtwnSamplePunch(&punch, s_seconds[i], &p),
                                 mtwnSampleShake(&shake, 1, s_seconds[i], &sh),
                                 mtwnShakeNoise(&shake, 2, s_seconds[i], &noise)};
        PUT(out, sampled);
        PUT(out, p);
        PUT(out, sh);
        PUT(out, noise);
    }
}

static mtwnKeyframe Key(double offset, mtwnVec4 value, mtwnSegmentKind kind)
{
    mtwnKeyframe key;
    memset(&key, 0, sizeof key);
    key.value = value;
    key.offset = offset;
    key.easing = mtwnLinearEasing();
    key.kind = kind;
    return key;
}

static void Paths(Out* out)
{
    mtwnKeyframe keys[4] = {Key(0.0, (mtwnVec4){0.0f, 0x1p-130f, 0, 0}, mtwn_segmentCatmullRom),
                            Key(0.3, (mtwnVec4){2.0f, 1.0f, 0, 0}, mtwn_segmentCatmullRom),
                            Key(0.7, (mtwnVec4){-1.0f, 0.5f, 0, 0}, mtwn_segmentCatmullRom),
                            Key(1.0, (mtwnVec4){3.0f, -2.0f, 0, 0}, mtwn_segmentCatmullRom)};
    double lengths[4096];
    mtwnPath path;
    mtwnResult r = mtwnMakePath(keys, 4, 2, 0.5, lengths, &path);
    PUT(out, r);
    for (int i = 0; i < s_progressCount; ++i)
    {
        double value[4] = {0};
        double tangent[4] = {0};
        double direction[4] = {0};
        double angle = 0.0;
        r = mtwnSamplePath(&path, s_progress[i], value, tangent);
        PUT(out, r);
        r = mtwnPathDirection(&path, s_progress[i], direction);
        PUT(out, r);
        r = mtwnPathAngle(&path, s_progress[i], &angle);
        PUT(out, r);
        PUT(out, value);
        PUT(out, tangent);
        PUT(out, direction);
        PUT(out, angle);
    }
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueVec2);
    mtwnKeyframe trackKeys[4];
    memcpy(trackKeys, keys, sizeof trackKeys);
    mtwnSegmentMix mixes[4];
    mtwnTrack track;
    r = mtwnMakeTrack(&def, trackKeys, 4, lengths, mixes, &track);
    PUT(out, r);
    for (int i = 0; i < s_progressCount; ++i)
    {
        mtwnVec4 v = {0};
        r = mtwnSampleTrack(&track, s_progress[i], &v);
        PUT(out, r);
        PUT(out, v);
    }
}

static const double s_local[] = {-0.5, 0.0, 0x1p-1060, 0.3, 1.25, 2.0, 2.6, 7.5};

static void Timings(Out* out)
{
    mtwnTiming timing = mtwnDefaultTiming();
    timing.duration = 0.75;
    timing.delay = 0.1;
    timing.iterations = 2.5;
    timing.iterationStart = 0.3;
    timing.direction = mtwn_directionAlternate;
    (void)mtwnMakeCssEasing(mtwn_cssEaseOut, &timing.easing);
    mtwnResult r = mtwnValidateTiming(&timing);
    PUT(out, r);
    for (int i = 0; i < (int)(sizeof s_local / sizeof s_local[0]); ++i)
    {
        mtwnTimingResult result = {0};
        r = mtwnComputeTiming(&timing, s_local[i], false, &result);
        PUT(out, r);
        PUT(out, result);
        r = mtwnComputeTiming(&timing, s_local[i], true, &result);
        PUT(out, r);
        PUT(out, result);
    }
    mtwnStagger stagger = mtwnDefaultStagger();
    stagger.spacing = 0.07;
    stagger.columns = 5;
    (void)mtwnMakePennerEasing(mtwn_easingSine, mtwn_easeIn, &stagger.easing);
    double offsets[23] = {0};
    r = mtwnComputeStagger(&stagger, 23, offsets);
    PUT(out, r);
    PUT(out, offsets);
}

// The context's calls, in one scene: targets, transitions of every spec,
// a clip built, read and played, every control, updates, reads and
// events.
static void Context(Out* out)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.targetCapacity = 16;
    def.playerCapacity = 8;
    mtwnContext* context = NULL;
    mtwnResult r = mtwnCreateContext(&def, &context);
    PUT(out, r);
    if (r != mtwn_success)
    {
        return;
    }
    def.targetCapacity = 32;
    r = mtwnGrowContext(&context, &def);
    PUT(out, r);
    r = mtwnContext_SetReducedMotion(context, false);
    PUT(out, r);
    mtwnTargetId t[5];
    mtwnResult made[5] = {
        mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0x1p-130f, 0, 0, 0}, &t[0]),
        mtwnCreateTarget(context, mtwn_valueColor, (mtwnVec4){0.9f, 0.1f, 0.0f, 1.0f}, &t[1]),
        mtwnCreateTarget(context, mtwn_valueVec3, (mtwnVec4){1.0f, 2.0f, 3.0f, 0}, &t[2]),
        mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){5.0f, 0, 0, 0}, &t[3]),
        mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &t[4])};
    PUT(out, made);
    r = mtwnTarget_SetBase(context, t[3], (mtwnVec4){7.5f, 0, 0, 0});
    PUT(out, r);
    static const mtwnVec4 s_ends[3] = {
        {0x1p-140f, 0, 0, 0}, {0.1f, 0.3f, 0.9f, 0.5f}, {-1.0f, 1e-30f, 3.3f, 0}};
    static const mtwnTransitionSpec s_specs[3] = {mtwn_transitionEasing, mtwn_transitionSpring,
                                                  mtwn_transitionSpring};
    for (int i = 0; i < 3; ++i)
    {
        mtwnTransitionDef transition = mtwnDefaultTransitionDef();
        transition.target = t[i];
        transition.value = s_ends[i];
        transition.spec = s_specs[i];
        transition.duration = 0.8;
        double distance = 0.0;
        r = mtwnMeasureTransition(context, &transition, &distance);
        PUT(out, r);
        PUT(out, distance);
        mtwnPlayerId id;
        r = mtwnStartTransition(context, &transition, &id);
        PUT(out, r);
    }
    mtwnTransitionDef coast = mtwnDefaultTransitionDef();
    coast.target = t[3];
    coast.spec = mtwn_transitionDecay;
    coast.velocitySource = mtwn_velocityGiven;
    coast.velocity = (mtwnVec4){250.0f, 0, 0, 0};
    mtwnPlayerId coasting;
    r = mtwnStartTransition(context, &coast, &coasting);
    PUT(out, r);
    // A clip: a timeline holding a track, a label and a mark.
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    uint64_t bytes = 0;
    r = mtwnContext_GetClipBytes(context, &clipDef, &bytes);
    PUT(out, r);
    PUT(out, bytes);
    mtwnClipId clip;
    r = mtwnCreateClip(context, &clipDef, &clip);
    PUT(out, r);
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.timing.playbackRate = 0.75;
    mtwnNode timeline = {0};
    r = mtwnClip_AddTimeline(context, clip, &group, &timeline);
    PUT(out, r);
    mtwnKeyframe keys[3] = {Key(0.0, (mtwnVec4){0.0f, 0, 0, 0}, mtwn_segmentMonotone),
                            Key(0.4, (mtwnVec4){0x1p-130f, 0, 0, 0}, mtwn_segmentMonotone),
                            Key(1.0, (mtwnVec4){1.0f, 0, 0, 0}, mtwn_segmentMonotone)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 3;
    track.parent = timeline;
    track.timing.duration = 1.3;
    mtwnNode trackNode = {0};
    r = mtwnClip_AddTrack(context, clip, &track, &trackNode);
    PUT(out, r);
    mtwnPosition at = {0.65, 0, mtwn_positionAbsolute};
    mtwnNode label = {0};
    mtwnNode mark = {0};
    r = mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, at, 4, &label);
    PUT(out, r);
    r = mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, at, 5, &mark);
    PUT(out, r);
    r = mtwnClip_Finish(context, clip);
    PUT(out, r);
    mtwnClipTrack read;
    r = mtwnClip_GetTrack(context, clip, trackNode, &read);
    PUT(out, r);
    mtwnVec4 sample = {0};
    r = mtwnSampleTrack(&read.track, 0.4, &sample);
    PUT(out, r);
    PUT(out, sample);
    mtwnClipNodeInfo info;
    r = mtwnClip_GetNode(context, clip, trackNode, &info);
    PUT(out, r);
    PUT(out, info);
    double seconds = 0.0;
    r = mtwnClip_FindLabel(context, clip, MTWN_ROOT_NODE, 4, &seconds);
    PUT(out, r);
    PUT(out, seconds);
    r = mtwnClip_GetDuration(context, clip, &seconds);
    PUT(out, r);
    PUT(out, seconds);
    // Its player, and every control.
    mtwnBinding binding = mtwnBind(0, t[4]);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    playerDef.keep = true;
    mtwnPlayerId player;
    r = mtwnCreatePlayer(context, &playerDef, &player);
    PUT(out, r);
    static const uint64_t s_times[] = {0u, 16666667u, 1000000001u, 1250000000u, 1900000000u};
    mtwnResult controls[11] = {0};
    uint64_t now = 0;
    for (int u = 0; u < 5; ++u)
    {
        now = s_times[u];
        mtwnTargetId changed[8];
        uint32_t total = 0;
        r = mtwnContext_Update(context, now, changed, 8, &total);
        PUT(out, r);
        PUT(out, total);
        mtwnVec4 values[5];
        r = mtwnContext_ReadTargets(context, t, 5, values);
        PUT(out, r);
        PUT(out, values);
        for (int i = 0; i < 5; ++i)
        {
            mtwnTargetState state;
            mtwnVec4 value;
            mtwnResult got[2] = {mtwnTarget_GetState(context, t[i], &state),
                                 mtwnTarget_GetValue(context, t[i], &value)};
            PUT(out, got);
            PUT(out, state);
            PUT(out, value);
        }
        mtwnPlayerState state;
        r = mtwnPlayer_GetState(context, player, &state);
        PUT(out, r);
        PUT(out, state);
        if (u == 1)
        {
            controls[0] = mtwnPlayer_Pause(context, player);
            controls[1] = mtwnPlayer_Seek(context, player, 0.35);
            controls[2] = mtwnPlayer_Play(context, player);
            controls[3] = mtwnPlayer_SetRate(context, player, -1.5);
            controls[4] = mtwnPlayer_SetWeight(context, player, 0.6);
        }
        if (u == 2)
        {
            controls[5] = mtwnPlayer_Reverse(context, player);
            controls[6] = mtwnPlayer_Sweep(context, player, 0.9);
            controls[7] = mtwnPlayer_SeekProgress(context, player, 0x1p-1030);
            controls[8] = mtwnPlayer_SeekLabel(context, player, 4);
        }
        if (u == 3)
        {
            controls[9] = mtwnPlayer_Finish(context, player);
            controls[10] = mtwnPlayer_Cancel(context, player);
        }
    }
    PUT(out, controls);
    mtwnEvent events[64] = {0};
    uint32_t eventCount = 0;
    uint64_t dropped = 0;
    r = mtwnContext_ReadEvents(context, events, 64, &eventCount, &dropped);
    PUT(out, r);
    PUT(out, eventCount);
    PUT(out, dropped);
    for (uint32_t e = 0; e < eventCount && e < 64; ++e)
    {
        PUT(out, events[e]);
    }
    mtwnResult destroyed[3] = {mtwnDestroyPlayer(context, player), mtwnDestroyClip(context, clip),
                               mtwnDestroyTarget(context, t[0])};
    PUT(out, destroyed);
    mtwnDestroyContext(context);
}

typedef void Case(Out* out);

static void Run(const char* name, Case* run, TestFloatSetting start)
{
    static Out plain;
    static Out hostile;
    memset(&plain, 0, sizeof plain);
    run(&plain);
    bool same = !plain.full && plain.at > 0;
    bool kept = true;
    for (int h = 0; h < TestFloatHostileCount(); ++h)
    {
        TestFloatSetting host = TestFloatHostileAt(h);
        memset(&hostile, 0, sizeof hostile);
        TestFloatSet(host);
        run(&hostile);
        kept = kept && TestFloatSame(TestFloatGet(), host);
        TestFloatSet(start);
        same = same && hostile.at == plain.at && memcmp(hostile.bytes, plain.bytes, plain.at) == 0;
    }
    if (!same || !kept)
    {
        printf("  %s: %s%s\n", name, same ? "" : "bits differ ", kept ? "" : "setting not kept");
    }
    CHECK(same, "the same bits under every host");
    CHECK(kept, "and the host's setting as it set it");
}

int main(void)
{
    TestFloatSetting start = TestFloatGet();
    Run("easings", Easings, start);
    Run("springs", Springs, start);
    Run("values", Values, start);
    Run("paths", Paths, start);
    Run("timings", Timings, start);
    Run("context", Context, start);
    CHECK(TestFloatSame(TestFloatGet(), start), "the thread's own setting at the end");
    return s_failures == 0 ? 0 : 1;
}
