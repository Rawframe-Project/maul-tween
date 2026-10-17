// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The timing model's discrete answers and the crossings' counts against
// answers worked out in exact rationals (tools/oracle/exact_oracle.py,
// records mtwn-0012 and mtwn-0037): timings and sweeps whose values are
// not dyadic, so the library rounds where the reference does not.

#include "crossing.h"
#include "test_harness.h"

#include "maul-tween/timing.h"

#include <string.h>

typedef struct ExactTiming
{
    double delay;
    double start;
    double iterations;
    double duration;
    double repeat;
    double rate;
    int direction;
} ExactTiming;

typedef struct ExactTimingCase
{
    ExactTiming timing;
    double local;
    double iterationLow;
    double iterationHigh;
    // 0 or 1, or -1 where either is right (an iteration's boundary).
    int inDelay;
} ExactTimingCase;

typedef struct ExactCountCase
{
    // The timelines above the leaf: 0, 1 or 2.
    int levels;
    ExactTiming outer;
    double innerStart;
    ExactTiming inner;
    int leafKind;
    double at;
    ExactTiming leaf;
    double from;
    double to;
    int same;
    int other;
} ExactCountCase;

#include "generated/exact_cases.h"

static mtwnTiming MakeTiming(const ExactTiming* in)
{
    mtwnTiming timing = mtwnDefaultTiming();
    timing.delay = in->delay;
    timing.iterationStart = in->start;
    timing.iterations = in->iterations;
    timing.duration = in->duration;
    timing.repeatDelay = in->repeat;
    timing.playbackRate = in->rate;
    timing.direction = (mtwnDirection)in->direction;
    timing.fill = mtwn_fillBoth;
    return timing;
}

static void TestTimings(void)
{
    int failures = 0;
    int count = (int)(sizeof s_exactTimings / sizeof s_exactTimings[0]);
    for (int i = 0; i < count; ++i)
    {
        const ExactTimingCase* c = &s_exactTimings[i];
        mtwnTiming timing = MakeTiming(&c->timing);
        mtwnTimingResult r;
        mtwnComputeTiming(&timing, c->local, false, &r);
        bool ok = r.phase == mtwn_phaseActive && r.iteration >= c->iterationLow &&
                  r.iteration <= c->iterationHigh &&
                  (c->inDelay < 0 || (int)r.inRepeatDelay == c->inDelay);
        if (!ok && failures++ < 5)
        {
            printf("  timing %d at %a: phase %d, iteration %.17g (%.17g to %.17g), delay %d/%d\n",
                   i, c->local, r.phase, r.iteration, c->iterationLow, c->iterationHigh,
                   r.inRepeatDelay, c->inDelay);
        }
    }
    CHECK(failures == 0, "every timing's phase, iteration and delay as exact rationals give");
    CHECK(count >= 1000, "the timings are all there");
}

static void TestCounts(void)
{
    int failures = 0;
    int count = (int)(sizeof s_exactCounts / sizeof s_exactCounts[0]);
    for (int i = 0; i < count; ++i)
    {
        const ExactCountCase* c = &s_exactCounts[i];
        mtwnLevel levels[2];
        memset(levels, 0, sizeof levels);
        if (c->levels >= 1)
        {
            levels[0].timing = MakeTiming(&c->outer);
        }
        if (c->levels == 2)
        {
            levels[1].timing = MakeTiming(&c->inner);
            levels[1].start = c->innerStart;
        }
        mtwnLeaf leaf;
        memset(&leaf, 0, sizeof leaf);
        leaf.kind = (mtwnLeafKind)c->leafKind;
        leaf.at = c->at;
        if (c->leafKind == mtwn_leafIterations)
        {
            leaf.timing = MakeTiming(&c->leaf);
        }
        mtwnPrepareLevels(levels, (uint32_t)c->levels, &leaf);
        mtwnCrossings got =
            mtwnCountCrossings(levels, (uint32_t)c->levels, &leaf, c->from, c->to, false);
        bool ok = got.same == (double)c->same && got.opposite == (double)c->other;
        if (!ok && failures++ < 5)
        {
            printf("  count %d (%d levels, leaf %d) %a to %a: %g/%g, exact %d/%d\n", i, c->levels,
                   c->leafKind, c->from, c->to, got.same, got.opposite, c->same, c->other);
        }
    }
    CHECK(failures == 0, "every sweep's crossings as exact rationals give");
    CHECK(count >= 1000, "the sweeps are all there");
}

int main(void)
{
    TestTimings();
    TestCounts();
    return s_failures == 0 ? 0 : 1;
}
