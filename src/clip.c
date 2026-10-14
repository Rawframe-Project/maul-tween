// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Clips (records mtwn-0004, mtwn-0013, mtwn-0033 and mtwn-0034): a block
// of storage per clip, sized by its def and counted against the context's
// clip storage; nodes added under the library's floating-point
// environment, their positions resolved to start times and their ends
// carried up to the root as they arrive.

#include "allocator.h"
#include "context_internal.h"
#include "fpenv.h"

#include <math.h>
#include <stdckdint.h>
#include <string.h>

static const size_t s_blockAlignment = alignof(max_align_t);

// Where each part of a clip's block starts, and its size.
typedef struct ClipLayout
{
    uint64_t nodes;
    uint64_t keyframes;
    uint64_t lengths;
    uint64_t mixes;
    uint64_t slots;
    uint64_t times;
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
    bool placed =
        Place(&end, (uint64_t)def->nodeCapacity + 1, sizeof(mtwnClipNode), &layout->nodes) &&
        Place(&end, def->keyframeCapacity, sizeof(mtwnKeyframe), &layout->keyframes) &&
        Place(&end, def->keyframeCapacity, sizeof(double), &layout->lengths) &&
        Place(&end, def->keyframeCapacity, sizeof(mtwnSegmentMix), &layout->mixes) &&
        Place(&end, def->slotCapacity, sizeof(uint8_t), &layout->slots) &&
        Place(&end, (uint64_t)def->nodeCapacity + 1, sizeof(mtwnNodeTime), &layout->times);
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
        !IsCount(def->slotCapacity))
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
    slot->times = (mtwnNodeTime*)(block + layout.times);
    slot->nodes[0].kind = mtwn_nodeTimeline;
    slot->nodes[0].autoDuration = true;
    slot->nodes[0].timing = mtwnDefaultTiming();
    // The root is the player's whole clip: it passes the player's time to
    // its children, held at its ends, and their fills decide.
    slot->nodes[0].timing.fill = mtwn_fillBoth;
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

static const mtwnClipNode* NodeAt(const mtwnClipSlot* clip, uint32_t index1)
{
    return &clip->nodes[index1 - 1];
}

static bool IsTimeline(const mtwnClipSlot* clip, uint32_t index1)
{
    return index1 >= 1 && index1 <= clip->nodeCount &&
           NodeAt(clip, index1)->kind == mtwn_nodeTimeline;
}

// The label of a parent with a key, 0 when none.
static uint32_t FindLabelIn(const mtwnClipSlot* clip, uint32_t parent, uint32_t key)
{
    for (uint32_t child = NodeAt(clip, parent)->firstChild; child != 0;
         child = NodeAt(clip, child)->nextSibling)
    {
        const mtwnClipNode* node = NodeAt(clip, child);
        if (node->kind == mtwn_nodeLabel && node->key == key)
        {
            return child;
        }
    }
    return 0;
}

// The start a position gives in a timeline.
static double StartOf(const mtwnClipSlot* clip, uint32_t parent, mtwnPosition position)
{
    const mtwnClipNode* timeline = NodeAt(clip, parent);
    const mtwnClipNode* previous =
        timeline->previous != 0 ? NodeAt(clip, timeline->previous) : NULL;
    switch (position.kind)
    {
    case mtwn_positionAfterEnd:
        return timeline->childrenEnd + position.offset;
    case mtwn_positionWithPrevious:
        return (previous != NULL ? previous->start : 0.0) + position.offset;
    case mtwn_positionAfterPrevious:
        return (previous != NULL ? previous->end : 0.0) + position.offset;
    case mtwn_positionAtLabel:
        return NodeAt(clip, FindLabelIn(clip, parent, position.label))->start + position.offset;
    default:
        return position.offset;
    }
}

// Whether a position names a finite start: after an endless child (whose
// timeline is endless too) there is none.
static bool IsPosition(const mtwnClipSlot* clip, uint32_t parent, mtwnPosition position)
{
    return position.kind <= mtwn_positionAtLabel && isfinite(position.offset) &&
           (position.kind != mtwn_positionAtLabel ||
            FindLabelIn(clip, parent, position.label) != 0) &&
           isfinite(StartOf(clip, parent, position));
}

