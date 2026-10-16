// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The clips' players on the host's task system (record mtwn-0056). Before
// an update's serial pass, the clips' players in creation order are split
// into up to MTWN_GROUPS groups, which the host runs on any thread. Each
// group advances its players in its own scratch, staging their records and
// track results; a player writes only its own state. The serial pass then
// takes each player in creation order, between the transitions, and
// queues its records and composes its results into its targets: what a
// serial update makes, bit for bit, whatever the host's split.

#include "context_internal.h"
#include "fpenv.h"

typedef struct GroupTask
{
    mtwnContext* context;
    double before;
    double now;
} GroupTask;

// A group's players: [first, end) of the staged order.
static void GroupRange(uint32_t count, uint32_t group, uint32_t* first, uint32_t* end)
{
    uint32_t groups = count < MTWN_GROUPS ? count : MTWN_GROUPS;
    *first = (uint32_t)((uint64_t)group * count / groups);
    *end = (uint32_t)((uint64_t)(group + 1u) * count / groups);
}

// Whether a player whose group writes its alone targets in place, which
// cannot be undone, stages all it can make: a result per node and at most
// six records a node a sweep (a start, iterations and an end, each way),
// unless records past the staging are dropped as the queue would. Under
// reduced motion a player not yet swept sweeps twice, to where it was
// first and then to the end it is finished at.
static bool Fits(const mtwnContext* context, const mtwnWork* work, const mtwnClipPlay* play)
{
    uint64_t nodes = mtwnClipAt(context, play->clip.index1)->nodeCount;
    uint64_t sweeps = context->reducedMotion && isnan(play->swept) ? 2u : 1u;
    bool records = work->recordCapacity == context->def.eventCapacity ||
                   work->recordCount + 6u * sweeps * nodes <= work->recordCapacity;
    return records && work->trackCount + nodes <= work->trackCapacity &&
           work->changeCount + 2u * (uint64_t)play->bindingCount <= work->changeCapacity;
}

// Advances a group's players, staging what they make; their alone targets
// are marked, written and shown their bases here. One whose clip has more
// nodes than a group keeps times for is left to the serial pass, as is one
// with alone targets that might not fit, and one whose staging ran out,
// put back as it was, and every one after it.
static MTWN_NOINLINE void RunGroup(const GroupTask* task, uint32_t group)
{
    mtwnContext* context = task->context;
    mtwnWork* work = &context->works[group];
    uint32_t first = 0;
    uint32_t end = 0;
    GroupRange(context->stagedCount, group, &first, &end);
    bool full = false;
    for (uint32_t i = first; i < end; ++i)
    {
        mtwnStaged* staged = &context->staged[i];
        mtwnClipPlay* play = &mtwnPlayerAt(context, staged->player)->play;
        staged->group = (uint8_t)group;
        staged->recordStart = work->recordCount;
        staged->trackStart = work->trackCount;
        staged->changeStart = work->changeCount;
        staged->done = false;
        uint32_t layered = 0;
        bool unfit =
            mtwnMarkUnwrittenOf(context, play, true, &layered) > 0 && !Fits(context, work, play);
        staged->layered = layered > 0;
        full = full || unfit;
        staged->serial =
            full || mtwnClipAt(context, play->clip.index1)->nodeCount > MTWN_GROUP_NODES;
        if (!staged->serial)
        {
            mtwnClipPlay was = *play;
            uint64_t dropped = work->dropped;
            work->overflow = false;
            staged->done =
                mtwnAdvanceClipPlayer(context, work, staged->player, task->before, task->now);
            if (work->overflow)
            {
                *play = was;
                work->recordCount = staged->recordStart;
                work->trackCount = staged->trackStart;
                work->dropped = dropped;
                staged->serial = true;
                full = true;
            }
        }
        staged->resetStart = work->changeCount;
        if (!staged->serial)
        {
            mtwnResetAlone(context, play, work);
        }
        staged->recordEnd = work->recordCount;
        staged->trackEnd = work->trackCount;
        staged->changeEnd = work->changeCount;
    }
}

// A range of groups, in the library's floating-point environment on
// whichever thread the host runs it.
static void RunGroups(int32_t startIndex, int32_t endIndex, void* taskContext)
{
    const GroupTask* task = (const GroupTask*)taskContext;
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    for (int32_t g = startIndex; g < endIndex; ++g)
    {
        RunGroup(task, (uint32_t)g);
    }
    mtwnFloatEnvLeave(&env);
}

// A pass of the count over a binding: its target's count cleared, the
// binding counted, or its alone worked out.
static void CountBinding(mtwnContext* context, mtwnBound* bound, uint32_t target, int pass)
{
    if (target == 0)
    {
        bound->alone = false;
        return;
    }
    mtwnTargetSlot* slot = mtwnTargetAt(context, target);
    if (pass == 0)
    {
        slot->clipWriters = 0;
    }
    else if (pass == 1)
    {
        slot->clipWriters = slot->clipWriters < 2 ? (uint8_t)(slot->clipWriters + 1u) : 2u;
    }
    else
    {
        bound->alone = slot->clipWriters == 1 && slot->header.link == 0;
        context->layeredBindings += bound->alone ? 0u : 1u;
    }
}

