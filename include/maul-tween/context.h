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

    // What a context holds: its allocator and every capacity. Start from
    // mtwnDefaultContextDef and set what differs.
    typedef struct mtwnContextDef
    {
        // Set by mtwnDefaultContextDef; checked by mtwnCreateContext.
        uint32_t cookie;
        // The headers' MTWN_ABI_VERSION.
        uint32_t version;
        // The allocator; zeroed means the C library's.
        mtwnAllocator allocator;
        // Targets alive at once, up to MTWN_MAX_COUNT.
        uint32_t targetCapacity;
        // Players alive at once, transitions included, up to
        // MTWN_MAX_COUNT.
        uint32_t playerCapacity;
        // Clips alive at once, up to MTWN_MAX_COUNT.
        uint32_t clipCapacity;
        // Event records the queue holds between reads, up to
        // MTWN_MAX_COUNT.
        uint32_t eventCapacity;
        // Bindings of slots to targets the players hold at once, up to
        // MTWN_MAX_COUNT.
        uint32_t bindingCapacity;
        uint32_t reserved;
        // Bytes for the clips' contents, up to MTWN_MAX_CLIP_STORAGE.
        uint64_t clipStorageBytes;
    } mtwnContextDef;

// The cookie mtwnDefaultContextDef sets and mtwnCreateContext checks.
#define MTWN_CONTEXT_DEF_COOKIE 0x4E575454u

    /// Makes a context def for a medium host: 1024 targets, 256 players, 64
    /// clips, 256 events, 1024 bindings and 64 KiB of clip storage, the C
    /// library's allocator. Built in the program, so it carries the version of the
    /// headers the program includes.
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
        def.clipCapacity = 64;
        def.eventCapacity = 256;
        def.bindingCapacity = 1024;
        def.clipStorageBytes = 65536;
        return def;
    }

    /// Makes a context: checks the def and allocates everything the context
    /// holds in one block.
    ///
    /// @param def         The def.
    /// @param contextOut  Receives the context, or NULL on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a def
    ///         without its cookie, an allocator with one function of the
    ///         two, or a capacity past its maximum; mtwn_errorVersion for a
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

    /// Returns how many calls against the context were refused for an
    /// invalid argument or a stale id since it was made.
    ///
    /// @param context  The context.
    /// @return The count.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API uint64_t mtwnContext_GetMisuseCount(const mtwnContext* context);

    /// Returns the size of the block the context holds everything in.
    ///
    /// @param context  The context.
    /// @return Bytes.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API uint64_t mtwnContext_GetMemoryBytes(const mtwnContext* context);

    /// Advances every player to the host's time: transitions sample their
    /// curves, write their targets and end. Reports the targets whose value
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
