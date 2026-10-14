// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context's layout, shared by the modules that work in it (records
// mtwn-0028 and mtwn-0029). The context, its pools and its list of
// changed targets live in one block.

#ifndef MAUL_TWEEN_SRC_CONTEXT_INTERNAL_H
#define MAUL_TWEEN_SRC_CONTEXT_INTERNAL_H

#include "pool.h"
#include "runs.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <stdbool.h>
#include <stdint.h>

// A target: 64 bytes. Its header's link names its transition.
typedef struct mtwnTargetSlot
{
    mtwnSlotHeader header;
    mtwnValueKind kind;
    uint8_t reserved[3];
    mtwnVec4 base;
    mtwnVec4 value;
    mtwnVec4 velocity;
} mtwnTargetSlot;

// The target is on the list of changed targets.
#define MTWN_TARGET_CHANGED 2u

// What a player is.
#define MTWN_PLAYER_TRANSITION 1u
#define MTWN_PLAYER_CLIP       2u

// A clip's player: Web Animations' times, in seconds, NaN unresolved.
typedef struct mtwnClipPlay
{
    mtwnClipId clip;
    double startTime;
    double holdTime;
    double previousTime;
    double rate;
    // A rate waiting for the next update, NaN for none: until then the old
    // rate runs, and from it the new one, the current time kept.
    double pendingRate;
    // Its bindings: one target per slot of the clip, from start.
    uint32_t bindingStart;
    uint32_t bindingCount;
    // Waiting for the next update's time to start.
    bool pending;
    bool keep;
    uint8_t reserved[6];
} mtwnClipPlay;

// A player: a transition or a clip's player. A transition's header link
// names its target.
typedef struct mtwnPlayerSlot
{
    mtwnSlotHeader header;
    uint8_t type;
    uint8_t reserved2[3];
    // The players before and after it in creation order, 0 for none.
    uint32_t earlier;
    uint32_t later;
    mtwnClipPlay play;
    mtwnValueKind kind;
    mtwnTransitionSpec spec;
    // Whether the first update has fixed the start time.
    bool started;
    uint8_t reserved;
    mtwnTargetId target;
    uint64_t startNs;
    double delay;
    // Seconds after the delay to the end; infinity for a spring that never
    // settles.
    double endTime;
    double duration;
    // The start velocity per channel; for a color or rotation, in progress
    // in the first.
    double velocity[4];
    mtwnEasing easing;
    mtwnSpring spring;
    mtwnDecay decay;
    mtwnVec4 from;
    // The end value: an angle's moved by its turns, a decay's where it
    // stops.
    mtwnVec4 to;
    union
    {
        mtwnColorMix color;
        mtwnRotationMix rotation;
    } mix;
} mtwnPlayerSlot;

// Whether a kind moves along a path by its progress: colors and rotations.
static inline bool mtwnIsPathKind(mtwnValueKind kind)
{
    return kind == mtwn_valueColor || kind == mtwn_valueRotation;
}

// A node of a clip.
typedef struct mtwnClipNode
{
    // A timeline's with the duration the host set; with autoDuration its
    // children's end stands for it.
    mtwnTiming timing;
    mtwnTrack track;
    // Its start and end in its parent.
    double start;
    double end;
    // For a timeline: the latest end of its children so far.
    double childrenEnd;
    // For a timeline: its first and last children, and the last track or
    // timeline among them (positions' previous child); 0 when none.
    uint32_t firstChild;
    uint32_t lastChild;
    uint32_t previous;
    // The next child of its parent, 0 for the last.
    uint32_t nextSibling;
    uint32_t parent;
    uint32_t slot;
    uint32_t key;
    mtwnNodeKind kind;
    bool autoDuration;
    uint8_t reserved[2];
} mtwnClipNode;

// A clip, in the context's pool; its storage is a block of its own.
typedef struct mtwnClipSlot
{
    mtwnSlotHeader header;
    bool finished;
    uint8_t reserved[3];
    mtwnClipDef def;
    void* block;
    uint64_t blockBytes;
    // The root is nodes[0].
    mtwnClipNode* nodes;
    mtwnKeyframe* keyframes;
    double* lengths;
    mtwnSegmentMix* mixes;
    // Each slot's value kind plus 1, 0 for a slot no track uses yet.
    uint8_t* slotKinds;
    uint32_t nodeCount;
    uint32_t keyframeCount;
    // The slots its tracks use: the largest plus 1.
    uint32_t slotCount;
    // The players playing it.
    uint32_t users;
} mtwnClipSlot;

struct mtwnContext
{
    mtwnAllocator allocator;
    mtwnContextDef def;
    // The block holding the context and everything in it, and its size.
    uint64_t blockBytes;
    uint64_t misuse;
    uint64_t lastTimeNs;
    mtwnPool targets;
    mtwnPool players;
    mtwnPool clips;
    // The players in creation order, first and last; 0 for none.
    uint32_t firstPlayer;
    uint32_t lastPlayer;
    // The players' bindings.
    mtwnRuns bindings;
    // The bytes the live clips' blocks hold.
    uint64_t clipStorageUsed;
    // The index1 of each target changed since the last report, in order.
    uint32_t* changed;
    uint32_t changedCount;
};

static inline mtwnTargetSlot* mtwnTargetAt(const mtwnContext* context, uint32_t index1)
{
    return (mtwnTargetSlot*)mtwnPoolAt(&context->targets, index1);
}

static inline mtwnClipSlot* mtwnClipAt(const mtwnContext* context, uint32_t index1)
{
    return (mtwnClipSlot*)mtwnPoolAt(&context->clips, index1);
}

static inline mtwnPlayerSlot* mtwnPlayerAt(const mtwnContext* context, uint32_t index1)
{
    return (mtwnPlayerSlot*)mtwnPoolAt(&context->players, index1);
}

// The channels a kind uses.
static inline uint8_t mtwnKindChannels(mtwnValueKind kind)
{
    static const uint8_t channels[] = {1, 2, 3, 4, 4, 1, 4, 1, 1};
    return channels[kind];
}

// Puts a target on the list of changed targets, once.
static inline void mtwnMarkChanged(mtwnContext* context, uint32_t index1)
{
    mtwnTargetSlot* slot = mtwnTargetAt(context, index1);
    if ((slot->header.flags & MTWN_TARGET_CHANGED) == 0)
    {
        slot->header.flags |= MTWN_TARGET_CHANGED;
        context->changed[context->changedCount++] = index1;
    }
}

// The value with the kind's channels kept and the others 0, or false when
// the kind refuses it: a channel not finite, or a rotation all zero.
bool mtwnAcceptValue(mtwnValueKind kind, mtwnVec4 value, mtwnVec4* out);

// Takes a player slot, put last in creation order; 0 when the pool is
// full.
uint32_t mtwnTakePlayer(mtwnContext* context, uint8_t type);

// Frees a player: out of creation order, its bindings given back, its
// clip no longer busy for it, its slot returned.
void mtwnFreePlayer(mtwnContext* context, uint32_t player);

// Ends a target's transition: frees the player and unlinks the target.
void mtwnEndTransition(mtwnContext* context, uint32_t player);

// Advances a clip's player to the host's time (src/player.c).
void mtwnAdvancePlayer(mtwnContext* context, uint32_t player, double now);

#endif
