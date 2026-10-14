// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Events (records mtwn-0004, mtwn-0013 and mtwn-0038): the bounded queue,
// and the records a sweep of a clip's player makes. Every node's active
// start and end, iteration starts and marks are counted by the crossing
// engine (record mtwn-0037) along the chain of timelines above it; a
// subtree whose timelines all stayed in one iteration without their time
// moving cannot have been crossed and is passed over. Records are kept in
// the order they are made (creation order, clip order, then start,
// iteration, end) and sorted, stably, by time once the update is done.

#include "maul-tween/event.h"

#include "context_internal.h"
#include "crossing.h"

#include <math.h>

void mtwnPushEvent(mtwnContext* context, const mtwnEvent* event, double along)
{
    uint32_t capacity = context->def.eventCapacity;
    if (context->eventCount == capacity)
    {
        ++context->droppedEvents;
        ++context->droppedSinceRead;
        return;
    }
    uint32_t at = (context->eventHead + context->eventCount) % capacity;
    context->events[at] = *event;
    context->eventAlong[at] = along;
    ++context->eventCount;
}

static uint32_t SlotOf(const mtwnContext* context, uint32_t i)
{
    return (context->eventHead + i) % context->def.eventCapacity;
}

// Whether a record comes before another: by time, then, for one player,
// by its place along the sweep. Records are made in the players' creation
// order, which the stable sort keeps among players.
static bool Before(const mtwnEvent* a, double alongA, const mtwnEvent* b, double alongB)
{
    if (a->hostTime != b->hostTime)
    {
        return a->hostTime < b->hostTime;
    }
    return a->player.index1 == b->player.index1 && alongA < alongB;
}

static mtwnEvent* EventAt(mtwnContext* context, uint32_t i)
{
    return &context->events[(context->eventHead + i) % context->def.eventCapacity];
}

void mtwnSortEvents(mtwnContext* context, uint32_t count)
{
    // Insertion, stable: records of one place keep the order they were made.
    double* orders = context->eventAlong;
    for (uint32_t i = count + 1; i < context->eventCount; ++i)
    {
        mtwnEvent moving = *EventAt(context, i);
        double order = orders[SlotOf(context, i)];
        uint32_t j = i;
        while (j > count &&
               Before(&moving, order, EventAt(context, j - 1), orders[SlotOf(context, j - 1)]))
        {
            *EventAt(context, j) = *EventAt(context, j - 1);
            orders[SlotOf(context, j)] = orders[SlotOf(context, j - 1)];
            --j;
        }
        *EventAt(context, j) = moving;
        orders[SlotOf(context, j)] = order;
    }
}

static mtwnPlayerId IdOf(const mtwnContext* context, uint32_t player)
{
    return (mtwnPlayerId){player, mtwnPlayerAt(context, player)->header.generation};
}

void mtwnPlayerEvent(mtwnContext* context, uint32_t player, mtwnEventKind kind, double hostTime,
                     double clipTime)
{
    const mtwnPlayerSlot* slot = mtwnPlayerAt(context, player);
    mtwnEvent event = {0};
    event.hostTime = hostTime;
    event.clipTime = clipTime;
    event.count = 1;
    event.player = IdOf(context, player);
    event.node = slot->type == MTWN_PLAYER_CLIP ? MTWN_ROOT_NODE : 0;
    event.kind = kind;
    event.backwards = slot->type == MTWN_PLAYER_CLIP && slot->play.rate < 0.0;
    mtwnPushEvent(context, &event, 0.0);
}

// A sweep in progress: its player, its ends, and the host's times.
typedef struct Sweep
{
    mtwnContext* context;
    uint32_t player;
    const mtwnClipPlay* play;
    double from;
    double to;
    bool fromNone;
    // Whether the sweep takes its start: from no time, or turning back.
    bool fromClosed;
    double hostFrom;
    double hostTo;
    // The rate the player's time ran at since hostFrom.
    double rate;
} Sweep;

