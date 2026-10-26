// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Crossings: exact counts at the sweep's ends, through
// repeats, reversed and alternate iterations, negative rates and repeat
// delays, a stall over an endless fast repeat counted, not walked; and
// random nested levels against an oracle that samples the timing itself,
// splitting the sweep wherever any level changes iteration or phase and
// counting only within the pieces where nothing jumps.

#include "crossing.h"
#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/easing.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mtwnTiming Timing(double duration, double iterations)
{
    mtwnTiming timing = mtwnDefaultTiming();
    timing.duration = duration;
    timing.iterations = iterations;
    timing.fill = mtwn_fillBoth;
    return timing;
}

static mtwnLeaf Instant(double at)
{
    mtwnLeaf leaf;
    memset(&leaf, 0, sizeof leaf);
    leaf.at = at;
    leaf.kind = mtwn_leafInstant;
    return leaf;
}

static mtwnCrossings Count(mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf, double from,
                           double to)
{
    mtwnPrepareLevels(levels, count, leaf);
    return mtwnCountCrossings(levels, count, leaf, from, to, false);
}

static double Total(mtwnCrossings c)
{
    return c.same + c.opposite;
}

static void TestEnds(void)
{
    mtwnLevel level[1];
    memset(level, 0, sizeof level);
    level[0].timing = Timing(2.0, 1.0);
    mtwnLeaf mark = Instant(1.0);
    CHECK(Total(Count(level, 1, &mark, 0.0, 1.0)) == 1.0, "forwards, at the sweep's end: crossed");
    CHECK(Total(Count(level, 1, &mark, 1.0, 2.0)) == 0.0, "and not again from it");
    mtwnCrossings back = Count(level, 1, &mark, 2.0, 1.0);
    CHECK(back.same == 0.0 && back.opposite == 1.0, "backwards, at the sweep's end: crossed, back");
    CHECK(Total(Count(level, 1, &mark, 1.0, 0.0)) == 0.0, "and not again from it");
    CHECK(Total(mtwnCountCrossings(level, 1, &mark, 1.0, 1.0, true)) == 1.0 &&
              Total(mtwnCountCrossings(level, 1, &mark, 1.0, 1.0, false)) == 0.0,
          "a sweep from no time takes its own point");
    // No levels: the leaf in the sweep's own time.
    mtwnLeaf zero = Instant(0.0);
    CHECK(Total(Count(level, 0, &zero, -1.0, 0.0)) == 1.0, "no levels");
}

static void TestRepeats(void)
{
    mtwnLevel level[1];
    memset(level, 0, sizeof level);
    level[0].timing = Timing(1.0, 3.0);
    mtwnLeaf start = Instant(0.0);
    mtwnLeaf end = Instant(1.0);
    mtwnLeaf mid = Instant(0.5);
    CHECK(Total(Count(level, 1, &start, -1.0, 5.0)) == 3.0, "each iteration's start");
    CHECK(Total(Count(level, 1, &end, -1.0, 5.0)) == 3.0, "each iteration's end");
    CHECK(Total(Count(level, 1, &start, 0.0, 3.0)) == 2.0,
          "the first start at the sweep's open end");
    CHECK(Total(Count(level, 1, &end, 0.0, 2.0)) == 2.0, "an end and a start at 1 and 2");
    CHECK(Total(Count(level, 1, &start, 1.0, 2.0)) == 1.0 &&
              Total(Count(level, 1, &end, 1.0, 2.0)) == 1.0,
          "at a shared time both are reached together");
    CHECK(mtwnFindCrossing(level, 1, &mid, 0.0, 3.0, false, false, mtwn_crossingEither) == 0.5 &&
              mtwnFindCrossing(level, 1, &mid, 0.0, 3.0, false, true, mtwn_crossingEither) == 2.5,
          "the first and last crossings' times");
    level[0].timing.direction = mtwn_directionAlternate;
    level[0].timing.iterations = 4.0;
    mtwnLeaf quarter = Instant(0.25);
    mtwnCrossings c = Count(level, 1, &quarter, 0.0, 4.0);
    CHECK(c.same == 2.0 && c.opposite == 2.0, "alternate: two forwards, two back");
    // An hour's stall over a 10 ms endless repeat.
    level[0].timing = Timing(0.01, HUGE_VAL);
    mtwnLeaf inside = Instant(0.005);
    CHECK(Total(Count(level, 1, &inside, 0.0, 3600.0)) == 360000.0, "counted, not walked");
}

