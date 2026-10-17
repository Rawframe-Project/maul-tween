// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The guide's snippets (docs/guide.md), each as written, built and run;
// tools/check_guide.py checks each appears here unchanged. Between them,
// checks of what the guide says they do.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>

static void TestEasing(void)
{
    mtwnEasing ease;
    mtwnResult made = mtwnMakeCssEasing(mtwn_cssEaseInOut, &ease); // CSS's ease-in-out
    double eased = mtwnEase(&ease, 0.25, false);                   // a quarter of the way

    mtwnEasing back;
    made = mtwnMakeBackEasing(mtwn_easeOut, 1.70158, &back); // GSAP's back.out
    CHECK(made == mtwn_success && eased > 0.0 && eased < 0.25, "ease-in-out slow at first");
    CHECK(mtwnEase(&back, 1.0, false) == 1.0, "back.out lands on 1");
}

static void TestRough(void)
{
    mtwnLinearPoint points[22]; // kept while the easing is used
    mtwnRoughDef def = {.shape = mtwnLinearEasing(), .strength = 0.1, .seed = 42};
    def.taper = mtwn_roughTaperOut; // calm by the end
    mtwnEasing rough;
    mtwnResult madeRough = mtwnMakeRoughEasing(&def, points, 22, &rough);
    CHECK(madeRough == mtwn_success && mtwnEase(&rough, 1.0, false) == 1.0, "a rough easing");
}

static void TestSprings(void)
{
    mtwnSpring spring;
    mtwnResult made = mtwnMakeSpringFromDuration(0.4, 0.2, &spring); // 0.4 s, a little bounce
    double offset = 0.0;
    double speed = 0.0;
    mtwnSampleSpring(&spring, -1.0, 0.0, 0.1, &offset, &speed); // 0.1 s after starting 1 short
    double settled = mtwnSpringSettlingTime(&spring, -1.0, 0.0, 1e-3, 1e-2);
    CHECK(made == mtwn_success && offset > -1.0 && speed > 0.0, "on its way to 0");
    CHECK(settled > 0.1 && isfinite(settled), "settled after a while");
}

static void TestInertia(void)
{
    mtwnDecay coast;
    mtwnResult made = mtwnMakeDecayFromDeceleration(0.998, &coast);
    double rest = 0.0;
    mtwnSampleDecay(&coast, 40.0, 1500.0, HUGE_VAL, &rest, NULL); // where it would stop
    double page = 320.0 * round(rest / 320.0);                    // the nearest page
    mtwnDecay snap;
    made = mtwnMakeDecayToRest(40.0, 1500.0, page, &snap);

    mtwnSpring bounce;
    made = mtwnMakeSpring(500.0, 10.0, 1.0, &bounce);
    mtwnBoundedDecay scroll;
    made = mtwnMakeBoundedDecay(&coast, &bounce, 0.0, 600.0, &scroll); // content 0 to 600
    double at = 0.0;
    mtwnSampleBoundedDecay(&scroll, 40.0, 1500.0, 0.5, &at, NULL);
    double still = mtwnBoundedDecayStopTime(&scroll, 40.0, 1500.0, 0.5, 1.0);
    CHECK(made == mtwn_success && page == 640.0 && snap.rate > 0.0, "snapped to the second page");
    CHECK(at > 500.0 && at < 700.0 && isfinite(still), "bounced at 600, coming to rest");
}

static void TestValues(void)
{
    mtwnColor red = {1.0f, 0.0f, 0.0f, 1.0f};
    mtwnColor blue = {0.0f, 0.0f, 1.0f, 1.0f};
    mtwnColorMix mix;
    mtwnResult made = mtwnMakeColorMix(red, blue, mtwn_colorOklch, mtwn_hueShorter, &mix);
    mtwnColor between = mtwnMixColor(&mix, 0.5); // half way, in OkLCh
    CHECK(made == mtwn_success && between.alpha == 1.0f && between.red > 0.0f &&
              between.blue > 0.0f,
          "a color between red and blue");
}

static void TestScales(void)
{
    mtwnScaleMix zoom;
    mtwnResult made = mtwnMakeScaleMix(1.0, 100.0, &zoom);
    double half = mtwnMixScale(&zoom, 0.5); // 10, not 50.5
    CHECK(made == mtwn_success && fabs(half - 10.0) <= 1e-12, "halfway from 1 to 100 is 10");
}

static void TestZoom(void)
{
    mtwnZoomView from = {0.0, 0.0, 1.0};
    mtwnZoomView to = {50.0, 20.0, 2.0};
    mtwnZoomMix flight;
    mtwnResult made = mtwnMakeZoomMix(from, to, 1.42, &flight);
    double seconds = mtwnZoomLength(&flight) * 1.42 / sqrt(2.0); // d3's pace
    // Halfway it is wider than both ends.
    mtwnZoomView view = mtwnMixZoom(&flight, 0.5);
    CHECK(made == mtwn_success && seconds > 0.0 && view.width > 2.0,
          "a flight that zooms out to pan");
}

