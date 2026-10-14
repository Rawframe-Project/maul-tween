// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Clips (records mtwn-0004, mtwn-0013 and mtwn-0033): a block of storage
// per clip, sized by its def and counted against the context's clip
// storage; tracks made in it as they are added, under the library's
// floating-point environment, their positions resolved to start times.

#include "allocator.h"
#include "context_internal.h"
#include "fpenv.h"

#include <math.h>
#include <stdckdint.h>
#include <string.h>

static const size_t s_blockAlignment = alignof(max_align_t);

// A mark or a label: a time and a host key, for the timelines' slice.
typedef struct mtwnClipInstant
{
    double time;
    uint32_t key;
    uint32_t parent;
} mtwnClipInstant;

// Where each part of a clip's block starts, and its size.
typedef struct ClipLayout
{
    uint64_t nodes;
    uint64_t keyframes;
    uint64_t lengths;
    uint64_t mixes;
    uint64_t slots;
    uint64_t bytes;
} ClipLayout;

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

static bool PlanClip(const mtwnClipDef* def, ClipLayout* layout)
{
    uint64_t end = 0;
    uint64_t marks = 0;
    uint64_t instants = (uint64_t)def->markCapacity + def->labelCapacity;
    bool placed =
        Place(&end, (uint64_t)def->nodeCapacity + 1, sizeof(mtwnClipNode), &layout->nodes) &&
        Place(&end, def->keyframeCapacity, sizeof(mtwnKeyframe), &layout->keyframes) &&
        Place(&end, def->keyframeCapacity, sizeof(double), &layout->lengths) &&
        Place(&end, def->keyframeCapacity, sizeof(mtwnSegmentMix), &layout->mixes) &&
        Place(&end, def->slotCapacity, sizeof(uint8_t), &layout->slots) &&
        Place(&end, instants, sizeof(mtwnClipInstant), &marks);
    layout->bytes = end;
    return placed && end <= SIZE_MAX;
}

static bool IsCount(uint32_t count)
{
    return count <= MTWN_MAX_COUNT;
}

static mtwnClipSlot* FindClip(const mtwnContext* context, mtwnClipId clip)
{
    return (mtwnClipSlot*)mtwnPoolFind(&context->clips, clip.index1, clip.generation);
}

// Counts a refusal against a live context as misuse.
static mtwnResult Misuse(mtwnContext* context, mtwnResult result)
{
    ++context->misuse;
    return result;
}

mtwnResult mtwnCreateClip(mtwnContext* context, const mtwnClipDef* def, mtwnClipId* clipOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (clipOut != NULL)
    {
        *clipOut = (mtwnClipId){0, 0};
    }
    if (def == NULL || clipOut == NULL || def->cookie != MTWN_CLIP_DEF_COOKIE)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    if (def->version != MTWN_ABI_VERSION)
    {
        return Misuse(context, mtwn_errorVersion);
    }
    ClipLayout layout = {0};
    if (!IsCount(def->nodeCapacity) || !IsCount(def->keyframeCapacity) ||
        !IsCount(def->slotCapacity) || !IsCount(def->markCapacity) || !IsCount(def->labelCapacity))
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    uint64_t used = 0;
    if (!PlanClip(def, &layout) || ckd_add(&used, context->clipStorageUsed, layout.bytes) ||
        used > context->def.clipStorageBytes)
    {
        return mtwn_errorLimit;
    }
    uint32_t index1 = mtwnPoolAcquire(&context->clips);
    if (index1 == 0)
    {
        return mtwn_errorLimit;
    }
    unsigned char* block =
        mtwnAllocate(&context->allocator, (size_t)layout.bytes, s_blockAlignment);
    // The root always takes room, so the block is never empty.
    if (block == NULL)
    {
        mtwnPoolRelease(&context->clips, index1);
        return mtwn_errorCapacity;
    }
    memset(block, 0, (size_t)layout.bytes);
    mtwnClipSlot* slot = mtwnClipAt(context, index1);
    slot->def = *def;
    slot->block = block;
    slot->blockBytes = layout.bytes;
    slot->nodes = (mtwnClipNode*)(block + layout.nodes);
    slot->keyframes = (mtwnKeyframe*)(block + layout.keyframes);
    slot->lengths = (double*)(block + layout.lengths);
    slot->mixes = (mtwnSegmentMix*)(block + layout.mixes);
    slot->slotKinds = block + layout.slots;
    slot->nodes[0].kind = MTWN_NODE_TIMELINE;
    slot->nodeCount = 1;
    context->clipStorageUsed = used;
    *clipOut = (mtwnClipId){index1, slot->header.generation};
    return mtwn_success;
}

