// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A clip's player sampled into its targets (records mtwn-0012, mtwn-0013
// and mtwn-0036). The nodes are walked in the order they were added, each
// after its parent: a timeline passes its children its transformed time
// (Web Animations Level 2), a track writes its value into the target bound
// to its slot, later tracks over earlier ones. A moving player's velocity
// is its values' change over the last millisecond, as a transition's is.

#include "compose.h"
#include "context_internal.h"
#include "elementary.h"

#include "maul-tween/path.h"

#include <math.h>

// The span the velocity is measured over, as a transition's.
static const double s_velocitySpan = 0.001;

uint32_t mtwnBoundTarget(const mtwnContext* context, const mtwnClipPlay* play, uint32_t slot)
{
    mtwnTargetId id = context->bindings.entries[play->bindingStart + slot].target;
    if (id.index1 == 0 || mtwnPoolFind(&context->targets, id.index1, id.generation) == NULL)
    {
        return 0;
    }
    return id.index1;
}

void mtwnMarkUnwritten(mtwnContext* context, const mtwnClipPlay* play)
{
    for (uint32_t s = 0; s < play->bindingCount; ++s)
    {
        uint32_t target = mtwnBoundTarget(context, play, s);
        if (target != 0)
        {
            mtwnTargetAt(context, target)->header.flags |= MTWN_TARGET_UNWRITTEN;
        }
    }
}

void mtwnResetUnwritten(mtwnContext* context, const mtwnClipPlay* play)
{
    for (uint32_t s = 0; s < play->bindingCount; ++s)
    {
        uint32_t target = mtwnBoundTarget(context, play, s);
        if (target != 0 &&
            (mtwnTargetAt(context, target)->header.flags & MTWN_TARGET_UNWRITTEN) != 0)
        {
            mtwnWriteTarget(context, target, mtwnTargetAt(context, target)->base, (mtwnVec4){0});
        }
    }
}

// The time a timeline gives its children: its transformed progress times
// its iteration duration; with an infinite duration, its active time at its
// rate going forwards, its end going backwards. NaN when it has no effect.
static double ChildTime(const mtwnTiming* timing, double local, bool backwards, bool* forwards)
{
    mtwnTimingResult result;
    mtwnComputeTiming(timing, local, backwards, &result);
    if (forwards != NULL)
    {
        *forwards = result.forwards;
    }
    if (isnan(result.transformedProgress))
    {
        return (double)NAN;
    }
    if (isinf(timing->duration))
    {
        return result.forwards ? result.activeTime * timing->playbackRate : HUGE_VAL;
    }
    return result.transformedProgress * timing->duration;
}

// A track's progress and iteration now and a moment ago (NaN when it has
// none), from its parent's times.
typedef struct TrackTimes
{
    double progress;
    double iteration;
    double earlier;
    double earlierIteration;
} TrackTimes;

// A track's progress at a local time, NaN when it has no effect, and its
// current iteration.
static double TrackProgress(const mtwnClipNode* node, double local, bool backwards,
                            double* iterationOut)
{
    *iterationOut = 0.0;
    if (isnan(local))
    {
        return (double)NAN;
    }
    mtwnTimingResult result;
    mtwnComputeTiming(&node->timing, local, backwards, &result);
    *iterationOut = result.iteration;
    return result.transformedProgress;
}

// A unit rotation turned k times: its angle times k about its axis.
static mtwnVec4 Power(mtwnVec4 q, double k)
{
    double x = (double)q.x;
    double y = (double)q.y;
    double z = (double)q.z;
    double w = (double)q.w;
    double sine = sqrt(x * x + y * y + z * z);
    if (!(sine > 0.0))
    {
        return (mtwnVec4){0.0f, 0.0f, 0.0f, 1.0f};
    }
    double half = mtwnAtan2(sine, w) * k;
    double sinHalf = 0.0;
    double cosHalf = 0.0;
    mtwnSinCos(half, &sinHalf, &cosHalf);
    double s = sinHalf / sine;
    return (mtwnVec4){(float)(x * s), (float)(y * s), (float)(z * s), (float)cosHalf};
}

// An incremental track's value at an iteration: moved on k steps.
static mtwnVec4 Moved(const mtwnClipNode* node, mtwnVec4 value, double k)
{
    if (!node->incremental || k == 0.0)
    {
        return value;
    }
    if (node->track.kind == mtwn_valueRotation)
    {
        mtwnVec4 turn = Power(node->step, k);
        mtwnMoving under = {turn, {0.0f, 0.0f, 0.0f, 0.0f}};
        mtwnMoving over = {value, {0.0f, 0.0f, 0.0f, 0.0f}};
        // The turn so far, then the iteration's own path in its frame.
        return mtwnCompose(mtwn_valueRotation, mtwn_compositeAdd, 1.0, under, over).value;
    }
    return (mtwnVec4){(float)((double)value.x + k * (double)node->step.x),
                      (float)((double)value.y + k * (double)node->step.y),
                      (float)((double)value.z + k * (double)node->step.z),
                      (float)((double)value.w + k * (double)node->step.w)};
}

