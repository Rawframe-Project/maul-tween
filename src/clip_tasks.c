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

// Advances a group's players, staging what they make. One whose clip has
// more nodes than a group keeps times for is left to the serial pass, as
// is one whose staging ran out, put back as it was, and every one after
// it.
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
        staged->done = false;
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
        staged->recordEnd = work->recordCount;
        staged->trackEnd = work->trackCount;
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
    const mtwnStaged* staged = &context->staged[context->stagedNext++];
    (void)player;
    if (staged->serial)
    {
        return mtwnAdvanceClipPlayer(context, context->serial, staged->player, before, now);
    }
    const mtwnWork* work = &context->works[staged->group];
    for (uint32_t r = staged->recordStart; r < staged->recordEnd; ++r)
    {
        mtwnPushEvent(context, &work->records[r], work->recordAlong[r]);
    }
    for (uint32_t t = staged->trackStart; t < staged->trackEnd; ++t)
    {
        mtwnApplyTrack(context, &work->tracks[t]);
    }
    return staged->done;
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
