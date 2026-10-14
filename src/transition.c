// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Transitions in the player pool (records mtwn-0004 and mtwn-0029): a
// start from the target's current value, the kind's mix made at once,
// and the transition it replaces ended. The mixes, a spring's settling
// time and a decay's stop compute, so starting runs in the library's
// floating-point environment (records mtwn-0029 and mtwn-0030).

#include "maul-tween/transition.h"

#include "context_internal.h"
#include "fpenv.h"
#include "spring_internal.h"

#include <math.h>

static mtwnColor ToColor(mtwnVec4 value)
{
    return (mtwnColor){value.x, value.y, value.z, value.w};
}

static mtwnQuat ToQuat(mtwnVec4 value)
{
    return (mtwnQuat){value.x, value.y, value.z, value.w};
}

static bool IsTime(double seconds)
{
    return isfinite(seconds) && seconds >= 0.0;
}

static bool IsTolerance(double value)
{
    return isfinite(value) && value > 0.0;
}

// Whether the def's spec suits the kind and its spring, decay and
// tolerances are valid.
static bool IsSpec(const mtwnTransitionDef* def, mtwnValueKind kind)
{
    switch (def->spec)
    {
    case mtwn_transitionEasing:
        return true;
    case mtwn_transitionSpring:
        return kind != mtwn_valueDiscrete && mtwnIsSpring(&def->spring) &&
               IsTolerance(def->restOffset) && IsTolerance(def->restSpeed);
    case mtwn_transitionDecay:
        return kind != mtwn_valueDiscrete && !mtwnIsPathKind(kind) &&
               IsTolerance(def->decay.rate) && IsTolerance(def->restSpeed);
    default:
        return false;
    }
}

// The start velocity per channel, or for a path, in progress: the channel
// velocity projected on the end minus the start.
static bool StartVelocity(const mtwnTransitionDef* def, const mtwnTargetSlot* target,
                          mtwnPlayerSlot* out)
{
    bool given = def->velocitySource == mtwn_velocityGiven;
    const float* v = given ? &def->velocity.x : &target->velocity.x;
    uint8_t channels = mtwnKindChannels(target->kind);
    for (uint8_t i = 0; i < channels; ++i)
    {
        if (!isfinite(v[i]))
        {
            return false;
        }
        out->velocity[i] = (double)v[i];
    }
    if (mtwnIsPathKind(target->kind))
    {
        const float* from = &out->from.x;
        const float* to = &out->to.x;
        double along = 0.0;
        double length = 0.0;
        for (uint8_t i = 0; i < channels; ++i)
        {
            double d = (double)to[i] - (double)from[i];
            along += out->velocity[i] * d;
            length += d * d;
        }
        out->velocity[0] = length > 0.0 ? along / length : 0.0;
        out->velocity[1] = out->velocity[2] = out->velocity[3] = 0.0;
    }
    return true;
}

// The time after the delay a spring or decay ends, and a decay's end.
static void EndOf(const mtwnTransitionDef* def, mtwnPlayerSlot* out)
{
    out->endTime = out->duration;
    if (def->spec == mtwn_transitionEasing)
    {
        return;
    }
    uint8_t channels = mtwnKindChannels(out->kind);
    if (def->spec == mtwn_transitionSpring && mtwnIsPathKind(out->kind))
    {
        out->endTime = mtwnSpringSettlingTime(&out->spring, -1.0, out->velocity[0], def->restOffset,
                                              def->restSpeed);
        return;
    }
    double end = 0.0;
    float* to = &out->to.x;
    const float* from = &out->from.x;
    for (uint8_t i = 0; i < channels; ++i)
    {
        double time = 0.0;
        if (def->spec == mtwn_transitionSpring)
        {
            time = mtwnSpringSettlingTime(&out->spring, (double)from[i] - (double)to[i],
                                          out->velocity[i], def->restOffset, def->restSpeed);
        }
        else
        {
            time = mtwnDecayStopTime(&out->decay, out->velocity[i], def->restSpeed);
        }
        end = time > end ? time : end;
    }
    out->endTime = end;
    if (def->spec == mtwn_transitionDecay)
    {
        for (uint8_t i = 0; i < channels; ++i)
        {
            double position = 0.0;
            mtwnSampleDecay(&out->decay, (double)from[i], out->velocity[i], end, &position, NULL);
            to[i] = (float)(out->kind == mtwn_valueInteger ? round(position) : position);
        }
    }
}