static void TestStagger(void)
{
    mtwnStagger stagger = mtwnDefaultStagger(); // 0.1 s apart, from the first
    stagger.columns = 4;                        // a grid of four columns
    stagger.origin = mtwn_staggerCentre;        // spreading from its centre
    double delays[12];
    mtwnResult spread = mtwnComputeStagger(&stagger, 12, delays);
    CHECK(spread == mtwn_success && delays[5] == 0.0 && delays[6] == 0.0 && delays[0] > delays[1],
          "the centre's cells first, the corners last");
}

static void* RunNow(mtwnTaskFn* task, int32_t itemCount, int32_t minRange, void* taskContext,
                    void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext); // a real host splits it over its workers
    return NULL;                     // ran it all: nothing to wait for
}

static void WaitFor(void* userTask, void* userContext)
{
    (void)userTask;
    (void)userContext;
}

static void TestThreads(void)
{
    mtwnContextDef threaded = mtwnDefaultContextDef();
    threaded.enqueueTask = RunNow;
    threaded.finishTask = WaitFor;
    threaded.userTaskContext = NULL; // passed to both, the host's own
    mtwnContext* parallel = NULL;
    mtwnResult madeThreaded = mtwnCreateContext(&threaded, &parallel);
    CHECK(madeThreaded == mtwn_success && mtwnUpdate(parallel, 0, NULL, 0, NULL) == mtwn_success,
          "a context on the host's tasks");
    mtwnDestroyContext(parallel);
}

static void TestFling(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    mtwnTargetId scroll = {0, 0};
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){40.0f, 0, 0, 0}, &scroll) ==
                  mtwn_success,
          "a scroll offset");
    mtwnTransitionDef fling = mtwnDefaultTransitionDef();
    fling.target = scroll;
    fling.spec = mtwn_transitionDecay;
    fling.velocitySource = mtwn_velocityGiven;
    fling.velocity = (mtwnVec4){1500.0f, 0, 0, 0}; // from the gesture, a second
    fling.bounded = true;                          // the content's ends
    fling.lower = (mtwnVec4){0.0f, 0, 0, 0};
    fling.upper = (mtwnVec4){600.0f, 0, 0, 0};
    mtwnPlayerId flinging;
    mtwnResult flung = mtwnStartTransition(context, &fling, &flinging);
    CHECK(flung == mtwn_success, "a bounded fling");
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnUpdate(context, 30000000000u, NULL, 0, NULL) == mtwn_success,
          "played out");
    mtwnVec4 value;
    CHECK(mtwnTarget_GetValue(context, scroll, &value) == mtwn_success && value.x == 600.0f,
          "resting on the content's end");
    mtwnDestroyContext(context);
}