// A track's value, an integer's unrounded for its velocity.
static mtwnVec4 Unrounded(const mtwnTrack* track, double progress)
{
    if (track->kind != mtwn_valueInteger)
    {
        return mtwnSampleTrack(track, progress);
    }
    mtwnTrack scalar = *track;
    scalar.kind = mtwn_valueScalar;
    return mtwnSampleTrack(&scalar, progress);
}

// Writes a track's value, and its velocity when it had a value a moment
// ago, into its target, composed onto what lies under it there: what
// earlier players wrote in this update, or the base.
static TrackTimes TimesOf(const mtwnClipNode* node, const mtwnNodeTime* parent, bool moving)
{
    TrackTimes t = {(double)NAN, 0.0, (double)NAN, 0.0};
    t.progress = TrackProgress(node, parent->now - node->start, parent->backwards, &t.iteration);
    if (moving && !isnan(t.progress))
    {
        t.earlier = TrackProgress(node, parent->before - node->start, parent->backwards,
                                  &t.earlierIteration);
    }
    return t;
}

static void WriteTrack(mtwnContext* context, const mtwnClipNode* node, uint32_t target,
                       const mtwnBound* bound, double weight, const TrackTimes* times, bool commit)
{
    double progress = times->progress;
    if (isnan(progress))
    {
        return;
    }
    mtwnVec4 value = Moved(node, Unrounded(&node->track, progress), times->iteration);
    float velocity[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    double earlier = times->earlier;
    if (!isnan(earlier) && node->track.kind != mtwn_valueDiscrete)
    {
        mtwnVec4 before = Moved(node, Unrounded(&node->track, earlier), times->earlierIteration);
        const float* a = &value.x;
        const float* b = &before.x;
        for (int i = 0; i < 4; ++i)
        {
            velocity[i] = (float)(((double)a[i] - (double)b[i]) / s_velocitySpan);
        }
    }
    mtwnTargetSlot* slot = mtwnTargetAt(context, target);
    bool unwritten = (slot->header.flags & MTWN_TARGET_UNWRITTEN) != 0;
    mtwnMoving under = {unwritten ? slot->base : slot->value,
                        unwritten ? (mtwnVec4){0.0f, 0.0f, 0.0f, 0.0f}
                                  : mtwnTargetVelocity(context, target)};
    mtwnMoving effect = {value, {velocity[0], velocity[1], velocity[2], velocity[3]}};
    mtwnMoving composed = mtwnCompose(node->track.kind, bound->composite,
                                      weight * (double)bound->weight, under, effect);
    value = composed.value;
    mtwnWriteTarget(context, target, value, composed.velocity);
    bool rebased = slot->base.x != value.x || slot->base.y != value.y || slot->base.z != value.z ||
                   slot->base.w != value.w;
    if (commit && rebased)
    {
        slot->base = value;
        mtwnMarkChanged(context, target);
    }
}

// A timeline's times for its children, from its parent's.
static void TimelineTimes(const mtwnClipNode* node, const mtwnNodeTime* parent, bool moving,
                          mtwnNodeTime* out)
{
    bool forwards = true;
    out->now = isnan(parent->now) ? (double)NAN
                                  : ChildTime(&node->timing, parent->now - node->start,
                                              parent->backwards, &forwards);
    out->before =
        moving && !isnan(parent->before)
            ? ChildTime(&node->timing, parent->before - node->start, parent->backwards, NULL)
            : (double)NAN;
    // Time runs back in a reversed iteration, and in a node played backwards.
    bool reversed = forwards != (node->timing.playbackRate > 0.0);
    out->backwards = parent->backwards != reversed;
}

void mtwnSampleClip(mtwnContext* context, const mtwnClipPlay* play, double current, double rate,
                    bool backwards, bool commit)
{
    mtwnClipSlot* clip = mtwnClipAt(context, play->clip.index1);
    mtwnNodeTime* times = clip->times;
    bool moving = rate != 0.0;
    // The player's time is the root's parent's.
    mtwnNodeTime player = {current, current - rate * s_velocitySpan, backwards, false, false, {0}};
    TimelineTimes(&clip->nodes[0], &player, moving, &times[0]);
    // The last track's timing results, for the tracks after it that share
    // its timing; 0 when none is kept.
    TrackTimes kept = {0.0, 0.0, 0.0, 0.0};
    uint32_t keptFor = 0;
    for (uint32_t i = 1; i < clip->nodeCount; ++i)
    {
        const mtwnClipNode* node = &clip->nodes[i];
        const mtwnNodeTime* parent = &times[node->parent - 1];
        if (node->kind == mtwn_nodeTimeline)
        {
            TimelineTimes(node, parent, moving, &times[i]);
        }
        else if (node->kind == mtwn_nodeTrack)
        {
            uint32_t target = mtwnBoundTarget(context, play, node->slot);
            if (target != 0)
            {
                if (!(node->sharesTiming && keptFor == i))
                {
                    kept = TimesOf(node, parent, moving);
                }
                keptFor = i + 1;
                const mtwnBound* bound =
                    &context->bindings.entries[play->bindingStart + node->slot];
                WriteTrack(context, node, target, bound, play->weight, &kept, commit);
            }
        }
    }
}