// The host's time a player's time was reached at: from the sweep's start
// at the rate that ran since (which the sweep runs with), not past the
// sweep's end; the sweep's end when no time passed or none ran.
static double HostAt(const Sweep* sweep, double t)
{
    if (sweep->fromNone || sweep->hostTo == sweep->hostFrom || sweep->rate == 0.0)
    {
        return sweep->hostTo;
    }
    double host = sweep->hostFrom + (t - sweep->from) / sweep->rate;

    return host > sweep->hostTo ? sweep->hostTo : host;
}

// What a crossing of a leaf is, either way: the kinds, and whether a kind
// counts at all for the way.
typedef struct Kinds
{
    mtwnEventKind forwards;
    mtwnEventKind backwards;
} Kinds;

// Records a leaf's crossings, chain levels [0, count) above it and the
// node whose iteration the records carry at levels[iterationLevel].
static void Record(const Sweep* sweep, uint32_t count, uint32_t iterationLevels,
                   const mtwnLeaf* leaf, Kinds kinds, mtwnNode node, uint32_t key)
{
    mtwnLevel* levels = sweep->context->levels;
    mtwnPrepareLevels(levels, count, leaf);
    mtwnCrossings crossings =
        mtwnCountCrossings(levels, count, leaf, sweep->from, sweep->to, sweep->fromClosed);
    if (sweep->fromNone && sweep->play->rate < 0.0)
    {
        // From no time, the point taken is passed the way the player runs.
        crossings = (mtwnCrossings){crossings.opposite, crossings.same};
    }
    for (int way = 0; way < 2; ++way)
    {
        double n = way == 0 ? crossings.same : crossings.opposite;
        if (n < 1.0)
        {
            continue;
        }
        mtwnCrossingWay search = sweep->fromNone ? mtwn_crossingEither
                                 : way == 0      ? mtwn_crossingForwards
                                                 : mtwn_crossingBackwards;
        double first = mtwnFindCrossing(levels, count, leaf, sweep->from, sweep->to,
                                        sweep->fromClosed, false, search);
        double last = mtwnFindCrossing(levels, count, leaf, sweep->from, sweep->to,
                                       sweep->fromClosed, true, search);
        mtwnEvent event = {0};
        event.hostTime = HostAt(sweep, first);
        event.clipTime = first;
        event.firstIteration = mtwnIterationAt(levels, iterationLevels, first);
        event.lastIteration = mtwnIterationAt(levels, iterationLevels, last);
        event.count = n >= 18446744073709549568.0 ? UINT64_MAX : (uint64_t)n;
        event.player = IdOf(sweep->context, sweep->player);
        event.node = node;
        event.key = key;
        event.kind = way == 0 ? kinds.forwards : kinds.backwards;
        event.backwards = way != 0;
        // Its place along the sweep: how far from the sweep's start.
        double along = fabs(first - sweep->from);
        mtwnPushEvent(sweep->context, &event, along);
    }
}

// Fills the chain of timelines above a node, the root first, and the node
// after them; returns the chain's length.
static uint32_t Chain(mtwnLevel* levels, const mtwnClipSlot* clip, uint32_t node)
{
    uint32_t count = 0;
    for (uint32_t at = clip->nodes[node - 1].parent; at != 0; at = clip->nodes[at - 1].parent)
    {
        ++count;
    }
    uint32_t i = count;
    for (uint32_t at = node; at != 0; at = clip->nodes[at - 1].parent)
    {
        const mtwnClipNode* n = &clip->nodes[at - 1];
        levels[i] = (mtwnLevel){0};
        levels[i].timing = n->timing;
        levels[i].start = n->start;
        if (i == 0)
        {
            break;
        }
        --i;
    }
    return count;
}

static mtwnLeaf InstantAt(double at)
{
    mtwnLeaf leaf = {0};
    leaf.at = at;
    leaf.kind = mtwn_leafInstant;
    return leaf;
}

