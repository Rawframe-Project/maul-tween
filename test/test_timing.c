// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Timing: every case the
// independent oracle in tools/oracle/timing_oracle.py computed, matched
// to the bit, and the specification's own examples written out.

#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/timing.h"

#include <float.h>
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
        CHECK(mtwnMakeStepsEasing(4, mtwn_jumpStart, &timing.easing) == mtwn_success, "steps");
    }
    else if (in->easing == 2)
    {
        CHECK(mtwnMakeStepsEasing(3, mtwn_jumpEnd, &timing.easing) == mtwn_success, "steps");
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
        mtwnTimingResult r = ComputeTiming(&timing, c->localTime, c->input.backwards != 0);
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
    CHECK(count >= 400, "the oracle's cases are all there");
}

static mtwnTimingResult At(const mtwnTiming* timing, double time)
{
    mtwnTimingResult r = ComputeTiming(timing, time, false);
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
    // A negative rate begins at the end: a double after its delay of
    // 0.171875 s, the active time 2.8e-17 s is lost from 2.3 s less it, and
    // the overall progress is the end, which holds, as it does at the delay
    // itself, not a sixth iteration's start.
    timing.duration = 0.2;
    timing.repeatDelay = 0.3;
    timing.playbackRate = -1.0;
    timing.delay = 0.171875;
    r = At(&timing, nextafter(0.171875, 1.0));
    CHECK(r.iteration == 4.0 && r.simpleProgress == 1.0,
          "a negative rate's start an ulp on holds the end");
    // Alternating from the second iteration: an even one runs back, at
    // 2^53 too, where one more rounds back to it (and the progress within
    // is lost: 0, directed to 1).
    timing = mtwnDefaultTiming();
    timing.duration = 1.0;
    timing.fill = mtwn_fillBoth;
    timing.direction = mtwn_directionAlternateReverse;
    timing.iterationStart = 0x1p53;
    r = At(&timing, 0.25);
    CHECK(r.iteration == 0x1p53 && !r.forwards && r.directedProgress == 1.0,
          "iteration 2^53 runs back");
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

// Finite fields whose active duration or cycle a double cannot hold: the
// timing steps would meet infinity less infinity, a NaN iteration. Refused
// whichever way the node plays; an endless timing still plays forwards.
static void TestOverflowingTimings(void)
{
    struct
    {
        double iterations;
        double duration;
        double repeatDelay;
        double rate;
        const char* name;
    } refused[] = {
        {1e308, 1.0, 0.0, -0.5, "backwards, iterations over the rate overflowing"},
        {1.0, 1e300, 0.0, -1e-300, "backwards, a duration over a tiny rate overflowing"},
        {1.0, 1e300, 0.0, 1e-300, "forwards, a duration over a tiny rate overflowing"},
        {1e300, 1e300, 0.0, 1.0, "forwards, iterations times duration overflowing"},
        {1e300, 1.0, 1e300, 1.0, "the repeat delays overflowing"},
        {0.0, 1e308, 1e308, 1.0, "an iteration and its repeat delay overflowing"},
        {0.0, 1e308, 1e308, -1.0, "the same backwards"},
    };
    struct
    {
        double delay;
        double duration;
        double endDelay;
        const char* name;
    } endless[] = {
        {1e308, 1e308, 0.0, "a delay and an active duration overflowing the end"},
        {0.0, 1e308, 1e308, "an active duration and an end delay overflowing the end"},
    };
    // Infinitely many of nothing, or none of forever, last 0 however they
    // play: endless forwards only when the active duration is.
    double infinite[][2] = {{HUGE_VAL, 0.0}, {0.0, HUGE_VAL}};
    for (size_t i = 0; i < 2; ++i)
    {
        mtwnTiming timing = mtwnDefaultTiming();
        timing.iterations = infinite[i][0];
        timing.duration = infinite[i][1];
        timing.delay = 1e308;
        timing.endDelay = 1e308;
        CHECK(mtwnValidateTiming(&timing) == mtwn_errorInvalid,
              i == 0 ? "forwards, infinitely many of nothing, delays overflowing the end"
                     : "forwards, none of forever, delays overflowing the end");
        timing.endDelay = 0.0;
        CHECK(mtwnValidateTiming(&timing) == mtwn_success,
              i == 0 ? "forwards, infinitely many of nothing after a 1e308 s delay"
                     : "forwards, none of forever after a 1e308 s delay");
    }
    for (size_t i = 0; i < sizeof endless / sizeof endless[0]; ++i)
    {
        mtwnTiming timing = mtwnDefaultTiming();
        timing.delay = endless[i].delay;
        timing.duration = endless[i].duration;
        timing.endDelay = endless[i].endDelay;
        CHECK(mtwnValidateTiming(&timing) == mtwn_errorInvalid, endless[i].name);
    }
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; ++i)
    {
        mtwnTiming timing = mtwnDefaultTiming();
        timing.iterations = refused[i].iterations;
        timing.duration = refused[i].duration;
        timing.repeatDelay = refused[i].repeatDelay;
        timing.playbackRate = refused[i].rate;
        CHECK(mtwnValidateTiming(&timing) == mtwn_errorInvalid, refused[i].name);
    }
    mtwnTiming timing = mtwnDefaultTiming();
    timing.iterations = 1e300;
    timing.duration = 1.0;
    timing.repeatDelay = 1.0;
    timing.playbackRate = -0.5;
    CHECK(mtwnValidateTiming(&timing) == mtwn_success, "4e300 s, within a double, backwards");
    timing.duration = HUGE_VAL;
    timing.playbackRate = 1e-300;
    CHECK(mtwnValidateTiming(&timing) == mtwn_success, "an endless iteration, forwards");
    timing = mtwnDefaultTiming();
    timing.delay = 1e308;
    timing.duration = 1e307;
    CHECK(mtwnValidateTiming(&timing) == mtwn_success, "a 1e308 s delay before 1e307 s");
    timing.delay = -1e308;
    timing.endDelay = -1e308;
    CHECK(mtwnValidateTiming(&timing) == mtwn_success,
          "delays below a double's range: the end time held at 0");
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
    TestOverflowingTimings();
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

// At an infinite local time, the limit: a finite active interval lies
// behind, an endless one is still active, an infinite duration's progress
// at its iteration start.
static void TestInfiniteLocalTime(void)
{
    mtwnTimingResult r;
    mtwnTiming endless = mtwnDefaultTiming();
    endless.duration = HUGE_VAL;
    endless.iterations = 2.0;
    endless.iterationStart = 0.25;
    r = ComputeTiming(&endless, HUGE_VAL, false);
    CHECK(r.phase == mtwn_phaseActive && r.iteration == 0.0 && r.simpleProgress == 0.25 &&
              r.transformedProgress == 0.25,
          "an endless iteration at its start's progress");
    mtwnTiming repeating = mtwnDefaultTiming();
    repeating.duration = 1.0;
    repeating.iterations = HUGE_VAL;
    r = ComputeTiming(&repeating, HUGE_VAL, false);
    CHECK(r.phase == mtwn_phaseActive && isinf(r.iteration) && !isnan(r.transformedProgress),
          "endless iterations at the last");
    mtwnTiming finite = mtwnDefaultTiming();
    finite.duration = 1.0;
    finite.fill = mtwn_fillForwards;
    r = ComputeTiming(&finite, HUGE_VAL, false);
    CHECK(r.phase == mtwn_phaseAfter && r.iteration == 0.0 && r.transformedProgress == 1.0,
          "a finite one past its end");
}

// Iterations lost to rounding against their start, their active duration
// or their cycle: each result is the end of the iterations, at the
// iteration the start is in or later, and a repeat delay's quotient past
// the doubles gives what no repeat delay gives.
static void TestIterationsLostToRounding(void)
{
    mtwnTimingResult r;
    // 2^-1074 iterations of 0.5 s: an active duration of 2^-1075, which
    // rounds to 0. At its end the overall progress is the iterations' end,
    // 2^-1074, not the start.
    mtwnTiming tiny = mtwnDefaultTiming();
    tiny.iterations = 0x1p-1074;
    tiny.duration = 0.5;
    tiny.fill = mtwn_fillBoth;
    r = ComputeTiming(&tiny, 0.0, false);
    CHECK(r.activeDuration == 0.0 && r.phase == mtwn_phaseAfter && r.overallProgress == 0x1p-1074 &&
              r.simpleProgress == 0x1p-1074 && r.iteration == 0.0,
          "iterations whose active duration rounds to 0 end at their own end, in iteration 0");
    tiny.playbackRate = -1.0;
    r = ComputeTiming(&tiny, 0.0, true);
    CHECK(r.phase == mtwn_phaseBefore && r.overallProgress == 0x1p-1074 && r.iteration == 0.0,
          "the same backwards, at the start a negative rate begins from");
    // One iteration from 2^53: the end, 2^53 + 1, rounds to the start, and
    // the iteration held at its end is the start's, 2^53.
    mtwnTiming far = mtwnDefaultTiming();
    far.iterationStart = 0x1p53;
    far.duration = 1.0;
    far.fill = mtwn_fillForwards;
    r = ComputeTiming(&far, 1.0, false);
    CHECK(r.phase == mtwn_phaseAfter && r.overallProgress == 0x1p53 && r.simpleProgress == 1.0 &&
              r.iteration == 0x1p53,
          "an end rounded to a start of 2^53 held at 1 in iteration 2^53, not 2^53 - 1");
    // Endless iterations of 2^-40 s at the largest time: the cycles past it
    // overflow the quotient. The progress is past the doubles, as without the
    // repeat delay, not NaN.
    mtwnTiming endless = mtwnDefaultTiming();
    endless.iterations = HUGE_VAL;
    endless.duration = 0x1p-40;
    endless.iterationStart = 8.0;
    endless.repeatDelay = 0x1p-53;
    mtwnTiming plain = endless;
    plain.repeatDelay = 0.0;
    r = ComputeTiming(&endless, DBL_MAX, false);
    mtwnTimingResult without = ComputeTiming(&plain, DBL_MAX, false);
    CHECK(r.phase == mtwn_phaseActive && r.overallProgress == HUGE_VAL && isinf(r.iteration) &&
              r.simpleProgress == without.simpleProgress && r.iteration == without.iteration &&
              r.transformedProgress == without.transformedProgress && !r.inRepeatDelay,
          "a repeat delay whose cycles overflow the quotient gives what none gives");
    // 2.5 iterations of about 1e308 s before the first: its start's
    // cycles pass the doubles, but the time into them does not. Its
    // iteration is 2, as without the repeat delay, not infinity.
    mtwnTiming late = mtwnDefaultTiming();
    late.iterations = HUGE_VAL;
    late.duration = 1e308;
    late.iterationStart = 2.5;
    late.playbackRate = 0.5;
    late.repeatDelay = 0x1.5p-4;
    plain = late;
    plain.repeatDelay = 0.0;
    r = ComputeTiming(&late, 1.4, false);
    without = ComputeTiming(&plain, 1.4, false);
    CHECK(r.phase == mtwn_phaseActive && r.iteration == 2.0 && r.overallProgress == 2.5 &&
              r.overallProgress == without.overallProgress && r.iteration == without.iteration &&
              r.simpleProgress == without.simpleProgress && !r.inRepeatDelay,
          "a start whose cycles pass the doubles counted from the first's: iteration 2");
    // One such iteration at rate 1, whose active duration a double holds.
    late.iterations = 1.0;
    late.playbackRate = 1.0;
    r = ComputeTiming(&late, 1.4, false);
    CHECK(r.phase == mtwn_phaseActive && r.iteration == 2.0 && r.simpleProgress == 0.5,
          "one such iteration, the same");
}

// The computation checks what it is given: a null pointer, a timing
// mtwnValidateTiming refuses and a local time that is NaN or negative
// infinity are refused, the result unchanged; positive infinity, the
// limit, is taken.
static bool ComputeRefused(const mtwnTiming* timing, double localTime)
{
    mtwnTimingResult result = {0};
    result.activeTime = 7.0;
    return mtwnComputeTiming(timing, localTime, false, &result) == mtwn_errorInvalid &&
           result.activeTime == 7.0;
}

static void TestComputeRefusals(void)
{
    mtwnTiming timing = mtwnDefaultTiming();
    timing.duration = 2.0;
    mtwnTimingResult result = {0};
    int refused = 0;
    refused += ComputeRefused(NULL, 0.5);
    refused += mtwnComputeTiming(&timing, 0.5, false, NULL) == mtwn_errorInvalid;
    refused += ComputeRefused(&timing, (double)NAN);
    refused += ComputeRefused(&timing, -(double)INFINITY);
    mtwnTiming stopped = timing;
    stopped.playbackRate = 0.0;
    refused += ComputeRefused(&stopped, 0.5);
    mtwnTiming eased = timing;
    eased.easing.kind = (mtwnEasingKind)200;
    refused += ComputeRefused(&eased, 0.5);
    CHECK(refused == 6, "a null timing or result, a NaN or negatively infinite time, and a "
                        "timing mtwnValidateTiming refuses (a rate of 0, an easing no "
                        "maker gives) refused, the result unchanged");
    CHECK(mtwnComputeTiming(&timing, HUGE_VAL, false, &result) == mtwn_success &&
              result.phase == mtwn_phaseAfter,
          "positive infinity taken, after the active interval");
    CHECK(mtwnComputeTiming(&timing, -1e300, true, &result) == mtwn_success &&
              result.phase == mtwn_phaseBefore,
          "a finite time far before taken, backwards too");
}

int main(void)
{
    TestOracleCases();
    TestExamples();
    TestValidation();
    TestInfiniteLocalTime();
    TestIterationsLostToRounding();
    TestComputeRefusals();
    return s_failures == 0 ? 0 : 1;
}