mtwnResult mtwnDestroyClip(mtwnContext* context, mtwnClipId clip)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnClipSlot* slot = FindClip(context, clip);
    if (slot == NULL)
    {
        return Misuse(context, mtwn_errorStale);
    }
    if (slot->users != 0)
    {
        return Misuse(context, mtwn_errorBusy);
    }
    mtwnRelease(&context->allocator, slot->block, (size_t)slot->blockBytes, s_blockAlignment);
    context->clipStorageUsed -= slot->blockBytes;
    mtwnPoolRelease(&context->clips, clip.index1);
    return mtwn_success;
}

bool mtwnClip_IsValid(const mtwnContext* context, mtwnClipId clip)
{
    return context != NULL && FindClip(context, clip) != NULL;
}

// The start a position gives in a timeline.
static double StartOf(const mtwnClipSlot* clip, const mtwnClipNode* parent, mtwnPosition position)
{
    const mtwnClipNode* previous =
        parent->lastChild != 0 ? &clip->nodes[parent->lastChild - 1] : NULL;
    switch (position.kind)
    {
    case mtwn_positionAfterEnd:
        return parent->childrenEnd + position.offset;
    case mtwn_positionWithPrevious:
        return (previous != NULL ? previous->start : 0.0) + position.offset;
    case mtwn_positionAfterPrevious:
        return (previous != NULL ? previous->end : 0.0) + position.offset;
    default:
        return position.offset;
    }
}

// What the def asks of the clip, before anything in the clip changes.
static mtwnResult CheckTrack(const mtwnClipSlot* clip, const mtwnClipTrackDef* def)
{
    bool parent = def->parent >= 1 && def->parent <= clip->nodeCount &&
                  clip->nodes[def->parent - 1].kind == MTWN_NODE_TIMELINE;
    bool position =
        def->position.kind <= mtwn_positionAfterPrevious && isfinite(def->position.offset);
    bool slot = def->slot < clip->def.slotCapacity && def->track.kind <= mtwn_valueDiscrete;
    if (slot)
    {
        uint8_t kind = clip->slotKinds[def->slot];
        slot = kind == 0 || kind == def->track.kind + 1;
    }
    if (!parent || !position || !slot || def->keyframes == NULL ||
        mtwnValidateTiming(&def->timing) != mtwn_success)
    {
        return mtwn_errorInvalid;
    }
    if (clip->nodeCount > clip->def.nodeCapacity ||
        def->keyframeCount > clip->def.keyframeCapacity - clip->keyframeCount)
    {
        return mtwn_errorLimit;
    }
    return mtwn_success;
}

