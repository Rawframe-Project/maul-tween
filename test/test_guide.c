// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The snippets of docs/user/, each as written, built and run;
// tools/check_guide.py checks each appears here unchanged. Between them,
// checks of what the pages say they do.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
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
    double eased = 0.0;
    mtwnResult sampled = mtwnEase(&ease, 0.25, false, &eased); // a quarter of the way

    mtwnEasing back;
    made = mtwnMakeBackEasing(mtwn_easeOut, 1.70158, &back); // GSAP's back.out
    CHECK(made == mtwn_success && sampled == mtwn_success && eased > 0.0 && eased < 0.25,
          "ease-in-out slow at first");
    double landed = 0.0;
    CHECK(mtwnEase(&back, 1.0, false, &landed) == mtwn_success && landed == 1.0,
          "back.out lands on 1");
}

static void TestRough(void)
{
    mtwnLinearPoint points[22]; // kept while the easing is used
    mtwnRoughDef def = {.shape = mtwnLinearEasing(), .strength = 0.1, .seed = 42};
    def.taper = mtwn_roughTaperOut; // calm by the end
    mtwnEasing rough;
    mtwnResult madeRough = mtwnMakeRoughEasing(&def, points, 22, &rough);
    double end = 0.0;
    CHECK(madeRough == mtwn_success && mtwnEase(&rough, 1.0, false, &end) == mtwn_success &&
              end == 1.0,
          "a rough easing");
}

static void TestSprings(void)
{
    mtwnSpring spring;
    mtwnResult made = mtwnMakeSpringFromDuration(0.4, 0.2, &spring); // 0.4 s, a little bounce
    double offset = 0.0;
    double speed = 0.0;
    // 0.1 s after starting 1 short of the target, at rest.
    mtwnResult sampled = mtwnSampleSpring(&spring, -1.0, 0.0, 0.1, &offset, &speed);
    double settled = 0.0;
    mtwnResult timed = mtwnSpringSettlingTime(&spring, -1.0, 0.0, 1e-3, 1e-2, &settled);
    CHECK(made == mtwn_success && sampled == mtwn_success && timed == mtwn_success &&
              offset > -1.0 && speed > 0.0,
          "on its way to 0");
    CHECK(settled > 0.1 && isfinite(settled), "settled after a while");
}

static void TestInertia(void)
{
    mtwnDecay coast;
    mtwnResult made = mtwnMakeDecayFromDeceleration(0.998, &coast);
    double rest = 0.0;
    // Where it would stop: its sample at infinite time.
    mtwnResult sampled = mtwnSampleDecay(&coast, 40.0, 1500.0, HUGE_VAL, &rest, NULL);
    double page = 320.0 * round(rest / 320.0); // the nearest page
    mtwnDecay snap;
    made = mtwnMakeDecayToRest(40.0, 1500.0, page, &snap);

    mtwnSpring bounce;
    made = mtwnMakeSpring(500.0, 10.0, 1.0, &bounce);
    mtwnBoundedDecay scroll;
    made = mtwnMakeBoundedDecay(&coast, &bounce, 0.0, 600.0, &scroll); // content 0 to 600
    double at = 0.0;
    mtwnResult bounced = mtwnSampleBoundedDecay(&scroll, 40.0, 1500.0, 0.5, &at, NULL);
    double still = 0.0;
    mtwnResult stopped = mtwnBoundedDecayStopTime(&scroll, 40.0, 1500.0, 0.5, 1.0, &still);
    CHECK(sampled == mtwn_success && bounced == mtwn_success && stopped == mtwn_success,
          "sampled and timed");
    CHECK(made == mtwn_success && page == 640.0 && snap.rate > 0.0, "snapped to the second page");
    CHECK(at > 500.0 && at < 700.0 && isfinite(still), "bounced at 600, coming to rest");
}

static void TestValues(void)
{
    mtwnColor red = {1.0f, 0.0f, 0.0f, 1.0f};
    mtwnColor blue = {0.0f, 0.0f, 1.0f, 1.0f};
    mtwnColorMix mix;
    mtwnResult made = mtwnMakeColorMix(red, blue, mtwn_colorOklch, mtwn_hueShorter, &mix);
    mtwnColor between;
    mtwnResult mixed = mtwnMixColor(&mix, 0.5, &between); // half way, in OkLCh
    CHECK(made == mtwn_success && mixed == mtwn_success && between.alpha == 1.0f &&
              between.red > 0.0f && between.blue > 0.0f,
          "a color between red and blue");
}

