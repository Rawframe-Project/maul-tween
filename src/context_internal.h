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
// The target's velocity is its eased transition's at the last update's
// time, computed when read (record mtwn-0042); its velocity field is
// stale.
#define MTWN_TARGET_LAZY 8u
// The target's velocity is a clip track's, computed when read: its
// velocity field holds an mtwnClipVelocity instead (src/clip_sample.c).
#define MTWN_TARGET_CLIP_LAZY 16u

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
    // The clips' players before and after it, in no order, 0 for none:
    // the passes over clips' players alone.
    uint32_t previousClip;
    uint32_t nextClip;
    // Its bindings: one target per slot of the clip, from start.
    uint32_t bindingStart;
    uint32_t bindingCount;
    // Waiting for the next update's time to start.
    bool pending;
    bool keep;
    // The way the last sweep went: 1 forwards, -1 backwards, 0 none (or a
    // jump since). A sweep turning back takes its start again.
    int8_t sweptWay;
    // Plays on under reduced motion.
    bool essential;
    uint8_t reserved[4];
} mtwnClipPlay;

// A player: a transition or a clip's player. A transition's header link
// names its target.
typedef struct mtwnPlayerSlot
{
    mtwnSlotHeader header;
    uint8_t type;
    // Whether a transition's delay has ended, its start recorded.
    bool begun;
    // Whether the first update has fixed a transition's start time.
    bool started;
    mtwnValueKind kind;
    // The players before and after it in creation order, 0 for none.
    uint32_t earlier;
    uint32_t later;
    union
    {
        mtwnClipPlay play;
        // A transition: what every update reads first, in the slot's first
        // three cache lines with the header (record mtwn-0042), then what it
        // reads when it starts, ends or is replaced. One curve, by its spec.
        struct
        {
            mtwnTransitionSpec spec;
            // For an easing that is an integer power already normal, its
            // exponent and mode, so the update reads no more of it;
            // otherwise 0.
            uint8_t power;
            mtwnEaseMode powerMode;
            // Plays on under reduced motion.
            bool essential;
            uint8_t reserved[4];
            uint64_t startNs;
            double delay;
            // Seconds after the delay to the end; infinity for a spring that
            // never settles.
            double endTime;
            double duration;
            mtwnVec4 from;
            // The end value: an angle's moved by its turns, a decay's where
            // it stops.
            mtwnVec4 to;
            union
            {
                mtwnEasing easing;
                mtwnSpring spring;
                mtwnDecay decay;
            };
            mtwnTargetId target;
            union
            {
                // For a spring or decay: the start velocity per channel;
                // for a color or rotation, in progress in the first.
                double velocity[4];
                // For an easing: CSS Transitions' reversing-adjusted start
                // value and reversing shortening factor (record mtwn-0045).
                struct
                {
                    mtwnVec4 reversingStart;
                    double shortening;
                };
            };
            union
            {
                mtwnColorMix color;
                mtwnRotationMix rotation;
            } mix;
        };
    };
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
    // Its active duration, from its timing as finished, for its events.
    double activeDuration;
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
    // For a track: the node before it is a track of the same parent,
    // start and timing, so its timing results are that track's.
    bool sharesTiming;
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
    // For events: whether, besides, every one's child time at both ends lay
    // clearly inside an iteration, off its boundaries, where a crossing's
    // position is one child time (record mtwn-0042).
    bool sure;
    // For events: whether the sweep's start is open (the sweep before ended
    // there) and every one's time there lay inside its active interval,
    // off a repeat delay: moving from there, held nowhere.
    bool openStart;
    uint8_t reserved[4];
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

// A clip's player's scratch and staging (below).
typedef struct mtwnWork mtwnWork;
typedef struct mtwnStaged mtwnStaged;

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
    // The first of the clips' players, 0 for none.
    uint32_t firstClipPlayer;
    // The clips' players' part of an update, set by the first clip's
    // player made, NULL before (record mtwn-0047). Never cleared: once a
    // clip's player was made its part runs, over an empty chain when the
    // last is gone.
    const struct mtwnClipUpdate* clipUpdate;
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
    // The serial work: those levels, each clip's own node times, no staging.
    mtwnWork* serial;
    // With the host's task system (record mtwn-0056): each group's scratch
    // and staging, and the clips' players in creation order as the last
    // update's parallel phase left them, the serial phase at 'next'; NULL
    // without, or 0 players when the phase did not run.
    mtwnWork* works;
    mtwnStaged* staged;
    uint32_t stagedCount;
    uint32_t stagedNext;
    // The bytes the live clips' blocks hold.
    uint64_t clipStorageUsed;
    // The index1 of each target changed since the last report, in order.
    uint32_t* changed;
    uint32_t changedCount;
    // Whether motion is reduced (record mtwn-0049), and the clips' players
    // the last update left running under it though not essential.
    bool reducedMotion;
    // For tests: every record counted, none left out as certainly missed
    // or settled, to check that leaving them out changes nothing.
    bool countEvery;
    uint32_t playingOn;
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

// A player's id from its slot.
static inline mtwnPlayerId mtwnPlayerIdAt(const mtwnContext* context, uint32_t index1)
{
    return (mtwnPlayerId){index1, mtwnPlayerAt(context, index1)->header.generation};
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

// The clips' players' part of an update (src/player.c), reached only
// through the context, so that a host that makes no clip's player links
// none of the players, clip sampling, composition, paths or crossings
// (record mtwn-0047).
typedef struct mtwnClipUpdate
{
    // Before the players: their targets marked unwritten, and with the
    // host's task system the parallel phase run.
    void (*begin)(mtwnContext* context, double before, double now);
    // A clip's player advanced from the last update's host time to this
    // one's, what it crossed recorded and it sampled; freed when it has
    // finished and is not kept.
    void (*advance)(mtwnContext* context, uint32_t player, double before, double now);
    // After them: a target no clip's player wrote shows its base.
    void (*end)(mtwnContext* context);
    // A target's velocity left to be computed when read by a clip's track.
    mtwnVec4 (*velocity)(const mtwnContext* context, const mtwnTargetSlot* target);
} mtwnClipUpdate;

// Writes a target's value and velocity, marking it changed when its value
// changed and written in this update (src/update.c).
void mtwnWriteTarget(mtwnContext* context, uint32_t index1, mtwnVec4 value, mtwnVec4 velocity);

// A target's velocity: its own, or for a lazy one its transition's at the
// last update's time, in the library's floating-point environment
// (src/update.c).
mtwnVec4 mtwnTargetVelocity(const mtwnContext* context, uint32_t index1);

// An eased transition's progress through its easing at the last update's
// time: 0 before it starts (src/update.c).
double mtwnEasedProgress(const mtwnContext* context, uint32_t player);

// Stores a lazy target's velocity in it, before its transition goes, its
// value changes or its clip goes.
void mtwnSettleVelocity(mtwnContext* context, uint32_t index1);

// Stores the velocities of the targets left lazy by a clip's tracks, before
// the clip goes.
void mtwnSettleClipVelocities(mtwnContext* context, uint32_t clip);

// A target's velocity left to be computed by a clip's track
// (src/clip_sample.c).
mtwnVec4 mtwnClipTrackVelocity(const mtwnContext* context, const mtwnTargetSlot* target);

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
void mtwnSweepEvents(mtwnContext* context, mtwnWork* work, uint32_t player, double to, double rate,
                     double hostFrom, double hostTo);
// The same for a player near the last the work advanced: one in step with
// the last noted takes its records, the second in step keeps its own
// (record mtwn-0057).
void mtwnSweepEventsNear(mtwnContext* context, mtwnWork* work, uint32_t player, double to,
                         double rate, double hostFrom, double hostTo);

// Records a player's event of the player itself: a seek, a cancel, a
// transition's start or end.
void mtwnPlayerEvent(mtwnContext* context, uint32_t player, mtwnEventKind kind, double hostTime,
                     double clipTime);

// Samples a clip's player's tree at its current time into its targets:
// rate is how fast its time moves (0 when held), backwards its playback
// direction; committing writes the values as the bases too
// (src/clip_sample.c).
void mtwnSampleClip(mtwnContext* context, mtwnWork* work, const mtwnClipPlay* play, double current,
                    double rate, bool backwards, bool commit);
// The same for a player near the last the work advanced, as
// mtwnSweepEventsNear: its track results taken or kept.
void mtwnSampleClipNear(mtwnContext* context, mtwnWork* work, const mtwnClipPlay* play,
                        double current, double rate, bool backwards, bool commit);

// The groups of clips' players the parallel phase hands the host, and the
// node times each group keeps: a clip of more nodes plays serially.
#define MTWN_GROUPS      16u
#define MTWN_GROUP_NODES 128u
// The most event records a group stages.
#define MTWN_GROUP_RECORDS 1024u

// A clip track's result for its target before it is composed there: its
// own value and velocity, or, left lazy, its lazy velocity's bytes
// (src/clip_sample.c).
typedef struct mtwnTrackOut
{
    mtwnVec4 value;
    mtwnVec4 velocity;
    double weight;
    uint32_t target;
    mtwnValueKind kind;
    mtwnComposite composite;
    bool lazy;
    bool commit;
    uint8_t reserved[4];
} mtwnTrackOut;

// A clip track's result as the last player of its clip in a work made it,
// for the next player in step (record mtwn-0057): its own value and
// timing results, its parent's time a moment ago and way, and its
// velocity once worked out.
typedef struct mtwnSharedTrack
{
    mtwnVec4 value;
    mtwnVec4 velocity;
    double progress;
    double iteration;
    double earlier;
    double earlierIteration;
    double parentBefore;
    bool earlierKnown;
    bool parentBackwards;
    // Worked out for the last player; with an effect; velocity known.
    bool known;
    bool effect;
    bool velocityKnown;
    uint8_t reserved[3];
} mtwnSharedTrack;

// A clip's player's records as the last player of its clip in a work made
// them, for the next one sweeping the same way.
typedef struct mtwnSharedRecord
{
    mtwnEvent event;
    double along;
} mtwnSharedRecord;

// The most nodes and records a work shares among players in step.
#define MTWN_SHARED_NODES   128u
#define MTWN_SHARED_RECORDS 64u

// What the last player in a work left for the next in step: the sample's
// key and its tracks, and the sweep's key and its records. A player only
// notes its key; the next in step with it keeps its results, for the rest
// to take, so players out of step keep nothing. A clip of more nodes, or a
// sweep of more records, is not shared.
typedef struct mtwnSharing
{
    mtwnSharedTrack* tracks;
    mtwnSharedRecord* records;
    uint32_t trackCapacity;
    uint32_t recordCapacity;
    // The last player's clip by index and its time: a player of both is
    // near it, and only one near checks the keys below.
    uint32_t nearClip;
    double nearCurrent;
    // The sample's key: the clip by index and generation, since a slot is
    // reused by a later clip. Whether a player commits is its own.
    mtwnClipId sampleClip;
    double current;
    double rate;
    bool backwards;
    // Whether a key is noted, and whether the tracks hold its results.
    bool sampleValid;
    bool sampleKept;
    // The sweep's key, noted, and whether the records hold its records.
    bool sweepValid;
    bool sweepKept;
    // Whether the sweep running keeps its records.
    bool capture;
    mtwnClipId sweepClip;
    uint32_t recordCount;
    double from;
    double to;
    double sweepRate;
    double hostFrom;
    double hostTo;
    bool fromNone;
    bool turning;
    bool negative;
    uint8_t reserved[3];
} mtwnSharing;

// What a clip's player's advance works in: the crossing levels and node
// times it uses as scratch, and, staging, where its records and track
// results go instead of the queue and the targets. The serial work stages
// nothing and uses the context's levels and each clip's own node times.
typedef struct mtwnWork
{
    mtwnLevel* levels;
    // NULL for each clip's own.
    mtwnNodeTime* times;
    // NULL to apply as made; else staged up to their capacities.
    mtwnEvent* records;
    double* recordAlong;
    mtwnTrackOut* tracks;
    uint32_t recordCapacity;
    uint32_t recordCount;
    uint32_t trackCapacity;
    uint32_t trackCount;
    // Records past a capacity equal to the queue's, which a full queue
    // would drop in the serial order anyway.
    uint64_t dropped;
    // Whether staging ran out for the player being advanced.
    bool overflow;
    uint8_t reserved[7];
    // What players in step share in this work.
    mtwnSharing sharing;
} mtwnWork;

// A clip's player as the parallel phase left it: its staged records and
// track results in its group's work, ending at the next one's start.
typedef struct mtwnStaged
{
    uint32_t player;
    uint32_t recordStart;
    uint32_t recordEnd;
    uint32_t trackStart;
    uint32_t trackEnd;
    uint8_t group;
    // Left whole to the serial phase.
    bool serial;
    // Finished and not kept: freed once applied.
    bool done;
    uint8_t reserved;
} mtwnStaged;

// Writes a track's result into its target, composed onto what lies under
// it there (src/clip_sample.c).
void mtwnApplyTrack(mtwnContext* context, const mtwnTrackOut* out);

// Advances a clip's player from the last update's host time to this one's
// in a work, what it crossed recorded and it sampled: true when it has
// finished and is not kept, to be freed (src/player.c).
bool mtwnAdvanceClipPlayer(mtwnContext* context, mtwnWork* work, uint32_t player, double before,
                           double now);

// The parallel phase: the clips' players in creation order, in groups on
// the host's task system, staged; nothing without one (src/clip_tasks.c).
void mtwnRunClipGroups(mtwnContext* context, double before, double now);

// The serial pass's part for the next clip's player the phase staged: its
// records queued and its results applied, or, left to this pass, advanced
// here; true when it has finished and is not kept.
bool mtwnApplyStaged(mtwnContext* context, uint32_t player, double before, double now);

// After the serial pass: the records the groups dropped counted.
void mtwnEndClipGroups(mtwnContext* context);

// Queues a record, or stages it in a staging work (src/event.c).
void mtwnWorkEvent(mtwnContext* context, mtwnWork* work, const mtwnEvent* event, double along);

// Marks a clip's player's targets unwritten, before the update's players
// run.
void mtwnMarkUnwritten(mtwnContext* context, const mtwnClipPlay* play);

// Shows their bases in a clip's player's targets no player wrote.
void mtwnResetUnwritten(mtwnContext* context, const mtwnClipPlay* play);

#endif