static void TestDelaysAndRates(void)
{
    mtwnLevel level[1];
    memset(level, 0, sizeof level);
    level[0].timing = Timing(1.0, 2.0);
    level[0].timing.repeatDelay = 0.5;
    mtwnLeaf end = Instant(1.0);
    mtwnLeaf start = Instant(0.0);
    CHECK(Total(Count(level, 1, &end, 0.0, 1.0)) == 1.0, "the end at the delay's start");
    CHECK(Total(Count(level, 1, &end, 1.0, 2.5)) == 1.0, "the last end");
    CHECK(Total(Count(level, 1, &start, 1.0, 1.5)) == 1.0, "the next start at the delay's end");
    CHECK(Total(Count(level, 1, &start, 1.5, 3.0)) == 0.0, "and not after");
    CHECK(Total(Count(level, 1, &start, 1.2, 1.4)) == 0.0, "nothing within the delay");
    // A rate of -1 runs the iterations from the last back.
    level[0].timing = Timing(1.0, 2.0);
    level[0].timing.playbackRate = -1.0;
    mtwnLeaf quarter = Instant(0.25);
    mtwnCrossings c = Count(level, 1, &quarter, -1.0, 3.0);
    CHECK(c.same == 0.0 && c.opposite == 2.0, "crossed twice, going back");
    double first =
        mtwnFindCrossing(level, 1, &quarter, -1.0, 3.0, false, false, mtwn_crossingEither);
    CHECK(fabs(first - 0.75) <= 2e-16, "first at 0.75, where the computed path reaches it");
}

static void TestExactEdges(void)
{
    mtwnLevel level[1];
    memset(level, 0, sizeof level);
    mtwnLeaf end = Instant(1.0);
    // The active end, from it backwards: its own point is left out.
    level[0].timing = Timing(1.0, 1.0);
    CHECK(Total(Count(level, 1, &end, 1.0, 0.5)) == 0.0, "a sweep back from the end leaves it");
    // Within a delay, backwards: the end it holds was reached before.
    level[0].timing = Timing(1.0, 2.0);
    level[0].timing.repeatDelay = 0.5;
    CHECK(Total(Count(level, 1, &end, 1.25, 0.5)) == 1.0, "from inside a delay back past its end");
    // At the delay's start exactly, going forwards: the end is reached at
    // that time, so a sweep taking only it takes the end.
    mtwnPrepareLevels(level, 1, &end);
    CHECK(Total(mtwnCountCrossings(level, 1, &end, 1.0, 1.0, true)) == 1.0,
          "the end at the delay's start, at that time");
    // A rate of -1 with a delay: iteration 1 runs back over [0, 1], the
    // delay over (1, 1.5], iteration 0 back over [1.5, 2.5]; iteration 0's
    // end is reached at 1, as iteration 1's start is left.
    level[0].timing.playbackRate = -1.0;
    CHECK(Total(Count(level, 1, &end, 1.5, 0.5)) == 1.0 &&
              Total(Count(level, 1, &end, 1.0, 1.5)) == 0.0,
          "backwards, the delay's far edge after the end reached");
    // Without a delay the end before a start shares its time.
    level[0].timing.repeatDelay = 0.0;
    CHECK(Total(Count(level, 1, &end, 0.5, 1.0)) == 1.0, "backwards, an end and a start together");
    // No iterations: one point, at the iteration start's progress.
    level[0].timing = Timing(1.0, 0.0);
    level[0].timing.iterationStart = 0.5;
    level[0].timing.delay = 1.0;
    mtwnLeaf half = Instant(0.5);
    mtwnLeaf quarter = Instant(0.25);
    CHECK(Total(Count(level, 1, &half, 0.0, 2.0)) == 1.0 &&
              Total(Count(level, 1, &quarter, 0.0, 2.0)) == 0.0,
          "no iterations: its one point");
    level[0].timing.iterationStart = 1.0;
    mtwnLeaf zero = Instant(0.0);
    CHECK(Total(Count(level, 1, &half, 0.0, 2.0)) == 0.0 &&
              Total(Count(level, 1, &zero, 0.0, 2.0)) == 1.0,
          "and at a whole iteration start, the start's point only");
    // Iteration starts after a delay: at 1.5 and 3, left out by a sweep
    // back from one.
    mtwnLeaf node;
    memset(&node, 0, sizeof node);
    node.kind = mtwn_leafIterations;
    node.timing = Timing(1.0, 3.0);
    node.timing.repeatDelay = 0.5;
    CHECK(Total(Count(level, 0, &node, 1.5, 1.2)) == 0.0 &&
              Total(Count(level, 0, &node, 1.2, 1.5)) == 1.0,
          "an iteration start after a delay, at the sweep's ends");
}