// A timeline's timing with its duration resolved.
static mtwnTiming ResolvedTiming(const mtwnClipNode* node, double childrenEnd)
{
    mtwnTiming timing = node->timing;
    if (node->autoDuration)
    {
        timing.duration = childrenEnd;
    }
    return timing;
}

// A node's end in its parent, from its start and timing.
static double EndOf(double start, const mtwnTiming* timing, double* activeOut)
{
    mtwnTimingResult result;
    mtwnComputeTiming(timing, 0.0, false, &result);
    if (activeOut != NULL)
    {
        *activeOut = result.activeDuration;
    }
    return start + result.endTime;
}

// Carries a child's end up its ancestors: each timeline's children end
// rises to it, and a timeline lasting as long as its children moves its
// own end. False, with nothing written, when a timeline played backwards
// would become infinitely long, which leaves it no end to start from.
static bool Raise(mtwnClipSlot* clip, uint32_t parent, double end, bool write)
{
    while (parent != 0)
    {
        mtwnClipNode* timeline = &clip->nodes[parent - 1];
        double childrenEnd = end > timeline->childrenEnd ? end : timeline->childrenEnd;
        if (write)
        {
            timeline->childrenEnd = childrenEnd;
        }
        if (!timeline->autoDuration || parent == MTWN_ROOT_NODE)
        {
            return true;
        }
        mtwnTiming timing = ResolvedTiming(timeline, childrenEnd);
        double active = 0.0;
        end = EndOf(timeline->start, &timing, &active);
        if (timing.playbackRate < 0.0 && !isfinite(active))
        {
            return false;
        }
        if (write)
        {
            timeline->end = end;
        }
        parent = timeline->parent;
    }
    return true;
}

// Whether there is room for one more node.
static bool HasRoom(const mtwnClipSlot* clip)
{
    return clip->nodeCount <= clip->def.nodeCapacity;
}

// Puts a prepared node into the clip, last among its parent's children.
static uint32_t Attach(mtwnClipSlot* clip, const mtwnClipNode* node)
{
    uint32_t index1 = ++clip->nodeCount;
    clip->nodes[index1 - 1] = *node;
    mtwnClipNode* parent = &clip->nodes[node->parent - 1];
    if (parent->lastChild != 0)
    {
        clip->nodes[parent->lastChild - 1].nextSibling = index1;
    }
    else
    {
        parent->firstChild = index1;
    }
    parent->lastChild = index1;
    if (node->kind == mtwn_nodeTrack || node->kind == mtwn_nodeTimeline)
    {
        parent->previous = index1;
        Raise(clip, node->parent, node->end, true);
    }
    return index1;
}

// What the def asks of the clip, before anything in the clip changes.
static mtwnResult CheckTrack(const mtwnClipSlot* clip, const mtwnClipTrackDef* def)
{
    bool slot = def->slot < clip->def.slotCapacity && def->track.kind <= mtwn_valueDiscrete;
    if (slot)
    {
        uint8_t kind = clip->slotKinds[def->slot];
        slot = kind == 0 || kind == def->track.kind + 1;
    }
    if (!IsTimeline(clip, def->parent) || !IsPosition(clip, def->parent, def->position) || !slot ||
        def->keyframes == NULL || mtwnValidateTiming(&def->timing) != mtwn_success)
    {
        return mtwn_errorInvalid;
    }
    if (!HasRoom(clip) || def->keyframeCount > clip->def.keyframeCapacity - clip->keyframeCount)
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
    node.timing = def->timing;
    node.start = StartOf(clip, def->parent, def->position);
    node.end = EndOf(node.start, &def->timing, NULL);
    node.parent = def->parent;
    node.slot = def->slot;
    node.kind = mtwn_nodeTrack;
    if (!Raise(clip, def->parent, node.end, false))
    {
        return mtwn_errorInvalid;
    }
    *nodeOut = Attach(clip, &node);
    clip->keyframeCount += def->keyframeCount;
    clip->slotKinds[def->slot] = (uint8_t)(def->track.kind + 1);
    clip->slotCount = def->slot + 1 > clip->slotCount ? def->slot + 1 : clip->slotCount;
    return mtwn_success;
}

