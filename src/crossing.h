// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Crossings: how many times a sweep of a player's time
// passes a point deep in a clip, exactly, through every ancestor's
// iterations, without visiting them one by one.
//
// Each timeline's child time runs along a path: its iterations in the
// order its rate takes them, each from its start to its end. The points
// of a path are positions on it; a point below is crossed once each time
// the path covers it, an iteration's two ends both covered. The count of
// points a level has covered by a local time is worked out from the level
// below at one moving point plus constants each level keeps, so a count
// costs a walk down the chain.

#ifndef MAUL_TWEEN_SRC_CROSSING_H
#define MAUL_TWEEN_SRC_CROSSING_H

#include "maul-tween/clip.h"
#include "maul-tween/timing.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Crossings of a leaf: moving the same way as the coordinate counted in,
// and the other way.
typedef struct mtwnCrossings
{
    double same;
    double opposite;
} mtwnCrossings;

// What is counted, in the deepest level's child time: an instant there,
// or the iteration starts of a node beginning there (its iterations
// after the first, Level 1's current iteration changing).
typedef uint8_t mtwnLeafKind;
enum
{
    mtwn_leafInstant = 0,
    mtwn_leafIterations = 1,
};

typedef struct mtwnLeaf
{
    // The instant, or the node's start.
    double at;
    // The node's timing, its duration resolved, for iterations.
    mtwnTiming timing;
    // Its active duration when known (a sealed clip's node keeps it),
    // else worked out from the timing at each use.
    double activeDuration;
    mtwnLeafKind kind;
    bool activeKnown;
} mtwnLeaf;

// A timeline on the way down: its timing, its duration resolved, its
// start in its parent's child time, and what the preparation keeps.
typedef struct mtwnLevel
{
    mtwnTiming timing;
    double start;
    // Filled by mtwnPrepareLevels, unless known as filled (a finished
    // clip's node keeps it).
    double activeDuration;
    bool activeKnown;
    // Its iterations' shape, with the four below, kept for the counts:
    // the first iteration, and the last as an offset from it.
    bool forwardRate;
    bool point;
    bool unbounded;
    double firstIteration;
    double firstProgress;
    double lastOffset;
    double lastProgress;
    // The coverage of the traversal's first iteration, of full forward
    // and reversed ones, and of the last, relative to the local time.
    mtwnCrossings cover[4];
    // Below at the iteration starts' child times (the first's, a forward
    // one's, a reversed one's) and ends' (a forward one's, a reversed
    // one's, the last's), closed and strict.
    mtwnCrossings startBelow[3][2];
    mtwnCrossings endBelow[3][2];
    // Below at the single point a zero-length level has.
    mtwnCrossings pointBelow;
} mtwnLevel;

// Whether a timing has iteration starts after its first: its last
// iteration, as the counts take it, past the first. Endless, or ending
// past the doubles, it has; otherwise its last, an offset of
// ceil(f + n) - 1 from the first for the start's fraction f, is past it
// when f + n passes 1.
MTWN_NODISCARD static inline bool mtwnStartsAfterFirst(const mtwnTiming* timing)
{
    double n = timing->iterations;
    double s = timing->iterationStart;
    return isinf(n) || isinf(s + n) || (s - floor(s)) + n > 1.0;
}

// Whether the count has positions in a timing's iterations at all
// (mtwnCountPosition): none for no duration or no iterations, a point,
// and none for an endless duration, whose iteration never ends.
MTWN_NODISCARD static inline bool mtwnHasPositions(const mtwnTiming* timing)
{
    return timing->duration > 0.0 && isfinite(timing->duration) && timing->iterations != 0.0;
}