static void TestTimes(void)
{
    mtwnLevel level[1];
    memset(level, 0, sizeof level);
    // At an iteration boundary: the next start, where the path jumps.
    level[0].timing = Timing(1.0, 3.0);
    mtwnLeaf start = Instant(0.0);
    mtwnPrepareLevels(level, 1, &start);
    CHECK(mtwnFindCrossing(level, 1, &start, 0.5, 3.0, false, false, mtwn_crossingEither) == 1.0 &&
              mtwnFindCrossing(level, 1, &start, 0.5, 3.0, false, true, mtwn_crossingEither) == 2.0,
          "at a jump: the starts at 1 and 2 exactly");
    // At a repeat delay's edges: the end at its start, the next start at
    // its end.
    level[0].timing = Timing(1.0, 2.0);
    level[0].timing.repeatDelay = 0.5;
    mtwnLeaf end = Instant(1.0);
    mtwnPrepareLevels(level, 1, &end);
    CHECK(mtwnFindCrossing(level, 1, &end, 0.5, 3.0, false, false, mtwn_crossingEither) == 1.0 &&
              mtwnFindCrossing(level, 1, &end, 0.5, 3.0, false, true, mtwn_crossingEither) == 2.5,
          "the ends at the delay's start and the active end");
    mtwnPrepareLevels(level, 1, &start);
    CHECK(mtwnFindCrossing(level, 1, &start, 0.5, 3.0, false, false, mtwn_crossingEither) == 1.5,
          "the next start at the delay's end");
    // Backwards, from 3 to 0.5: the last start crossed first.
    CHECK(mtwnFindCrossing(level, 1, &start, 3.0, 0.5, false, false, mtwn_crossingEither) == 1.5,
          "backwards too");
    // Far from 0, where doubles thin out: still exact, in 64 halvings.
    level[0].timing = Timing(1.0, HUGE_VAL);
    mtwnLeaf mid = Instant(0.5);
    mtwnPrepareLevels(level, 1, &mid);
    double first =
        mtwnFindCrossing(level, 1, &mid, 1e12, 1e12 + 2.0, false, false, mtwn_crossingEither);
    CHECK(first == 1e12 + 0.5, "at 10^12 seconds");
}

static void TestIterations(void)
{
    mtwnLevel level[1];
    memset(level, 0, sizeof level);
    level[0].timing = Timing(3.0, 2.0);
    mtwnLeaf node;
    memset(&node, 0, sizeof node);
    node.kind = mtwn_leafIterations;
    node.timing = Timing(1.0, 3.0);
    node.at = 0.0;
    CHECK(Total(Count(level, 0, &node, -1.0, 2.0)) == 2.0, "two iteration starts, at 1 and 2");
    CHECK(Total(Count(level, 0, &node, -1.0, 1.0)) == 1.0 &&
              Total(Count(level, 0, &node, 1.0, 1.5)) == 0.0,
          "the one at the sweep's end once");
    CHECK(Total(Count(level, 1, &node, -1.0, 7.0)) == 4.0, "twice in a timeline repeated twice");
    mtwnCrossings back = Count(level, 1, &node, 7.0, -1.0);
    CHECK(back.same == 0.0 && back.opposite == 4.0, "backwards");
}

// Random numbers from a fixed seed (xorshift64), for the scan below.
static uint64_t s_scan = 88172645463325252u;

static double Uniform(void)
{
    s_scan ^= s_scan << 13;
    s_scan ^= s_scan >> 7;
    s_scan ^= s_scan << 17;
    return (double)(s_scan >> 11) * 0x1p-53;
}

// Times on 1/64 s, on tenths, or anywhere.
static double ScanTime(void)
{
    int kind = (int)(Uniform() * 3.0);
    return kind == 0   ? (1 + (int)(Uniform() * 16.0)) / 64.0
           : kind == 1 ? (1 + (int)(Uniform() * 4.0)) / 10.0
                       : 0.02 + 0.3 * Uniform();
}

// The iteration starts a local time has reached never fall as it grows,
// over every double within 40 of each iteration's start and end, for 400
// timings with repeat delays and either rate: a search for a crossing
// halves on the count, and one that fell again an ulp later would make
// where it ends depend on where it began. The count's
// position rounded as the timing model's does: a quotient rounded up past
// an iteration's start, or a progress rounded to its end.
// A whole iteration start whose cycles' quotient rounds below it (339
// times 0.2 over 0.2 floors to 338): the count starts in the first
// iteration, not at the end of one before it.
static void TestStartBelowItsQuotient(void)
{
    // A child of such a level starts at its own start, not at the end
    // of an iteration before the first: a mark late in it is crossed
    // once reached, not 1e-15 s into the level (an active time that
    // small is lost in the position, which would hold the end of
    // iteration 338, and a count from it would record the mark there).
    mtwnLevel level[1];
    memset(level, 0, sizeof level);
    level[0].timing = Timing(0.1, 3.0);
    level[0].timing.iterationStart = 339.0;
    level[0].timing.repeatDelay = 0.1;
    mtwnLeaf late = Instant(0.075);
    CHECK(Total(Count(level, 1, &late, -1.0, 1e-15)) == 0.0 &&
              Total(Count(level, 1, &late, -1.0, 0.08)) == 1.0,
          "a child at its start at the active start");
}

