// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Players (records mtwn-0004 and mtwn-0029): what writes targets. A
// transition is a player; a player ends by committing its last value into
// its targets' base values and is then freed, its id stale.

#ifndef MAUL_TWEEN_PLAYER_H
#define MAUL_TWEEN_PLAYER_H

#include "maul-tween/base.h"
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
