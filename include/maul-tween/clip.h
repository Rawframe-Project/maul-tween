// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Clips (records mtwn-0004, mtwn-0013, mtwn-0033 and mtwn-0034):
// immutable descriptions of tracks on nested timelines with labels and
// marks, built node by node at positions
// resolved when added, then finished. A finished clip is read through the
// values layer from any thread while it lives, and plays on any set of
// targets, its tracks addressing slots.

#ifndef MAUL_TWEEN_CLIP_H
#define MAUL_TWEEN_CLIP_H

#include "maul-tween/base.h"
#include "maul-tween/context.h"
#include "maul-tween/path.h"
#include "maul-tween/timing.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // A clip, by a generation-checked id; zero is null.
    typedef struct mtwnClipId
    {
        uint32_t index1;
        uint32_t generation;
    } mtwnClipId;

    // A node of a clip: its 1-based index in the clip, returned when it is
    // added. Node 1 is the clip's root timeline.
    typedef uint32_t mtwnNode;
#define MTWN_ROOT_NODE 1u

    // What a node is.
    typedef uint8_t mtwnNodeKind;
    enum
    {
        // Keyframes on a slot.
        mtwn_nodeTrack = 1,
        // A timeline of children: the root, a group, a sequence.
        mtwn_nodeTimeline = 2,
        // A named time in its timeline, for positions and seeking.
        mtwn_nodeLabel = 3,
        // An instant that gives events when crossed.
        mtwn_nodeMark = 4,
    };

    // Where a child starts in its parent.
    typedef uint8_t mtwnPositionKind;
    enum
    {
        // At offset seconds.
        mtwn_positionAbsolute = 0,
        // At the parent's end so far, plus offset.
        mtwn_positionAfterEnd = 1,
        // With the previous child's start, plus offset; 0 when none.
        mtwn_positionWithPrevious = 2,
        // At the previous child's end, plus offset; 0 when none.
        mtwn_positionAfterPrevious = 3,
        // At the label of the same parent named by label, plus offset.
        mtwn_positionAtLabel = 4,
    };

    // A position, resolved to a start time when the child is added; one
    // that resolves to no finite start is refused, as after the end of an
    // endless timeline. The previous child is the last track or timeline
    // added to the parent; labels and marks are not.
    typedef struct mtwnPosition
    {
        // Seconds, finite; may be negative.
        double offset;
        // For mtwn_positionAtLabel: the label's key.
        uint32_t label;
        mtwnPositionKind kind;
        // 0: a call refuses another value (record mtwn-0071).
        uint8_t reserved[3];
    } mtwnPosition;

    // What a clip may hold. Start from mtwnDefaultClipDef.
    typedef struct mtwnClipDef
    {
        // Set by mtwnDefaultClipDef; checked by mtwnCreateClip.
        uint32_t cookie;
        // The headers' MTWN_ABI_VERSION.
        uint32_t version;
        // Nodes besides the root (tracks, timelines, labels, marks),
        // keyframes and binding slots, each up to MTWN_MAX_COUNT.
        uint32_t nodeCapacity;
        uint32_t keyframeCapacity;
        uint32_t slotCapacity;
        // 0: a call refuses another value (record mtwn-0071).
        uint32_t reserved;
    } mtwnClipDef;

