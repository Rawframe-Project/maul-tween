// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context: the def checked, the context, its pools and its list of
// changed targets allocated in one block through the host's allocator,
// and freed in one call with its clips' blocks (records mtwn-0006,
// mtwn-0028, mtwn-0029 and mtwn-0033).

#include "allocator.h"
#include "context_internal.h"

#include <stdckdint.h>
#include <string.h>

static_assert(sizeof(mtwnTargetSlot) == 64, "a target slot is 64 bytes");

// The block's alignment: what the C library's malloc serves, and what
// every part of the block starts on.
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
                 IsCount(def->bindingCapacity) && def->clipStorageBytes <= MTWN_MAX_CLIP_STORAGE;
    return valid ? mtwn_success : mtwn_errorInvalid;
}

// Where each part of the block starts, and its size.
typedef struct Layout
{
    uint64_t targets;
    uint64_t players;
    uint64_t clips;
    uint64_t bindings;
    uint64_t events;
    uint64_t eventAlong;
    uint64_t levels;
    uint64_t changed;
    uint64_t bytes;
} Layout;

// Adds count elements of size bytes at the end, aligned; false on
// overflow.
static bool Place(uint64_t* end, uint64_t count, uint64_t size, uint64_t* offsetOut)
{
    uint64_t bytes = 0;
    uint64_t start = 0;
    uint64_t align = s_blockAlignment - 1;
    if (ckd_add(&start, *end, align) || ckd_mul(&bytes, count, size))
    {
        return false;
    }
    start &= ~align;
    *offsetOut = start;
    return !ckd_add(end, start, bytes);
}

static bool PlanBlock(const mtwnContextDef* def, Layout* layout)
{
    uint64_t end = sizeof(mtwnContext);
    bool placed = Place(&end, def->targetCapacity, sizeof(mtwnTargetSlot), &layout->targets) &&
                  Place(&end, def->playerCapacity, sizeof(mtwnPlayerSlot), &layout->players) &&
                  Place(&end, def->clipCapacity, sizeof(mtwnClipSlot), &layout->clips) &&
                  Place(&end, def->bindingCapacity, sizeof(mtwnTargetId), &layout->bindings) &&
                  Place(&end, def->eventCapacity, sizeof(mtwnEvent), &layout->events) &&
                  Place(&end, def->eventCapacity, sizeof(double), &layout->eventAlong) &&
                  Place(&end, MTWN_MAX_CLIP_DEPTH + 2u, sizeof(mtwnLevel), &layout->levels) &&
                  Place(&end, def->targetCapacity, sizeof(uint32_t), &layout->changed);
    layout->bytes = end;
    return placed && end <= SIZE_MAX;
}

static mtwnPool MakePool(unsigned char* slots, uint32_t stride, uint32_t capacity)
{
    return (mtwnPool){slots, stride, capacity, 0, 0, 0};
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
    Layout layout = {0};
    if (!PlanBlock(def, &layout))
    {
        return mtwn_errorCapacity;
    }
    unsigned char* block = mtwnAllocate(&def->allocator, (size_t)layout.bytes, s_blockAlignment);
    if (block == NULL)
    {
        return mtwn_errorCapacity;
    }
    memset(block, 0, (size_t)layout.bytes);
    mtwnContext* context = (mtwnContext*)block;
    context->allocator = def->allocator;
    context->def = *def;
    context->blockBytes = layout.bytes;
    context->targets =
        MakePool(block + layout.targets, sizeof(mtwnTargetSlot), def->targetCapacity);
    context->players =
        MakePool(block + layout.players, sizeof(mtwnPlayerSlot), def->playerCapacity);
    context->clips = MakePool(block + layout.clips, sizeof(mtwnClipSlot), def->clipCapacity);
    context->bindings =
        mtwnMakeRuns((mtwnTargetId*)(block + layout.bindings), def->bindingCapacity);
    context->events = (mtwnEvent*)(block + layout.events);
    context->eventAlong = (double*)(block + layout.eventAlong);
    context->levels = (mtwnLevel*)(block + layout.levels);
    context->changed = (uint32_t*)(block + layout.changed);
    *contextOut = context;
    return mtwn_success;
}

void mtwnDestroyContext(mtwnContext* context)
{
    if (context == NULL)
    {
        return;
    }
    // The clips' blocks first, then the context's; the allocator is read
    // out before the block that holds it is freed.
    mtwnAllocator allocator = context->allocator;
    for (uint32_t index1 = 1; index1 <= context->clips.high; ++index1)
    {
        mtwnClipSlot* clip = mtwnClipAt(context, index1);
        if ((clip->header.flags & MTWN_SLOT_ALIVE) != 0)
        {
            mtwnRelease(&allocator, clip->block, (size_t)clip->blockBytes, s_blockAlignment);
        }
    }
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
