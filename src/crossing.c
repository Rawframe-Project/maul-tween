// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Crossings (record mtwn-0037). A level's path: its iterations from the
// first its rate reaches to the last, each from its start to its end;
// iteration j at progress q has the child time ChildAt(j, q). A local time
// reaches part of the path: none, all, or every iteration before k and
// iteration k up to q, q itself in or out. Where two positions share a
// local time (an iteration's end and the next one's start with no repeat
// delay between) both are reached together. The count below a local time
// is constants for the whole iterations reached plus the iteration
// reached partly, whose moving end is counted at the level below: one
// walk down the chain.

#include "crossing.h"

#include "timing_internal.h"

#include "maul-tween/easing.h"

#include <math.h>

// What a local time reaches of a level's path.
typedef enum ReachKind
{
    s_none,
    s_all,
    s_part,
} ReachKind;

typedef struct Reach
{
    ReachKind kind;
    bool inclusive;
    double iteration;
    double progress;
} Reach;

// A path's shape: its iterations, the traversal's direction, and whether
// it is a single point or runs forever on one iteration.
typedef struct Shape
{
    double active;
    double first;
    double firstProgress;
    double last;
    double lastProgress;
    bool forwardRate;
    bool point;
    bool unbounded;
} Shape;

static Shape ShapeOf(const mtwnTiming* timing, double active)
{
    Shape shape = {0};
    double d = timing->duration;
    double n = timing->iterations;
    double s = timing->iterationStart;
    shape.active = active;
    shape.forwardRate = timing->playbackRate > 0.0;
    shape.point = d == 0.0 || n == 0.0;
    shape.unbounded = isinf(d) && !shape.point;
    shape.first = floor(s);
    shape.firstProgress = s - shape.first;
    if (isinf(n))
    {
        shape.last = HUGE_VAL;
        shape.lastProgress = 1.0;
    }
    else
    {
        shape.last = ceil(s + n) - 1.0;
        shape.lastProgress = s + n - shape.last;
    }
    return shape;
}

static double ActiveOf(const mtwnTiming* timing)
{
    mtwnTimingResult result;
    mtwnComputeTiming(timing, 0.0, false, &result);
    return result.activeDuration;
}

// A child time on an iteration running forwards or not, at a progress.
static double ChildAt(const mtwnTiming* timing, bool forwards, double q)
{
    double eased = 0.0;
    if (!forwards && timing->alternateEasing == mtwn_alternateRepeatsShape)
    {
        eased = 1.0 - mtwnEase(&timing->easing, q, false);
    }
    else
    {
        eased = mtwnEase(&timing->easing, forwards ? q : 1.0 - q, false);
    }
    return eased * timing->duration;
}

static bool Forwards(const mtwnTiming* timing, double iteration)
{
    return mtwnIterationForwards(timing->direction, iteration);
}

// The progress an iteration's traversal starts and ends at.
static double StartProgress(const Shape* shape, double j)
{
    if (shape->forwardRate)
    {
        return j == shape->first ? shape->firstProgress : 0.0;
    }
    return j == shape->last ? shape->lastProgress : 1.0;
}

static double EndProgress(const Shape* shape, double j)
{
    if (shape->forwardRate)
    {
        return j == shape->last ? shape->lastProgress : 1.0;
    }
    return j == shape->first ? shape->firstProgress : 0.0;
}

static Reach Part(double j, double q, bool inclusive)
{
    return (Reach){s_part, inclusive, j, q};
}

// The position a scaled time is at: its iteration and progress, and
// whether it lies in a repeat delay (at the iteration's end).
static Reach Position(const mtwnTiming* timing, const Shape* shape, double scaled, bool* inDelay)
{
    double d = timing->duration;
    double s = timing->iterationStart;
    double j = 0.0;
    double q = 0.0;
    *inDelay = false;
    if (!mtwnHasRepeatDelay(timing))
    {
        double overall = scaled / d + s;
        j = floor(overall);
        q = overall - j;
    }
    else
    {
        double cycle = d + timing->repeatDelay;
        double position = scaled + floor(s) * cycle + (s - floor(s)) * d;
        j = floor(position / cycle);
        double within = position - j * cycle;
        *inDelay = within >= d;
        q = *inDelay ? 1.0 : within / d;
        if (*inDelay && within == d && shape->forwardRate)
        {
            // At the delay's start exactly, going forwards: the end is
            // reached now.
            q = 1.0;
            *inDelay = false;
        }
    }
    // Rounding at the ends stays on the path.
    if (j < shape->first || (j == shape->first && q < shape->firstProgress))
    {
        j = shape->first;
        q = shape->firstProgress;
    }
    if (j > shape->last || (j == shape->last && q > shape->lastProgress))
    {
        j = shape->last;
        q = shape->lastProgress;
        *inDelay = false;
    }
    return Part(j, q, true);
}