static void TestScales(void)
{
    mtwnScaleMix zoom;
    mtwnResult made = mtwnMakeScaleMix(1.0, 100.0, &zoom);
    double half = 0.0;
    mtwnResult mixed = mtwnMixScale(&zoom, 0.5, &half); // 10, not 50.5
    CHECK(made == mtwn_success && mixed == mtwn_success && fabs(half - 10.0) <= 1e-12,
          "halfway from 1 to 100 is 10");
}

static void TestZoom(void)
{
    mtwnZoomView from = {0.0, 0.0, 1.0};
    mtwnZoomView to = {50.0, 20.0, 2.0};
    mtwnZoomMix flight;
    mtwnResult made = mtwnMakeZoomMix(from, to, 1.42, &flight);
    double length = 0.0;
    mtwnResult measured = mtwnZoomLength(&flight, &length);
    double seconds = length * 1.42 / sqrt(2.0); // d3's pace
    // Halfway it is wider than both ends.
    mtwnZoomView view;
    mtwnResult mixed = mtwnMixZoom(&flight, 0.5, &view);
    CHECK(made == mtwn_success && measured == mtwn_success && mixed == mtwn_success &&
              seconds > 0.0 && view.width > 2.0,
          "a flight that zooms out to pan");
}

// A cubic Bezier's coordinate at u.
static double Bezier3(double a, double b, double c, double d, double u)
{
    double v = 1.0 - u;
    return v * v * v * a + 3.0 * v * v * u * b + 3.0 * v * u * u * c + u * u * u * d;
}

static void TestHandles(void)
{
    mtwnKeyframe keys[2] = {0};
    keys[0].kind = mtwn_segmentBezier;
    keys[0].out = (mtwnVec4){6.0f, 0, 0, 0}; // the out handle's value
    keys[1].offset = 1.0;
    keys[1].value = (mtwnVec4){10.0f, 0, 0, 0};
    keys[1].in = (mtwnVec4){10.0f, 0, 0, 0}; // the in handle's value
    // Their times, 0.5 s and 1.2 s, as shares of the 2 s span.
    mtwnResult made = mtwnMakeCubicBezier(0.25, 1.0 / 3.0, 0.6, 2.0 / 3.0, &keys[0].easing);

    mtwnPath path;
    CHECK(made == mtwn_success && mtwnMakePath(keys, 2, 1, 0.5, NULL, &path) == mtwn_success,
          "a segment timed by its handles");
    // Blender's way: the parameter whose time is the one asked, by
    // bisection, and the value there.
    double worst = 0.0;
    for (int k = 0; k <= 200; ++k)
    {
        double time = 2.0 * k / 200.0;
        double lo = 0.0;
        double hi = 1.0;
        for (int i = 0; i < 100; ++i)
        {
            double mid = 0.5 * (lo + hi);
            if (Bezier3(0.0, 0.5, 1.2, 2.0, mid) < time)
            {
                lo = mid;
            }
            else
            {
                hi = mid;
            }
        }
        double expected = Bezier3(0.0, 6.0, 10.0, 10.0, 0.5 * (lo + hi));
        double value[4];
        CHECK(mtwnSamplePath(&path, time / 2.0, value, NULL) == mtwn_success, "the curve sampled");
        worst = fmax(worst, fabs(value[0] - expected));
    }
    CHECK(worst < 1e-9, "the editors' curve, solved for its time");
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
    CHECK(madeThreaded == mtwn_success &&
              mtwnContext_Update(parallel, 0, NULL, 0, NULL) == mtwn_success,
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
    CHECK(mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, 30000000000u, NULL, 0, NULL) == mtwn_success,
          "played out");
    mtwnVec4 value;
    CHECK(mtwnTarget_GetValue(context, scroll, &value) == mtwn_success && value.x == 600.0f,
          "resting on the content's end");
    mtwnDestroyContext(context);
}

