// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Randomized invariants (records mtwn-0020, mtwn-0046), from fixed seeds, run under
// the sanitizers in CI: the timing model over random timings and times,
// valid or not; the clip builder over random calls, valid or not; and
// random clips played under random controls, and random transitions on
// every value kind, frame by frame, reduced motion switched on and off
// among them; and random staggers, valid or not. What each must keep is
// checked, not what it computes: refusals change nothing, results stay
// in their ranges, values stay finite, records come in order, and under
// reduced motion nothing that can finish is left moving.

#include "context_internal.h"
#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static uint64_t s_state;

static uint32_t Next(void)
{
    s_state = s_state * 6364136223846793005u + 1442695040888963407u;
    return (uint32_t)(s_state >> 33);
}

// A double in [0, 1).
static double Unit(void)
{
    return (double)(Next() & 0xFFFFFFu) / 16777216.0;
}

// A double from a few kinds: ordinary, zero, negative, huge, infinite,
// NaN, so that refusals are tried too.
static double Wild(double scale)
{
    switch (Next() % 16)
    {
    case 0:
        return 0.0;
    case 1:
        return -scale * Unit();
    case 2:
        return HUGE_VAL;
    case 3:
        return (double)NAN;
    case 4:
        return 1e300;
    default:
        return scale * Unit();
    }
}

static mtwnTiming RandomTiming(bool wild)
{
    mtwnTiming t = mtwnDefaultTiming();
    t.duration = wild ? Wild(2.0) : 0.05 + 2.0 * Unit();
    t.delay = Next() % 3 == 0 ? (wild ? Wild(1.0) : Unit()) : 0.0;
    t.endDelay = Next() % 4 == 0 ? (wild ? Wild(1.0) : 0.5 * Unit()) : 0.0;
    switch (Next() % 5)
    {
    case 0:
        t.iterations = 1.0;
        break;
    case 1:
        t.iterations = (double)(1 + Next() % 4);
        break;
    case 2:
        t.iterations = wild ? Wild(4.0) : 0.25 + 3.0 * Unit();
        break;
    default:
        t.iterations = 2.5;
        break;
    }
    t.iterationStart = Next() % 3 == 0 ? (wild ? Wild(2.0) : 2.0 * Unit()) : 0.0;
    t.repeatDelay = Next() % 4 == 0 ? (wild ? Wild(0.5) : 0.3 * Unit()) : 0.0;
    t.playbackRate = Next() % 4 == 0 ? (Next() % 2 ? -1.5 : 2.0) : 1.0;
    t.direction = (mtwnDirection)(Next() % 4);
    t.fill = (mtwnFill)(Next() % 4);
    t.alternateEasing = (mtwnAlternateEasing)(Next() % 2);
    if (Next() % 3 == 0)
    {
        CHECK(mtwnMakePowerEasing(mtwn_easeInOut, 2.0, &t.easing) == mtwn_success, "an easing");
    }
    return t;
}

static bool Within(double x, double low, double high)
{
    return x >= low && x <= high;
}

// One result's invariants.
static bool Holds(const mtwnTiming* t, const mtwnTimingResult* r)
{
    double end = t->delay + r->activeDuration + t->endDelay;
    bool ok = r->activeDuration >= 0.0 && r->endTime == (end > 0.0 ? end : 0.0) &&
              r->phase <= mtwn_phaseAfter;
    if (isnan(r->activeTime))
    {
        return ok && isnan(r->transformedProgress);
    }
    double s = t->iterationStart;
    ok = ok && Within(r->simpleProgress, 0.0, 1.0) &&
         Within(r->overallProgress, s, s + t->iterations) &&
         r->directedProgress == (r->forwards ? r->simpleProgress : 1.0 - r->simpleProgress) &&
         !isnan(r->transformedProgress);
    if (isfinite(r->iteration))
    {
        ok = ok && r->iteration == floor(r->iteration) && r->iteration <= r->overallProgress &&
             r->iteration >= floor(s);
    }
    return ok;
}

