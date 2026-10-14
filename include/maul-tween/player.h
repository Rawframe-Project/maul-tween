// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Players (records mtwn-0004, mtwn-0012, mtwn-0029 and mtwn-0035): what
// writes targets. A player plays a finished clip on targets bound to its
// slots, by Web Animations' procedures on the host's clock; a transition
// is a player too. A player not kept ends by committing its last value
// into its targets' base values and is then freed, its id stale.

#ifndef MAUL_TWEEN_PLAYER_H
#define MAUL_TWEEN_PLAYER_H

#include "maul-tween/base.h"
#include "maul-tween/clip.h"
#include "maul-tween/context.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // A player, by a generation-checked id; zero is null.
    typedef struct mtwnPlayerId
    {
        uint32_t index1;
        uint32_t generation;
    } mtwnPlayerId;

    // A slot of a clip bound to a target.
    typedef struct mtwnBinding
    {
        uint32_t slot;
        uint32_t reserved;
        mtwnTargetId target;
    } mtwnBinding;

    // How a player is made. Start from mtwnDefaultPlayerDef.
    typedef struct mtwnPlayerDef
    {
        // Set by mtwnDefaultPlayerDef; checked by mtwnCreatePlayer.
        uint32_t cookie;
        // The headers' MTWN_ABI_VERSION.
        uint32_t version;
        // A finished clip.
        mtwnClipId clip;
        // Its slots' targets, copied; each slot at most once and used by the
        // clip's tracks; a slot left out plays on nothing.
        const mtwnBinding* bindings;
        uint32_t bindingCount;
        // Whether it plays at once, starting at the next update.
        bool play;
        // Whether it is kept when it ends, rather than committed and freed.
        bool keep;
        uint8_t reserved[2];
        // The playback rate, finite; negative plays backwards, 0 holds.
        double rate;
    } mtwnPlayerDef;

