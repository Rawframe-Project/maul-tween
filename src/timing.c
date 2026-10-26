// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Web Animations Level 1's timing steps, in the order its sections give
// them, with Level 2's effect playback
// rate in the active duration, the overall progress and the before
// flag. A repeat delay above 0, with a finite duration above 0, puts
// d + r of active time into each iteration but the last; without, the
// arithmetic is Level 1's exactly.

#include "maul-tween/timing.h"

#include "fpenv.h"
#include "reserved_internal.h"
#include "timing_internal.h"
#include "valid.h"
#include "work.h"

#include <math.h>
#include <stdbool.h>

mtwnTiming mtwnDefaultTiming(void)
{
    mtwnTiming timing = {0};
    timing.iterations = 1.0;
    timing.playbackRate = 1.0;
    timing.easing = mtwnLinearEasing();
    return timing;
}

static double ActiveDuration(const mtwnTiming* timing);

MTWN_NOINLINE mtwnResult mtwnValidateTimingWork(const mtwnTiming* timing)
{
    if (timing == NULL)
    {
        return mtwn_errorInvalid;
    }
    bool valid =
        isfinite(timing->delay) && isfinite(timing->endDelay) && isfinite(timing->iterationStart) &&
        timing->iterationStart >= 0.0 && timing->iterations >= 0.0 && timing->duration >= 0.0 &&
        isfinite(timing->repeatDelay) && timing->repeatDelay >= 0.0 &&
        isfinite(timing->playbackRate) && timing->playbackRate != 0.0 &&
        timing->fill <= mtwn_fillBoth && timing->direction <= mtwn_directionAlternateReverse &&
        timing->alternateEasing <= mtwn_alternateRepeatsShape &&
        mtwnReservedClear(timing->reserved, sizeof timing->reserved) &&
        mtwnValidEasing(&timing->easing);
    // An active duration and an end time a double holds: endless only when
    // the iterations or the duration are, making the active duration
    // infinite (not 0, when the other is 0), and the node plays forwards, since
    // one played backwards starts from its active interval's end, which must
    // exist. Finite fields whose product, repeat delays, rate or delays
    // would overflow them are refused (an active interval's end past a
    // double puts the end time past it too), as is an iteration whose
    // repeat delay would overflow its cycle: the timing steps would meet
    // infinity less infinity.
    if (valid)
    {
        bool endless = isinf(timing->iterations) || isinf(timing->duration);
        double active = ActiveDuration(timing);
        bool ends = isfinite(active) && timing->delay + active + timing->endDelay < HUGE_VAL;
        valid = ends || (endless && isinf(active) && timing->playbackRate > 0.0);
        valid = valid &&
                (!mtwnHasRepeatDelay(timing) || isfinite(timing->duration + timing->repeatDelay));
    }
    return valid ? mtwn_success : mtwn_errorInvalid;
}

mtwnResult mtwnValidateTiming(const mtwnTiming* timing)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnValidateTimingWork(timing);
    mtwnFloatEnvLeave(env);
    return result;
}

bool mtwnHasRepeatDelay(const mtwnTiming* timing)
{
    return timing->repeatDelay > 0.0 && timing->duration > 0.0 && isfinite(timing->duration);
}

// The active duration: the repeated duration over |rate|, the repeated
// duration 0 when the duration or the iterations are (Level 1 says so,
// as infinity times 0 is undefined).
static double ActiveDuration(const mtwnTiming* timing)
{
    double d = timing->duration;
    double n = timing->iterations;
    double repeated = 0.0;
    if (d != 0.0 && n != 0.0)
    {
        repeated = n * d;
        if (mtwnHasRepeatDelay(timing))
        {
            // One delay at each iteration boundary the path crosses.
            double s = timing->iterationStart;
            // ceil(s + n) - floor(s) - 1, from s's fraction so that a large
            // s, its fraction lost in s + n, counts none below 0.
            repeated += (ceil(s - floor(s) + n) - 1.0) * timing->repeatDelay;
        }
    }
    return repeated / fabs(timing->playbackRate);
}

static mtwnPhase Phase(double localTime, double beforeActive, double activeAfter, bool backwards)
{
    if (localTime < beforeActive || (backwards && localTime == beforeActive))
    {
        return mtwn_phaseBefore;
    }
    // An endless active interval is never left, not even at an infinite
    // local time, its limit.
    if (localTime > activeAfter ||
        (!backwards && localTime == activeAfter && isfinite(activeAfter)))
    {
        return mtwn_phaseAfter;
    }
    return mtwn_phaseActive;
}

