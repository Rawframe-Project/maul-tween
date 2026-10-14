// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Transitions (records mtwn-0004 and mtwn-0029): the short path, one call
// that animates a target from its current value to another. A new
// transition on a target replaces the one running there, starting from
// the value it had reached.

#ifndef MAUL_TWEEN_TRANSITION_H
#define MAUL_TWEEN_TRANSITION_H

#include "maul-tween/base.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A transition: the target, its end value and how it gets there. Start
    // from mtwnDefaultTransitionDef and set what differs.
    typedef struct mtwnTransitionDef
    {
        // Set by mtwnDefaultTransitionDef; checked by mtwnStartTransition.
        uint32_t cookie;
        // The headers' MTWN_ABI_VERSION.
        uint32_t version;
        mtwnTargetId target;
        // The end value, as the target's kind takes it.
        mtwnVec4 value;
        // Seconds from the next update to the start, and from the start to
        // the end; finite, from 0.
        double delay;
        double duration;
        // The easing of the progress.
        mtwnEasing easing;
        // For a color: the space it is interpolated in, and the hue method
        // for OkLCh.
        mtwnColorSpace colorSpace;
        mtwnHueMethod hueMethod;
        // For an angle: the method.
        mtwnAngleMethod angleMethod;
        // For a rotation: the way it turns.
        mtwnRotationPath rotationPath;
        uint8_t reserved[4];
    } mtwnTransitionDef;

// The cookie mtwnDefaultTransitionDef sets and mtwnStartTransition checks.
#define MTWN_TRANSITION_DEF_COOKIE 0x4E415254u

    /// Makes a transition def: no target, a duration of 0.25 seconds, no
    /// delay, CSS's ease, colors in Oklab with the shorter hue, angles the
    /// shorter way, rotations the shortest. Built in the program, so it
    /// carries the version of the headers the program includes.
    ///
    /// @return The def.
    /// @par Thread safety
    /// Safe from any thread.
    static inline mtwnTransitionDef mtwnDefaultTransitionDef(void)
    {
#ifdef __cplusplus
        mtwnTransitionDef def = {};
#else
    mtwnTransitionDef def = {0};
#endif
        def.cookie = MTWN_TRANSITION_DEF_COOKIE;
        def.version = MTWN_ABI_VERSION;
        def.duration = 0.25;
        // CSS's ease, cubic-bezier(0.25, 0.1, 0.25, 1), always accepted.
        (void)mtwnMakeCssEasing(mtwn_cssEase, &def.easing);
        def.colorSpace = mtwn_colorOklab;
        def.hueMethod = mtwn_hueShorter;
        def.angleMethod = mtwn_angleShorter;
        def.rotationPath = mtwn_rotationShortest;
        return def;
    }

    /// Starts a transition: from the target's current value, at the next
    /// update, to the def's value. It replaces the transition running on
    /// the target, whose id goes stale. During the delay the start holds;
    /// at its end the end value becomes the target's base value, its
    /// velocity 0, and the transition is freed.
    ///
    /// @param context    The context.
    /// @param def        The def.
    /// @param playerOut  Receives the transition's player id, or the null id
    ///                   on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a def
    ///         without its cookie, an end value the target's kind refuses,
    ///         a delay or duration outside the above, an unknown color
    ///         space, hue method or rotation path, a color with alpha
    ///         outside 0 to 1, or the longest way between equal rotations;
    ///         mtwn_errorVersion for a def built against another major or
    ///         minor version; mtwn_errorStale for a target that is not alive;
    ///         mtwn_errorLimit when the context holds its capacity of
    ///         players.
    /// @par Thread safety
    /// Safe from any thread; the context is used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnStartTransition(mtwnContext* context,
                                                           const mtwnTransitionDef* def,
                                                           mtwnPlayerId* playerOut);

#ifdef __cplusplus
}
#endif

#endif
