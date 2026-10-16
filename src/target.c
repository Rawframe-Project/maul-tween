// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Targets in the context's pool (records mtwn-0004, mtwn-0028 and
// mtwn-0029). Values are stored as given and
// checked by their bits, so these calls do no floating-point arithmetic
// and give the same result in any floating-point environment.

#include "context_internal.h"

#include <string.h>

static bool IsKind(mtwnValueKind kind)
{
    return kind <= mtwn_valueDiscrete;
}

static uint32_t Bits(float value)
{
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

// Finite: the exponent's bits are not all ones.
static bool IsFiniteBits(float value)
{
    return (Bits(value) & 0x7F800000u) != 0x7F800000u;
}

// Zero of either sign.
static bool IsZeroBits(float value)
{
    return (Bits(value) & 0x7FFFFFFFu) == 0;
}

bool mtwnAcceptValue(mtwnValueKind kind, mtwnVec4 value, mtwnVec4* out)
{
    float channels[4] = {value.x, value.y, value.z, value.w};
    uint8_t used = mtwnKindChannels(kind);
    bool zero = true;
    for (uint8_t i = 0; i < 4; ++i)
    {
        if (i >= used)
        {
            channels[i] = 0.0f;
            continue;
        }
        if (!IsFiniteBits(channels[i]))
        {
            return false;
        }
        zero = zero && IsZeroBits(channels[i]);
    }
    if (kind == mtwn_valueRotation && zero)
    {
        return false;
    }
    *out = (mtwnVec4){channels[0], channels[1], channels[2], channels[3]};
    return true;
}

// The live slot an id names, or NULL.
static mtwnTargetSlot* Find(const mtwnContext* context, mtwnTargetId target)
{
    return (mtwnTargetSlot*)mtwnPoolFind(&context->targets, target.index1, target.generation);
}

mtwnResult mtwnCreateTarget(mtwnContext* context, mtwnValueKind kind, mtwnVec4 value,
                            mtwnTargetId* targetOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnVec4 accepted = {0};
    if (targetOut == NULL || !IsKind(kind) || !mtwnAcceptValue(kind, value, &accepted))
    {
        if (targetOut != NULL)
        {
            *targetOut = (mtwnTargetId){0, 0};
        }
        ++context->misuse;
        return mtwn_errorInvalid;
    }
    *targetOut = (mtwnTargetId){0, 0};
    uint32_t index1 = mtwnPoolAcquire(&context->targets);
    if (index1 == 0)
    {
        return mtwn_errorLimit;
    }
    mtwnTargetSlot* slot = mtwnTargetAt(context, index1);
    slot->base = accepted;
    slot->value = accepted;
    slot->kind = kind;
    *targetOut = (mtwnTargetId){index1, slot->header.generation};
    return mtwn_success;
}

mtwnResult mtwnDestroyTarget(mtwnContext* context, mtwnTargetId target)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnTargetSlot* slot = Find(context, target);
    if (slot == NULL)
    {
        ++context->misuse;
        return mtwn_errorStale;
    }
    if (slot->header.link != 0)
    {
        mtwnEndTransition(context, slot->header.link);
    }
    // A target on the list of changed targets leaves it.
    if ((slot->header.flags & MTWN_TARGET_CHANGED) != 0)
    {
        uint32_t kept = 0;
        for (uint32_t i = 0; i < context->changedCount; ++i)
        {
            if (context->changed[i] != target.index1)
            {
                context->changed[kept++] = context->changed[i];
            }
        }
        context->changedCount = kept;
    }
    mtwnWritersChanged(context, target.index1);
    mtwnPoolRelease(&context->targets, target.index1);
    return mtwn_success;
}

bool mtwnTarget_IsValid(const mtwnContext* context, mtwnTargetId target)
{
    return context != NULL && Find(context, target) != NULL;
}

// The live slot an id names; a stale id counts as misuse.
static mtwnResult Resolve(mtwnContext* context, mtwnTargetId target, mtwnTargetSlot** slotOut)
{
    *slotOut = Find(context, target);
    if (*slotOut == NULL)
    {
        ++context->misuse;
        return mtwn_errorStale;
    }
    return mtwn_success;
}

mtwnResult mtwnTarget_SetBase(mtwnContext* context, mtwnTargetId target, mtwnVec4 value)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnTargetSlot* slot = NULL;
    mtwnResult resolved = Resolve(context, target, &slot);
    if (resolved != mtwn_success)
    {
        return resolved;
    }
    mtwnVec4 accepted = {0};
    if (!mtwnAcceptValue(slot->kind, value, &accepted))
    {
        ++context->misuse;
        return mtwn_errorInvalid;
    }
    slot->base = accepted;
    // A target no transition writes shows its base at once; a clip track's
    // velocity left lazy is the old value's, so it is kept first.
    if (slot->header.link == 0)
    {
        mtwnSettleVelocity(context, target.index1);
        slot->value = accepted;
    }
    mtwnMarkChanged(context, target.index1);
    return mtwn_success;
}

mtwnResult mtwnTarget_GetValue(mtwnContext* context, mtwnTargetId target, mtwnVec4* valueOut)
{
    if (context == NULL || valueOut == NULL)
    {
        if (context != NULL)
        {
            ++context->misuse;
        }
        return mtwn_errorInvalid;
    }
    *valueOut = (mtwnVec4){0};
    mtwnTargetSlot* slot = NULL;
    mtwnResult resolved = Resolve(context, target, &slot);
    if (resolved == mtwn_success)
    {
        *valueOut = slot->value;
    }
    return resolved;
}

mtwnResult mtwnTarget_GetState(mtwnContext* context, mtwnTargetId target, mtwnTargetState* stateOut)
{
    if (context == NULL || stateOut == NULL)
    {
        if (context != NULL)
        {
            ++context->misuse;
        }
        return mtwn_errorInvalid;
    }
    *stateOut = (mtwnTargetState){0};
    mtwnTargetSlot* slot = NULL;
    mtwnResult resolved = Resolve(context, target, &slot);
    if (resolved == mtwn_success)
    {
        stateOut->base = slot->base;
        stateOut->value = slot->value;
        stateOut->velocity = mtwnTargetVelocity(context, target.index1);
        stateOut->kind = slot->kind;
    }
    return resolved;
}

mtwnResult mtwnReadTargets(mtwnContext* context, const mtwnTargetId* targets, uint32_t count,
                           mtwnVec4* valuesOut)
{
    if (context == NULL || (count != 0 && (targets == NULL || valuesOut == NULL)))
    {
        if (context != NULL)
        {
            ++context->misuse;
        }
        return mtwn_errorInvalid;
    }
    mtwnResult result = mtwn_success;
    for (uint32_t i = 0; i < count; ++i)
    {
        const mtwnTargetSlot* slot = Find(context, targets[i]);
        if (slot == NULL)
        {
            ++context->misuse;
            result = mtwn_errorStale;
            valuesOut[i] = (mtwnVec4){0};
            continue;
        }
        valuesOut[i] = slot->value;
    }
    return result;
}