// The transition as the def asks it, before it takes a slot.
static mtwnResult Prepare(const mtwnTransitionDef* def, const mtwnTargetSlot* target,
                          mtwnPlayerSlot* out)
{
    out->kind = target->kind;
    out->spec = def->spec;
    out->from = target->value;
    mtwnVec4 end = def->spec == mtwn_transitionDecay ? target->value : def->value;
    if (!mtwnAcceptValue(target->kind, end, &out->to) || !IsTime(def->delay) ||
        !IsTime(def->duration) || !IsSpec(def, target->kind) ||
        def->velocitySource > mtwn_velocityGiven || !StartVelocity(def, target, out))
    {
        return mtwn_errorInvalid;
    }
    out->delay = def->delay;
    out->duration = def->duration;
    out->easing = def->easing;
    out->spring = def->spring;
    out->decay = def->decay;
    // A decay's end, computed after, replaces this.
    if (target->kind == mtwn_valueAngle)
    {
        // The end moved by the method's turns, once.
        out->to.x =
            (float)mtwnMixAngle((double)out->from.x, (double)out->to.x, def->angleMethod, 1.0);
    }
    EndOf(def, out);
    switch (target->kind)
    {
    case mtwn_valueColor:
        return mtwnMakeColorMix(ToColor(out->from), ToColor(out->to), def->colorSpace,
                                def->hueMethod, &out->mix.color);
    case mtwn_valueRotation:
        return mtwnMakeRotationMix(ToQuat(out->from), ToQuat(out->to), def->rotationPath,
                                   &out->mix.rotation);
    default:
        return mtwn_success;
    }
}

static MTWN_NOINLINE mtwnResult StartWorker(mtwnContext* context, const mtwnTransitionDef* def,
                                            mtwnPlayerId* playerOut)
{
    mtwnTargetSlot* target = (mtwnTargetSlot*)mtwnPoolFind(&context->targets, def->target.index1,
                                                           def->target.generation);
    if (target == NULL)
    {
        ++context->misuse;
        return mtwn_errorStale;
    }
    mtwnPlayerSlot prepared = {0};
    mtwnResult result = Prepare(def, target, &prepared);
    if (result != mtwn_success)
    {
        ++context->misuse;
        return result;
    }
    // The transition it replaces frees its slot first.
    if (target->header.link != 0)
    {
        mtwnPlayerSlot* replaced = mtwnPlayerAt(context, target->header.link);
        double now = (double)context->lastTimeNs / 1e9;
        double since =
            replaced->started ? now - (double)replaced->startNs / 1e9 - replaced->delay : 0.0;
        mtwnPlayerEvent(context, target->header.link, mtwn_eventCancel, now, since);
        mtwnEndTransition(context, target->header.link);
    }
    uint32_t index1 = mtwnTakePlayer(context, MTWN_PLAYER_TRANSITION);
    if (index1 == 0)
    {
        return mtwn_errorLimit;
    }
    mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
    prepared.header = slot->header;
    prepared.type = slot->type;

    prepared.earlier = slot->earlier;
    prepared.later = slot->later;
    prepared.header.link = def->target.index1;
    prepared.target = def->target;
    *slot = prepared;
    target->header.link = index1;
    *playerOut = (mtwnPlayerId){index1, slot->header.generation};
    return mtwn_success;
}

mtwnResult mtwnStartTransition(mtwnContext* context, const mtwnTransitionDef* def,
                               mtwnPlayerId* playerOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (playerOut != NULL)
    {
        *playerOut = (mtwnPlayerId){0, 0};
    }
    if (def == NULL || playerOut == NULL || def->cookie != MTWN_TRANSITION_DEF_COOKIE)
    {
        ++context->misuse;
        return mtwn_errorInvalid;
    }
    if (def->version != MTWN_ABI_VERSION)
    {
        ++context->misuse;
        return mtwn_errorVersion;
    }
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    mtwnResult result = StartWorker(context, def, playerOut);
    mtwnFloatEnvLeave(&env);
    return result;
}

void mtwnEndTransition(mtwnContext* context, uint32_t player)
{
    uint32_t target = mtwnPlayerAt(context, player)->header.link;
    mtwnTargetAt(context, target)->header.link = 0;
    mtwnFreePlayer(context, player);
}