static void TestTiming(void)
{
    s_state = 1;
    uint32_t checked = 0;
    uint32_t failures = 0;
    for (uint32_t i = 0; i < 20000; ++i)
    {
        mtwnTiming t = RandomTiming(i % 4 == 0);
        if (mtwnValidateTiming(&t) != mtwn_success)
        {
            continue;
        }
        mtwnTimingResult shape;
        mtwnComputeTiming(&t, 0.0, false, &shape);
        // Every phase's boundary exactly, and times around them; local
        // times are finite, so an endless node's end stands far off.
        double active = isinf(shape.activeDuration) ? 1e6 : shape.activeDuration;
        double end = isinf(shape.endTime) ? 1e6 : shape.endTime;
        double times[8] = {-1.0,
                           0.0,
                           t.delay,
                           t.delay + 0.5 * active,
                           t.delay + active,
                           end,
                           end + 1.0,
                           Unit() * (end + 1.0)};
        for (int k = 0; k < 8; ++k)
        {
            for (int way = 0; way < 2; ++way)
            {
                mtwnTimingResult r;
                mtwnComputeTiming(&t, times[k], way != 0, &r);
                failures += Holds(&t, &r) ? 0u : 1u;
                ++checked;
            }
        }
        // Forwards, filled, one way, linear, without repeat delays: the
        // overall progress never goes back as time goes on.
        if (t.playbackRate > 0.0 && t.fill == mtwn_fillBoth && t.repeatDelay == 0.0)
        {
            double last = -HUGE_VAL;
            for (int k = 0; k <= 64; ++k)
            {
                mtwnTimingResult r;
                double local = (isinf(shape.endTime) ? 8.0 : shape.endTime) * k / 64.0;
                mtwnComputeTiming(&t, local, false, &r);
                failures += r.overallProgress >= last ? 0u : 1u;
                last = r.overallProgress;
            }
        }
    }
    CHECK(checked > 100000 && failures == 0, "every timing result in its ranges");
}

// A random call on a clip being built: whether it was refused, and then
// that nothing changed.
static void BuildOne(mtwnContext* context, mtwnClipId clip, mtwnNode* timelines,
                     uint32_t* timelineCount, uint32_t* refusals, uint32_t* changedOnRefusal)
{
    mtwnClipSlot* slot = mtwnClipAt(context, clip.index1);
    uint32_t nodes = slot->nodeCount;
    uint32_t keyframes = slot->keyframeCount;
    mtwnNode parent = timelines[Next() % *timelineCount];
    if (Next() % 10 == 0)
    {
        parent = 1000 + Next() % 100;
    }
    mtwnPosition at;
    memset(&at, 0, sizeof at);
    at.kind = (mtwnPositionKind)(Next() % 6);
    at.offset = Wild(1.0);
    at.label = Next() % 4;
    mtwnNode node = 0;
    mtwnResult r = mtwn_success;
    switch (Next() % 4)
    {
    case 0:
    {
        mtwnClipTimelineDef def = mtwnDefaultClipTimelineDef();
        def.parent = parent;
        def.position = at;
        def.autoDuration = Next() % 2 == 0;
        def.timing = RandomTiming(true);
        r = mtwnClip_AddTimeline(context, clip, &def, &node);
        if (r == mtwn_success && *timelineCount < 8)
        {
            timelines[(*timelineCount)++] = node;
        }
        break;
    }
    case 1:
        r = mtwnClip_AddMark(context, clip, parent, at, Next() % 4, &node);
        break;
    case 2:
        r = mtwnClip_AddLabel(context, clip, parent, at, Next() % 4, &node);
        break;
    default:
    {
        mtwnKeyframe keys[3];
        memset(keys, 0, sizeof keys);
        uint32_t count = 1 + Next() % 3;
        for (uint32_t k = 0; k < count; ++k)
        {
            keys[k].value = (mtwnVec4){(float)Wild(5.0), 1.0f, 0.0f, 1.0f};
            keys[k].offset = k == 0 ? 0.0 : (k + 1 == count ? 1.0 : Wild(1.0));
            keys[k].easing = mtwnLinearEasing();
            keys[k].kind = (mtwnSegmentKind)(Next() % 5);
        }
        mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
        def.parent = parent;
        def.position = at;
        def.slot = Next() % 6;
        def.track.kind = (mtwnValueKind)(Next() % 10);
        def.keyframes = keys;
        def.keyframeCount = count;
        def.timing = RandomTiming(true);
        def.incremental = Next() % 4 == 0;
        def.constantSpeed = Next() % 4 == 0;
        r = mtwnClip_AddTrack(context, clip, &def, &node);
        break;
    }
    }
    if (r != mtwn_success)
    {
        ++*refusals;
        bool same = slot->nodeCount == nodes && slot->keyframeCount == keyframes && node == 0;
        *changedOnRefusal += same ? 0u : 1u;
    }
    else
    {
        *changedOnRefusal += slot->nodeCount == nodes + 1 ? 0u : 1u;
    }
}