// What a local time reaches at an iteration's start: going forwards the
// previous end shares the time without a delay; going backwards it is
// reached as the start is left, whatever lies between.
static Reach AtStart(const mtwnTiming* timing, const Shape* shape, double j, bool strict)
{
    if (shape->forwardRate)
    {
        if (!strict)
        {
            return Part(j, 0.0, true);
        }
        return mtwnHasRepeatDelay(timing) ? Part(j, 0.0, false) : Part(j - 1.0, 1.0, false);
    }
    return strict ? Part(j, 0.0, false) : Part(j - 1.0, 1.0, true);
}

// What a local time reaches of a level's path with finite iterations of
// positive duration; strict leaves out positions at the time itself.
static Reach Locate(const mtwnTiming* timing, const Shape* shape, double local, bool strict)
{
    double a = local - timing->delay;
    if (a < 0.0 || (a == 0.0 && strict))
    {
        return (Reach){s_none, false, 0.0, 0.0};
    }
    double startJ = shape->forwardRate ? shape->first : shape->last;
    if (a == 0.0)
    {
        return Part(startJ, StartProgress(shape, startJ), true);
    }
    if (a > shape->active)
    {
        return (Reach){s_all, true, 0.0, 0.0};
    }
    if (a == shape->active)
    {
        double endJ = shape->forwardRate ? shape->last : shape->first;
        return strict ? Part(endJ, EndProgress(shape, endJ), false)
                      : (Reach){s_all, true, 0.0, 0.0};
    }
    double rate = fabs(timing->playbackRate);
    double scaled = shape->forwardRate ? a * rate : (shape->active - a) * rate;
    bool inDelay = false;
    Reach at = Position(timing, shape, scaled, &inDelay);
    double j = at.iteration;
    if (inDelay)
    {
        // Going forwards the iteration's end was reached at the delay's
        // start; backwards the next start's time is its time too.
        return Part(j, 1.0, true);
    }
    if (at.progress == 0.0 && j > shape->first)
    {
        return AtStart(timing, shape, j, strict);
    }
    return Part(j, at.progress, !strict);
}

// Adds a pair, swapped or not, times a sign.
static void Add(mtwnCrossings* sum, double sign, bool swap, mtwnCrossings pair)
{
    sum->same += sign * (swap ? pair.opposite : pair.same);
    sum->opposite += sign * (swap ? pair.same : pair.opposite);
}

static mtwnCrossings Scaled(mtwnCrossings pair, double times)
{
    return (mtwnCrossings){pair.same * times, pair.opposite * times};
}

// The iterations strictly between two that run forwards, and all of them.
static void Between(const mtwnTiming* timing, double a, double b, double* forwardsOut,
                    double* allOut)
{
    double lo = (a < b ? a : b) + 1.0;
    double hi = (a < b ? b : a) - 1.0;
    double all = hi >= lo ? hi - lo + 1.0 : 0.0;
    double even = hi >= lo ? floor(hi / 2.0) - ceil(lo / 2.0) + 1.0 : 0.0;
    *allOut = all;
    switch (timing->direction)
    {
    case mtwn_directionNormal:
        *forwardsOut = all;
        break;
    case mtwn_directionReverse:
        *forwardsOut = 0.0;
        break;
    case mtwn_directionAlternate:
        *forwardsOut = even;
        break;
    default:
        *forwardsOut = all - even;
        break;
    }
}