static void TestScroll(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    mtwnTargetId shown = {0, 0};
    mtwnClipId story = {0, 0};
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnKeyframe keys[2] = {0};
    keys[1].offset = 1.0;
    keys[1].value = (mtwnVec4){1.0f, 0, 0, 0};
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 2.0;
    mtwnNode node = 0;
    mtwnNode mark = 0;
    mtwnPosition half = {0};
    half.offset = 1.0;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &shown) == mtwn_success &&
              mtwnCreateClip(context, &clipDef, &story) == mtwn_success &&
              mtwnClip_AddTrack(context, story, &track, &node) == mtwn_success &&
              mtwnClip_AddMark(context, story, MTWN_ROOT_NODE, half, 1, &mark) == mtwn_success &&
              mtwnClip_Finish(context, story) == mtwn_success,
          "a clip of 2 s with a mark at 1 s");
    mtwnBinding binding = mtwnBind(0, shown);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = story;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    playerDef.keep = true;
    mtwnPlayerId reader = {0, 0};
    double length = 0.0;
    CHECK(mtwnCreatePlayer(context, &playerDef, &reader) == mtwn_success &&
              mtwnPlayer_Pause(context, reader) == mtwn_success &&
              mtwnClip_GetDuration(context, story, &length) == mtwn_success && length == 2.0,
          "a paused player");
    mtwnSpring chase;
    CHECK(mtwnMakeSpringFromDuration(0.5, 0.0, &chase) == mtwn_success, "the chase");
    double goal = 0.0;
    double away = 0.0;
    double pace = 0.0;
    double since = 0.0;
    double now = 0.0;
    double scrolled = 0.75;
    // At a scroll event: the chase from where it is, toward the new progress.
    double offset = 0.0;
    double speed = 0.0;
    mtwnResult chased = mtwnSampleSpring(&chase, away, pace, now - since, &offset, &speed);
    away = goal + offset - scrolled; // the same place, against the new goal
    pace = speed;
    goal = scrolled;
    since = now;

    int behind = 0;
    for (int frame = 1; frame <= 90; ++frame)
    {
        now = frame / 60.0;
        // Each frame, before the update: the paused player swept to the chase.
        chased = mtwnSampleSpring(&chase, away, pace, now - since, &offset, NULL);
        mtwnResult swept = mtwnPlayer_Sweep(context, reader, (goal + offset) * length);
        CHECK(chased == mtwn_success && swept == mtwn_success, "swept");
        CHECK(mtwnContext_Update(context, (uint64_t)llround(now * 1e9), NULL, 0, NULL) ==
                  mtwn_success,
              "updated");
        mtwnVec4 value;
        CHECK(mtwnTarget_GetValue(context, shown, &value) == mtwn_success, "read");
        behind += fabs((double)value.x - (goal + offset)) > 1e-6;
    }
    mtwnEvent events[16];
    uint32_t count = 0;
    CHECK(mtwnContext_ReadEvents(context, events, 16, &count, NULL) == mtwn_success, "the records");
    int marks = 0;
    for (uint32_t k = 0; k < count; ++k)
    {
        marks += events[k].kind == mtwn_eventMark;
    }
    CHECK(behind == 0, "each frame shows the chase, not the frame before");
    CHECK(marks == 1, "the mark at the middle crossed once, as scrolled past");
    mtwnDestroyContext(context);
}

// A track from 0 to a value over a duration, an easing over it.
static mtwnResult Line(mtwnContext* context, mtwnClipId clip, uint32_t slot, float to,
                       double seconds, mtwnEasing easing, mtwnNode* nodeOut)
{
    mtwnKeyframe keys[2] = {0};
    keys[1].offset = 1.0;
    keys[1].value = (mtwnVec4){to, 0, 0, 0};
    keys[0].easing = easing;
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.slot = slot;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = seconds;
    track.timing.fill = mtwn_fillForwards;
    return mtwnClip_AddTrack(context, clip, &track, nodeOut);
}