static void TestBuilder(void)
{
    s_state = 2;
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    uint32_t refusals = 0;
    uint32_t changedOnRefusal = 0;
    uint32_t broken = 0;
    for (uint32_t c = 0; c < 400; ++c)
    {
        mtwnContext* context = NULL;
        CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
        mtwnClipDef def = mtwnDefaultClipDef();
        def.nodeCapacity = 4 + Next() % 12;
        def.keyframeCapacity = 4 + Next() % 24;
        def.slotCapacity = 1 + Next() % 6;
        mtwnClipId clip;
        CHECK(mtwnCreateClip(context, &def, &clip) == mtwn_success, "a clip");
        mtwnNode timelines[8] = {MTWN_ROOT_NODE};
        uint32_t timelineCount = 1;
        for (uint32_t k = 0; k < 24; ++k)
        {
            BuildOne(context, clip, timelines, &timelineCount, &refusals, &changedOnRefusal);
        }
        if (mtwnClip_Finish(context, clip) == mtwn_success)
        {
            double duration = 0.0;
            bool ok =
                mtwnClip_GetDuration(context, clip, &duration) == mtwn_success && duration >= 0.0;
            uint32_t nodes = mtwnClipAt(context, clip.index1)->nodeCount;
            for (mtwnNode n = 1; n <= nodes; ++n)
            {
                mtwnClipNodeInfo info;
                ok = ok && mtwnClip_GetNode(context, clip, n, &info) == mtwn_success &&
                     isfinite(info.start) && info.end >= info.start;
            }
            broken += ok ? 0u : 1u;
        }
        mtwnDestroyContext(context);
    }
    CHECK(refusals > 1000, "refusals tried");
    CHECK(changedOnRefusal == 0, "a refused call changes nothing, an accepted one adds a node");
    CHECK(broken == 0, "every finished clip's nodes at finite starts, ends after them");
}

static bool SameBits(mtwnVec4 a, mtwnVec4 b)
{
    uint32_t x[4];
    uint32_t y[4];
    memcpy(x, &a, sizeof x);
    memcpy(y, &b, sizeof y);
    return x[0] == y[0] && x[1] == y[1] && x[2] == y[2] && x[3] == y[3];
}

static bool Finite(mtwnVec4 v)
{
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z) && isfinite(v.w);
}

