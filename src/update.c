// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The update (records mtwn-0004, mtwn-0007 and mtwn-0029): every player
// advanced to the host's time in the library's floating-point
// environment, then the changed targets reported. A transition samples
// its easing, spring or decay, with the spring's or decay's own velocity
// or else its curve's over the last millisecond, and at its end commits
// its end value as the target's base (records mtwn-0029 and mtwn-0030).

#include "context_internal.h"
#include "fpenv.h"

#include <math.h>
#include <string.h>

// The span the velocity is measured over, as Compose measures a tween's.
static const double s_velocitySpan = 0.001;

// The value along the transition's path at a progress, before rounding:
// a color's or rotation's mix, a discrete value's step, the others'
// straight line (an angle's end already moved by its turns).
static void PathAt(const mtwnPlayerSlot* slot, double progress, double out[4])
{
    const float* from = &slot->from.x;
    const float* to = &slot->to.x;
    out[0] = out[1] = out[2] = out[3] = 0.0;
    switch (slot->kind)
    {
    case mtwn_valueColor:
    {
        mtwnColor color = mtwnMixColor(&slot->mix.color, progress);
        out[0] = (double)color.red;
        out[1] = (double)color.green;
        out[2] = (double)color.blue;
        out[3] = (double)color.alpha;
        return;
    }
    case mtwn_valueRotation:
    {
        mtwnQuat q = mtwnMixRotation(&slot->mix.rotation, progress);
        out[0] = (double)q.x;
        out[1] = (double)q.y;
        out[2] = (double)q.z;
        out[3] = (double)q.w;
        return;
    }
    case mtwn_valueDiscrete:
        out[0] = (double)(progress < 0.5 ? from[0] : to[0]);
        return;
    default:
        for (uint8_t i = 0; i < mtwnKindChannels(slot->kind); ++i)
        {
            out[i] = (1.0 - progress) * (double)from[i] + progress * (double)to[i];
        }
        return;
    }
}

// The transition at t seconds after its delay, before rounding. A spring
// or decay on channels gives its own velocity and returns true; the
// others leave the velocity to the caller.
static bool Sample(const mtwnPlayerSlot* slot, double t, double value[4], double velocity[4])
{
    if (slot->spec == mtwn_transitionEasing)
    {
        PathAt(slot, mtwnEase(&slot->easing, t / slot->duration, false), value);
        return false;
    }
    if (slot->spec == mtwn_transitionSpring && mtwnIsPathKind(slot->kind))
    {
        double offset = 0.0;
        mtwnSampleSpring(&slot->spring, -1.0, slot->velocity[0], t, &offset, NULL);
        PathAt(slot, 1.0 + offset, value);
        return false;
    }
    const float* from = &slot->from.x;
    const float* to = &slot->to.x;
    mtwnSpringMotion motion = {0};
    if (slot->spec == mtwn_transitionSpring)
    {
        motion = mtwnSpringMotionAt(&slot->spring, t);
    }
    for (uint8_t i = 0; i < 4; ++i)
    {
        value[i] = velocity[i] = 0.0;
        if (i >= mtwnKindChannels(slot->kind))
        {
            continue;
        }
        double v = slot->velocity[i];
        if (slot->spec == mtwn_transitionSpring)
        {
            double offset = (double)from[i] - (double)to[i];
            value[i] =
                (double)to[i] + motion.offsetFromOffset * offset + motion.offsetFromVelocity * v;
            velocity[i] = motion.velocityFromOffset * offset + motion.velocityFromVelocity * v;
        }
        else
        {
            mtwnSampleDecay(&slot->decay, (double)from[i], v, t, &value[i], &velocity[i]);
        }
    }
    return true;
}

// Whether two values have the same bits, -0 and 0 apart.
static bool SameBits(mtwnVec4 a, mtwnVec4 b)
{
    uint32_t x[4];
    uint32_t y[4];
    memcpy(x, &a, sizeof x);
    memcpy(y, &b, sizeof y);
    return x[0] == y[0] && x[1] == y[1] && x[2] == y[2] && x[3] == y[3];
}

void mtwnWriteTarget(mtwnContext* context, uint32_t index1, mtwnVec4 value, mtwnVec4 velocity)
{
    mtwnTargetSlot* target = mtwnTargetAt(context, index1);
    target->header.flags &= (uint8_t)~MTWN_TARGET_UNWRITTEN;
    if (!SameBits(target->value, value))
    {
        target->value = value;
        mtwnMarkChanged(context, index1);
    }
    target->velocity = velocity;
}

