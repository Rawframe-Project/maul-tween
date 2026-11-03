// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Clip positions against GSAP's timeline positions, run under Node by
// tools/oracle/rivals.mjs: each script of children, labels and callbacks,
// translated as the clips guide's porting note says, places every op
// where GSAP does, within its rounding.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

typedef struct RivalOp
{
    int kind;
    int base;
    double offset;
    int percent;
    uint32_t label;
    double duration;
    double delay;
    double iterations;
    double repeatDelay;
    double rate;
} RivalOp;

typedef struct RivalScript
{
    uint32_t count;
    RivalOp ops[6];
    double starts[6];
} RivalScript;

#include "generated/rival_position.h"

enum
{
    rivalChild = 0,
    rivalLabel = 1,
    rivalCall = 2,
    rivalAbsolute = 0,
    rivalEnd = 1,
    rivalPreviousStart = 2,
    rivalPreviousEnd = 3,
    rivalAtLabel = 4,
    // A callback's label: its key past every script label's.
    callKey = 100,
};

// GSAP rounds a start to 1e-7, so half of that, and a few units in the
// last place of both sides' sums of times under 100.
static const double s_bound = 0.5e-7 + 0x1.0p-40;

static mtwnContext* s_context;

// An op as translated: how it is added, and for a child that names a
// label it has to add first at the end, where that goes.
typedef struct Placed
{
    mtwnPosition position;
    mtwnPosition missing;
    // A label's own key: the labels before it, as GSAP names them.
    uint32_t key;
    bool addMissing;
} Placed;

// How often each translation was taken, so that every one is checked.
enum
{
    pathDelay,
    pathPercent,
    pathEndless,
    pathAfterCall,
    pathCallPastEnd,
    pathMissing,
    pathCount,
};
static uint32_t s_paths[pathCount];

// What a port reads from the clip it has built so far: the children's
// end, and the latest callback and its time.
typedef struct SoFar
{
    double end;
    double latestCall;
    int32_t latestCallOp;
} SoFar;

static mtwnPosition At(mtwnPositionKind kind, double offset, uint32_t label)
{
    mtwnPosition position = {offset, label, kind};
    return position;
}

// A GSAP child's total duration: its iterations and repeat delays, before
// its time scale.
static double Total(const RivalOp* op)
{
    return op->duration * op->iterations + op->repeatDelay * (op->iterations - 1.0);
}

static mtwnResult Add(mtwnClipId clip, const RivalOp* op, const Placed* placed, uint32_t index,
                      mtwnNode* nodeOut)
{
    mtwnNode node;
    if (placed->addMissing && mtwnClip_AddLabel(s_context, clip, MTWN_ROOT_NODE, placed->missing,
                                                op->label, &node) != mtwn_success)
    {
        return mtwn_errorInvalid;
    }
    if (op->kind == rivalLabel)
    {
        return mtwnClip_AddLabel(s_context, clip, MTWN_ROOT_NODE, placed->position, placed->key,
                                 nodeOut);
    }
    if (op->kind == rivalCall)
    {
        mtwnResult marked =
            mtwnClip_AddMark(s_context, clip, MTWN_ROOT_NODE, placed->position, index, nodeOut);
        return marked != mtwn_success ? marked
                                      : mtwnClip_AddLabel(s_context, clip, MTWN_ROOT_NODE,
                                                          placed->position, callKey + index, &node);
    }
    static const mtwnKeyframe s_keyframes[2] = {{.offset = 0.0}, {.offset = 1.0}};
    mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
    def.keyframeCount = 2;
    def.keyframes = s_keyframes;
    def.position = placed->position;
    def.timing.duration = op->duration;
    def.timing.iterations = op->iterations;
    def.timing.repeatDelay = op->repeatDelay;
    def.timing.playbackRate = op->rate;
    return mtwnClip_AddTrack(s_context, clip, &def, nodeOut);
}

// Builds the first count ops as placed, seals the clip and reads each op's
// start, the children's end and the latest callback.
static bool Build(const RivalScript* script, const Placed* placed, uint32_t count,
                  double* startsOut, SoFar* soFar)
{
    mtwnClipDef def = mtwnDefaultClipDef();
    mtwnClipId clip;
    if (mtwnCreateClip(s_context, &def, &clip) != mtwn_success)
    {
        return false;
    }
    mtwnNode nodes[6];
    bool built = true;
    for (uint32_t i = 0; i < count && built; ++i)
    {
        built = Add(clip, &script->ops[i], &placed[i], i, &nodes[i]) == mtwn_success;
    }
    built = built && mtwnClip_Seal(s_context, clip) == mtwn_success;
    mtwnClipNodeInfo info;
    built = built && mtwnClip_GetNode(s_context, clip, MTWN_ROOT_NODE, &info) == mtwn_success;
    soFar->end = built ? info.timing.duration : 0.0;
    soFar->latestCall = -(double)INFINITY;
    soFar->latestCallOp = -1;
    for (uint32_t i = 0; i < count && built; ++i)
    {
        built = mtwnClip_GetNode(s_context, clip, nodes[i], &info) == mtwn_success;
        startsOut[i] = info.start;
        if (built && script->ops[i].kind == rivalCall && info.start >= soFar->latestCall)
        {
            soFar->latestCall = info.start;
            soFar->latestCallOp = (int32_t)i;
        }
    }
    return mtwnDestroyClip(s_context, clip) == mtwn_success && built;
}