static void TestJump(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    mtwnTargetId x = {0, 0};
    mtwnTargetId y = {0, 0};
    mtwnClipId clip = {0, 0};
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnNode across = 0;
    mtwnNode rise = 0;
    mtwnEasing outQuad;
    mtwnResult made = mtwnMakePowerEasing(mtwn_easeOut, 2.0, &outQuad);
    CHECK(made == mtwn_success && mtwnCreateContext(&contextDef, &context) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &y) == mtwn_success &&
              mtwnCreateClip(context, &clipDef, &clip) == mtwn_success &&
              Line(context, clip, 0, 300.0f, 1.2, mtwnLinearEasing(), &across) == mtwn_success &&
              Line(context, clip, 1, 20.0f, 1.2, outQuad, &rise) == mtwn_success,
          "across to 300 and up to 20, as DOJump's line");
    // Three hops of 50 over 1.2 s: up and down each 0.4 s, eased out
    // going up and back the same way down (DOTween's DOJump).
    mtwnKeyframe hop[2] = {0};
    hop[1].offset = 1.0;
    hop[1].value = (mtwnVec4){50.0f, 0, 0, 0};
    made = mtwnMakePowerEasing(mtwn_easeOut, 2.0, &hop[0].easing);
    mtwnClipTrackDef jump = mtwnDefaultClipTrackDef();
    jump.slot = 2; // bound to the height too, added over its line
    jump.keyframes = hop;
    jump.keyframeCount = 2;
    jump.timing.duration = 0.2;
    jump.timing.iterations = 6.0;
    jump.timing.direction = mtwn_directionAlternate; // each hop down the way it went up
    jump.timing.fill = mtwn_fillForwards;            // landed, it stays
    mtwnNode hops;
    made = mtwnClip_AddTrack(context, clip, &jump, &hops);

    CHECK(made == mtwn_success && mtwnClip_Finish(context, clip) == mtwn_success, "the hops");
    mtwnBinding bindings[3] = {mtwnBind(0, x), mtwnBind(1, y), mtwnBind(2, y)};
    bindings[2].composite = mtwn_compositeAdd;
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindings = bindings;
    playerDef.bindingCount = 3;
    playerDef.keep = true;
    mtwnPlayerId player;
    CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success &&
              mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success,
          "a jump");
    int off = 0;
    for (int k = 1; k <= 12; ++k)
    {
        double t = k * 0.1;
        CHECK(mtwnContext_Update(context, (uint64_t)llround(t * 1e9), NULL, 0, NULL) ==
                  mtwn_success,
              "on");
        mtwnVec4 vx;
        mtwnVec4 vy;
        CHECK(mtwnTarget_GetValue(context, x, &vx) == mtwn_success &&
                  mtwnTarget_GetValue(context, y, &vy) == mtwn_success,
              "read");
        double line = 20.0 * (1.0 - (1.0 - t / 1.2) * (1.0 - t / 1.2));
        // At the tops (0.2, 0.6, 1.0 s) 50 over the line; landed between.
        double over = k % 4 == 2 ? 50.0 : (k % 4 == 0 ? 0.0 : 50.0 * 0.75);
        off += fabs((double)vx.x - 250.0 * t) > 1e-3 || fabs((double)vy.x - (line + over)) > 1e-3;
    }
    CHECK(off == 0, "across at a steady pace, hopping 50 over the line, landing on it");
    mtwnDestroyContext(context);
}

static void TestTwos(void)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    mtwnTargetId x = {0, 0};
    mtwnClipId clip = {0, 0};
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success &&
              mtwnCreateClip(context, &clipDef, &clip) == mtwn_success,
          "a clip");
    mtwnResult made = mtwn_success;
    // Held 12 times a second (on twos at 24 frames): 2 s in 24 steps.
    mtwnClipTimelineDef twos = mtwnDefaultClipTimelineDef();
    twos.timing.duration = 2.0;
    made = mtwnMakeSteps(24, mtwn_jumpEnd, &twos.timing.easing);
    mtwnNode held;
    made = mtwnClip_AddTimeline(context, clip, &twos, &held);

    CHECK(made == mtwn_success, "a stepped timeline");
    // 0 to 240 across it, smooth on its own.
    mtwnKeyframe keys[2] = {0};
    keys[1].offset = 1.0;
    keys[1].value = (mtwnVec4){240.0f, 0, 0, 0};
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef across = mtwnDefaultClipTrackDef();
    across.parent = held;
    across.keyframes = keys;
    across.keyframeCount = 2;
    across.timing.duration = 2.0;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &across, &node) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a track in it");
    mtwnBinding binding = mtwnBind(0, x);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    mtwnPlayerId player;
    CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success &&
              mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success,
          "played");
    // At t it shows the pose of the last twelfth of a second: 10 floor(12 t).
    int off = 0;
    for (int k = 0; k < 28; ++k)
    {
        double t = 0.03 + 0.07 * k;
        CHECK(mtwnContext_Update(context, (uint64_t)llround(t * 1e9), NULL, 0, NULL) ==
                  mtwn_success,
              "on");
        mtwnVec4 v;
        CHECK(mtwnTarget_GetValue(context, x, &v) == mtwn_success, "read");
        off += (double)v.x != 10.0 * floor(12.0 * t);
    }
    CHECK(off == 0, "a pose every twelfth of a second");
    mtwnDestroyContext(context);
}