// Random clips of nested timelines, marks and tracks of numbers played
// under random controls: values finite, records in order and in the
// update's span.
static void TestPlaying(void)
{
    s_state = 3;
    uint32_t bad = 0;
    uint32_t records = 0;
    uint32_t stillRunning = 0;
    for (uint32_t c = 0; c < 150; ++c)
    {
        mtwnContextDef contextDef = mtwnDefaultContextDef();
        contextDef.eventCapacity = 1024;
        mtwnContext* context = NULL;
        CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
        mtwnClipDef def = mtwnDefaultClipDef();
        mtwnClipId clip;
        CHECK(mtwnCreateClip(context, &def, &clip) == mtwn_success, "a clip");
        mtwnNode timelines[8] = {MTWN_ROOT_NODE};
        uint32_t timelineCount = 1;
        uint32_t ignored = 0;
        uint32_t ignoredToo = 0;
        for (uint32_t k = 0; k < 12; ++k)
        {
            BuildOne(context, clip, timelines, &timelineCount, &ignored, &ignoredToo);
        }
        if (mtwnClip_Finish(context, clip) != mtwn_success)
        {
            mtwnDestroyContext(context);
            continue;
        }
        mtwnTargetId targets[6];
        mtwnBinding bindings[6];
        // A target for each slot a track uses, of the track's kind.
        uint32_t slots = 0;
        for (uint32_t used = 0; used < mtwnClipAt(context, clip.index1)->slotCount; ++used)
        {
            uint8_t kind = mtwnClipAt(context, clip.index1)->slotKinds[used];
            if (kind == 0)
            {
                continue;
            }
            uint32_t s = slots++;
            mtwnValueKind valueKind = (mtwnValueKind)(kind - 1);
            mtwnVec4 start = valueKind == mtwn_valueRotation || valueKind == mtwn_valueColor
                                 ? (mtwnVec4){0, 0, 0, 1.0f}
                                 : (mtwnVec4){0};
            CHECK(mtwnCreateTarget(context, valueKind, start, &targets[s]) == mtwn_success,
                  "a target");
            bindings[s] = mtwnBind(used, targets[s]);
        }
        mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
        playerDef.clip = clip;
        playerDef.bindings = bindings;
        playerDef.bindingCount = slots;
        playerDef.keep = Next() % 2 == 0;
        playerDef.essential = Next() % 4 == 0;
        mtwnPlayerId player;
        CHECK(mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success, "a player");
        uint64_t now = 0;
        for (uint32_t f = 0; f < 200; ++f)
        {
            if (mtwnPlayer_IsValid(context, player))
            {
                mtwnResult r = mtwn_success;
                switch (Next() % 30)
                {
                case 0:
                    r = mtwnPlayer_Seek(context, player, Wild(5.0));
                    break;
                case 1:
                    r = mtwnPlayer_Reverse(context, player);
                    break;
                case 2:
                    r = mtwnPlayer_SetRate(context, player, Wild(3.0) - 1.0);
                    break;
                case 3:
                    r = mtwnPlayer_Pause(context, player);
                    break;
                case 4:
                    r = mtwnPlayer_Play(context, player);
                    break;
                case 5:
                    r = mtwnPlayer_Sweep(context, player, Wild(5.0));
                    break;
                case 6:
                    r = mtwnPlayer_Finish(context, player);
                    break;
                case 7:
                    r = mtwnContext_SetReducedMotion(context, Next() % 2 == 0);
                    break;
                default:
                    break;
                }
                (void)r;
            }
            uint64_t before = now;
            now += 1000000u + (uint64_t)(Next() % 50) * 1000000u;
            CHECK(mtwnUpdate(context, now, NULL, 0, NULL) == mtwn_success, "an update");
            // Under reduced motion a player still running is essential,
            // or one that cannot finish: an infinite end ahead, or a rate
            // of 0.
            mtwnPlayerState played;
            if (mtwnContext_IsReducedMotion(context) && !playerDef.essential &&
                mtwnPlayer_GetState(context, player, &played) == mtwn_success &&
                played.state == mtwn_playRunning)
            {
                bool cannot = played.rate == 0.0 || (played.rate > 0.0 && isinf(played.duration));
                bad += cannot ? 0u : 1u;
                stillRunning += cannot ? 1u : 0u;
            }
            for (uint32_t s = 0; s < slots; ++s)
            {
                mtwnTargetState state;
                bad += mtwnTarget_GetState(context, targets[s], &state) == mtwn_success &&
                               Finite(state.value) && Finite(state.velocity)
                           ? 0u
                           : 1u;
            }
            // Static: 64 kB, the web build's whole stack.
            static mtwnEvent events[1024];
            uint32_t n = 0;
            uint64_t dropped = 0;
            CHECK(mtwnReadEvents(context, events, 1024, &n, &dropped) == mtwn_success, "read");
            records += n;
            for (uint32_t e = 0; e < n; ++e)
            {
                double host = events[e].hostTime;
                bool ordered = e == 0 || events[e - 1].hostTime <= host;
                bool spanned =
                    host >= (double)before / 1e9 - 1e-9 && host <= (double)now / 1e9 + 1e-9;
                bool counted = events[e].count >= 1 && events[e].kind >= mtwn_eventStart &&
                               events[e].kind <= mtwn_eventCancel;
                bad += ordered && spanned && counted && dropped == 0 ? 0u : 1u;
            }
        }
        mtwnDestroyContext(context);
    }
    CHECK(records > 1000, "records made");
    CHECK(stillRunning > 0, "players that cannot finish met under reduced motion");
    CHECK(bad == 0, "values finite, records ordered, counted and in their update's span, "
                    "and under reduced motion only what cannot finish still running");
}