static void TestCountNeverFalls(void)
{
    s_scan = 88172645463325252u;
    mtwnLevel none[1];
    memset(none, 0, sizeof none);
    uint32_t falls = 0;
    uint32_t scanned = 0;
    for (int n = 0; n < 400; ++n)
    {
        mtwnLeaf node;
        memset(&node, 0, sizeof node);
        node.kind = mtwn_leafIterations;
        node.timing = Timing(ScanTime() * (1 + (int)(Uniform() * 3.0)),
                             1 + (int)(Uniform() * 6.0) + (Uniform() < 0.3 ? 0.5 : 0.0));
        node.timing.iterationStart = Uniform() < 0.25 ? 0.25 * (int)(Uniform() * 12.0) : 0.0;
        node.timing.repeatDelay = ScanTime();
        node.timing.direction = (mtwnDirection)(int)(Uniform() * 4.0);
        static const double rates[] = {1.0, 2.0, 0.5, 3.0, -1.0, -1.5};
        node.timing.playbackRate = rates[(int)(Uniform() * 6.0)];
        mtwnTimingResult shape = ComputeTiming(&node.timing, 0.0, false);
        double d = node.timing.duration;
        double cycle = d + node.timing.repeatDelay;
        double s = node.timing.iterationStart;
        double rate = fabs(node.timing.playbackRate);
        for (double j = floor(s) + 1.0; j <= ceil(s + node.timing.iterations); ++j)
        {
            double positions[2] = {j * cycle, (j - 1.0) * cycle + d};
            for (int w = 0; w < 2; ++w)
            {
                double scaled = (positions[w] - floor(s) * cycle - (s - floor(s)) * d) / rate;
                double x = node.timing.playbackRate > 0.0 ? scaled : shape.activeDuration - scaled;
                for (int k = 0; k < 40; ++k)
                {
                    x = nextafter(x, -HUGE_VAL);
                }
                double last = -1.0;
                for (int k = 0; k < 80; ++k, x = nextafter(x, HUGE_VAL))
                {
                    double reached = Total(Count(none, 0, &node, -1.0, x));
                    falls += reached < last ? 1u : 0u;
                    last = reached;
                    ++scanned;
                }
            }
        }
    }
    if (falls != 0)
    {
        printf("  the count fell %u times\n", falls);
    }
    CHECK(scanned > 50000 && falls == 0, "the count never falls as the time grows");
}

// The oracle: each level's state from the timing itself.
typedef struct State
{
    double iteration[MTWN_MAX_CLIP_DEPTH + 1];
    int phase[MTWN_MAX_CLIP_DEPTH + 1];
    double value;
} State;

static State Eval(const mtwnLevel* levels, uint32_t count, double t)
{
    State state;
    memset(&state, 0, sizeof state);
    double x = t - levels[0].start;
    for (uint32_t i = 0; i < count; ++i)
    {
        mtwnTimingResult r = ComputeTiming(&levels[i].timing, x, false);
        state.iteration[i] = r.iteration;
        state.phase[i] = (int)r.phase;
        double child = isinf(levels[i].timing.duration)
                           ? (r.forwards ? r.activeTime * levels[i].timing.playbackRate : HUGE_VAL)
                           : r.transformedProgress * levels[i].timing.duration;
        double next = i + 1 < count ? levels[i + 1].start : 0.0;
        x = child - next;
    }
    state.value = x;
    return state;
}

static bool Same(const State* a, const State* b, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        if (a->iteration[i] != b->iteration[i] || a->phase[i] != b->phase[i])
        {
            return false;
        }
    }
    return true;
}

// The iteration a node beginning at a child time is in, at a value of
// that child time.
static double NodeIteration(const mtwnLeaf* leaf, double value)
{
    mtwnTimingResult r = ComputeTiming(&leaf->timing, value - leaf->at, false);
    return r.iteration;
}

// The earliest crossing the oracle met, in the sweep's order.
static double s_oracleFirst = 0.0;
static bool s_oracleFound = false;

static void Met(double t, bool forwardSweep)
{
    if (!s_oracleFound || (forwardSweep ? t < s_oracleFirst : t > s_oracleFirst))
    {
        s_oracleFirst = t;
        s_oracleFound = true;
    }
}