// A track at 100 a second, its speed eased from 1 to 0 over the second
// from 1 s by a quadratic out, at a frame rate: by the rate set each
// frame, or exactly, swept to the time the eased speed travels. The
// value it stops at, its frames' steps checked against the rate running.
static double EasedRateStop(uint32_t fps, bool exact)
{
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    mtwnTargetId x = {0, 0};
    mtwnTargetId speed = {0, 0};
    mtwnClipId clip = {0, 0};
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnNode node = 0;
    mtwnResult made = mtwn_success;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){1.0f, 0, 0, 0}, &speed) ==
                  mtwn_success &&
              mtwnCreateClip(context, &clipDef, &clip) == mtwn_success &&
              Line(context, clip, 0, 1000.0f, 10.0, mtwnLinearEasing(), &node) == mtwn_success &&
              mtwnClip_Finish(context, clip) == mtwn_success,
          "a track at 100 a second");
    mtwnBinding binding = mtwnBind(0, x);
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindings = &binding;
    playerDef.bindingCount = 1;
    mtwnPlayerId player;
    CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success &&
              mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success,
          "playing");
    mtwnTransitionDef slow = mtwnDefaultTransitionDef();
    slow.target = speed;
    slow.value = (mtwnVec4){0.0f, 0, 0, 0};
    slow.duration = 1.0;
    CHECK(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &slow.easing) == mtwn_success, "an ease out");
    double rampStart = 1.0;
    double rampTime = 0.0;
    double last = 0.0;
    double lastRate = 1.0;
    double running = 1.0; // the rate the player moves at, a frame behind
    int jumps = 0;
    for (uint32_t frame = 1; frame <= 3u * fps; ++frame)
    {
        double seconds = (double)frame / (double)fps;
        if (frame == fps && !exact)
        {
            mtwnPlayerId slowing;
            CHECK(mtwnStartTransition(context, &slow, &slowing) == mtwn_success, "slowing");
        }
        if (frame > fps && exact)
        {
            // Exact at any frame rate: paused where the ramp starts, swept
            // each frame to the time the eased speed has travelled.
            double u = fmin(seconds - rampStart, 1.0);
            made = mtwnPlayer_Sweep(context, player, rampTime + (1.0 - pow(1.0 - u, 3.0)) / 3.0);

            CHECK(made == mtwn_success, "swept");
        }
        CHECK(mtwnContext_Update(context, (uint64_t)frame * 1000000000u / fps, NULL, 0, NULL) ==
                  mtwn_success,
              "a frame");
        if (frame == fps && exact)
        {
            mtwnPlayerState state;
            CHECK(mtwnPlayer_GetState(context, player, &state) == mtwn_success &&
                      mtwnPlayer_Pause(context, player) == mtwn_success,
                  "paused at the ramp");
            rampTime = state.currentTime;
        }
        mtwnVec4 v;
        CHECK(mtwnTarget_GetValue(context, x, &v) == mtwn_success, "read");
        if (!exact)
        {
            // Each frame moves by the rate running since the last update:
            // no jump, no step back. A rate set takes over at the next one.
            double step = (double)v.x - last;
            jumps += fabs(step - 100.0 * running / (double)fps) > 1e-3;
            running = lastRate;
            // After each update: the rate the speed transition has reached.
            mtwnVec4 rate;
            made = mtwnTarget_GetValue(context, speed, &rate);
            made = mtwnPlayer_SetRate(context, player, (double)rate.x);

            CHECK(made == mtwn_success, "the rate set");
            lastRate = (double)rate.x;
        }
        last = (double)v.x;
    }
    CHECK(jumps == 0, "each frame the distance its rate gives");
    mtwnDestroyContext(context);
    return last;
}

