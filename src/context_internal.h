// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context's layout, shared by the modules that work in it (records
// mtwn-0028 and mtwn-0029). The context, its pools and its list of
// changed targets live in one block.

#ifndef MAUL_TWEEN_SRC_CONTEXT_INTERNAL_H
#define MAUL_TWEEN_SRC_CONTEXT_INTERNAL_H

#include "crossing.h"
#include "pool.h"
#include "runs.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
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
// A clip's player bound to the target has not written it in this update
// yet: if none does, it shows its base.
#define MTWN_TARGET_UNWRITTEN 4u

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
    // Scales its bindings' weights.
    double weight;
    // A rate waiting for the next update, NaN for none: until then the old
    // rate runs, and from it the new one, the current time kept.
    double pendingRate;
    // The time its events have swept to, NaN for none (idle, or not yet
    // updated since it played): the next sweep starts there.
    double swept;
    // Its bindings: one target per slot of the clip, from start.
    uint32_t bindingStart;
    uint32_t bindingCount;
    // Waiting for the next update's time to start.
    bool pending;
    bool keep;
    // The way the last sweep went: 1 forwards, -1 backwards, 0 none (or a
    // jump since). A sweep turning back takes its start again.
    int8_t sweptWay;
    uint8_t reserved[5];
} mtwnClipPlay;

// A player: a transition or a clip's player. A transition's header link
// names its target.
typedef struct mtwnPlayerSlot
{
    mtwnSlotHeader header;
    uint8_t type;
    // Whether a transition's delay has ended, its start recorded.
    bool begun;
    uint8_t reserved2[2];

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
    // Timelines above it below the root; the root's is 0.
    uint8_t depth;
    // A track's iterations move on by step each (record mtwn-0040): its
    // last keyframe minus its first, or for a rotation the rotation from
    // its first to its last.
    bool incremental;
    mtwnVec4 step;
} mtwnClipNode;

// A clip, in the context's pool; its storage is a block of its own.
// A node's time during a sample: the time it gives its children now and
// a moment ago (for the velocity), and whether time runs backwards in it;
// during a sweep of events, at the sweep's end and start.
typedef struct mtwnNodeTime
{
    double now;
    double before;
    bool backwards;
    // For events: whether it and every timeline above it stayed in one
    // iteration over the sweep, so its child time moved one way.
    bool steady;
    uint8_t reserved[6];
} mtwnNodeTime;

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
    // Scratch for sampling, one per node.
    mtwnNodeTime* times;
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

    // The event queue: a ring of def.eventCapacity records, the oldest at
    // head; and those dropped at a full queue.
    mtwnEvent* events;
    // Beside each record, its place along its sweep, which orders one
    // player's records of one time.
    double* eventAlong;
    uint32_t eventHead;
    uint32_t eventCount;
    uint64_t droppedEvents;
    // Those dropped since the last read.
    uint64_t droppedSinceRead;
    // Scratch for counting crossings: a chain of timelines and the node.
    mtwnLevel* levels;
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

// Advances a clip's player from the last update's host time to this one's,
// records what it crossed and samples it; true when it has finished and is
// not kept, to be freed (src/player.c).
bool mtwnAdvancePlayer(mtwnContext* context, uint32_t player, double before, double now);

// Writes a target's value and velocity, marking it changed when its value
// changed and written in this update (src/update.c).
void mtwnWriteTarget(mtwnContext* context, uint32_t index1, mtwnVec4 value, mtwnVec4 velocity);

// The live target a player's binding names, 0 for none.
uint32_t mtwnBoundTarget(const mtwnContext* context, const mtwnClipPlay* play, uint32_t slot);

// Appends a record, with its place along its sweep, dropping it at a full
// queue (src/event.c).
void mtwnPushEvent(mtwnContext* context, const mtwnEvent* event, double along);

// Sorts the records appended since the queue held count, by time, then a
// player's by their place along its sweep, keeping the order made.
void mtwnSortEvents(mtwnContext* context, uint32_t count);

// Records what a clip's player's time crossed sweeping from its swept
// time to 'to' at a rate, the host's time running from hostFrom to hostTo;
// a sweep from no time takes 'to' itself.
void mtwnSweepEvents(mtwnContext* context, uint32_t player, double to, double rate, double hostFrom,
                     double hostTo);

// Records a player's event of the player itself: a seek, a cancel, a
// transition's start or end.
void mtwnPlayerEvent(mtwnContext* context, uint32_t player, mtwnEventKind kind, double hostTime,
                     double clipTime);

// Samples a clip's player's tree at its current time into its targets:
// rate is how fast its time moves (0 when held), backwards its playback
// direction; committing writes the values as the bases too
// (src/clip_sample.c).
void mtwnSampleClip(mtwnContext* context, const mtwnClipPlay* play, double current, double rate,
                    bool backwards, bool commit);

// Marks a clip's player's targets unwritten, before the update's players
// run.
void mtwnMarkUnwritten(mtwnContext* context, const mtwnClipPlay* play);

// Shows their bases in a clip's player's targets no player wrote.
void mtwnResetUnwritten(mtwnContext* context, const mtwnClipPlay* play);

#endif