// Where in a smooth piece the leaf's coordinate passes at.
static double Locate(const mtwnLevel* levels, uint32_t count, double at, double t1, double t2)
{
    double v1 = Eval(levels, count, t1).value;
    for (int i = 0; i < 80; ++i)
    {
        double mid = t1 + (t2 - t1) / 2.0;
        double v = Eval(levels, count, mid).value;
        if ((v < at) == (v1 < at))
        {
            t1 = mid;
        }
        else
        {
            t2 = mid;
        }
    }
    return t1 + (t2 - t1) / 2.0;
}

static void Oracle(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf, double t1,
                   double t2, int depth, mtwnCrossings* sum)
{
    double at = leaf->at;
    State a = Eval(levels, count, t1);
    State b = Eval(levels, count, t2);
    bool same = Same(&a, &b, count);
    if (same && leaf->kind == mtwn_leafIterations)
    {
        double from = NodeIteration(leaf, a.value);
        double to = NodeIteration(leaf, b.value);
        if (from == to)
        {
            return;
        }
        if (fabs(t2 - t1) < 1e-11 || depth > 200)
        {
            // An iteration start, its node moving as its parent's time does.
            *(b.value > a.value ? &sum->same : &sum->opposite) += fabs(to - from);
            return;
        }
    }
    else if (same)
    {
        bool forwards = b.value > a.value;
        double lo = forwards ? a.value : b.value;
        double hi = forwards ? b.value : a.value;
        if (lo < at && at < hi)
        {
            *(forwards ? &sum->same : &sum->opposite) += 1.0;
            Met(Locate(levels, count, at, t1, t2), t2 > t1);
        }
        return;
    }
    if (fabs(t2 - t1) < 1e-11 || depth > 200)
    {
        return;
    }
    double mid = t1 + (t2 - t1) / 2.0;
    Oracle(levels, count, leaf, t1, mid, depth + 1, sum);
    Oracle(levels, count, leaf, mid, t2, depth + 1, sum);
}

static uint64_t s_seed = 0x9E3779B97F4A7C15u;

static double Random(void)
{
    s_seed ^= s_seed << 13;
    s_seed ^= s_seed >> 7;
    s_seed ^= s_seed << 17;
    return (double)(s_seed >> 11) / 9007199254740992.0;
}

// The generator's state from a number (SplitMix64's finalizer): each
// oracle trial seeds its own, so a slice of the trials runs the very cases
// the whole run does.
static void Seed(uint64_t n)
{
    uint64_t z = n * 0x9E3779B97F4A7C15u + 0x632BE59BD9B4E019u;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    s_seed = (z ^ (z >> 31)) | 1u;
}

static double Pick(const double* values, int count)
{
    return values[(int)(Random() * count) % count];
}

static mtwnTiming RandomTiming(void)
{
    static const double durations[] = {0.75, 1.0, 1.6};
    static const double iterations[] = {1.0, 2.0, 2.5, 3.7};
    // Starts past the first iteration too, where the iterations before
    // the start each take a repeat delay.
    static const double starts[] = {0.0, 0.0, 0.3, 1.3, 2.0};
    static const double delays[] = {0.0, 0.0, 0.4};
    static const double rates[] = {1.0, 2.0, 0.5, -1.0};
    mtwnTiming timing = Timing(Pick(durations, 3), Pick(iterations, 4));
    timing.iterationStart = Pick(starts, 5);
    timing.repeatDelay = Pick(delays, 3);
    timing.delay = Random() * 0.5;
    timing.playbackRate = Pick(rates, 4);
    timing.direction = (mtwnDirection)(int)(Random() * 4.0);
    timing.alternateEasing = (uint8_t)(Random() < 0.5 ? 0 : 1);
    // A symmetric easing and one that is not: a reversed iteration runs
    // its shape back or repeats it, which only the latter tells apart.
    double easing = Random();
    if (easing < 0.33)
    {
        CHECK(mtwnMakePowerEasing(mtwn_easeInOut, 2.0, &timing.easing) == mtwn_success, "eased");
    }
    else if (easing < 0.66)
    {
        CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &timing.easing) == mtwn_success, "eased in");
    }
    return timing;
}

static bool SameBits(double a, double b)
{
    return memcmp(&a, &b, sizeof a) == 0;
}

static double Way(mtwnCrossings c, mtwnCrossingWay way)
{
    return way == mtwn_crossingForwards    ? c.same
           : way == mtwn_crossingBackwards ? c.opposite
                                           : c.same + c.opposite;
}