// The iteration starts of a node a local time has reached.
static double IterationsReached(const mtwnTiming* timing, double local, bool strict)
{
    Shape shape = ShapeOf(timing, ActiveOf(timing));
    if (shape.point || shape.unbounded)
    {
        return 0.0;
    }
    Reach reach = Locate(timing, &shape, local, strict);
    if (reach.kind == s_none)
    {
        return 0.0;
    }
    if (reach.kind == s_all)
    {
        return shape.last - shape.first;
    }
    double k = reach.iteration;
    bool start = reach.progress == 0.0 && k > shape.first;
    if (shape.forwardRate)
    {
        return k > shape.first ? k - shape.first - (start && !reach.inclusive ? 1.0 : 0.0) : 0.0;
    }
    // Backwards a start reached is always followed by the end before it.
    return shape.last - k;
}

static mtwnCrossings LeafBelow(const mtwnLeaf* leaf, double y, bool strict)
{
    if (leaf->kind == mtwn_leafIterations)
    {
        return (mtwnCrossings){IterationsReached(&leaf->timing, y - leaf->at, strict), 0.0};
    }
    bool reached = strict ? leaf->at < y : leaf->at <= y;
    return (mtwnCrossings){reached ? 1.0 : 0.0, 0.0};
}

static double NextStart(const mtwnLevel* levels, uint32_t count, uint32_t i)
{
    return i + 1 < count ? levels[i + 1].start : 0.0;
}

// The whole reach of a prepared level, relative to its local time.
static mtwnCrossings All(const mtwnLevel* level, const Shape* shape)
{
    mtwnCrossings sum = level->cover[0];
    double firstJ = shape->forwardRate ? shape->first : shape->last;
    double lastJ = shape->forwardRate ? shape->last : shape->first;
    if (firstJ != lastJ)
    {
        double forwards = 0.0;
        double all = 0.0;
        Between(&level->timing, firstJ, lastJ, &forwards, &all);
        Add(&sum, 1.0, false, Scaled(level->cover[1], forwards));
        Add(&sum, 1.0, false, Scaled(level->cover[2], all - forwards));
        Add(&sum, 1.0, false, level->cover[3]);
    }
    return sum;
}

// The whole iterations a reach of iteration k covers before k.
static mtwnCrossings Prefix(const mtwnLevel* level, const Shape* shape, double k)
{
    mtwnCrossings sum = {0.0, 0.0};
    double firstJ = shape->forwardRate ? shape->first : shape->last;
    if (k == firstJ)
    {
        return sum;
    }
    sum = level->cover[0];
    double forwards = 0.0;
    double all = 0.0;
    Between(&level->timing, firstJ, k, &forwards, &all);
    Add(&sum, 1.0, false, Scaled(level->cover[1], forwards));
    Add(&sum, 1.0, false, Scaled(level->cover[2], all - forwards));
    return sum;
}

// A walk down the chain: the sum so far, the sign and swap the level
// below's count enters it with, and the point and strictness counted
// there.
typedef struct Walk
{
    mtwnCrossings sum;
    double sign;
    bool swap;
    bool strict;
    double x;
} Walk;

// Where a level's reach leaves its moving part to the level below: the
// child times its partly reached iteration runs between, whether it runs
// down the local time, the constant kept for its start, and whether its
// end is in. False when the level ends the walk.
typedef struct Partial
{
    double from;
    double to;
    bool down;
    bool inclusive;
    uint32_t start;
} Partial;

static bool Reaching(const mtwnLevel* level, Walk* walk, Partial* partial)
{
    const mtwnTiming* timing = &level->timing;
    Shape shape = ShapeOf(timing, level->activeDuration);
    double a = walk->x - timing->delay;
    if (a < 0.0 || (a == 0.0 && walk->strict))
    {
        return false;
    }
    if (shape.point)
    {
        Add(&walk->sum, walk->sign, walk->swap, level->point);
        return false;
    }
    if (shape.unbounded)
    {
        // One forward iteration from 0, at the rate; reversed, at infinity.
        *partial = (Partial){0.0, a * timing->playbackRate, false, !walk->strict, 1};
        return Forwards(timing, shape.first);
    }
    Reach reach = Locate(timing, &shape, walk->x, walk->strict);
    if (reach.kind == s_all)
    {
        Add(&walk->sum, walk->sign, walk->swap, All(level, &shape));
        return false;
    }
    double k = reach.iteration;
    Add(&walk->sum, walk->sign, walk->swap, Prefix(level, &shape, k));
    bool forwards = Forwards(timing, k);
    double firstJ = shape.forwardRate ? shape.first : shape.last;
    partial->start = k == firstJ ? 0u : (forwards ? 1u : 2u);
    partial->from = ChildAt(timing, forwards, StartProgress(&shape, k));
    partial->to = ChildAt(timing, forwards, reach.progress);
    partial->down = shape.forwardRate != forwards;
    partial->inclusive = reach.inclusive;
    return true;
}