// The cookie mtwnDefaultClipDef sets and mtwnCreateClip checks.
#define MTWN_CLIP_DEF_COOKIE 0x50494C43u

    /// Makes a clip def for a small clip: 16 nodes, 32 keyframes, 8 slots,
    /// about 13 KB of clip storage (mtwnGetClipBytes says how much). Built
    /// in the program, so it carries the version of the headers the
    /// program includes.
    ///
    /// @return The def.
    /// @par Thread safety
    /// Safe from any thread.
    static inline mtwnClipDef mtwnDefaultClipDef(void)
    {
#ifdef __cplusplus
        mtwnClipDef def = {};
#else
    mtwnClipDef def = {0};
#endif
        def.cookie = MTWN_CLIP_DEF_COOKIE;
        def.version = MTWN_ABI_VERSION;
        def.nodeCapacity = 16;
        def.keyframeCapacity = 32;
        def.slotCapacity = 8;
        return def;
    }

    // A track to add: where, on which slot, what and when. Start from
    // mtwnDefaultClipTrackDef.
    typedef struct mtwnClipTrackDef
    {
        // Set by mtwnDefaultClipTrackDef; checked by mtwnClip_AddTrack.
        uint32_t cookie;
        // The headers' MTWN_ABI_VERSION.
        uint32_t version;
        // The timeline it is added to.
        mtwnNode parent;
        // The binding slot it writes, below the clip's slot capacity; a slot
        // keeps one value kind.
        uint32_t slot;
        mtwnPosition position;
        // The kind and its choices.
        mtwnTrackDef track;
        // The node's timing: its duration is one pass of the keyframes.
        mtwnTiming timing;
        // The keyframes, copied into the clip.
        const mtwnKeyframe* keyframes;
        uint32_t keyframeCount;
        // Whether it travels at constant speed (numbers, vectors, angles,
        // integers).
        bool constantSpeed;
        // Whether each iteration moves on from the last (Web Animations
        // Level 2's iteration accumulate, as DOTween's incremental loops
        // do): iteration k adds k times its last keyframe minus its first,
        // a rotation turns by the rotation from first to last k times.
        // Numbers, vectors, angles, integers and rotations; not endless
        // iterations of no duration, which pass at once.
        bool incremental;
        // 0: a call refuses another value (record mtwn-0071).
        uint8_t reserved[2];
    } mtwnClipTrackDef;

// The cookie mtwnDefaultClipTrackDef sets and mtwnClip_AddTrack checks.
#define MTWN_CLIP_TRACK_DEF_COOKIE 0x4B435254u

    /// Makes a clip track def: on the root at time 0, slot 0, a scalar
    /// track (mtwnDefaultTrackDef), Web Animations' default timing with a
    /// duration of 1 second, no keyframes. Built in the program, so it
    /// carries the version of the headers the program includes.
    ///
    /// @return The def.
    /// @par Thread safety
    /// Safe from any thread.
    static inline mtwnClipTrackDef mtwnDefaultClipTrackDef(void)
    {
#ifdef __cplusplus
        mtwnClipTrackDef def = {};
#else
    mtwnClipTrackDef def = {0};
#endif
        def.cookie = MTWN_CLIP_TRACK_DEF_COOKIE;
        def.version = MTWN_ABI_VERSION;
        def.parent = MTWN_ROOT_NODE;
        def.track = mtwnDefaultTrackDef(mtwn_valueScalar);
        def.timing = mtwnDefaultTiming();
        def.timing.duration = 1.0;
        return def;
    }

    // A timeline to add. Start from mtwnDefaultClipTimelineDef.
    typedef struct mtwnClipTimelineDef
    {
        // Set by mtwnDefaultClipTimelineDef; checked by
        // mtwnClip_AddTimeline.
        uint32_t cookie;
        // The headers' MTWN_ABI_VERSION.
        uint32_t version;
        // The timeline it is added to.
        mtwnNode parent;
        // Whether its iteration duration is its latest child's end, kept as
        // children arrive (infinite after an endless child, which makes the
        // timeline endless); otherwise the timing's duration, which clips.
        bool autoDuration;
        // 0: a call refuses another value (record mtwn-0071).
        uint8_t reserved[3];
        mtwnPosition position;
        // Its timing; its easing must be monotonic.
        mtwnTiming timing;
    } mtwnClipTimelineDef;