// Whether a found time is where the count flips: the sweep to it (from
// it, for the last) counts one, and to its neighbour toward the near end
// does not; or it is the near end itself, which counts.
static bool Flips(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf, double t1,
                  double t2, bool last, mtwnCrossingWay way, double t)
{
    double near = last ? t2 : t1;
    if (t == near)
    {
        return last && Way(mtwnCountCrossings(levels, count, leaf, t, t, true), way) >= 1.0;
    }
    double before = nextafter(t, near);
    double at = last ? Way(mtwnCountCrossings(levels, count, leaf, t, t2, true), way)
                     : Way(mtwnCountCrossings(levels, count, leaf, t1, t, false), way);
    double short_ = last ? Way(mtwnCountCrossings(levels, count, leaf, before, t2, true), way)
                         : Way(mtwnCountCrossings(levels, count, leaf, t1, before, false), way);
    return at >= 1.0 && short_ < 1.0;
}

// For each way a sweep counts a crossing, first and last: the search
// from guesses (the ends, the middle, a random point, far outside either
// way, NaN, and every double within four of the answer) finds a time
// where the count flips,
// within 1e-9 of the search from none (the count's rounding can flip it
// more than once within a few doubles, and the halving's path picks one).
// Returns the searches compared.
static int GuessesAgree(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf, double t1,
                        double t2, mtwnCrossings exact, int* mismatches)
{
    static const mtwnCrossingWay s_ways[3] = {mtwn_crossingEither, mtwn_crossingForwards,
                                              mtwn_crossingBackwards};
    int compared = 0;
    for (int w = 0; w < 3; ++w)
    {
        double n = s_ways[w] == mtwn_crossingForwards    ? exact.same
                   : s_ways[w] == mtwn_crossingBackwards ? exact.opposite
                                                         : exact.same + exact.opposite;
        if (n < 1.0)
        {
            continue;
        }
        for (int last = 0; last < 2; ++last)
        {
            double plain =
                mtwnFindCrossing(levels, count, leaf, t1, t2, false, last == 1, s_ways[w]);
            // The answer and every double within four of it, where the
            // count's rounding can flip it back and forth.
            double guesses[16] = {t1,       t2,       (t1 + t2) / 2.0, t1 + Random() * (t2 - t1),
                                  t2 + 1e6, t1 - 1e6, (double)NAN,     plain};
            double up = plain;
            double down = plain;
            for (int k = 0; k < 4; ++k)
            {
                up = nextafter(up, HUGE_VAL);
                down = nextafter(down, -HUGE_VAL);
                guesses[8 + 2 * k] = up;
                guesses[9 + 2 * k] = down;
            }
            for (int g = 0; g < 16; ++g)
            {
                double from = mtwnFindCrossingFrom(levels, count, leaf, t1, t2, false, last == 1,
                                                   s_ways[w], guesses[g], NULL);
                bool flips = Flips(levels, count, leaf, t1, t2, last == 1, s_ways[w], from);
                *mismatches += flips && fabs(from - plain) <= 1e-9 ? 0 : 1;
                ++compared;
            }
        }
    }
    return compared;
}