// The crossings at points reached by a local time of level i (or the
// leaf, at the bottom): one walk down the chain.
static mtwnCrossings Below(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf,
                           uint32_t i, double x, bool strict)
{
    Walk walk = {{0.0, 0.0}, 1.0, false, strict, x};
    for (; i < count; ++i)
    {
        const mtwnLevel* level = &levels[i];
        Partial partial;
        if (!Reaching(level, &walk, &partial))
        {
            return walk.sum;
        }
        bool childSwap = walk.swap != partial.down;
        if (partial.from <= partial.to)
        {
            // Points in [from, to] or [from, to).
            Add(&walk.sum, -walk.sign, childSwap, level->startBelow[partial.start][1]);
            walk.strict = !partial.inclusive;
        }
        else
        {
            // Points in [to, from] or (to, from].
            Add(&walk.sum, walk.sign, childSwap, level->startBelow[partial.start][0]);
            walk.sign = -walk.sign;
            walk.strict = partial.inclusive;
        }
        walk.swap = childSwap;
        walk.x = partial.to - NextStart(levels, count, i);
    }
    Add(&walk.sum, walk.sign, walk.swap, LeafBelow(leaf, walk.x, walk.strict));
    return walk.sum;
}

// The points of one traversal of an iteration, from a progress to
// another, relative to the level's local time.
static mtwnCrossings Cover(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf,
                           uint32_t i, bool forwards, double from, double to)
{
    const mtwnLevel* level = &levels[i];
    double next = NextStart(levels, count, i);
    double childFrom = ChildAt(&level->timing, forwards, from);
    double childTo = ChildAt(&level->timing, forwards, to);
    double lo = childFrom < childTo ? childFrom : childTo;
    double hi = childFrom < childTo ? childTo : childFrom;
    mtwnCrossings sum = Below(levels, count, leaf, i + 1, hi - next, false);
    Add(&sum, -1.0, false, Below(levels, count, leaf, i + 1, lo - next, true));
    bool down = (level->timing.playbackRate > 0.0 ? 1 : -1) * (forwards ? 1 : -1) < 0;
    return down ? (mtwnCrossings){sum.opposite, sum.same} : sum;
}

static void PrepareLevel(mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf, uint32_t i)
{
    mtwnLevel* level = &levels[i];
    const mtwnTiming* timing = &level->timing;
    level->activeDuration = ActiveOf(timing);
    Shape shape = ShapeOf(timing, level->activeDuration);
    level->firstIteration = shape.first;
    level->firstProgress = shape.firstProgress;
    level->lastIteration = shape.last;
    level->lastProgress = shape.lastProgress;
    double next = NextStart(levels, count, i);
    if (shape.point)
    {
        double at = timing->duration == 0.0
                        ? 0.0
                        : ChildAt(timing, Forwards(timing, shape.first), shape.firstProgress);
        level->point = Below(levels, count, leaf, i + 1, at - next, false);
        Add(&level->point, -1.0, false, Below(levels, count, leaf, i + 1, at - next, true));
        return;
    }
    if (shape.unbounded)
    {
        for (int strict = 0; strict < 2; ++strict)
        {
            level->startBelow[1][strict] = Below(levels, count, leaf, i + 1, -next, strict != 0);
        }
        return;
    }
    double firstJ = shape.forwardRate ? shape.first : shape.last;
    double lastJ = shape.forwardRate ? shape.last : shape.first;
    double starts[3] = {ChildAt(timing, Forwards(timing, firstJ), StartProgress(&shape, firstJ)),
                        ChildAt(timing, true, shape.forwardRate ? 0.0 : 1.0),
                        ChildAt(timing, false, shape.forwardRate ? 0.0 : 1.0)};
    for (int k = 0; k < 3; ++k)
    {
        for (int strict = 0; strict < 2; ++strict)
        {
            level->startBelow[k][strict] =
                Below(levels, count, leaf, i + 1, starts[k] - next, strict != 0);
        }
    }
    double full0 = shape.forwardRate ? 0.0 : 1.0;
    level->cover[0] = Cover(levels, count, leaf, i, Forwards(timing, firstJ),
                            StartProgress(&shape, firstJ), EndProgress(&shape, firstJ));
    level->cover[1] = Cover(levels, count, leaf, i, true, full0, 1.0 - full0);
    level->cover[2] = Cover(levels, count, leaf, i, false, full0, 1.0 - full0);
    if (isfinite(lastJ))
    {
        level->cover[3] = Cover(levels, count, leaf, i, Forwards(timing, lastJ),
                                StartProgress(&shape, lastJ), EndProgress(&shape, lastJ));
    }
}