// The cookie mtwnDefaultClipTimelineDef sets and mtwnClip_AddTimeline
// checks.
#define MTWN_CLIP_TIMELINE_DEF_COOKIE 0x454E4C54u

    /// Makes a clip timeline def: on the root at time 0, lasting to its
    /// latest child's end, Web Animations' default timing. Built in the
    /// program, so it carries the version of the headers the program
    /// includes.
    ///
    /// @return The def.
    /// @par Thread safety
    /// Safe from any thread.
    static inline mtwnClipTimelineDef mtwnDefaultClipTimelineDef(void)
    {
#ifdef __cplusplus
        mtwnClipTimelineDef def = {};
#else
    mtwnClipTimelineDef def = {0};
#endif
        def.cookie = MTWN_CLIP_TIMELINE_DEF_COOKIE;
        def.version = MTWN_ABI_VERSION;
        def.parent = MTWN_ROOT_NODE;
        def.autoDuration = true;
        def.timing = mtwnDefaultTiming();
        return def;
    }

    // Any node of a finished clip.
    typedef struct mtwnClipNodeInfo
    {
        // A timeline's with its iteration duration resolved.
        mtwnTiming timing;
        // Its start and end in its parent, seconds; equal for an instant.
        double start;
        double end;
        mtwnNode parent;
        // A track's slot, a label's or mark's key, else 0.
        uint32_t slot;
        uint32_t key;
        mtwnNodeKind kind;
        // Written 0 (record mtwn-0071).
        uint8_t reserved[3];
    } mtwnClipNodeInfo;

    // A finished clip's track, for sampling through the values layer: it
    // references the clip's storage, valid while the clip lives.
    typedef struct mtwnClipTrack
    {
        mtwnTrack track;
        mtwnTiming timing;
        // Its start in its parent, seconds.
        double start;
        mtwnNode parent;
        uint32_t slot;
    } mtwnClipTrack;

    /// Gives the clip storage a clip of a def takes, so a host can size its
    /// context's clipStorageBytes for the clips it makes.
    ///
    /// @param def         The clip's def.
    /// @param bytesOut    Receives the bytes; unchanged on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, a def
    ///         without its cookie or a capacity past MTWN_MAX_COUNT;
    ///         mtwn_errorVersion for a def of another version.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnGetClipBytes(const mtwnClipDef* def, uint64_t* bytesOut);

    /// Makes a clip: allocates its storage in one block, counted against the
    /// context's clip storage. Its root timeline, node 1, lasts as long as
    /// its latest child.
    ///
    /// @param context  The context.
    /// @param def      The def.
    /// @param clipOut  Receives the clip's id, or the null id on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a def
    ///         without its cookie or a capacity past MTWN_MAX_COUNT;
    ///         mtwn_errorVersion for a def of another major or minor
    ///         version; mtwn_errorLimit when the context holds its capacity
    ///         of clips or the storage would pass its clip storage;
    ///         mtwn_errorCapacity when the allocator fails.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnCreateClip(mtwnContext* context, const mtwnClipDef* def,
                                                      mtwnClipId* clipOut);

    /// Destroys a clip and frees its storage.
    ///
    /// @param context  The context.
    /// @param clip     The clip.
    /// @return mtwn_success; mtwn_errorInvalid for a null context;
    ///         mtwn_errorStale for an id that names no live clip;
    ///         mtwn_errorBusy while a player plays it.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnDestroyClip(mtwnContext* context, mtwnClipId clip);

    /// Tells whether an id names a live clip. Not counted as misuse.
    ///
    /// @param context  The context, or NULL.
    /// @param clip     The id.
    /// @return Whether the clip is alive.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API bool mtwnClip_IsValid(const mtwnContext* context, mtwnClipId clip);

    /// Adds a track: copies its keyframes into the clip, makes the track
    /// there (mtwnMakeTrack, with the lengths and mixes in the clip) and
    /// resolves its position to a start time.
    ///
    /// @param context  The context.
    /// @param clip     The clip, not finished.
    /// @param def      The def: a timeline of the clip as parent, a known
    ///                 position kind and a finite offset, a slot below the
    ///                 slot capacity and of the same kind as its other
    ///                 tracks, a timing mtwnValidateTiming accepts, and a
    ///                 track and keyframes mtwnMakeTrack accepts.
    /// @param nodeOut  Receives the node, or 0 on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a def
    ///         without its cookie or outside the above; mtwn_errorVersion
    ///         for a def of another version; mtwn_errorStale for a clip that
    ///         is not alive; mtwn_errorState for a finished clip;
    ///         mtwn_errorLimit when the clip holds its capacity of nodes or
    ///         keyframes.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_AddTrack(mtwnContext* context, mtwnClipId clip,
                                                         const mtwnClipTrackDef* def,
                                                         mtwnNode* nodeOut);

