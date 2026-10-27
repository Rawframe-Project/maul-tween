// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What the timing shares with the crossings.

#ifndef MAUL_TWEEN_SRC_TIMING_INTERNAL_H
#define MAUL_TWEEN_SRC_TIMING_INTERNAL_H

#include "maul-tween/timing.h"

#include <math.h>
#include <stdbool.h>

// Whether an iteration runs forwards under a direction (Web Animations
// Level 1's directed progress).
bool mtwnIterationForwards(mtwnDirection direction, double iteration);

// Whether a timing has a repeat delay that takes time.
bool mtwnHasRepeatDelay(const mtwnTiming* timing);

// The iteration a scaled time is in, with a repeat delay: its cycles
// counted from the start's, at least the start's own; and its time into
// that cycle. The timing and the crossings take it alike, so a crossing
// is where the timing puts it, bit for bit. The quotient rounds either
// way: below the first iteration it is that one (339 times 0.2 over 0.2
// floors to 338), where the position holds the first's start as the
// same product, so nothing lies before it; rounded up to k it leaves a
// time an ulp before k's start, below 0, in the delay before it. Infinite
// for a position past the doubles' cycles, at a time or a rate near the
// largest double, and for one past them whose cycles' count rounds by
// more than one.
static inline double mtwnCycleOf(double scaled, double start, double duration, double cycle,
                                 double* withinOut)
{
    double first = floor(start);
    double position = scaled + first * cycle + (start - first) * duration;
    if (isinf(position) && isfinite(scaled))
    {
        // A position past the doubles whose cycles are not (2.5 iterations
        // of 1e308 s): the same steps with every term scaled by 2^-m, which
        // commutes with rounding in the normal range, so the iteration and
        // the time into it are what an exponent without bounds gives. The
        // terms that fall below the normal doubles are below the
        // position's rounding too.
        int largest = 0;
        (void)frexp(scaled > duration ? scaled : duration, &largest);
        if (first > 0.0)
        {
            int product = 0;
            int factor = 0;
            (void)frexp(first, &product);
            (void)frexp(cycle, &factor);
            largest = product + factor > largest ? product + factor : largest;
        }
        int m = largest - 960;
        double c = ldexp(cycle, -m);
        double p = ldexp(scaled, -m) + first * c + (start - first) * ldexp(duration, -m);
        double k = floor(p / c);
        k = k < first ? first : k;
        double within = p - k * c;
        // Taken where the quotient is a double and the time into its cycle
        // within a cycle, as the step back from a quotient rounded up
        // assumes; a start past 2^53 cycles can round it further.
        if (!isinf(k) && fabs(within) <= c)
        {
            *withinOut = ldexp(within, m);
            return k;
        }
    }
    double k = floor(position / cycle);
    k = k < first ? first : k;
    *withinOut = position - k * cycle;
    return k;
}

#endif