static double ActiveTime(const mtwnTiming* timing, mtwnPhase phase, double localTime,
                         double activeDuration)
{
    double fromDelay = localTime - timing->delay;
    switch (phase)
    {
    case mtwn_phaseBefore:
        if (timing->fill == mtwn_fillBackwards || timing->fill == mtwn_fillBoth)
        {
            return fromDelay > 0.0 ? fromDelay : 0.0;
        }
        return (double)NAN;
    case mtwn_phaseActive:
        return fromDelay;
    default:
        if (timing->fill == mtwn_fillForwards || timing->fill == mtwn_fillBoth)
        {
            double held = fromDelay < activeDuration ? fromDelay : activeDuration;
            return held > 0.0 ? held : 0.0;
        }
        return (double)NAN;
    }
}

// The time on the iterations' own timeline: the active time scaled by
// the rate, from the far end for a negative rate (Level 2).
static double ScaledTime(const mtwnTiming* timing, double activeTime, double activeDuration)
{
    double rate = timing->playbackRate;
    return rate < 0.0 ? (activeTime - activeDuration) * rate : activeTime * rate;
}

// The overall progress (Level 1, with Level 2's rate). Within a repeat
// delay it sets the flag, and held to the iteration whose end holds.
static double OverallProgress(const mtwnTiming* timing, mtwnTimingResult* result, double* held)
{
    double d = timing->duration;
    double n = timing->iterations;
    double start = timing->iterationStart;
    double active = result->activeDuration;
    if (d == 0.0)
    {
        // A negative rate begins the iterations at their end (Level 2).
        mtwnPhase first = timing->playbackRate > 0.0 ? mtwn_phaseBefore : mtwn_phaseAfter;
        return (result->phase == first ? 0.0 : n) + start;
    }
    // An endless iteration's progress is 0 at every finite active time,
    // and so at an infinite one, the limit.
    if (isinf(d))
    {
        return start;
    }
    // Never before the first iteration: an active time an ulp past the
    // active duration (local - delay rounding up) would put it there.
    double scaled = ScaledTime(timing, result->activeTime, active);
    scaled = scaled < 0.0 ? 0.0 : scaled;
    if (!mtwnHasRepeatDelay(timing))
    {
        return scaled / d + start;
    }
    double cycle = d + timing->repeatDelay;
    double position = scaled + floor(start) * cycle + (start - floor(start)) * d;
    // The quotient rounds either way. Below the first iteration it is
    // that one (339 times 0.2 over 0.2 floors to 338), where the
    // position holds the first's start as the same product, so nothing
    // lies before it; rounded up to k it leaves a position an ulp before
    // k's start, in the delay before it.
    double k = floor(position / cycle);
    k = k < floor(start) ? floor(start) : k;
    double within = position - k * cycle;
    if (within < 0.0)
    {
        k -= 1.0;
        within += cycle;
    }
    // Past an iteration's end lies its repeat delay, but none follows the
    // last: there the end hold applies. A progress that rounds to the end
    // has reached it: an iteration's start shown for one time before the
    // delay steps the position back.
    double overall = within < d ? k + within / d : k + 1.0;
    bool ended = within >= d || overall - k >= 1.0;
    bool last = k + 1.0 >= start + n;
    if (ended && !last)
    {
        result->inRepeatDelay = true;
        *held = k;
    }
    return overall;
}

// Whether a current iteration runs forwards.
bool mtwnIterationForwards(mtwnDirection direction, double iteration)
{
    switch (direction)
    {
    case mtwn_directionNormal:
        return true;
    case mtwn_directionReverse:
        return false;
    default:
    {
        if (isinf(iteration))
        {
            return true;
        }
        // Even: fmod(i, 2) == 0 for the whole numbers an iteration is,
        // without the division (halving is exact). The parity of the
        // iteration itself, not of it plus one, which from 2^53 on rounds
        // back to it.
        bool even = floor(iteration * 0.5) * 2.0 == iteration;
        return direction == mtwn_directionAlternateReverse ? !even : even;
    }
    }
}

