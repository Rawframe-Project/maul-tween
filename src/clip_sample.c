// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A clip's player sampled into its targets (records mtwn-0012, mtwn-0013
// and mtwn-0036). The nodes are walked in the order they were added, each
// after its parent: a timeline passes its children its transformed time
// (Web Animations Level 2), a track writes its value into the target bound
// to its slot, later tracks over earlier ones. A moving player's velocity
// is its values' change over the last millisecond, as a transition's is.

#include "context_internal.h"

#include "maul-tween/path.h"

#include <math.h>

// The span the velocity is measured over, as a transition's.
static const double s_velocitySpan = 0.001;

uint32_t mtwnBoundTarget(const mtwnContext* context, const mtwnClipPlay* play, uint32_t slot)
{
    mtwnTargetId id = context->bindings.entries[play->bindingStart + slot];
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

// A track's progress at a local time, NaN when it has no effect.
static double TrackProgress(const mtwnClipNode* node, double local, bool backwards)
{
    if (isnan(local))
    {
        return (double)NAN;
    }
    mtwnTimingResult result;
    mtwnComputeTiming(&node->timing, local, backwards, &result);
    return result.transformedProgress;
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
// ago, into its target.
static void WriteTrack(mtwnContext* context, const mtwnClipNode* node, uint32_t target,
                       const mtwnNodeTime* parent, bool moving, bool commit)
{
    double progress = TrackProgress(node, parent->now - node->start, parent->backwards);
    if (isnan(progress))
    {
        return;
    }
    mtwnVec4 value = Unrounded(&node->track, progress);
    float velocity[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    double earlier =
        moving ? TrackProgress(node, parent->before - node->start, parent->backwards) : (double)NAN;
    if (!isnan(earlier) && node->track.kind != mtwn_valueDiscrete)
    {
        mtwnVec4 before = Unrounded(&node->track, earlier);
        const float* a = &value.x;
        const float* b = &before.x;
        for (int i = 0; i < 4; ++i)
        {
            velocity[i] = (float)(((double)a[i] - (double)b[i]) / s_velocitySpan);
        }
    }
    if (node->track.kind == mtwn_valueInteger)
    {
        value.x = (float)round((double)value.x);
    }
    mtwnWriteTarget(context, target, value,
                    (mtwnVec4){velocity[0], velocity[1], velocity[2], velocity[3]});
    mtwnTargetSlot* slot = mtwnTargetAt(context, target);
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
    mtwnNodeTime player = {current, current - rate * s_velocitySpan, backwards, {0}};
    TimelineTimes(&clip->nodes[0], &player, moving, &times[0]);
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
                WriteTrack(context, node, target, parent, moving, commit);
            }
        }
    }
}