static void TestHost(void)
{
    uint64_t nowNs = 0;
    mtwnContextDef def = mtwnDefaultContextDef(); // capacities, allocator, version
    def.targetCapacity = 4096;
    mtwnContext* context = NULL;
    mtwnResult made = mtwnCreateContext(&def, &context);

    mtwnTargetId opacity;
    made = mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){1.0f, 0, 0, 0}, &opacity);
    CHECK(made == mtwn_success, "a context and a target");
    mtwnTransitionDef fade = mtwnDefaultTransitionDef();
    fade.target = opacity;
    fade.value = (mtwnVec4){0.0f, 0, 0, 0};
    fade.duration = 0.3;
    mtwnResult started = mtwnMakeCssEasing(mtwn_cssEaseOut, &fade.easing);
    mtwnPlayerId fading;
    started = mtwnStartTransition(context, &fade, &fading);
    CHECK(started == mtwn_success, "a fade");
    CHECK(mtwnUpdate(context, 100000000u, NULL, 0, NULL) == mtwn_success, "under way");
    mtwnTransitionDef settle = mtwnDefaultTransitionDef();
    settle.target = opacity;
    settle.value = (mtwnVec4){1.0f, 0, 0, 0};
    settle.spec = mtwn_transitionSpring; // from the value and velocity it has now
    mtwnResult sprung = mtwnMakeSpringFromDuration(0.5, 0.0, &settle.spring);
    mtwnPlayerId settling;
    sprung = mtwnStartTransition(context, &settle, &settling);
    CHECK(sprung == mtwn_success && !mtwnPlayer_IsValid(context, fading),
          "a spring replacing the fade");
    mtwnTransitionDef move = mtwnDefaultTransitionDef();
    move.target = opacity;
    move.value = (mtwnVec4){0.25f, 0, 0, 0};
    double distance = 0.0;
    mtwnResult measured = mtwnMeasureTransition(context, &move, &distance);
    move.duration = fmin(fmax(distance / 2.0, 0.15), 0.4); // 2 a second, 150 to 400 ms
    CHECK(measured == mtwn_success && move.duration >= 0.15 && move.duration <= 0.4,
          "a duration from the distance");
    bool reduce = true; // the platform's preference, as the host reads it
    mtwnResult set = mtwnContext_SetReducedMotion(context, reduce);
    fade.essential = true; // an opacity change is not motion: it may play on
    CHECK(set == mtwn_success && mtwnContext_IsReducedMotion(context) && fade.essential,
          "reduced motion set");
    CHECK(mtwnContext_SetReducedMotion(context, false) == mtwn_success, "and unset");
    nowNs = 200000000u;
    mtwnTargetId changed[64];
    uint32_t total = 0;
    mtwnResult updated = mtwnUpdate(context, nowNs, changed, 64, &total);
    for (uint32_t i = 0; i < total && i < 64; ++i)
    {
        mtwnVec4 value;
        if (mtwnTarget_GetValue(context, changed[i], &value) == mtwn_success)
        {
            // Show value on whatever changed[i] stands for.
        }
    }
    CHECK(updated == mtwn_success && total == 1 && changed[0].index1 == opacity.index1,
          "the opacity reported changed");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    uint64_t bytes = 0;
    mtwnResult built = mtwnGetClipBytes(&clipDef, &bytes); // its share of the clip storage
    mtwnClipId clip;
    built = mtwnCreateClip(context, &clipDef, &clip);

    mtwnKeyframe keys[2] = {0};
    keys[1].value = (mtwnVec4){1.0f, 0, 0, 0};
    keys[1].offset = 1.0;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef(); // on the root at 0, slot 0, a number
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 0.4;
    mtwnNode fadeIn;
    built = mtwnClip_AddTrack(context, clip, &track, &fadeIn);

    track.slot = 1;
    track.position.kind = mtwn_positionAfterPrevious; // when the fade ends
    mtwnNode slideIn;
    built = mtwnClip_AddTrack(context, clip, &track, &slideIn);
    built = mtwnClip_Finish(context, clip);
    CHECK(built == mtwn_success && bytes > 0, "a clip of two tracks");
    double seconds = 0.0;
    CHECK(mtwnClip_GetDuration(context, clip, &seconds) == mtwn_success && seconds == 0.8,
          "the second track after the first");
    mtwnTargetId slide;
    mtwnResult playing = mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &slide);
    mtwnBinding bindings[2] = {mtwnBind(0, opacity), mtwnBind(1, slide)};
    mtwnPlayerDef play = mtwnDefaultPlayerDef();
    play.clip = clip;
    play.bindings = bindings;
    play.bindingCount = 2;
    play.keep = true; // kept when finished, to play again
    mtwnPlayerId intro;
    playing = mtwnCreatePlayer(context, &play, &intro); // plays from the next update
    playing = mtwnPlayer_Pause(context, intro);
    playing = mtwnPlayer_Seek(context, intro, 0.2);
    playing = mtwnPlayer_SetRate(context, intro, -1.0); // backwards
    playing = mtwnPlayer_Play(context, intro);
    CHECK(playing == mtwn_success, "a player paused, seeked, reversed and played");
    CHECK(mtwnUpdate(context, 300000000u, NULL, 0, NULL) == mtwn_success, "updated");
    mtwnPlayerState state;
    CHECK(mtwnPlayer_GetState(context, intro, &state) == mtwn_success && state.rate == -1.0 &&
              state.currentTime == 0.2,
          "at 0.2 s, running backwards from the next update");
    mtwnEvent events[32];
    uint32_t count = 0;
    uint64_t dropped = 0;
    mtwnResult read = mtwnReadEvents(context, events, 32, &count, &dropped);
    for (uint32_t i = 0; i < count; ++i)
    {
        if (events[i].kind == mtwn_eventMark && events[i].key == 7)
        {
            // Play the sound: crossed events[i].count times since the last read.
        }
    }
    CHECK(read == mtwn_success && dropped == 0, "the events read");
    mtwnBinding layer = mtwnBind(0, opacity);
    layer.composite = mtwn_compositeAdd; // added to what lies under it
    layer.weight = 0.5f;
    play.bindings = &layer;
    play.bindingCount = 1;
    mtwnPlayerId pulse;
    mtwnResult layered = mtwnCreatePlayer(context, &play, &pulse);
    layered = mtwnPlayer_SetWeight(context, pulse, 0.25); // the whole player's weight
    CHECK(layered == mtwn_success, "a layer added at a weight");
    mtwnContextDef bigger = def;
    bigger.targetCapacity *= 2;
    bigger.playerCapacity *= 2;
    mtwnResult grown = mtwnGrowContext(&context, &bigger); // it may move; ids stay valid
    CHECK(grown == mtwn_success && mtwnTarget_IsValid(context, opacity) &&
              mtwnPlayer_IsValid(context, intro),
          "grown, every id valid");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestThreads();
    TestEasing();
    TestRough();
    TestSprings();
    TestInertia();
    TestFling();
    TestValues();
    TestScales();
    TestZoom();
    TestStagger();
    TestHost();
    return s_failures == 0 ? 0 : 1;
}
