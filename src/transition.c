// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Transitions in the player pool (records mtwn-0004 and mtwn-0029): a
// start from the target's current value, the kind's mix made at once,
// and the transition it replaces ended. The mixes compute, so starting
// runs in the library's floating-point environment.

#include "maul-tween/transition.h"

#include "context_internal.h"
#include "fpenv.h"

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

// The transition as the def asks it, before it takes a slot.
static mtwnResult Prepare(const mtwnTransitionDef* def, const mtwnTargetSlot* target,
                          mtwnPlayerSlot* out)
{
    out->kind = target->kind;
    out->from = target->value;
    if (!mtwnAcceptValue(target->kind, def->value, &out->to) || !IsTime(def->delay) ||
        !IsTime(def->duration))
    {
        return mtwn_errorInvalid;
    }
    out->delay = def->delay;
    out->duration = def->duration;
    out->easing = def->easing;
    out->angleMethod = def->angleMethod;
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
        mtwnEndTransition(context, target->header.link);
    }
    uint32_t index1 = mtwnPoolAcquire(&context->players);
    if (index1 == 0)
    {
        return mtwn_errorLimit;
    }
    mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
    prepared.header = slot->header;
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
    mtwnPoolRelease(&context->players, player);
}

mtwnResult mtwnDestroyPlayer(mtwnContext* context, mtwnPlayerId player)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnPlayerSlot* slot =
        (mtwnPlayerSlot*)mtwnPoolFind(&context->players, player.index1, player.generation);
    if (slot == NULL)
    {
        ++context->misuse;
        return mtwn_errorStale;
    }
    uint32_t index1 = slot->header.link;
    mtwnEndTransition(context, player.index1);
    // The target shows its base again.
    mtwnTargetSlot* target = mtwnTargetAt(context, index1);
    target->value = target->base;
    target->velocity = (mtwnVec4){0};
    mtwnMarkChanged(context, index1);
    return mtwn_success;
}

bool mtwnPlayer_IsValid(const mtwnContext* context, mtwnPlayerId player)
{
    return context != NULL &&
           mtwnPoolFind(&context->players, player.index1, player.generation) != NULL;
}