static MTWN_NOINLINE mtwnResult AddTimelineWorker(mtwnClipSlot* clip,
                                                  const mtwnClipTimelineDef* def, mtwnNode* nodeOut)
{
    mtwnClipNode node = {0};
    node.timing = def->timing;
    node.autoDuration = def->autoDuration;
    mtwnTiming resolved = ResolvedTiming(&node, 0.0);
    if (!IsTimeline(clip, def->parent) || !IsPosition(clip, def->parent, def->position) ||
        mtwnValidateTiming(&resolved) != mtwn_success ||
        !mtwnIsEasingMonotonic(&def->timing.easing))
    {
        return mtwn_errorInvalid;
    }
    node.depth = (uint8_t)(NodeAt(clip, def->parent)->depth + 1);
    if (!HasRoom(clip) || node.depth > MTWN_MAX_CLIP_DEPTH)
    {
        return mtwn_errorLimit;
    }
    node.start = StartOf(clip, def->parent, def->position);
    node.end = EndOf(node.start, &resolved, NULL);
    node.parent = def->parent;
    node.kind = mtwn_nodeTimeline;
    if (!Raise(clip, def->parent, node.end, false))
    {
        return mtwn_errorInvalid;
    }
    *nodeOut = Attach(clip, &node);
    return mtwn_success;
}

static MTWN_NOINLINE mtwnResult AddInstantWorker(mtwnClipSlot* clip, mtwnNodeKind kind,
                                                 mtwnNode parent, mtwnPosition position,
                                                 uint32_t key, mtwnNode* nodeOut)
{
    if (!IsTimeline(clip, parent) || !IsPosition(clip, parent, position) ||
        (kind == mtwn_nodeLabel && FindLabelIn(clip, parent, key) != 0))
    {
        return mtwn_errorInvalid;
    }
    if (!HasRoom(clip))
    {
        return mtwn_errorLimit;
    }
    mtwnClipNode node = {0};
    node.start = StartOf(clip, parent, position);
    node.end = node.start;
    node.parent = parent;
    node.key = key;
    node.kind = kind;
    *nodeOut = Attach(clip, &node);
    return mtwn_success;
}

// The checks every add shares: the clip alive and not finished, the def
// given with its cookie and version. Returns the clip, or NULL with the
// refusal, counted, in resultOut.
static mtwnClipSlot* Adding(mtwnContext* context, mtwnClipId clip, bool given, bool cookie,
                            uint32_t version, mtwnNode* nodeOut, mtwnResult* resultOut)
{
    if (nodeOut != NULL)
    {
        *nodeOut = 0;
    }
    mtwnClipSlot* slot = FindClip(context, clip);
    mtwnResult result = mtwn_success;
    if (slot == NULL)
    {
        result = mtwn_errorStale;
    }
    else if (!given || nodeOut == NULL || !cookie)
    {
        result = mtwn_errorInvalid;
    }
    else if (version != MTWN_ABI_VERSION)
    {
        result = mtwn_errorVersion;
    }
    else if (slot->finished)
    {
        result = mtwn_errorState;
    }
    *resultOut = result == mtwn_success ? result : Misuse(context, result);
    return result == mtwn_success ? slot : NULL;
}

// A worker's result, an invalid argument counted as misuse.
static mtwnResult Counted(mtwnContext* context, mtwnResult result)
{
    return result == mtwn_errorInvalid ? Misuse(context, result) : result;
}

mtwnResult mtwnClip_AddTrack(mtwnContext* context, mtwnClipId clip, const mtwnClipTrackDef* def,
                             mtwnNode* nodeOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnResult result = mtwn_success;
    bool given = def != NULL;
    mtwnClipSlot* slot =
        Adding(context, clip, given, given && def->cookie == MTWN_CLIP_TRACK_DEF_COOKIE,
               given ? def->version : 0, nodeOut, &result);
    if (slot == NULL)
    {
        return result;
    }
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    result = AddTrackWorker(slot, def, nodeOut);
    mtwnFloatEnvLeave(&env);
    return Counted(context, result);
}