static MTWN_NOINLINE mtwnResult AddTrackWorker(mtwnClipSlot* clip, const mtwnClipTrackDef* def,
                                               mtwnNode* nodeOut)
{
    mtwnResult checked = CheckTrack(clip, def);
    if (checked != mtwn_success)
    {
        return checked;
    }
    uint32_t base = clip->keyframeCount;
    mtwnKeyframe* keyframes = &clip->keyframes[base];
    memcpy(keyframes, def->keyframes, (size_t)def->keyframeCount * sizeof(mtwnKeyframe));
    mtwnClipNode node = {0};
    mtwnResult made = mtwnMakeTrack(&def->track, keyframes, def->keyframeCount,
                                    def->constantSpeed ? &clip->lengths[base] : NULL,
                                    &clip->mixes[base], &node.track);
    if (made != mtwn_success)
    {
        return made;
    }
    mtwnClipNode* parent = &clip->nodes[def->parent - 1];
    mtwnTimingResult timing;
    mtwnComputeTiming(&def->timing, 0.0, false, &timing);
    node.timing = def->timing;
    node.start = StartOf(clip, parent, def->position);
    node.end = node.start + timing.endTime;
    node.parent = def->parent;
    node.slot = def->slot;
    node.kind = MTWN_NODE_TRACK;
    uint32_t index1 = ++clip->nodeCount;
    clip->nodes[index1 - 1] = node;
    clip->keyframeCount += def->keyframeCount;
    clip->slotKinds[def->slot] = (uint8_t)(def->track.kind + 1);
    parent->childrenEnd = node.end > parent->childrenEnd ? node.end : parent->childrenEnd;
    parent->lastChild = index1;
    *nodeOut = index1;
    return mtwn_success;
}

mtwnResult mtwnClip_AddTrack(mtwnContext* context, mtwnClipId clip, const mtwnClipTrackDef* def,
                             mtwnNode* nodeOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (nodeOut != NULL)
    {
        *nodeOut = 0;
    }
    mtwnClipSlot* slot = FindClip(context, clip);
    if (slot == NULL)
    {
        return Misuse(context, mtwn_errorStale);
    }
    if (def == NULL || nodeOut == NULL || def->cookie != MTWN_CLIP_TRACK_DEF_COOKIE)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    if (def->version != MTWN_ABI_VERSION)
    {
        return Misuse(context, mtwn_errorVersion);
    }
    if (slot->finished)
    {
        return Misuse(context, mtwn_errorState);
    }
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    mtwnResult result = AddTrackWorker(slot, def, nodeOut);
    mtwnFloatEnvLeave(&env);
    return result == mtwn_errorInvalid ? Misuse(context, result) : result;
}

mtwnResult mtwnClip_Finish(mtwnContext* context, mtwnClipId clip)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnClipSlot* slot = FindClip(context, clip);
    if (slot == NULL)
    {
        return Misuse(context, mtwn_errorStale);
    }
    if (slot->finished)
    {
        return Misuse(context, mtwn_errorState);
    }
    slot->finished = true;
    return mtwn_success;
}

// The finished clip an id names, with the refusal counted.
static mtwnResult Finished(mtwnContext* context, mtwnClipId clip, mtwnClipSlot** slotOut)
{
    *slotOut = FindClip(context, clip);
    if (*slotOut == NULL)
    {
        return Misuse(context, mtwn_errorStale);
    }
    if (!(*slotOut)->finished)
    {
        return Misuse(context, mtwn_errorState);
    }
    return mtwn_success;
}

mtwnResult mtwnClip_GetTrack(mtwnContext* context, mtwnClipId clip, mtwnNode node,
                             mtwnClipTrack* trackOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (trackOut == NULL)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    memset(trackOut, 0, sizeof *trackOut);
    mtwnClipSlot* slot = NULL;
    mtwnResult result = Finished(context, clip, &slot);
    if (result != mtwn_success)
    {
        return result;
    }
    if (node < 1 || node > slot->nodeCount || slot->nodes[node - 1].kind != MTWN_NODE_TRACK)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    const mtwnClipNode* n = &slot->nodes[node - 1];
    *trackOut = (mtwnClipTrack){n->track, n->timing, n->start, n->parent, n->slot};
    return mtwn_success;
}

mtwnResult mtwnClip_GetDuration(mtwnContext* context, mtwnClipId clip, double* secondsOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (secondsOut == NULL)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    *secondsOut = 0.0;
    mtwnClipSlot* slot = NULL;
    mtwnResult result = Finished(context, clip, &slot);
    if (result == mtwn_success)
    {
        *secondsOut = slot->nodes[0].childrenEnd;
    }
    return result;
}
