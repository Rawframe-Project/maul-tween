// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Timing (records mtwn-0012, mtwn-0020, mtwn-0027): every case the
// independent oracle in tools/oracle/timing_oracle.py computed, matched
// to the bit, and the specification's own examples written out.

#include "test_harness.h"

#include "maul-tween/timing.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct TimingInput
{
    double delay;
    double endDelay;
    double iterationStart;
    double iterations;
    double duration;
    double repeatDelay;
    double rate;
    int fill;
    int direction;
    int alternate;
    int easing;
    int backwards;
} TimingInput;

typedef struct TimingCase
{
    TimingInput input;
    double localTime;
    int phase;
    double expected[8];
    int before;
    int inRepeatDelay;
} TimingCase;

#include "generated/timing_cases.h"

static mtwnTiming MakeTiming(const TimingInput* in)
{
    mtwnTiming timing = mtwnDefaultTiming();
    timing.delay = in->delay;
    timing.endDelay = in->endDelay;
    timing.iterationStart = in->iterationStart;
    timing.iterations = in->iterations;
    timing.duration = in->duration;
    timing.repeatDelay = in->repeatDelay;
    timing.playbackRate = in->rate;
    timing.fill = (mtwnFill)in->fill;
    timing.direction = (mtwnDirection)in->direction;
    timing.alternateEasing = (mtwnAlternateEasing)in->alternate;
    if (in->easing == 1)
    {
        CHECK(mtwnMakeSteps(4, mtwn_jumpStart, &timing.easing) == mtwn_success, "steps");
    }
    else if (in->easing == 2)
    {
        CHECK(mtwnMakeSteps(3, mtwn_jumpEnd, &timing.easing) == mtwn_success, "steps");
    }
    return timing;
}

// Equal to the bit, NaN matching NaN.
static bool Same(double a, double b)
{
    return (isnan(a) && isnan(b)) || memcmp(&a, &b, sizeof a) == 0 || (a == 0.0 && b == 0.0);
}

static void TestOracleCases(void)
{
    int failures = 0;
    int count = (int)(sizeof s_cases / sizeof s_cases[0]);
    for (int i = 0; i < count; ++i)
    {
        const TimingCase* c = &s_cases[i];
        mtwnTiming timing = MakeTiming(&c->input);
        if (mtwnValidateTiming(&timing) != mtwn_success)
        {
            ++failures;
            printf("  case %d refused\n", i);
            continue;
        }
        mtwnTimingResult r;
        mtwnComputeTiming(&timing, c->localTime, c->input.backwards != 0, &r);
        double got[8] = {r.activeDuration, r.endTime,   r.activeTime,       r.overallProgress,
                         r.simpleProgress, r.iteration, r.directedProgress, r.transformedProgress};
        bool ok = r.phase == c->phase && (int)r.before == c->before &&
                  (int)r.inRepeatDelay == c->inRepeatDelay;
        for (int k = 0; k < 8; ++k)
        {
            ok = ok && Same(got[k], c->expected[k]);
        }
        if (!ok)
        {
            if (failures < 5)
            {
                printf("  case %d at %a: phase %d/%d before %d/%d repeat %d/%d\n", i, c->localTime,
                       r.phase, c->phase, r.before, c->before, r.inRepeatDelay, c->inRepeatDelay);
                for (int k = 0; k < 8; ++k)
                {
                    if (!Same(got[k], c->expected[k]))
                    {
                        printf("    field %d: %a, oracle %a\n", k, got[k], c->expected[k]);
                    }
                }
            }
            ++failures;
        }
    }
    CHECK(failures == 0, "every oracle case to the bit");
    CHECK(count >= 1000, "the oracle's cases are all there");
}

static mtwnTimingResult At(const mtwnTiming* timing, double time)
{
    mtwnTimingResult r;
    mtwnComputeTiming(timing, time, false, &r);
    return r;
}

