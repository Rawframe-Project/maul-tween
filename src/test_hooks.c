// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tests' switches and counts (test_hooks.h).

#include "test_hooks.h"

#include "context_internal.h"

void mtwnTestSetCountEvery(mtwnContext* context, bool on)
{
    context->countEvery = on;
}

static void Add(mtwnTestSharing* out, const mtwnSharing* sharing)
{
    out->sampleKept = out->sampleKept || sharing->sampleKept;
    out->sweepKept = out->sweepKept || sharing->sweepKept;
    out->noted = out->noted || sharing->sampleValid || sharing->sweepValid;
}

void mtwnTestSweep(mtwnContext* context, mtwnPlayerId player, const mtwnTestSweepInputs* in,
                   bool near)
{
    mtwnClipPlay* play = &mtwnPlayerAt(context, player.index1)->play;
    play->swept = in->swept;
    play->sweptWay = in->way;
    play->rate = in->playRate;
    if (near)
    {
        mtwnSweepEventsNear(context, context->serial, player.index1, in->to, in->rate, in->hostFrom,
                            in->hostTo);
    }
    else
    {
        mtwnSweepEvents(context, context->serial, player.index1, in->to, in->rate, in->hostFrom,
                        in->hostTo);
    }
}

bool mtwnTestIsBusy(const mtwnContext* context)
{
    return context->busy;
}

mtwnTestSharing mtwnTestGetSharing(const mtwnContext* context)
{
    mtwnTestSharing out = {false, false, false};
    Add(&out, &context->serial->sharing);
    for (uint32_t g = 0; context->works != NULL && g < MTWN_GROUPS; ++g)
    {
        Add(&out, &context->works[g].sharing);
    }
    return out;
}