void mtwnPrepareLevels(mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf)
{
    for (uint32_t i = count; i > 0; --i)
    {
        PrepareLevel(levels, count, leaf, i - 1);
    }
}

static mtwnCrossings Reached(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf,
                             double t, bool strict)
{
    double local = count > 0 ? t - levels[0].start : t;
    return Below(levels, count, leaf, 0, local, strict);
}

mtwnCrossings mtwnCountCrossings(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf,
                                 double from, double to, bool fromClosed)
{
    mtwnCrossings sum = {0.0, 0.0};
    if (to >= from)
    {
        sum = Reached(levels, count, leaf, to, false);
        Add(&sum, -1.0, false, Reached(levels, count, leaf, from, fromClosed));
        if (to == from && !fromClosed)
        {
            return (mtwnCrossings){0.0, 0.0};
        }
        return sum;
    }
    sum = Reached(levels, count, leaf, from, !fromClosed);
    Add(&sum, -1.0, false, Reached(levels, count, leaf, to, true));
    // The sweep runs against the coordinate: the leaf moves the other way.
    return (mtwnCrossings){sum.opposite, sum.same};
}

static double Of(mtwnCrossings c, mtwnCrossingWay way)
{
    return way == mtwn_crossingForwards    ? c.same
           : way == mtwn_crossingBackwards ? c.opposite
                                           : c.same + c.opposite;
}

double mtwnFindCrossing(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf, double from,
                        double to, bool fromClosed, bool last, mtwnCrossingWay way)
{
    double lo = from;
    double hi = to;
    if (!last)
    {
        if (fromClosed && Of(mtwnCountCrossings(levels, count, leaf, from, from, true), way) >= 1.0)
        {
            return from;
        }
        // The sweep to lo counts none, to hi one at least.
        for (;;)
        {
            double mid = lo + (hi - lo) / 2.0;
            if (mid == lo || mid == hi)
            {
                return hi;
            }
            bool found =
                Of(mtwnCountCrossings(levels, count, leaf, from, mid, fromClosed), way) >= 1.0;
            *(found ? &hi : &lo) = mid;
        }
    }
    if (Of(mtwnCountCrossings(levels, count, leaf, to, to, true), way) >= 1.0)
    {
        return to;
    }
    // The sweep from lo to the end counts one at least, from hi none.
    for (;;)
    {
        double mid = lo + (hi - lo) / 2.0;
        if (mid == lo || mid == hi)
        {
            return lo;
        }
        bool found = Of(mtwnCountCrossings(levels, count, leaf, mid, to, true), way) >= 1.0;
        *(found ? &lo : &hi) = mid;
    }
}

double mtwnIterationAt(const mtwnLevel* levels, uint32_t count, double t)
{
    double x = t - levels[0].start;
    double iteration = 0.0;
    for (uint32_t i = 0; i < count; ++i)
    {
        mtwnTiming timing = levels[i].timing;
        timing.fill = mtwn_fillBoth;
        mtwnTimingResult result;
        mtwnComputeTiming(&timing, x, false, &result);
        iteration = result.iteration;
        double child = 0.0;
        if (isinf(timing.duration))
        {
            child = result.forwards ? result.activeTime * timing.playbackRate : HUGE_VAL;
        }
        else
        {
            child = result.transformedProgress * timing.duration;
        }
        x = child - NextStart(levels, count, i);
    }
    return iteration;
}