static void TestExamples(void)
{
    mtwnTiming timing = mtwnDefaultTiming();
    timing.duration = 2.0;
    timing.iterations = 3.0;
    timing.fill = mtwn_fillForwards;
    mtwnTimingResult r = At(&timing, 6.0);
    CHECK(r.phase == mtwn_phaseAfter && r.simpleProgress == 1.0 && r.iteration == 2.0,
          "an end on an iteration's end holds it, not the next one's start");
    timing.fill = mtwn_fillNone;
    r = At(&timing, 6.0);
    CHECK(isnan(r.transformedProgress), "no fill: unresolved after");
    timing.direction = mtwn_directionAlternate;
    r = At(&timing, 3.0);
    CHECK(r.iteration == 1.0 && r.directedProgress == 0.5, "alternate");
    r = At(&timing, 2.5);
    CHECK(r.directedProgress == 0.75, "the second iteration runs back");
    // A repeat delay of 1 between three iterations of 2: 8 seconds.
    timing = mtwnDefaultTiming();
    timing.duration = 2.0;
    timing.iterations = 3.0;
    timing.repeatDelay = 1.0;
    timing.fill = mtwn_fillBoth;
    CHECK(At(&timing, 0.0).activeDuration == 8.0, "the repeat delay between, not after");
    r = At(&timing, 2.5);
    CHECK(r.inRepeatDelay && r.simpleProgress == 1.0 && r.iteration == 0.0,
          "within the delay the first iteration's end holds");
    r = At(&timing, 4.0);
    CHECK(!r.inRepeatDelay && r.iteration == 1.0 && r.simpleProgress == 0.5, "the second");
    r = At(&timing, 8.0);
    CHECK(r.phase == mtwn_phaseAfter && r.iteration == 2.0 && r.simpleProgress == 1.0,
          "the last iteration has no delay after it");
    // Iterations of 0.1 s with delays of 0.1 s: at 0.5 s the third
    // iteration's delay starts, where the progress 0.1 / 0.1 rounds to just
    // below 1 and 2 plus it rounds to 3, the next iteration's start shown
    // for one time before the delay held the end.
    timing.duration = 0.1;
    timing.iterations = 5.0;
    timing.repeatDelay = 0.1;
    r = At(&timing, 0.5);
    CHECK(r.inRepeatDelay && r.iteration == 2.0 && r.simpleProgress == 1.0,
          "a progress rounded to the end holds it");
    // Repeating the shape: an ease-in stays an ease-in on the way back.
    timing = mtwnDefaultTiming();
    timing.duration = 1.0;
    timing.iterations = 2.0;
    timing.direction = mtwn_directionAlternate;
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &timing.easing) == mtwn_success, "ease-in");
    double back = At(&timing, 1.25).transformedProgress;
    CHECK(back == 0.75 * 0.75, "time runs back: ease(1 - p)");
    timing.alternateEasing = mtwn_alternateRepeatsShape;
    back = At(&timing, 1.25).transformedProgress;
    CHECK(back == 1.0 - 0.25 * 0.25, "the shape repeats: 1 - ease(p)");
    // A rate of 2 halves the active duration; -1 plays from the end.
    timing = mtwnDefaultTiming();
    timing.duration = 4.0;
    timing.playbackRate = 2.0;
    CHECK(At(&timing, 1.0).activeDuration == 2.0 && At(&timing, 1.0).simpleProgress == 0.5,
          "rate 2");
    timing.playbackRate = -1.0;
    CHECK(At(&timing, 1.0).simpleProgress == 0.75, "rate -1 from the end");
    timing = mtwnDefaultTiming();
    timing.duration = HUGE_VAL;
    timing.repeatDelay = 1.0;
    CHECK(At(&timing, 5.0).simpleProgress == 0.0 && At(&timing, 5.0).activeDuration == HUGE_VAL,
          "an infinite iteration takes no repeat delay");
}

static void TestValidation(void)
{
    mtwnTiming timing = mtwnDefaultTiming();
    CHECK(mtwnValidateTiming(&timing) == mtwn_success, "the default");
    CHECK(mtwnValidateTiming(NULL) == mtwn_errorInvalid, "null");
    struct
    {
        double* field;
        double value;
        const char* name;
    } bad[] = {
        {&timing.delay, HUGE_VAL, "infinite delay"},
        {&timing.endDelay, (double)NAN, "NaN end delay"},
        {&timing.iterationStart, -0.5, "negative iteration start"},
        {&timing.iterations, (double)NAN, "NaN iterations"},
        {&timing.iterations, -1.0, "negative iterations"},
        {&timing.duration, -1.0, "negative duration"},
        {&timing.repeatDelay, -1.0, "negative repeat delay"},
        {&timing.repeatDelay, HUGE_VAL, "infinite repeat delay"},
        {&timing.playbackRate, 0.0, "rate 0"},
        {&timing.playbackRate, HUGE_VAL, "infinite rate"},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
    {
        timing = mtwnDefaultTiming();
        *bad[i].field = bad[i].value;
        CHECK(mtwnValidateTiming(&timing) == mtwn_errorInvalid, bad[i].name);
    }
    timing = mtwnDefaultTiming();
    timing.iterations = HUGE_VAL;
    timing.duration = 1.0;
    CHECK(mtwnValidateTiming(&timing) == mtwn_success, "infinite iterations forwards");
    timing.playbackRate = -1.0;
    CHECK(mtwnValidateTiming(&timing) == mtwn_errorInvalid, "backwards with no end");
    timing.duration = 0.0;
    CHECK(mtwnValidateTiming(&timing) == mtwn_success, "backwards, infinitely many of nothing");
    timing = mtwnDefaultTiming();
    timing.fill = 4;
    CHECK(mtwnValidateTiming(&timing) == mtwn_errorInvalid, "a fill");
    timing = mtwnDefaultTiming();
    timing.direction = 4;
    CHECK(mtwnValidateTiming(&timing) == mtwn_errorInvalid, "a direction");
    timing = mtwnDefaultTiming();
    timing.alternateEasing = 2;
    CHECK(mtwnValidateTiming(&timing) == mtwn_errorInvalid, "an alternate easing");
}

int main(void)
{
    TestOracleCases();
    TestExamples();
    TestValidation();
    return s_failures == 0 ? 0 : 1;
}