static void TestEasedRate(void)
{
    // The speed (1 - p)^2 over the second travels 1/3 of it: 100 + 100/3.
    double stop = 100.0 + 100.0 / 3.0;
    double at60 = EasedRateStop(60, false);
    double at144 = EasedRateStop(144, false);
    // Past it by about a frame and a half at full speed: the rate a frame
    // behind, and each frame's left rectangle of a falling rate.
    CHECK(at60 > stop && at60 - stop <= 1.6 * 100.0 / 60.0 && at144 > stop &&
              at144 - stop <= 1.6 * 100.0 / 144.0,
          "by the rate set: past the stop by a frame and a half, 60 and 144 apart");
    CHECK(fabs(EasedRateStop(60, true) - stop) <= 1e-4 &&
              fabs(EasedRateStop(144, true) - stop) <= 1e-4,
          "swept exactly: the same stop at both");
}

static void TestTrauma(void)
{
    mtwnShake noise;
    CHECK(mtwnMakeShake(20.0, 1.0, 1.0, 99, &noise) == mtwn_success, "a noise");
    double trauma = 0.0;
    double dt = 1.0 / 60.0;
    int over = 0;
    int moved = 0;
    double last = 0.0;
    for (int frame = 0; frame <= 240; ++frame)
    {
        double now = frame * dt;
        if (frame == 0 || frame == 30)
        {
            // A hit raises the trauma (Eiserloh's camera shake), up to 1.
            trauma = fmin(trauma + 0.4, 1.0);
        }
        // Each frame: trauma decays 0.5 a second; the shake is its square, on
        // one clock that runs through every hit, so the noise never jumps.
        trauma = fmax(trauma - 0.5 * dt, 0.0);
        double shake = trauma * trauma;
        double n[3];
        double roll = 0.0; // degrees
        double dx = 0.0;   // pixels
        double dy = 0.0;
        if (mtwnShakeNoise(&noise, 0, now, &n[0]) == mtwn_success &&
            mtwnShakeNoise(&noise, 1, now, &n[1]) == mtwn_success &&
            mtwnShakeNoise(&noise, 2, now, &n[2]) == mtwn_success)
        {
            roll = 10.0 * shake * n[0];
            dx = 20.0 * shake * n[1];
            dy = 20.0 * shake * n[2];
        }
        over += fabs(roll) > 10.0 * shake || fabs(dx) > 20.0 * shake || fabs(dy) > 20.0 * shake;
        moved += dx != 0.0;
        last = fabs(roll) + fabs(dx) + fabs(dy);
    }
    CHECK(over == 0 && moved > 60, "within the trauma's square, shaking while it lasts");
    CHECK(last == 0.0 && trauma == 0.0, "still once the trauma is gone");
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
    CHECK(mtwnContext_Update(context, 100000000u, NULL, 0, NULL) == mtwn_success, "under way");
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
    mtwnResult updated = mtwnContext_Update(context, nowNs, changed, 64, &total);
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
    // Its share of the clip storage.
    mtwnResult built = mtwnContext_GetClipBytes(context, &clipDef, &bytes);
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
    CHECK(mtwnContext_Update(context, 300000000u, NULL, 0, NULL) == mtwn_success, "updated");
    mtwnPlayerState state;
    CHECK(mtwnPlayer_GetState(context, intro, &state) == mtwn_success && state.rate == -1.0 &&
              state.currentTime == 0.2,
          "at 0.2 s, running backwards from the next update");
    mtwnEvent events[32];
    uint32_t count = 0;
    uint64_t dropped = 0;
    mtwnResult read = mtwnContext_ReadEvents(context, events, 32, &count, &dropped);
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
    layered = mtwnPlayer_SetWeight(context, pulse, 0.25f); // the whole player's weight
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
    TestScroll();
    TestJump();
    TestTwos();
    TestEasedRate();
    TestHandles();
    TestTrauma();
    TestValues();
    TestScales();
    TestZoom();
    TestStagger();
    TestHost();
    return s_failures == 0 ? 0 : 1;
}