// Random transitions of every value kind and spec, replacing each other:
// values finite, and each ends exactly on its committed end.
static void TestTransitions(void)
{
    s_state = 4;
    uint32_t bad = 0;
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&contextDef, &context) == mtwn_success, "a context");
    mtwnTargetId targets[9];
    for (uint32_t k = 0; k < 9; ++k)
    {
        mtwnVec4 start = k == mtwn_valueRotation || k == mtwn_valueColor ? (mtwnVec4){0, 0, 0, 1.0f}
                                                                         : (mtwnVec4){0};
        CHECK(mtwnCreateTarget(context, (mtwnValueKind)k, start, &targets[k]) == mtwn_success,
              "a target");
    }
    uint64_t now = 0;
    // Whether each target's latest transition is essential.
    bool essential[9] = {false};
    uint32_t reducedFrames = 0;
    for (uint32_t f = 0; f < 3000; ++f)
    {
        if (Next() % 50 == 0)
        {
            CHECK(mtwnContext_SetReducedMotion(context, !mtwnContext_IsReducedMotion(context)) ==
                      mtwn_success,
                  "reduced motion switched");
        }
        uint32_t k = Next() % 9;
        if (Next() % 4 == 0)
        {
            mtwnTransitionDef def = mtwnDefaultTransitionDef();
            def.target = targets[k];
            float a = (float)Unit();
            def.value = k == mtwn_valueRotation ? (mtwnVec4){a, 0.0f, 0.0f, 1.0f}
                                                : (mtwnVec4){a, 1.0f - a, a * 0.5f, 1.0f};
            def.duration = 0.3 * Unit();
            def.delay = Next() % 3 == 0 ? 0.1 * Unit() : 0.0;
            def.reverses = Next() % 2 == 0;
            def.essential = Next() % 3 == 0;
            uint32_t spec = Next() % 3;
            if (spec == 1 && k != mtwn_valueDiscrete)
            {
                def.spec = mtwn_transitionSpring;
                CHECK(mtwnMakeSpringFromDuration(0.1 + 0.3 * Unit(), 0.5 * Unit(), &def.spring) ==
                          mtwn_success,
                      "a spring");
            }
            else if (spec == 2 && k != mtwn_valueDiscrete && k != mtwn_valueColor &&
                     k != mtwn_valueRotation)
            {
                def.spec = mtwn_transitionDecay;
                def.velocitySource = mtwn_velocityGiven;
                def.velocity = (mtwnVec4){(float)(4.0 * Unit() - 2.0), 1.0f, -1.0f, 0.5f};
            }
            mtwnPlayerId player;
            bool started = mtwnStartTransition(context, &def, &player) == mtwn_success;
            bad += started ? 0u : 1u;
            essential[k] = started && def.essential;
        }
        now += 1000000u + (uint64_t)(Next() % 30) * 1000000u;
        CHECK(mtwnUpdate(context, now, NULL, 0, NULL) == mtwn_success, "an update");
        for (uint32_t t = 0; t < 9; ++t)
        {
            mtwnTargetState state;
            bool read = mtwnTarget_GetState(context, targets[t], &state) == mtwn_success;
            bool resting = mtwnTargetAt(context, targets[t].index1)->header.link == 0;
            bool ok = read && Finite(state.value) && Finite(state.velocity) && Finite(state.base) &&
                      (!resting || SameBits(state.value, state.base));
            // Reduced: nothing but an essential transition is left moving.
            bool reducedOk = !mtwnContext_IsReducedMotion(context) || resting || essential[t];
            bad += ok && reducedOk ? 0u : 1u;
        }
        reducedFrames += mtwnContext_IsReducedMotion(context) ? 1u : 0u;
    }
    CHECK(reducedFrames > 300 && reducedFrames < 2700, "reduced motion on and off by turns");
    CHECK(bad == 0, "every transition started, values finite, each at rest on its base, "
                    "under reduced motion only essential ones moving");
    mtwnDestroyContext(context);
}