// Slice part of parts of the trials: every one when parts is 1, the
// counts the checks ask for in proportion.
static void TestOracle(int part, int parts)
{
    int mismatches = 0;
    int timeMismatches = 0;
    int timed = 0;
    int guessed = 0;
    int guessMismatches = 0;
    int memoCounted = 0;
    int memoMismatches = 0;
    // A block of consecutive trials, which keeps every third one's leaf of
    // iterations in each slice.
    for (int trial = part * 3000 / parts; trial < (part + 1) * 3000 / parts; ++trial)
    {
        Seed((uint64_t)trial);
        mtwnLevel levels[3];
        memset(levels, 0, sizeof levels);
        uint32_t count = 1 + (uint32_t)(Random() * 3.0) % 3;
        for (uint32_t i = 0; i < count; ++i)
        {
            levels[i].timing = RandomTiming();
            levels[i].start = i == 0 ? 0.0 : Random() * 0.6;
        }
        mtwnLeaf leaf = Instant(Random() * levels[count - 1].timing.duration);
        if (trial % 3 == 2)
        {
            // A node's iteration starts, its timing positive in rate.
            leaf.kind = mtwn_leafIterations;
            leaf.timing = RandomTiming();
            leaf.timing.delay = 0.0;
            leaf.timing.duration = leaf.timing.duration * 0.25;
        }
        mtwnPrepareLevels(levels, count, &leaf);
        // One memo for this leaf's sweeps: its walks are shared across them.
        mtwnCrossingMemo memo;
        memo.count = 0;
        memo.next = 0;
        mtwnTimingResult root = ComputeTiming(&levels[0].timing, 0.0, false);
        double span = root.endTime + 1.0;
        for (int sweep = 0; sweep < 12; ++sweep)
        {
            double t1 = Random() * span - 0.5;
            double t2 = Random() * span - 0.5;
            mtwnCrossings exact = mtwnCountCrossings(levels, count, &leaf, t1, t2, false);
            for (int closed = 0; closed < 2; ++closed)
            {
                mtwnCrossings plain = mtwnCountCrossings(levels, count, &leaf, t1, t2, closed);
                mtwnCrossings kept =
                    mtwnCountCrossingsMemo(levels, count, &leaf, t1, t2, closed, &memo);
                memoMismatches +=
                    SameBits(plain.same, kept.same) && SameBits(plain.opposite, kept.opposite) ? 0
                                                                                               : 1;
                ++memoCounted;
            }
            mtwnCrossings oracle = {0.0, 0.0};
            s_oracleFound = false;
            Oracle(levels, count, &leaf, t1, t2, 0, &oracle);
            if (leaf.kind == mtwn_leafInstant && s_oracleFound && exact.same == oracle.same &&
                exact.opposite == oracle.opposite)
            {
                double first = mtwnFindCrossing(levels, count, &leaf, t1, t2, false, false,
                                                mtwn_crossingEither);
                if (fabs(first - s_oracleFirst) > 1e-9)
                {
                    timeMismatches++;
                }
                timed++;
            }
            guessed += GuessesAgree(levels, count, &leaf, t1, t2, exact, &guessMismatches);
            if (exact.same != oracle.same || exact.opposite != oracle.opposite)
            {
                if (mismatches++ < 5)
                {
                    printf("trial %d sweep %d: depth %u, %g to %g: %g/%g, oracle %g/%g\n", trial,
                           sweep, count, t1, t2, exact.same, exact.opposite, oracle.same,
                           oracle.opposite);
                    if (leaf.kind == mtwn_leafIterations)
                    {
                        const mtwnTiming* t = &leaf.timing;
                        printf("  node: d %g n %g s %g r %g rate %g dir %d\n", t->duration,
                               t->iterations, t->iterationStart, t->repeatDelay, t->playbackRate,
                               (int)t->direction);
                    }
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        const mtwnTiming* t = &levels[i].timing;
                        printf("  level %u start %g: d %g n %g s %g r %g delay %g rate %g dir %d "
                               "alt %d eased %d; leaf %g\n",
                               i, levels[i].start, t->duration, t->iterations, t->iterationStart,
                               t->repeatDelay, t->delay, t->playbackRate, (int)t->direction,
                               (int)t->alternateEasing, (int)t->easing.kind, leaf.at);
                    }
                }
            }
        }
    }
    CHECK(mismatches == 0, "random nested levels agree with the sampled oracle");
    CHECK(memoMismatches == 0 && memoCounted > 50000 / parts,
          "counts with remembered walks the same bits as without, open and closed");
    CHECK(guessMismatches == 0 && guessed > 20000 / parts,
          "a search from any guess finds where the count flips, as one from none does");
    CHECK(timeMismatches == 0 && timed > 5000 / parts,
          "and the first crossing's time with where the oracle meets it");
}