// The margin Missed leaves: the child times it compares are worked out
// by other arithmetic than the crossings' exact positions, and may differ
// from them by rounding, far below this.
static const double s_missedMargin = 1e-6;

// Whether the sweep certainly left the parent's child time short of the
// span [from, to] in it: the parent moved one way in one iteration of each
// timeline above (its child time monotonic, timelines' easings being so),
// both ends clearly inside those iterations, so it covered the span
// between its two ends, and a point well outside was not crossed (record
// mtwn-0042). False when unsure, NaN included.
static bool Missed(const mtwnNodeTime* parent, double from, double to)
{
    if (!parent->sure)
    {
        return false;
    }
    double low = parent->now < parent->before ? parent->now : parent->before;
    double high = parent->now < parent->before ? parent->before : parent->now;
    double below = low - s_missedMargin * (1.0 + fabs(low));
    double above = high + s_missedMargin * (1.0 + fabs(high));
    return to < below || from > above;
}

// Whether a timing has iteration starts after its first: an integer
// strictly inside (iterationStart, iterationStart + iterations).
static bool Iterates(const mtwnTiming* timing)
{
    return floor(timing->iterationStart) + 1.0 < timing->iterationStart + timing->iterations;
}

// A node's records: a mark's crossings, or a track's or timeline's start,
// iterations and end. Those its parent certainly did not reach are not
// counted, their counts being 0.
static void NodeEvents(const Sweep* sweep, const mtwnClipSlot* clip, uint32_t node,
                       const mtwnNodeTime* parent)
{
    const mtwnClipNode* n = &clip->nodes[node - 1];
    mtwnLevel* levels = sweep->context->levels;
    if (n->kind == mtwn_nodeMark)
    {
        if (!Missed(parent, n->start, n->start))
        {
            mtwnLeaf mark = InstantAt(n->start);
            uint32_t count = Chain(levels, clip, node);
            Record(sweep, count, count, &mark, (Kinds){mtwn_eventMark, mtwn_eventMark}, node,
                   n->key);
        }
        return;
    }
    if (n->kind == mtwn_nodeLabel)
    {
        return;
    }
    double activeStart = n->start + n->timing.delay;
    double activeEnd = activeStart + n->activeDuration;
    mtwnLeaf start = InstantAt(activeStart);
    mtwnLeaf end = InstantAt(activeEnd);
    mtwnLeaf iterations = {0};
    iterations.at = n->start;
    iterations.timing = n->timing;
    iterations.kind = mtwn_leafIterations;
    // The levels below the chain hold the node itself, for its iteration;
    // preparing a leaf leaves them be, so the chain is filled again for
    // each.
    if (!Missed(parent, activeStart, activeStart))
    {
        uint32_t count = Chain(levels, clip, node);
        Record(sweep, count, count + 1, &start, (Kinds){mtwn_eventStart, mtwn_eventEnd}, node, 0);
    }
    if (Iterates(&n->timing) && !Missed(parent, activeStart, activeEnd))
    {
        uint32_t count = Chain(levels, clip, node);
        Record(sweep, count, count + 1, &iterations,
               (Kinds){mtwn_eventIteration, mtwn_eventIteration}, node, 0);
    }
    if (!Missed(parent, activeEnd, activeEnd))
    {
        uint32_t count = Chain(levels, clip, node);
        Record(sweep, count, count + 1, &end, (Kinds){mtwn_eventEnd, mtwn_eventStart}, node, 0);
    }
}

// Whether a child time lies clearly inside an iteration of a duration:
// off 0 and the duration by more than the margin Missed leaves. False for
// NaN.
static bool Inside(double child, double duration)
{
    double margin = s_missedMargin * (1.0 + fabs(child));
    bool offStart = fabs(child) > margin;
    bool offEnd = isinf(duration) || fabs(child - duration) > margin;
    return offStart && offEnd;
}