// The previous child or callback before op index, as GSAP's recent: -1 when
// none.
static int32_t Previous(const RivalScript* script, uint32_t index)
{
    for (uint32_t i = index; i-- > 0;)
    {
        if (script->ops[i].kind != rivalLabel)
        {
            return (int32_t)i;
        }
    }
    return -1;
}

// GSAP's end as the note translates it: after an endless child, the
// previous child's first iteration end, or a callback's time; where a
// callback lies past every child's end, its time; else the end.
static mtwnPosition End(const RivalScript* script, uint32_t index, const SoFar* soFar,
                        double offset)
{
    bool endless = false;
    for (uint32_t i = 0; i < index; ++i)
    {
        endless = endless || isinf(script->ops[i].iterations);
    }
    int32_t previous = Previous(script, index);
    if (endless && previous >= 0)
    {
        ++s_paths[pathEndless];
        const RivalOp* op = &script->ops[previous];
        return op->kind == rivalCall
                   ? At(mtwn_positionAtLabel, offset, callKey + (uint32_t)previous)
                   : At(mtwn_positionWithPrevious, op->duration / op->rate + offset, 0);
    }
    if (soFar->latestCallOp >= 0 && soFar->latestCall > soFar->end)
    {
        ++s_paths[pathCallPastEnd];
        return At(mtwn_positionAtLabel, offset, callKey + (uint32_t)soFar->latestCallOp);
    }
    return At(mtwn_positionAfterEnd, offset, 0);
}

// The op at index as the porting note translates it, given the clip so far.
static Placed Translate(const RivalScript* script, uint32_t index, const SoFar* soFar,
                        uint32_t labels)
{
    const RivalOp* op = &script->ops[index];
    int32_t previous = Previous(script, index);
    const RivalOp* before = previous >= 0 ? &script->ops[previous] : NULL;
    double offset = op->offset;
    s_paths[pathPercent] += op->percent ? 1u : 0u;
    s_paths[pathDelay] += op->delay != 0.0 ? 1u : 0u;
    if (op->percent)
    {
        offset *= (op->base == rivalEnd ? Total(op) : before != NULL ? Total(before) : 0.0) / 100.0;
    }
    // A delay is part of the start.
    offset += op->delay;
    Placed placed = {0};
    placed.key = labels;
    bool afterCall = before != NULL && before->kind == rivalCall &&
                     (op->base == rivalPreviousStart || op->base == rivalPreviousEnd);
    s_paths[pathAfterCall] += afterCall ? 1u : 0u;
    uint32_t callLabel = callKey + (uint32_t)previous;
    switch (op->base)
    {
    case rivalAbsolute:
        placed.position = At(mtwn_positionAbsolute, offset, 0);
        break;
    case rivalEnd:
        placed.position = End(script, index, soFar, offset);
        break;
    case rivalPreviousStart:
        placed.position = afterCall ? At(mtwn_positionAtLabel, offset, callLabel)
                                    : At(mtwn_positionWithPrevious, offset, 0);
        break;
    case rivalPreviousEnd:
        placed.position =
            afterCall ? At(mtwn_positionAtLabel, offset, callLabel)
            : before != NULL && isinf(before->iterations)
                ? At(mtwn_positionWithPrevious, before->duration / before->rate + offset, 0)
                : At(mtwn_positionAfterPrevious, offset, 0);
        break;
    default:
        // A label not yet added is added at the end first.
        placed.addMissing = op->label == labels;
        s_paths[pathMissing] += placed.addMissing ? 1u : 0u;
        if (placed.addMissing)
        {
            placed.missing = End(script, index, soFar, 0.0);
        }
        placed.position = At(mtwn_positionAtLabel, offset, op->label);
        break;
    }
    return placed;
}

static void TestRivals(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&def, &s_context) == mtwn_success, "a context");
    uint32_t count = (uint32_t)(sizeof s_rivalScripts / sizeof s_rivalScripts[0]);
    uint32_t within = 0;
    for (uint32_t c = 0; c < count; ++c)
    {
        const RivalScript* script = &s_rivalScripts[c];
        Placed placed[6];
        double starts[6];
        SoFar soFar;
        uint32_t labels = 0;
        bool built = true;
        for (uint32_t i = 0; i < script->count && built; ++i)
        {
            built = Build(script, placed, i, starts, &soFar);
            placed[i] = Translate(script, i, &soFar, labels);
            const RivalOp* op = &script->ops[i];
            labels += op->kind == rivalLabel || placed[i].addMissing ? 1u : 0u;
        }
        built = built && Build(script, placed, script->count, starts, &soFar);
        double worst = built ? 0.0 : (double)INFINITY;
        for (uint32_t i = 0; i < script->count && built; ++i)
        {
            double error = fabs(starts[i] - script->starts[i]);
            worst = error > worst ? error : worst;
        }
        if (worst <= s_bound)
        {
            ++within;
        }
        else
        {
            printf("script %u of %u ops: %s, off by %.3g\n", c, script->count,
                   built ? "built" : "refused", worst);
        }
    }
    mtwnDestroyContext(s_context);
    CHECK(count == 400, "every script the oracle wrote");
    bool every = true;
    for (int path = 0; path < pathCount; ++path)
    {
        every = every && s_paths[path] > 0;
    }
    CHECK(every, "every translation the note gives, taken");
    CHECK(within == count, "every script, translated, placed where GSAP places it");
}

int main(void)
{
    TestRivals();
    return s_failures == 0 ? 0 : 1;
}