mtwnResult mtwnClip_AddTimeline(mtwnContext* context, mtwnClipId clip,
                                const mtwnClipTimelineDef* def, mtwnNode* nodeOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnResult result = mtwn_success;
    bool given = def != NULL;
    mtwnClipSlot* slot =
        Adding(context, clip, given, given && def->cookie == MTWN_CLIP_TIMELINE_DEF_COOKIE,
               given ? def->version : 0, nodeOut, &result);
    if (slot == NULL)
    {
        return result;
    }
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    result = AddTimelineWorker(slot, def, nodeOut);
    mtwnFloatEnvLeave(&env);
    return Counted(context, result);
}

static mtwnResult AddInstant(mtwnContext* context, mtwnClipId clip, mtwnNodeKind kind,
                             mtwnNode parent, mtwnPosition position, uint32_t key,
                             mtwnNode* nodeOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    // No def to check: these take their arguments directly.
    mtwnResult result = mtwn_success;
    mtwnClipSlot* slot = Adding(context, clip, true, true, MTWN_ABI_VERSION, nodeOut, &result);
    if (slot == NULL)
    {
        return result;
    }
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    result = AddInstantWorker(slot, kind, parent, position, key, nodeOut);
    mtwnFloatEnvLeave(&env);
    return Counted(context, result);
}

mtwnResult mtwnClip_AddLabel(mtwnContext* context, mtwnClipId clip, mtwnNode parent,
                             mtwnPosition position, uint32_t key, mtwnNode* nodeOut)
{
    return AddInstant(context, clip, mtwn_nodeLabel, parent, position, key, nodeOut);
}

mtwnResult mtwnClip_AddMark(mtwnContext* context, mtwnClipId clip, mtwnNode parent,
                            mtwnPosition position, uint32_t key, mtwnNode* nodeOut)
{
    return AddInstant(context, clip, mtwn_nodeMark, parent, position, key, nodeOut);
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
    // Timelines sized by their children take the size for good.
    for (uint32_t i = 0; i < slot->nodeCount; ++i)
    {
        mtwnClipNode* node = &slot->nodes[i];
        if (node->kind == mtwn_nodeTimeline && node->autoDuration)
        {
            node->timing.duration = node->childrenEnd;
        }
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
    if (node < 1 || node > slot->nodeCount || slot->nodes[node - 1].kind != mtwn_nodeTrack)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    const mtwnClipNode* n = &slot->nodes[node - 1];
    *trackOut = (mtwnClipTrack){n->track, n->timing, n->start, n->parent, n->slot};
    return mtwn_success;
}

mtwnResult mtwnClip_GetNode(mtwnContext* context, mtwnClipId clip, mtwnNode node,
                            mtwnClipNodeInfo* infoOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (infoOut == NULL)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    memset(infoOut, 0, sizeof *infoOut);
    mtwnClipSlot* slot = NULL;
    mtwnResult result = Finished(context, clip, &slot);
    if (result != mtwn_success)
    {
        return result;
    }
    if (node < 1 || node > slot->nodeCount)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    const mtwnClipNode* n = NodeAt(slot, node);
    infoOut->timing = n->kind == mtwn_nodeTimeline ? ResolvedTiming(n, n->childrenEnd) : n->timing;
    infoOut->start = n->start;
    infoOut->end = node == MTWN_ROOT_NODE ? n->childrenEnd : n->end;
    infoOut->parent = n->parent;
    infoOut->slot = n->kind == mtwn_nodeTrack ? n->slot : 0;
    infoOut->key = n->key;
    infoOut->kind = n->kind;
    return mtwn_success;
}

mtwnResult mtwnClip_FindLabel(mtwnContext* context, mtwnClipId clip, mtwnNode parent, uint32_t key,
                              double* secondsOut)
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
    if (result != mtwn_success)
    {
        return result;
    }
    uint32_t label = IsTimeline(slot, parent) ? FindLabelIn(slot, parent, key) : 0;
    if (label == 0)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    *secondsOut = NodeAt(slot, label)->start;
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