void mtwnCountWriters(mtwnContext* context)
{
    context->layeredBindings = 0;
    for (int pass = 0; pass < 3; ++pass)
    {
        for (uint32_t index1 = context->firstClipPlayer; index1 != 0;)
        {
            const mtwnClipPlay* play = &mtwnPlayerAt(context, index1)->play;
            for (uint32_t s = 0; s < play->bindingCount; ++s)
            {
                CountBinding(context, &context->bindings.entries[play->bindingStart + s],
                             mtwnBoundTarget(context, play, s), pass);
            }
            index1 = play->nextClip;
        }
    }
    context->writersStale = false;
}

void mtwnRunClipGroups(mtwnContext* context, double before, double now)
{
    context->stagedCount = 0;
    context->stagedNext = 0;
    if (context->works == NULL)
    {
        return;
    }
    uint32_t count = 0;
    for (uint32_t index1 = context->firstPlayer; index1 != 0;)
    {
        const mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
        if (slot->type == MTWN_PLAYER_CLIP)
        {
            context->staged[count++].player = index1;
            mtwnPlayerAt(context, index1)->play.stagedAt = count;
        }
        index1 = slot->later;
    }
    // One player gains nothing from the host's threads.
    if (count < 2)
    {
        return;
    }
    context->stagedCount = count;
    for (uint32_t g = 0; g < MTWN_GROUPS; ++g)
    {
        context->works[g].recordCount = 0;
        context->works[g].trackCount = 0;
        context->works[g].dropped = 0;
        context->works[g].changeCount = 0;
    }
    GroupTask task = {context, before, now};
    const mtwnContextDef* def = &context->def;
    int32_t groups = (int32_t)(count < MTWN_GROUPS ? count : MTWN_GROUPS);
    void* handle = def->enqueueTask(RunGroups, groups, 1, &task, def->userTaskContext);
    if (handle != NULL)
    {
        def->finishTask(handle, def->userTaskContext);
    }
}

bool mtwnApplyStaged(mtwnContext* context, uint32_t player, double before, double now)
{
    // The serial pass meets the clips' players in the creation order the
    // staging was made in, so the next is this player.
    mtwnStaged* staged = &context->staged[context->stagedNext++];
    (void)player;
    if (staged->serial)
    {
        ++context->serialAdvances;
        staged->done = mtwnAdvanceClipPlayer(context, context->serial, staged->player, before, now);
        return staged->done;
    }
    const mtwnWork* work = &context->works[staged->group];
    for (uint32_t r = staged->recordStart; r < staged->recordEnd; ++r)
    {
        mtwnPushEvent(context, &work->records[r], work->recordAlong[r]);
    }
    // The group's changes to alone targets between the results, in the
    // order the serial pass would have made them.
    uint32_t k = staged->changeStart;
    for (uint32_t t = staged->trackStart; t < staged->trackEnd; ++t)
    {
        for (; k < work->tracks[t].changesBefore; ++k)
        {
            context->changed[context->changedCount++] = work->changes[k];
        }
        mtwnApplyTrack(context, &work->tracks[t], NULL);
    }
    for (; k < staged->resetStart; ++k)
    {
        context->changed[context->changedCount++] = work->changes[k];
    }
    return staged->done;
}

void mtwnResetEveryStaged(mtwnContext* context)
{
    // The clips' chain is newest first: the staging walked from its end.
    // A player with every target its own has its group's changes put on
    // the list whole, its own slot not read.
    for (uint32_t i = context->stagedCount; i-- > 0;)
    {
        const mtwnStaged* staged = &context->staged[i];
        if (staged->done)
        {
            continue;
        }
        if (staged->serial || staged->layered)
        {
            mtwnResetStaged(context, &mtwnPlayerAt(context, staged->player)->play);
            continue;
        }
        const mtwnWork* work = &context->works[staged->group];
        for (uint32_t k = staged->resetStart; k < staged->changeEnd; ++k)
        {
            context->changed[context->changedCount++] = work->changes[k];
        }
    }
}

void mtwnResetStaged(mtwnContext* context, const mtwnClipPlay* play)
{
    const mtwnStaged* staged =
        context->stagedCount > 0 ? &context->staged[play->stagedAt - 1u] : NULL;
    if (staged == NULL || staged->serial)
    {
        mtwnResetUnwritten(context, play);
        return;
    }
    // Binding by binding: an alone target its group showed, its change
    // put on the list here, the target itself not read (its group's core
    // wrote it); another shown now.
    const mtwnWork* work = &context->works[staged->group];
    uint32_t k = staged->resetStart;
    for (uint32_t s = 0; s < play->bindingCount; ++s)
    {
        const mtwnBound* bound = &context->bindings.entries[play->bindingStart + s];
        uint32_t target = bound->alone ? 0 : mtwnBoundTarget(context, play, s);
        if (bound->alone)
        {
            if (k < staged->changeEnd && work->changes[k] == bound->target.index1)
            {
                context->changed[context->changedCount++] = work->changes[k++];
            }
        }
        else if (target != 0 &&
                 (mtwnTargetAt(context, target)->header.flags & MTWN_TARGET_UNWRITTEN) != 0)
        {
            mtwnWriteTarget(context, target, mtwnTargetAt(context, target)->base, (mtwnVec4){0});
        }
    }
}

void mtwnEndClipGroups(mtwnContext* context)
{
    if (context->stagedCount == 0)
    {
        return;
    }
    for (uint32_t g = 0; g < MTWN_GROUPS; ++g)
    {
        context->droppedEvents += context->works[g].dropped;
        context->droppedSinceRead += context->works[g].dropped;
    }
    context->stagedCount = 0;
}