static void Advance(mtwnContext* context, uint32_t index1, uint64_t timeNs)
{
    mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
    uint32_t target = slot->header.link;
    if (!slot->started)
    {
        slot->started = true;
        slot->startNs = timeNs;
    }
    double t = (double)(timeNs - slot->startNs) / 1e9 - slot->delay;
    double begin = (double)slot->startNs / 1e9 + slot->delay;
    if (t >= 0.0 && !slot->begun)
    {
        // Its delay has ended: its start, at that time.
        slot->begun = true;
        mtwnPlayerEvent(context, index1, mtwn_eventStart, begin, 0.0);
    }
    if (t >= slot->endTime)
    {
        mtwnPlayerEvent(context, index1, mtwn_eventEnd, begin + slot->endTime, slot->endTime);
        // The end: committed as the base, and the transition freed.
        mtwnTargetSlot* written = mtwnTargetAt(context, target);
        written->base = slot->to;
        mtwnWriteTarget(context, target, slot->to, (mtwnVec4){0});
        mtwnMarkChanged(context, target);
        mtwnEndTransition(context, index1);
        return;
    }
    if (t <= 0.0)
    {
        mtwnWriteTarget(context, target, slot->from, (mtwnVec4){0});
        return;
    }
    double now[4];
    double speed[4];
    if (!Sample(slot, t, now, speed))
    {
        // The change over the curve's last millisecond, from its start
        // before that; none for a discrete value.
        double earlier = t > s_velocitySpan ? t - s_velocitySpan : 0.0;
        double before[4];
        double unused[4];
        Sample(slot, earlier, before, unused);
        for (int i = 0; i < 4; ++i)
        {
            speed[i] =
                slot->kind == mtwn_valueDiscrete ? 0.0 : (now[i] - before[i]) / (t - earlier);
        }
    }
    // An integer's value rounds to nearest, ties away from zero; its
    // velocity is the unrounded curve's.
    bool integer = slot->kind == mtwn_valueInteger;
    float value[4];
    float velocity[4];
    for (int i = 0; i < 4; ++i)
    {
        value[i] = (float)(integer && i == 0 ? round(now[i]) : now[i]);
        velocity[i] = (float)speed[i];
    }
    mtwnWriteTarget(context, target, (mtwnVec4){value[0], value[1], value[2], value[3]},
                    (mtwnVec4){velocity[0], velocity[1], velocity[2], velocity[3]});
}

static MTWN_NOINLINE void UpdateWorker(mtwnContext* context, uint64_t beforeNs, uint64_t timeNs)
{
    double now = (double)timeNs / 1e9;
    double before = (double)beforeNs / 1e9;
    uint32_t queued = context->eventCount;
    for (uint32_t index1 = context->firstPlayer; index1 != 0;)
    {
        mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
        if (slot->type == MTWN_PLAYER_CLIP)
        {
            mtwnMarkUnwritten(context, &slot->play);
        }
        index1 = slot->later;
    }
    // In creation order, the composite order; a player may free itself.
    for (uint32_t index1 = context->firstPlayer; index1 != 0;)
    {
        mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
        uint32_t later = slot->later;
        if (slot->type == MTWN_PLAYER_TRANSITION)
        {
            Advance(context, index1, timeNs);
        }
        else if (mtwnAdvancePlayer(context, index1, before, now))
        {
            mtwnResetUnwritten(context, &slot->play);
            mtwnFreePlayer(context, index1);
        }
        index1 = later;
    }
    mtwnSortEvents(context, queued);
    // A target no clip's player wrote shows its base.
    for (uint32_t index1 = context->firstPlayer; index1 != 0;)
    {
        mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
        if (slot->type == MTWN_PLAYER_CLIP)
        {
            mtwnResetUnwritten(context, &slot->play);
        }
        index1 = slot->later;
    }
}

mtwnResult mtwnUpdate(mtwnContext* context, uint64_t timeNs, mtwnTargetId* changedOut,
                      uint32_t changedCapacity, uint32_t* changedTotalOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    // The first update may come at any time: none is below 0.
    if ((changedOut == NULL && changedCapacity != 0) || timeNs < context->lastTimeNs)
    {
        ++context->misuse;
        return mtwn_errorInvalid;
    }
    uint64_t beforeNs = context->lastTimeNs;
    context->lastTimeNs = timeNs;
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    UpdateWorker(context, beforeNs, timeNs);
    mtwnFloatEnvLeave(&env);
    for (uint32_t i = 0; i < context->changedCount; ++i)
    {
        uint32_t index1 = context->changed[i];
        mtwnTargetSlot* target = mtwnTargetAt(context, index1);
        target->header.flags &= (uint8_t)~MTWN_TARGET_CHANGED;
        if (i < changedCapacity)
        {
            changedOut[i] = (mtwnTargetId){index1, target->header.generation};
        }
    }
    if (changedTotalOut != NULL)
    {
        *changedTotalOut = context->changedCount;
    }
    context->changedCount = 0;
    return mtwn_success;
}