// Random staggers, valid or not: a refusal writes nothing; an accepted
// one gives finite offsets, the nearest item's at the start, and with an
// amount and a monotonic easing every offset within the spread.
static void TestStaggers(void)
{
    s_state = 5;
    uint32_t bad = 0;
    uint32_t accepted = 0;
    uint32_t refused = 0;
    static double offsets[203];
    for (uint32_t c = 0; c < 4000; ++c)
    {
        mtwnStagger stagger = mtwnDefaultStagger();
        uint32_t count = Next() % 201;
        stagger.start = Next() % 8 == 0 ? Wild(10.0) : 10.0 * Unit() - 5.0;
        stagger.spacing = Next() % 8 == 0 ? Wild(2.0) : 2.0 * Unit() - 1.0;
        stagger.pointX = Next() % 8 == 0 ? Wild(2.0) : 2.0 * Unit() - 0.5;
        stagger.pointY = Next() % 8 == 0 ? Wild(2.0) : 2.0 * Unit() - 0.5;
        stagger.columns = Next() % 12;
        stagger.origin = (mtwnStaggerOrigin)(Next() % 8);
        stagger.axis = (mtwnStaggerAxis)(Next() % 4);
        stagger.spacingKind = (mtwnStaggerSpacing)(Next() % 3);
        stagger.index = count == 0 ? Next() % 3 : Next() % (count + 2);
        stagger.seed = Next();
        bool monotonic = Next() % 2 == 0;
        if (!monotonic)
        {
            CHECK(mtwnMakeBackEasing(mtwn_easeInOut, 1.70158, &stagger.easing) == mtwn_success,
                  "a back easing");
        }
        else if (Next() % 2 == 0)
        {
            CHECK(mtwnMakePowerEasing(mtwn_easeOut, 1.0 + 3.0 * Unit(), &stagger.easing) ==
                      mtwn_success,
                  "a power easing");
        }
        for (uint32_t i = 0; i < 203; ++i)
        {
            offsets[i] = -12345.0;
        }
        if (mtwnComputeStagger(&stagger, count, offsets) != mtwn_success)
        {
            ++refused;
            for (uint32_t i = 0; i < 203; ++i)
            {
                bad += offsets[i] == -12345.0 ? 0u : 1u;
            }
            continue;
        }
        ++accepted;
        bool atStart = count == 0;
        double low = stagger.start;
        double high = stagger.start + stagger.spacing;
        if (low > high)
        {
            double swap = low;
            low = high;
            high = swap;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            bad += isfinite(offsets[i]) ? 0u : 1u;
            atStart = atStart || offsets[i] == stagger.start;
            if (monotonic && stagger.spacingKind == mtwn_staggerAmount)
            {
                double slack = 1e-12 * (1.0 + fabs(low) + fabs(high));
                bad += offsets[i] >= low - slack && offsets[i] <= high + slack ? 0u : 1u;
            }
        }
        bad += offsets[count] == -12345.0 ? 0u : 1u;
        bad += atStart ? 0u : 1u;
    }
    CHECK(accepted > 1000 && refused > 500, "staggers accepted and refused");
    CHECK(bad == 0, "refusals write nothing; offsets finite, the nearest at the start, "
                    "within an amount's spread");
}

int main(void)
{
    TestTiming();
    TestBuilder();
    TestPlaying();
    TestTransitions();
    TestStaggers();
    return s_failures == 0 ? 0 : 1;
}
