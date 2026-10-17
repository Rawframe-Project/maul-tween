// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context (records mtwn-0003, mtwn-0006 and mtwn-0028): the owner of
// targets, clips and players, made from a def that names every capacity
// and allocated in one block through the host's allocator. Nothing
// allocates after creation.

#ifndef MAUL_TWEEN_CONTEXT_H
#define MAUL_TWEEN_CONTEXT_H

#include "maul-tween/base.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // A context. Made by mtwnCreateContext.
    typedef struct mtwnContext mtwnContext;

    // A target the context holds, by a generation-checked id; zero is null.
    typedef struct mtwnTargetId
    {
        uint32_t index1;
        uint32_t generation;
    } mtwnTargetId;

    // The most of each count a def may name, and the most clip storage.
#define MTWN_MAX_COUNT        16777216u
#define MTWN_MAX_CLIP_STORAGE 2147483648u

    // A range of work items, [startIndex, endIndex), run by the host on any
    // thread. The library passes taskContext through unchanged.
    typedef void mtwnTaskFn(int32_t startIndex, int32_t endIndex, void* taskContext);

    // Hands the host itemCount items of work: the host splits [0, itemCount)
    // into ranges of at least minRange items (the last may be shorter),
    // runs task once on each range, on any thread and in any order, and
    // returns a handle that finishTask waits on. A host that ran every range
    // before returning may return NULL, and finishTask is then not called.
    // Results never depend on the split or the order: each item writes only
    // its own memory.
    typedef void* mtwnEnqueueTaskFn(mtwnTaskFn* task, int32_t itemCount, int32_t minRange,
                                    void* taskContext, void* userContext);

    // Waits until every range of the work behind userTask has run.
    typedef void mtwnFinishTaskFn(void* userTask, void* userContext);

    // What a context holds: its allocator, the host's task system and every
    // capacity. Start from mtwnDefaultContextDef and set what differs.
    typedef struct mtwnContextDef
    {
        // Set by mtwnDefaultContextDef; checked by mtwnCreateContext.
        uint32_t cookie;
        // The headers' MTWN_ABI_VERSION.
        uint32_t version;
        // The allocator; zeroed means the C library's.
        mtwnAllocator allocator;
        // The host's task system, which mtwnUpdate hands the clips' players
        // to in up to 16 groups (record mtwn-0056); both NULL, the default,
        // runs every player on the calling thread. One without the other is
        // invalid. With them the context holds each group's scratch: about
        // 16 KB, 72 bytes for each event record up to 1,024 and 60 for each
        // of a sixteenth of twice the bindings (about 900 KB more for the
        // default def), and 36 bytes a player.
        mtwnEnqueueTaskFn* enqueueTask;
        mtwnFinishTaskFn* finishTask;
        // Passed to enqueueTask and finishTask as their userContext.
        void* userTaskContext;
        // Targets alive at once, up to MTWN_MAX_COUNT.
        uint32_t targetCapacity;
        // Players alive at once, transitions included, up to
        // MTWN_MAX_COUNT.
        uint32_t playerCapacity;
        // Clips alive at once, up to MTWN_MAX_COUNT.
        uint32_t clipCapacity;
        // Event records the queue holds between reads, up to
        // MTWN_MAX_COUNT; 0 makes none, and clips' players skip the
        // sweeps that find their crossings (record mtwn-0062).
        uint32_t eventCapacity;
        // Bindings of slots to targets the players hold at once, up to
        // MTWN_MAX_COUNT.
        uint32_t bindingCapacity;
        // The most nodes a clip may have for players in step to share its
        // sample, and the most records a sweep keeps to share (record
        // mtwn-0057), each up to MTWN_MAX_COUNT; 0 shares none of that
        // kind. The context holds 80 and 72 bytes for each, in its own
        // work and in each group's with a task system.
        uint32_t sharedNodeCapacity;
        uint32_t sharedRecordCapacity;
        uint32_t reserved;
        // Bytes for the clips' contents, up to MTWN_MAX_CLIP_STORAGE.
        uint64_t clipStorageBytes;
    } mtwnContextDef;