// A timeline's child times at the sweep's ends, and whether it stayed in
// one iteration.
static void Times(const mtwnClipNode* n, const mtwnNodeTime* parent, mtwnNodeTime* out)
{
    mtwnTiming timing = n->timing;
    timing.fill = mtwn_fillBoth;
    double ends[2] = {parent->now, parent->before};
    double iterations[2] = {0.0, 0.0};
    double children[2] = {0.0, 0.0};
    for (int k = 0; k < 2; ++k)
    {
        mtwnTimingResult result;
        mtwnComputeTiming(&timing, ends[k] - n->start, false, &result);
        iterations[k] = result.iteration;
        children[k] = isinf(timing.duration)
                          ? (result.forwards ? result.activeTime * timing.playbackRate : HUGE_VAL)
                          : result.transformedProgress * timing.duration;
    }
    out->now = children[0];
    out->before = children[1];
    out->steady = parent->steady && iterations[0] == iterations[1];
    out->sure = parent->sure && out->steady && Inside(children[0], timing.duration) &&
                Inside(children[1], timing.duration);
}

void mtwnSweepEvents(mtwnContext* context, uint32_t player, double to, double rate, double hostFrom,
                     double hostTo)
{
    mtwnClipPlay* play = &mtwnPlayerAt(context, player)->play;
    double from = play->swept;
    bool fromNone = isnan(from);
    play->swept = to;
    if (!fromNone && from == to)
    {
        return;
    }
    int8_t way = fromNone ? (int8_t)(play->rate < 0.0 ? -1 : 1) : (int8_t)(to > from ? 1 : -1);
    bool turning = !fromNone && play->sweptWay != 0 && play->sweptWay != way;
    play->sweptWay = way;
    mtwnClipSlot* clip = mtwnClipAt(context, play->clip.index1);
    Sweep sweep = {context,  player, play, fromNone ? to : from, to, fromNone, fromNone || turning,
                   hostFrom, hostTo, rate};
    // The player's time is the root's parent's, and moves one way.
    mtwnNodeTime top = {to, sweep.from, false, true, true, {0}};
    for (uint32_t i = 0; i < clip->nodeCount; ++i)
    {
        const mtwnClipNode* n = &clip->nodes[i];
        const mtwnNodeTime* parent = i == 0 ? &top : &clip->times[n->parent - 1];
        bool still = !fromNone && parent->steady && parent->now == parent->before;
        if (n->kind == mtwn_nodeTimeline)
        {
            if (still)
            {
                clip->times[i] = (mtwnNodeTime){0.0, 0.0, false, true, true, {0}};
            }
            else
            {
                Times(n, parent, &clip->times[i]);
            }
        }
        if (!still)
        {
            NodeEvents(&sweep, clip, i + 1, parent);
        }
    }
}

mtwnResult mtwnReadEvents(mtwnContext* context, mtwnEvent* eventsOut, uint32_t capacity,
                          uint32_t* countOut, uint64_t* droppedOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (countOut == NULL || (eventsOut == NULL && capacity != 0))
    {
        ++context->misuse;
        return mtwn_errorInvalid;
    }
    uint32_t n = capacity < context->eventCount ? capacity : context->eventCount;
    for (uint32_t i = 0; i < n; ++i)
    {
        eventsOut[i] = *EventAt(context, i);
    }
    if (n > 0)
    {
        context->eventHead = (context->eventHead + n) % context->def.eventCapacity;
        context->eventCount -= n;
    }
    *countOut = n;
    if (droppedOut != NULL)
    {
        *droppedOut = context->droppedSinceRead;
    }
    context->droppedSinceRead = 0;
    return mtwn_success;
}

uint32_t mtwnContext_GetEventCount(const mtwnContext* context)
{
    return context != NULL ? context->eventCount : 0;
}

uint64_t mtwnContext_GetDroppedEvents(const mtwnContext* context)
{
    return context != NULL ? context->droppedEvents : 0;
}
