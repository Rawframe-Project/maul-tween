// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context: the def checked, the context and its pools allocated in
// one block through the host's allocator, and freed in one call (records
// mtwn-0006 and mtwn-0028).

#include "allocator.h"
#include "context_internal.h"

#include <stdckdint.h>
#include <string.h>

static_assert(sizeof(mtwnTargetSlot) == 64, "a target slot is 64 bytes");

// The block's alignment: what the C library's malloc serves.
static const size_t s_blockAlignment = alignof(max_align_t);

static bool IsCount(uint32_t count)
{
    return count <= MTWN_MAX_COUNT;
}

static mtwnResult CheckDef(const mtwnContextDef* def)
{
    if (def->cookie != MTWN_CONTEXT_DEF_COOKIE)
    {
        return mtwn_errorInvalid;
    }
    if (def->version != MTWN_ABI_VERSION)
    {
        return mtwn_errorVersion;
    }
    bool oneFunction = (def->allocator.alloc == NULL) != (def->allocator.free == NULL);
    bool valid = !oneFunction && IsCount(def->targetCapacity) && IsCount(def->playerCapacity) &&
                 IsCount(def->clipCapacity) && IsCount(def->eventCapacity) &&
                 def->clipStorageBytes <= MTWN_MAX_CLIP_STORAGE;
    return valid ? mtwn_success : mtwn_errorInvalid;
}

// The block: the context, then its target slots.
static bool BlockBytes(const mtwnContextDef* def, uint64_t* bytesOut)
{
    uint64_t targets = 0;
    uint64_t total = 0;
    bool overflow = ckd_mul(&targets, (uint64_t)def->targetCapacity, sizeof(mtwnTargetSlot)) ||
                    ckd_add(&total, (uint64_t)sizeof(mtwnContext), targets) || total > SIZE_MAX;
    *bytesOut = total;
    return !overflow;
}

mtwnResult mtwnCreateContext(const mtwnContextDef* def, mtwnContext** contextOut)
{
    if (contextOut == NULL)
    {
        return mtwn_errorInvalid;
    }
    *contextOut = NULL;
    if (def == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnResult checked = CheckDef(def);
    if (checked != mtwn_success)
    {
        return checked;
    }
    uint64_t bytes = 0;
    if (!BlockBytes(def, &bytes))
    {
        return mtwn_errorCapacity;
    }
    unsigned char* block = mtwnAllocate(&def->allocator, (size_t)bytes, s_blockAlignment);
    if (block == NULL)
    {
        return mtwn_errorCapacity;
    }
    memset(block, 0, (size_t)bytes);
    mtwnContext* context = (mtwnContext*)block;
    context->allocator = def->allocator;
    context->def = *def;
    context->blockBytes = bytes;
    context->targets = (mtwnTargetSlot*)(block + sizeof(mtwnContext));
    *contextOut = context;
    return mtwn_success;
}

void mtwnDestroyContext(mtwnContext* context)
{
    if (context == NULL)
    {
        return;
    }
    // The allocator is read out before the block that holds it is freed.
    mtwnAllocator allocator = context->allocator;
    mtwnRelease(&allocator, context, (size_t)context->blockBytes, s_blockAlignment);
}

uint64_t mtwnContext_GetMisuseCount(const mtwnContext* context)
{
    return context->misuse;
}

uint64_t mtwnContext_GetMemoryBytes(const mtwnContext* context)
{
    return context->blockBytes;
}