// Whether the timing model's iteration and child time in a timing's
// active interval, before its active end, are the count's own
// (mtwnCountPosition) bit for bit: going forwards from iteration start 0
// through a finite duration.
// - The count's time is the model's active time, local less the delay,
//   and both scale it by the rate.
// - Without a repeat delay, the count's sum of the quotient and the
//   start's fraction, 0, is the quotient with no error, so its floor and
//   progress are the model's floor(x) and x - floor(x). With one, both
//   take mtwnCycleOf of the same scaled time, start 0, duration and cycle,
//   step back a time below 0 in it alike, and form the progress, the
//   delay and the held iteration from it alike.
// - A quotient rounded to or past the iterations' end is clamped by both
//   to the last iteration's end: the model holds the overall progress at
//   the end and its progress at 1 when the end is whole, the count its
//   offset at the last and its progress at the last's.
// - The child time samples the same easing at the same parity, times the
//   duration.
// The active end itself is the count's alone, which reaches all of the
// node there: the model's active phase ends where the local time reaches
// the delay plus the active duration, rounded, so the active time, the
// local time less the delay, can reach the active duration while it is
// active.
MTWN_NODISCARD static inline bool mtwnModelPositions(const mtwnTiming* timing)
{
    return timing->playbackRate > 0.0 && timing->iterationStart == 0.0 && timing->duration > 0.0 &&
           isfinite(timing->duration);
}

// The iteration, an offset from the first, and the child time a local
// time reaches of a timing with finite iterations of positive, finite
// duration and an active duration, as the counts take them, bit for bit:
// false where it reaches none or all of its path.
MTWN_NODISCARD bool mtwnCountPosition(const mtwnTiming* timing, double active, double local,
                                      double* iterationOut, double* childOut);

// Prepares the levels, outermost first, for counting the leaf: each keeps
// counts of the levels below it.
void mtwnPrepareLevels(mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf);

// The outer levels prepared levels may be counted without: 1 for an
// outermost level passing its time through unchanged (endless, from 0,
// without delay, at rate 1, forwards) to the level or leaf below with
// nothing before its start, else 0. Counted from the level below, the counts are
// the same, as counts from it are none at or before 0.
uint32_t mtwnPassedLevels(const mtwnLevel* levels, uint32_t count);

// Counts the leaf's crossings as the outermost level's parent time sweeps
// from 'from' to 'to': (from, to] forwards, [to, from) backwards, closed
// at 'from' too when asked (a sweep from no time). The result's same is
// the crossings with the leaf moving forwards. The count is taken between
// the two ends, not as what each reaches from the clip's start, so it does
// not depend on how much lies before the sweep.
mtwnCrossings mtwnCountCrossings(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf,
                                 double from, double to, bool fromClosed);

// Which crossings a search counts.
typedef uint8_t mtwnCrossingWay;
enum
{
    mtwn_crossingEither = 0,
    mtwn_crossingForwards = 1,
    mtwn_crossingBackwards = 2,
};

// The time of the first (or last) crossing, of a way, in a sweep that has
// one: a double t where the count flips, the sweep to t (from t, for the
// last) counting one and to its neighbor toward 'from' (toward 'to')
// not, found by halving the doubles between, in their order as integers:
// at most 64 halvings, each a count. Where the count rises monotonically
// that is the double nearest the near end; its rounding can flip it more
// than once within a few doubles, and the halving's path then picks one.
// The search starts from a guess: a guess past the near end, up to
// the far one, is tested, and its neighbor toward the near end when it
// counts, and the halving goes on between whatever they leave. The time
// found flips the count as above; a right guess finds it in two counts.
// Where the count flips more than once the guess can pick another of
// those few doubles, the same one on every platform. NaN guesses nothing.
double mtwnFindCrossingFrom(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf,
                            double from, double to, bool fromClosed, bool last, mtwnCrossingWay way,
                            double guess);

// The search from no guess.
static inline double mtwnFindCrossing(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf,
                                      double from, double to, bool fromClosed, bool last,
                                      mtwnCrossingWay way)
{
    return mtwnFindCrossingFrom(levels, count, leaf, from, to, fromClosed, last, way, (double)NAN);
}

// The iteration the last level is in at a time of the outermost level's
// parent, each level filling both ways.
double mtwnIterationAt(const mtwnLevel* levels, uint32_t count, double t);

#endif