// The cookie mtwnDefaultPlayerDef sets and mtwnCreatePlayer checks.
#define MTWN_PLAYER_DEF_COOKIE 0x59414C50u

    /// Makes a player def: no clip, no bindings, playing at once at rate 1,
    /// freed when it ends. Built in the program, so it carries the version of
    /// the headers the program includes.
    ///
    /// @return The def.
    /// @par Thread safety
    /// Safe from any thread.
    static inline mtwnPlayerDef mtwnDefaultPlayerDef(void)
    {
#ifdef __cplusplus
        mtwnPlayerDef def = {};
#else
    mtwnPlayerDef def = {0};
#endif
        def.cookie = MTWN_PLAYER_DEF_COOKIE;
        def.version = MTWN_ABI_VERSION;
        def.play = true;
        def.rate = 1.0;
        return def;
    }

    // A player's play state, Web Animations'.
    typedef uint8_t mtwnPlayState;
    enum
    {
        // No current time: it has no effect.
        mtwn_playIdle = 0,
        // Its current time moves with the host's.
        mtwn_playRunning = 1,
        // Its current time is held.
        mtwn_playPaused = 2,
        // At the end of its playback direction.
        mtwn_playFinished = 3,
    };

    // A player as it is, at the last update's time.
    typedef struct mtwnPlayerState
    {
        // Seconds into the clip; NaN when idle.
        double currentTime;
        double rate;
        // The clip's duration.
        double duration;
        mtwnPlayState state;
        // Whether it waits for the next update to start.
        bool pending;
        uint8_t reserved[6];
    } mtwnPlayerState;

    /// Makes a player of a finished clip, which it holds busy until it is
    /// freed; it starts at the next update if the def says play, else idle.
    ///
    /// @param context    The context.
    /// @param def        The def.
    /// @param playerOut  Receives the player's id, or the null id on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a def
    ///         without its cookie, a rate not finite, a binding outside the
    ///         above; mtwn_errorVersion for a def of another version;
    ///         mtwn_errorStale for a clip that is not alive; mtwn_errorState
    ///         for one not finished; mtwn_errorLimit when the context holds
    ///         its capacity of players or of bindings.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnCreatePlayer(mtwnContext* context,
                                                        const mtwnPlayerDef* def,
                                                        mtwnPlayerId* playerOut);

    /// Plays a clip's player, as Web Animations plays an animation with
    /// auto-rewind: from the start (or the end, backwards) when idle, at
    /// its end or before its start; a paused player resumes. One that needs
    /// a start time takes the next update's.
    ///
    /// @param context  The context.
    /// @param player   A clip's player.
    /// @return mtwn_success; mtwn_errorInvalid for a null context;
    ///         mtwn_errorStale for an id that names no live player;
    ///         mtwn_errorState for a transition, or backwards from an end
    ///         that is infinite.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Play(mtwnContext* context, mtwnPlayerId player);

    /// Pauses a clip's player at its current time, or at its start (its
    /// end, backwards) when idle.
    ///
    /// @param context  The context.
    /// @param player   A clip's player.
    /// @return As mtwnPlayer_Play.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Pause(mtwnContext* context, mtwnPlayerId player);

    /// Reverses a clip's player: negates its rate, keeping its current time,
    /// and plays it with auto-rewind; refused, and nothing changed, when that
    /// would play an infinite clip backwards from its end.
    ///
    /// @param context  The context.
    /// @param player   A clip's player.
    /// @return As mtwnPlayer_Play.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Reverse(mtwnContext* context,
                                                          mtwnPlayerId player);

    /// Finishes a clip's player: its current time goes to the end of its
    /// playback direction, the clip's end forwards, 0 backwards.
    ///
    /// @param context  The context.
    /// @param player   A clip's player.
    /// @return As mtwnPlayer_Play; mtwn_errorState also for a rate of 0 or
    ///         an infinite clip played forwards.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Finish(mtwnContext* context, mtwnPlayerId player);

    /// Cancels a clip's player: it becomes idle, without effect, and is
    /// kept, to be played again.
    ///
    /// @param context  The context.
    /// @param player   A clip's player.
    /// @return As mtwnPlayer_Play.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Cancel(mtwnContext* context, mtwnPlayerId player);

    /// Seeks a clip's player to a current time.
    ///
    /// @param context  The context.
    /// @param player   A clip's player.
    /// @param seconds  The time into the clip, finite.
    /// @return As mtwnPlayer_Play; mtwn_errorInvalid also for a time not
    ///         finite.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_Seek(mtwnContext* context, mtwnPlayerId player,
                                                       double seconds);

    /// Seeks a clip's player to a share of its clip's duration.
    ///
    /// @param context   The context.
    /// @param player    A clip's player.
    /// @param progress  The share, finite; the duration must be finite.
    /// @return As mtwnPlayer_Seek; mtwn_errorState also for an infinite
    ///         duration.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_SeekProgress(mtwnContext* context,
                                                               mtwnPlayerId player,
                                                               double progress);

    /// Seeks a clip's player to a label of its clip's root.
    ///
    /// @param context  The context.
    /// @param player   A clip's player.
    /// @param key      The label's key.
    /// @return As mtwnPlayer_Play; mtwn_errorInvalid also for no such label.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_SeekLabel(mtwnContext* context,
                                                            mtwnPlayerId player, uint32_t key);

    /// Sets a clip's player's rate, keeping its current time.
    ///
    /// @param context  The context.
    /// @param player   A clip's player.
    /// @param rate     Finite; negative plays backwards, 0 holds.
    /// @return As mtwnPlayer_Seek.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_SetRate(mtwnContext* context, mtwnPlayerId player,
                                                          double rate);

    /// Reads a clip's player's state at the last update's time.
    ///
    /// @param context   The context.
    /// @param player    A clip's player.
    /// @param stateOut  Receives the state.
    /// @return As mtwnPlayer_Play; mtwn_errorInvalid also for a null
    ///         stateOut.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnPlayer_GetState(mtwnContext* context,
                                                           mtwnPlayerId player,
                                                           mtwnPlayerState* stateOut);

    /// Stops a player without committing: its targets show their base
    /// values again at once.
    ///
    /// @param context  The context.
    /// @param player   The player.
    /// @return mtwn_success; mtwn_errorInvalid for a null context;
    ///         mtwn_errorStale for an id that names no live player, as one
    ///         that ended does.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnDestroyPlayer(mtwnContext* context, mtwnPlayerId player);

    /// Tells whether an id names a live player: one that has not ended, been
    /// replaced or been destroyed. Not counted as misuse.
    ///
    /// @param context  The context, or NULL.
    /// @param player   The id.
    /// @return Whether the player is alive.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_API bool mtwnPlayer_IsValid(const mtwnContext* context, mtwnPlayerId player);

#ifdef __cplusplus
}
#endif

#endif