// An outer level passing its time through (endless, from 0, without
// delay, at rate 1, forwards): the levels below it count the same without
// it, and the searches find the same times, before 0 too; one below with
// something before its start, a negative delay, is not passed. Over a
// number of random trials: 400 in the exact cases, 2,000 in the dense
// test (`test_crossing passed`).
static void TestPassedLevels(int trials)
{
    Seed(1u << 20);
    static const mtwnCrossingWay s_ways[3] = {mtwn_crossingEither, mtwn_crossingForwards,
                                              mtwn_crossingBackwards};
    int passed = 0;
    int kept = 0;
    int notPassing = 0;
    int wronglyPassed = 0;
    int compared = 0;
    int mismatches = 0;
    for (int trial = 0; trial < trials; ++trial)
    {
        mtwnLevel levels[3];
        memset(levels, 0, sizeof levels);
        // The outer level alone above the leaf, or over one or two more.
        uint32_t count = 1 + (uint32_t)(Random() * 3.0) % 3;
        levels[0].timing = Timing(HUGE_VAL, 1.0);
        for (uint32_t i = 1; i < count; ++i)
        {
            levels[i].timing = RandomTiming();
            levels[i].start = Random() * 0.6;
        }
        if (trial % 2 == 0 && count > 1)
        {
            // Repeating forever, as a loop does.
            levels[1].timing.iterations = HUGE_VAL;
            levels[1].timing.playbackRate = fabs(levels[1].timing.playbackRate);
        }
        if (trial % 5 == 0 && count > 1)
        {
            levels[1].timing.delay = -0.3;
        }
        // An outer level that does not pass its time through: delayed, at
        // another rate, starting later, or running backwards.
        int changed = trial % 11;
        levels[0].timing.delay = changed == 1 ? 0.2 : 0.0;
        levels[0].timing.playbackRate = changed == 2 ? 2.0 : 1.0;
        levels[0].start = changed == 3 ? 0.1 : 0.0;
        levels[0].timing.direction = changed == 4 ? mtwn_directionReverse : mtwn_directionNormal;
        mtwnLeaf leaf = Instant(Random() * (count > 1 ? levels[count - 1].timing.duration : 2.0));
        if (trial % 3 == 2)
        {
            leaf.kind = mtwn_leafIterations;
            leaf.timing = RandomTiming();
            leaf.timing.delay = 0.0;
            leaf.timing.duration = leaf.timing.duration * 0.25;
        }
        mtwnPrepareLevels(levels, count, &leaf);
        uint32_t p = mtwnPassedLevels(levels, count);
        notPassing += changed >= 1 && changed <= 4 ? 1 : 0;
        wronglyPassed += changed >= 1 && changed <= 4 && p == 1 ? 1 : 0;
        passed += p == 1 ? 1 : 0;
        kept += p == 0 ? 1 : 0;
        for (int sweep = 0; sweep < 12 && p == 1; ++sweep)
        {
            double t1 = Random() * 5.0 - 1.0;
            double t2 = Random() * 5.0 - 1.0;
            for (int closed = 0; closed < 2; ++closed)
            {
                mtwnCrossings all = mtwnCountCrossings(levels, count, &leaf, t1, t2, closed);
                mtwnCrossings below =
                    mtwnCountCrossings(levels + 1, count - 1, &leaf, t1, t2, closed);
                mismatches += all.same == below.same && all.opposite == below.opposite ? 0 : 1;
                ++compared;
                for (int w = 0; w < 3; ++w)
                {
                    if (Way(all, s_ways[w]) < 1.0)
                    {
                        continue;
                    }
                    for (int last = 0; last < 2; ++last)
                    {
                        double a = mtwnFindCrossing(levels, count, &leaf, t1, t2, closed, last == 1,
                                                    s_ways[w]);
                        double b = mtwnFindCrossing(levels + 1, count - 1, &leaf, t1, t2, closed,
                                                    last == 1, s_ways[w]);
                        mismatches += SameBits(a, b) ? 0 : 1;
                        ++compared;
                    }
                }
            }
        }
    }
    CHECK(mismatches == 0 && compared > 10 * trials,
          "levels below one passing its time through count and find the same without it");
    CHECK(passed > trials / 2 && kept > trials / 200,
          "an outer level passed, and not over a negative delay");
    CHECK(notPassing > trials / 4 && wronglyPassed == 0,
          "an outer level delayed, at another rate, later or backwards not passed");

    // An instant the level below reaches before 0: passing the outer level
    // would count it on a sweep that ends before 0, which the outer level
    // never reaches.
    mtwnLevel levels[2];
    memset(levels, 0, sizeof levels);
    levels[0].timing = Timing(HUGE_VAL, 1.0);
    levels[1].timing = Timing(1.0, 1.0);
    levels[1].timing.delay = -0.3;
    mtwnLeaf leaf = Instant(0.1);
    mtwnPrepareLevels(levels, 2, &leaf);
    mtwnCrossings all = mtwnCountCrossings(levels, 2, &leaf, -0.5, -0.1, false);
    mtwnCrossings below = mtwnCountCrossings(levels + 1, 1, &leaf, -0.5, -0.1, false);
    CHECK(mtwnPassedLevels(levels, 2) == 0 && all.same == 0.0 && below.same == 1.0,
          "not passed over a level with something before its start");
}

// No argument: every test. "exact": all but the oracle's trials, with
// the passed levels' fewer trials; "passed": the passed levels' every
// trial; "oracle K N": slice K of N of the oracle's, so ctest runs the
// slices side by side.
int main(int argc, char** argv)
{
    bool exact = argc == 1 || (argc == 2 && strcmp(argv[1], "exact") == 0);
    bool passed = argc == 2 && strcmp(argv[1], "passed") == 0;
    bool sliced = argc == 4 && strcmp(argv[1], "oracle") == 0;
    int part = sliced ? atoi(argv[2]) : 0;
    int parts = sliced ? atoi(argv[3]) : 1;
    if ((!exact && !passed && !sliced) || parts < 1 || part < 0 || part >= parts)
    {
        printf("usage: test_crossing [exact | passed | oracle K N]\n");
        return 2;
    }
    if (passed)
    {
        TestPassedLevels(2000);
    }
    if (exact)
    {
        TestEnds();
        TestRepeats();
        TestDelaysAndRates();
        TestIterations();
        TestStartBelowItsQuotient();
        TestCountNeverFalls();
        TestExactEdges();
        TestTimes();
        TestPassedLevels(argc == 1 ? 2000 : 400);
    }
    if (sliced || argc == 1)
    {
        TestOracle(part, parts);
    }
    return s_failures == 0 ? 0 : 1;
}