// The cookie mtwnDefaultContextDef sets and mtwnCreateContext checks.
#define MTWN_CONTEXT_DEF_COOKIE 0x4E575454u

    /// Makes a context def for a medium host: 1024 targets, 256 players, 16
    /// clips, 256 events, 1024 bindings, sharing for clips of up to 128
    /// nodes and 64 records, and 208 KiB of clip storage, which holds 16
    /// clips of the default clip def, the C library's allocator and no task
    /// system.
    /// Built in the program, so it carries the version of the headers the
    /// program includes.
    ///
    /// @return The def.
    /// @par Thread safety
    /// Safe from any thread.
    static inline mtwnContextDef mtwnDefaultContextDef(void)
    {
#ifdef __cplusplus
        mtwnContextDef def = {};
#else
    mtwnContextDef def = {0};
#endif
        def.cookie = MTWN_CONTEXT_DEF_COOKIE;
        def.version = MTWN_ABI_VERSION;
        def.targetCapacity = 1024;
        def.playerCapacity = 256;
        def.clipCapacity = 16;
        def.eventCapacity = 256;
        def.bindingCapacity = 1024;
        def.sharedNodeCapacity = 128;
        def.sharedRecordCapacity = 64;
        def.clipStorageBytes = 16u * 13312u;
        return def;
    }

    /// Makes a context: checks the def and allocates everything the context
    /// holds in one block.
    ///
    /// @param def         The def.
    /// @param contextOut  Receives the context, or NULL on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a def
    ///         without its cookie, an allocator or a task system with one
    ///         function of the two, or a capacity past its maximum; mtwn_errorVersion for a
    ///         def built against another major or minor version, before
    ///         anything else in it is read; mtwn_errorCapacity when the
    ///         allocator fails or the block's size does not fit.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnCreateContext(const mtwnContextDef* def,
                                                         mtwnContext** contextOut);

    /// Destroys a context and everything it holds.
    ///
    /// @param context  The context, or NULL.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API void mtwnDestroyContext(mtwnContext* context);

    /// Grows a context to a def's capacities, each at least the context's
    /// own but the sharing's, which may be any, and takes the def's task
    /// system, with the allocator it was made
    /// with: the one time after its creation it allocates, and only when
    /// asked (it never grows by itself). Its objects move to one new block, their ids unchanged,
    /// and the old block is freed, so the context's address changes: the call writes the new one.
    /// On any failure nothing changes.
    ///
    /// @param contextInOut  The context; receives the grown one.
    /// @param def           The capacities: start from the context's own.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a def
    ///         without its cookie, a capacity below the context's or past
    ///         its limit, another allocator, or a task system with one
    ///         function of the two; mtwn_errorVersion for a def of another
    ///         version; mtwn_errorCapacity when the allocator fails.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnGrowContext(mtwnContext** contextInOut,
                                                       const mtwnContextDef* def);

    /// Returns how many calls against the context were refused for an
    /// invalid argument or a stale id since it was made.
    ///
    /// @param context  The context.
    /// @return The count.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API uint64_t mtwnContext_GetMisuseCount(const mtwnContext* context);

    /// Returns how many clip players' advances a context with a task system
    /// ran on the calling thread since it was made (record mtwn-0056): a
    /// player whose clip has more than 128 nodes and other players, one
    /// whose targets of its
    /// own might not fit its group's staging, one whose staging ran out,
    /// and those after it in its group. A count that grows asks for larger
    /// binding or event capacities, or smaller clips.
    ///
    /// @param context  The context.
    /// @return The count; 0 without a task system.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API uint64_t mtwnContext_GetSerialAdvances(const mtwnContext* context);

    /// Returns the size of the block the context holds everything in.
    ///
    /// @param context  The context.
    /// @return Bytes.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API uint64_t mtwnContext_GetMemoryBytes(const mtwnContext* context);

    /// Sets reduced motion (record mtwn-0049), off when a context is made.
    /// While it is on, every transition and every running clip's player
    /// with a finite end finishes at the next update, its delay skipped and
    /// its events at that update's time, unless its def marks it
    /// essential; a player with an infinite end plays on, the host's to
    /// keep, seek or stop. Turning it off reaches what has not finished.
    /// The host reads the platform's preference and sets it.
    ///
    /// @param context  The context.
    /// @param reduced  Whether motion is reduced.
    /// @return mtwn_success; mtwn_errorInvalid for a null context.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnContext_SetReducedMotion(mtwnContext* context,
                                                                    bool reduced);

    /// Returns whether motion is reduced.
    ///
    /// @param context  The context, or NULL.
    /// @return Whether it is; false for a null context.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API bool mtwnContext_IsReducedMotion(const mtwnContext* context);

    /// Returns how many clips' players the last update left running under
    /// reduced motion though not essential: those that could not finish,
    /// their end infinite or their rate 0. When it is not 0, the host finds
    /// them by mtwnPlayer_GetState's playsOnReduced and keeps, seeks or
    /// stops each.
    ///
    /// @param context  The context, or NULL.
    /// @return The count; 0 for a null context.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API uint32_t mtwnContext_GetPlayingOnCount(const mtwnContext* context);

    /// Advances every player to the host's time, in the order they were
    /// made, later ones writing over earlier ones: transitions sample their
    /// curves, write their targets and end; clips' players sample their
    /// clips into their bound targets, and those that finish, unless kept,
    /// commit what they fill forward as their targets' bases and are freed.
    /// A target bound to a clip's player that no player writes shows its
    /// base. Reports the targets whose value
    /// or base changed since the last report, in the order they changed;
    /// a target appears once.
    ///
    /// @param context          The context.
    /// @param timeNs           The host's time in nanoseconds, never less
    ///                         than the last update's.
    /// @param changedOut       Receives the first changedCapacity changed
    ///                         targets; NULL only with a capacity of 0.
    /// @param changedCapacity  Its length.
    /// @param changedTotalOut  Receives how many targets changed, which may
    ///                         exceed the capacity; those past it are left
    ///                         out of the report only. May be NULL.
    /// @return mtwn_success; mtwn_errorInvalid for a null context, a null
    ///         array with a capacity, or a time before the last update's,
    ///         nothing advanced.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnUpdate(mtwnContext* context, uint64_t timeNs,
                                                  mtwnTargetId* changedOut,
                                                  uint32_t changedCapacity,
                                                  uint32_t* changedTotalOut);

#ifdef __cplusplus
}
#endif

#endif