// The deepest a timeline nests below the root: a child of the root is at
// depth 1.
#define MTWN_MAX_CLIP_DEPTH 32u

    /// Adds a timeline: a group of children taking its transformed time.
    /// Its end in its parent follows its children as they arrive when its
    /// duration is theirs; children already placed after it keep their
    /// starts.
    ///
    /// @param context  The context.
    /// @param clip     The clip, not finished.
    /// @param def      The def: a timeline of the clip as parent, a position
    ///                 as for tracks, a timing mtwnValidateTiming accepts
    ///                 with a monotonic easing (mtwnIsEasingMonotonic).
    /// @param nodeOut  Receives the node, or 0 on failure.
    /// @return As mtwnClip_AddTrack; mtwn_errorLimit also for a timeline
    ///         past MTWN_MAX_CLIP_DEPTH.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_AddTimeline(mtwnContext* context, mtwnClipId clip,
                                                            const mtwnClipTimelineDef* def,
                                                            mtwnNode* nodeOut);

    /// Adds a label: a time in its parent named by a host key, unique in the
    /// parent, for positions and seeking. It is not a previous child and
    /// does not lengthen its parent.
    ///
    /// @param context   The context.
    /// @param clip      The clip, not finished.
    /// @param parent    A timeline of the clip.
    /// @param position  Where, as for tracks.
    /// @param key       The host's key.
    /// @param nodeOut   Receives the node, or 0 on failure.
    /// @return As mtwnClip_AddTrack; mtwn_errorInvalid also for a key the
    ///         parent already has.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_AddLabel(mtwnContext* context, mtwnClipId clip,
                                                         mtwnNode parent, mtwnPosition position,
                                                         uint32_t key, mtwnNode* nodeOut);

    /// Adds a mark: an instant in its parent that gives an event, with the
    /// host's key, each time playback crosses it. It is not a previous
    /// child and does not lengthen its parent.
    ///
    /// @param context   The context.
    /// @param clip      The clip, not finished.
    /// @param parent    A timeline of the clip.
    /// @param position  Where, as for tracks.
    /// @param key       The host's key; marks may share one.
    /// @param nodeOut   Receives the node, or 0 on failure.
    /// @return As mtwnClip_AddTrack.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_AddMark(mtwnContext* context, mtwnClipId clip,
                                                        mtwnNode parent, mtwnPosition position,
                                                        uint32_t key, mtwnNode* nodeOut);

    /// Finishes a clip: it can be read and played, and no longer changed.
    ///
    /// @param context  The context.
    /// @param clip     The clip.
    /// @return mtwn_success; mtwn_errorInvalid for a null context;
    ///         mtwn_errorStale for a clip that is not alive;
    ///         mtwn_errorState for a clip already finished.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_Finish(mtwnContext* context, mtwnClipId clip);

    /// Reads a finished clip's track node.
    ///
    /// @param context   The context.
    /// @param clip      The clip.
    /// @param node      A track node of the clip.
    /// @param trackOut  Receives the track, its timing, start, parent and slot;
    ///                  sample it with mtwnComputeTiming and mtwnSampleTrack
    ///                  from any thread while the clip lives; zero on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument or a node
    ///         that is not a track of the clip; mtwn_errorStale for a clip
    ///         that is not alive; mtwn_errorState for one not finished.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_GetTrack(mtwnContext* context, mtwnClipId clip,
                                                         mtwnNode node, mtwnClipTrack* trackOut);

    /// Reads any node of a finished clip.
    ///
    /// @param context  The context.
    /// @param clip     The clip.
    /// @param node     A node of the clip, the root included.
    /// @param infoOut  Receives what the node is; zero on failure.
    /// @return As mtwnClip_GetTrack, for any node.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_GetNode(mtwnContext* context, mtwnClipId clip,
                                                        mtwnNode node, mtwnClipNodeInfo* infoOut);

    /// Finds a label of a finished clip by its parent and key.
    ///
    /// @param context     The context.
    /// @param clip        The clip.
    /// @param parent      The timeline the label is in.
    /// @param key         The label's key.
    /// @param secondsOut  Receives its time in the parent; 0 on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a parent
    ///         that is not a timeline of the clip, or no such label;
    ///         mtwn_errorStale and mtwn_errorState as mtwnClip_GetTrack.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_FindLabel(mtwnContext* context, mtwnClipId clip,
                                                          mtwnNode parent, uint32_t key,
                                                          double* secondsOut);

    /// Reads a finished clip's duration: its root's end.
    ///
    /// @param context     The context.
    /// @param clip        The clip.
    /// @param secondsOut  Receives the duration; 0 on failure.
    /// @return As mtwnClip_GetTrack, without the node.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnClip_GetDuration(mtwnContext* context, mtwnClipId clip,
                                                            double* secondsOut);

#ifdef __cplusplus
}
#endif

#endif
