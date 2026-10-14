// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The update (records mtwn-0004, mtwn-0007 and mtwn-0029): every player
// advanced to the host's time in the library's floating-point
// environment, then the changed targets reported. A transition samples
// its own curve, with the velocity of its last millisecond, and at its
// end commits its end value as the target's base.

#include "context_internal.h"
#include "fpenv.h"

#include <math.h>
#include <string.h>

// The span the velocity is measured over, as Compose measures a tween's.
static const double s_velocitySpan = 0.001;

// The transition's value at t seconds into it, before rounding.
static void Sample(const mtwnPlayerSlot* slot, double t, double out[4])
{
    double eased = mtwnEase(&slot->easing, t / slot->duration, false);
    const float* from = &slot->from.x;
    const float* to = &slot->to.x;
    out[0] = out[1] = out[2] = out[3] = 0.0;
    switch (slot->kind)
    {
    case mtwn_valueColor:
    {
        mtwnColor color = mtwnMixColor(&slot->mix.color, eased);
        out[0] = (double)color.red;
        out[1] = (double)color.green;
        out[2] = (double)color.blue;
        out[3] = (double)color.alpha;
        return;
    }
    case mtwn_valueRotation:
    {
        mtwnQuat q = mtwnMixRotation(&slot->mix.rotation, eased);
        out[0] = (double)q.x;
        out[1] = (double)q.y;
        out[2] = (double)q.z;
        out[3] = (double)q.w;
        return;
    }
    case mtwn_valueAngle:
        out[0] = mtwnMixAngle((double)from[0], (double)to[0], slot->angleMethod, eased);
        return;
    case mtwn_valueDiscrete:
        out[0] = (double)(eased < 0.5 ? from[0] : to[0]);
        return;
    default:
        for (uint8_t i = 0; i < mtwnKindChannels(slot->kind); ++i)
        {
            out[i] = (1.0 - eased) * (double)from[i] + eased * (double)to[i];
        }
        return;
    }
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

// Writes a target's value and velocity, marking it changed when its value
// changed.
static void Write(mtwnContext* context, uint32_t index1, mtwnVec4 value, mtwnVec4 velocity)
{
    mtwnTargetSlot* target = mtwnTargetAt(context, index1);
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
    if (t >= slot->duration)
    {
        // The end: committed as the base, and the transition freed.
        mtwnTargetSlot* written = mtwnTargetAt(context, target);
        written->base = slot->to;
        Write(context, target, slot->to, (mtwnVec4){0});
        mtwnMarkChanged(context, target);
        mtwnEndTransition(context, index1);
        return;
    }
    if (t <= 0.0)
    {
        Write(context, target, slot->from, (mtwnVec4){0});
        return;
    }
    double now[4];
    double before[4];
    double earlier = t > s_velocitySpan ? t - s_velocitySpan : 0.0;
    Sample(slot, t, now);
    Sample(slot, earlier, before);
    // The velocity is the unrounded curve's; an integer's value rounds to
    // nearest, ties away from zero.
    double span = t - earlier;
    bool integer = slot->kind == mtwn_valueInteger;
    float speed[4];
    float value[4];
    for (int i = 0; i < 4; ++i)
    {
        value[i] = (float)(integer && i == 0 ? round(now[i]) : now[i]);
        speed[i] = slot->kind == mtwn_valueDiscrete ? 0.0f : (float)((now[i] - before[i]) / span);
    }
    Write(context, target, (mtwnVec4){value[0], value[1], value[2], value[3]},
          (mtwnVec4){speed[0], speed[1], speed[2], speed[3]});
}

static MTWN_NOINLINE void UpdateWorker(mtwnContext* context, uint64_t timeNs)
{
    for (uint32_t index1 = 1; index1 <= context->players.high; ++index1)
    {
        if ((mtwnPoolAt(&context->players, index1)->flags & MTWN_SLOT_ALIVE) != 0)
        {
            Advance(context, index1, timeNs);
        }
    }
}

mtwnResult mtwnUpdate(mtwnContext* context, uint64_t timeNs, mtwnTargetId* changedOut,
                      uint32_t changedCapacity, uint32_t* changedTotalOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if ((changedOut == NULL && changedCapacity != 0) ||
        (context->updated && timeNs < context->lastTimeNs))
    {
        ++context->misuse;
        return mtwn_errorInvalid;
    }
    context->updated = true;
    context->lastTimeNs = timeNs;
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    UpdateWorker(context, timeNs);
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