// The simple iteration progress, the overall progress first kept to the
// iterations against rounding (the active interval's ends are the
// iteration start and its end exactly), holding an iteration's end where
// the active interval ends on one.
static double SimpleProgress(const mtwnTiming* timing, mtwnTimingResult* result)
{
    double start = timing->iterationStart;
    double ends = start + timing->iterations;
    double overall = result->overallProgress;
    overall = overall > ends ? ends : (overall < start ? start : overall);
    result->overallProgress = overall;
    // fmod(x, 1) for the x >= +0 these are: a double's fraction is a
    // double, so x - floor(x) is exact.
    double simple = isinf(overall) ? start - floor(start) : overall - floor(overall);
    // The iterations' end: the active end, or for a negative rate (which
    // begins there, Level 2) the active start; or, in the phases the end
    // lies in, an overall progress rounded to the end from a time beside
    // it, where an iteration past the last showed its start for that one
    // time (Blink holds the end within a microsecond of it). Not where the iterations add nothing
    // to a start too large to keep them.
    bool forwardRate = timing->playbackRate > 0.0;
    bool endPhase =
        forwardRate ? result->phase != mtwn_phaseBefore : result->phase != mtwn_phaseAfter;
    bool atEnd =
        endPhase && isfinite(result->activeDuration) &&
        (forwardRate ? result->activeTime == result->activeDuration : result->activeTime == 0.0);
    atEnd = atEnd || (endPhase && overall == ends && ends > start);
    if (result->inRepeatDelay || (simple == 0.0 && atEnd && timing->iterations != 0.0))
    {
        simple = 1.0;
    }
    return simple;
}

MTWN_NOINLINE void mtwnComputeTimingWork(const mtwnTiming* timing, double localTime, bool backwards,
                                         mtwnTimingResult* resultOut)
{
    // Built in place: a copy out of a local read back the fields it had
    // just stored, in wider loads than the stores.
    *resultOut = (mtwnTimingResult){0};
    mtwnTimingResult* result = resultOut;
    double active = ActiveDuration(timing);
    result->activeDuration = active;
    double end = timing->delay + active + timing->endDelay;
    result->endTime = end > 0.0 ? end : 0.0;
    double beforeActive = timing->delay < result->endTime ? timing->delay : result->endTime;
    beforeActive = beforeActive > 0.0 ? beforeActive : 0.0;
    double activeEnd = timing->delay + active;
    double activeAfter = activeEnd < result->endTime ? activeEnd : result->endTime;
    activeAfter = activeAfter > 0.0 ? activeAfter : 0.0;
    result->phase = Phase(localTime, beforeActive, activeAfter, backwards);
    result->activeTime = ActiveTime(timing, result->phase, localTime, active);
    if (isnan(result->activeTime))
    {
        result->overallProgress = (double)NAN;
        result->simpleProgress = (double)NAN;
        result->iteration = (double)NAN;
        result->directedProgress = (double)NAN;
        result->transformedProgress = (double)NAN;
        return;
    }
    double n = timing->iterations;
    double held = 0.0;
    result->overallProgress = OverallProgress(timing, result, &held);
    double simple = SimpleProgress(timing, result);
    double overall = result->overallProgress;
    result->simpleProgress = simple;
    // The current iteration.
    if (result->inRepeatDelay)
    {
        result->iteration = held;
    }
    else if (result->phase == mtwn_phaseAfter && isinf(n))
    {
        result->iteration = HUGE_VAL;
    }
    else
    {
        result->iteration = simple == 1.0 ? floor(overall) - 1.0 : floor(overall);
    }
    // The directed and transformed progress.
    bool forwards = mtwnIterationForwards(timing->direction, result->iteration);
    result->forwards = forwards;
    result->directedProgress = forwards ? simple : 1.0 - simple;
    bool goingForwards = forwards == (timing->playbackRate > 0.0);
    result->before = (result->phase == mtwn_phaseBefore && goingForwards) ||
                     (result->phase == mtwn_phaseAfter && !goingForwards);
    if (!forwards && timing->alternateEasing == mtwn_alternateRepeatsShape)
    {
        result->transformedProgress = 1.0 - mtwnEaseWork(&timing->easing, simple, result->before);
    }
    else
    {
        result->transformedProgress =
            mtwnEaseWork(&timing->easing, result->directedProgress, result->before);
    }
}

static MTWN_NOINLINE mtwnResult mtwnComputeTimingChecked(const mtwnTiming* timing, double localTime,
                                                         bool backwards,
                                                         mtwnTimingResult* resultOut)
{
    if (resultOut == NULL || mtwnValidateTimingWork(timing) != mtwn_success || isnan(localTime) ||
        localTime == -HUGE_VAL)
    {
        return mtwn_errorInvalid;
    }
    mtwnComputeTimingWork(timing, localTime, backwards, resultOut);
    return mtwn_success;
}

mtwnResult mtwnComputeTiming(const mtwnTiming* timing, double localTime, bool backwards,
                             mtwnTimingResult* resultOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnComputeTimingChecked(timing, localTime, backwards, resultOut);
    mtwnFloatEnvLeave(env);
    return result;
}
