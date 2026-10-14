// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Timing (records mtwn-0012 and mtwn-0027): Web Animations Level 1's
// calculation of a node's phase, iteration and progress from its local
// time, with Level 2's effect playback rate, a repeat delay between
// iterations and a choice of how reverse iterations ease.

#ifndef MAUL_TWEEN_TIMING_H
#define MAUL_TWEEN_TIMING_H

#include "maul-tween/base.h"
#include "maul-tween/easing.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Where a node's local time falls.
    typedef uint8_t mtwnPhase;
    enum
    {
        mtwn_phaseBefore = 0,
        mtwn_phaseActive = 1,
        mtwn_phaseAfter = 2,
    };

    // Whether a node holds its value outside its active interval.
    typedef uint8_t mtwnFill;
    enum
    {
        mtwn_fillNone = 0,
        mtwn_fillForwards = 1,
        mtwn_fillBackwards = 2,
        mtwn_fillBoth = 3,
    };

    // The direction of each iteration.
    typedef uint8_t mtwnDirection;
    enum
    {
        mtwn_directionNormal = 0,
        mtwn_directionReverse = 1,
        mtwn_directionAlternate = 2,
        mtwn_directionAlternateReverse = 3,
    };

    // How a reverse iteration eases.
    typedef uint8_t mtwnAlternateEasing;
    enum
    {
        // Time runs back: ease(1 - p), so an ease-in looks like an ease-out
        // on the way back. Web Animations'.
        mtwn_alternateRunsBack = 0,
        // The shape repeats: 1 - ease(p), the same curve on the way back.
        mtwn_alternateRepeatsShape = 1,
    };

    // A node's timing: Web Animations' effect timing and Maul Tween's
    // additions. Start from mtwnDefaultTiming and set what differs; check
    // it with mtwnValidateTiming. Times are seconds.
    typedef struct mtwnTiming
    {
        // From the node's start to its active interval; may be negative.
        double delay;
        // From the active interval's end to the node's end; may be
        // negative.
        double endDelay;
        // Where in its iterations the node starts, from 0.
        double iterationStart;
        // How many iterations, from 0, infinity allowed.
        double iterations;
        // One iteration's length, from 0, infinity allowed.
        double duration;
        // Between iterations, not after the last, from 0.
        double repeatDelay;
        // The node's own speed, finite and not 0; negative plays it
        // backwards.
        double playbackRate;
        // The easing of each iteration's progress.
        mtwnEasing easing;
        mtwnFill fill;
        mtwnDirection direction;
        mtwnAlternateEasing alternateEasing;
        uint8_t reserved[5];
    } mtwnTiming;

    // A node's timing at one local time. Values a phase leaves undefined
    // (Web Animations' unresolved) are NaN.
    typedef struct mtwnTimingResult
    {
        // The active interval's length, and the node's end time.
        double activeDuration;
        double endTime;
        // The time into the active interval, NaN outside it without fill.
        double activeTime;
        // Iterations completed, partial ones included, iteration start
        // added.
        double overallProgress;
        // Progress within the current iteration, from 0 to 1.
        double simpleProgress;
        // The current iteration's index; infinity after an infinite run.
        double iteration;
        // The progress in the iteration's direction, and eased.
        double directedProgress;
        double transformedProgress;
        mtwnPhase phase;
        // The before flag the easing was given.
        bool before;
        // Whether the time lies in a repeat delay, the progress held at
        // its iteration's end.
        bool inRepeatDelay;
        // Whether the current iteration runs forwards, by the direction.
        bool forwards;
        uint8_t reserved[4];
    } mtwnTimingResult;

    /// Returns Web Animations' default timing: no delays, fill none, one
    /// iteration from 0, a duration of 0, normal direction, linear easing,
    /// no repeat delay, time running back in reverse iterations, rate 1.
    ///
    /// @return The timing.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API mtwnTiming mtwnDefaultTiming(void);

    /// Checks a timing: delays finite, iteration start finite and from 0,
    /// iterations and duration from 0 (infinity allowed, NaN not), repeat
    /// delay finite and from 0, playback rate finite and not 0 (negative
    /// only with a finite active duration, whose end a backward play
    /// starts from), every enumeration known.
    ///
    /// @param timing  The timing.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or a
    ///         timing outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnValidateTiming(const mtwnTiming* timing);

    /// Computes a node's timing at a local time, by Web Animations' steps.
    ///
    /// @param timing     A timing mtwnValidateTiming accepts.
    /// @param localTime  The node's local time, seconds, finite.
    /// @param backwards  Whether the time is running backwards (the
    ///                   playing animation's rate is negative), which moves
    ///                   the active interval's boundaries.
    /// @param resultOut  Receives the result.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API void mtwnComputeTiming(const mtwnTiming* timing, double localTime, bool backwards,
                                    mtwnTimingResult* resultOut);

#ifdef __cplusplus
}
#endif

#endif
