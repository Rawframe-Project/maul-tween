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
#include "maul-tween/spring.h"
#include "maul-tween/target.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // How a transition moves.
    typedef uint8_t mtwnTransitionSpec;
    enum
    {
        // Over a duration, by an easing.
        mtwn_transitionEasing = 0,
        // By a spring, until it settles within the rest tolerances.
        mtwn_transitionSpring = 1,
        // Coasting by a decay from the start velocity, with no end value,
        // until its speed falls to restSpeed.
        mtwn_transitionDecay = 2,
    };

    // Where a transition's start velocity comes from.
    typedef uint8_t mtwnVelocitySource;
    enum
    {
        // The target's velocity, as the animation it interrupts left it.
        mtwn_velocityInherit = 0,
        // The def's velocity, as a fling gives it.
        mtwn_velocityGiven = 1,
    };

    // A transition: the target, its end value and how it gets there. Start
    // from mtwnDefaultTransitionDef and set what differs.
    typedef struct mtwnTransitionDef
    {
        // Set by mtwnDefaultTransitionDef; checked by mtwnStartTransition.
        uint32_t cookie;
        // The headers' MTWN_ABI_VERSION.
        uint32_t version;
        mtwnTargetId target;
        // The end value, as the target's kind takes it; a decay has none.
        mtwnVec4 value;
        // With mtwn_velocityGiven, the start velocity in the kind's
        // channels per second, finite.
        mtwnVec4 velocity;
        // Seconds from the next update to the start, and for an easing from
        // the start to the end; finite, from 0.
        double delay;
        double duration;
        // The easing of the progress.
        mtwnEasing easing;
        // The spring of mtwn_transitionSpring.
        mtwnSpring spring;
        // The decay of mtwn_transitionDecay.
        mtwnDecay decay;
        // A spring rests within restOffset of its end and restSpeed of
        // rest, a decay at restSpeed: in the kind's channels, in progress
        // from 0 to 1 for colors, rotations and scales, and for a scale's
        // decay as a share of the value a second; finite, above 0.
        double restOffset;
        double restSpeed;
        // For a color: the space it is interpolated in, and the hue method
        // for OkLCh.
        mtwnColorSpace colorSpace;
        mtwnHueMethod hueMethod;
        // For an angle: the method.
        mtwnAngleMethod angleMethod;
        // For a rotation: the way it turns.
        mtwnRotationPath rotationPath;
        mtwnTransitionSpec spec;
        mtwnVelocitySource velocitySource;
        // For an easing: whether a reversal is shortened as CSS Transitions
        // shortens one. When this transition replaces an eased one and its
        // end, as given, is where that one would return to (its reversing-
        // adjusted start), its duration is scaled by the share of the way
        // the other covered, eased: a hover left half way takes half the
        // time back.
        bool reverses;
        // Whether it plays on under reduced motion (record mtwn-0049): a
        // fade, a color, or motion essential to what it shows; the others
        // finish at once.
        bool essential;
    } mtwnTransitionDef;

// The cookie mtwnDefaultTransitionDef sets and mtwnStartTransition checks.
#define MTWN_TRANSITION_DEF_COOKIE 0x4E415254u

    /// Makes a transition def: no target, an easing over 0.25 seconds, no
    /// delay, CSS's ease; for a spring, a critical one of 0.5 seconds'
    /// perceptual duration; for a decay, iOS's normal deceleration, 0.998
    /// kept a millisecond; rest within 1e-3 and 1e-2 a second; the target's
    /// velocity; colors in Oklab with the shorter hue, angles the shorter
    /// way, rotations the shortest. Built in the program, so it carries the
    /// version of the headers the program includes.
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
        // Both always accepted.
        (void)mtwnMakeSpringFromDuration(0.5, 0.0, &def.spring);
        (void)mtwnMakeDecayFromDeceleration(0.998, &def.decay);
        def.restOffset = 0.001;
        def.restSpeed = 0.01;
        def.colorSpace = mtwn_colorOklab;
        def.hueMethod = mtwn_hueShorter;
        def.angleMethod = mtwn_angleShorter;
        def.rotationPath = mtwn_rotationShortest;
        return def;
    }

    /// Starts a transition: from the target's current value, at the next
    /// update, to the def's value. It replaces the transition running on
    /// the target, whose id goes stale. During the delay the start holds at
    /// rest. A spring or decay starts with the start velocity: on numbers,
    /// vectors, angles and integers per channel, with the spring's or
    /// decay's own velocity; colors and rotations spring their path's
    /// progress, the start velocity projected on the end minus the start;
    /// a scale springs its mix's progress, so it overshoots by a ratio, and
    /// decays in its logarithm, its start velocity a share of its value.
    /// An angle's end moves by the method's whole turns when started. At its
    /// end, a spring's settling time or a decay's stop, the end value (a
    /// decay's: where it stopped) becomes the target's base value, its
    /// velocity 0, and the transition is freed; an undamped spring runs
    /// until replaced or stopped.
    ///
    /// @param context    The context.
    /// @param def        The def.
    /// @param playerOut  Receives the transition's player id, or the null id
    ///                   on failure.
    /// @return mtwn_success; mtwn_errorInvalid for a null argument, a def
    ///         without its cookie, an end value the target's kind refuses,
    ///         a delay, duration, given velocity or rest tolerance outside
    ///         the above, a spring or decay not made by this library, an
    ///         unknown spec, velocity source, color space, hue method or
    ///         rotation path, a color with alpha outside 0 to 1, the longest
    ///         way between equal rotations, a spring or decay for a discrete
    ///         value, a decay for a color or rotation, or a scale's decay
    ///         stopping where a float is 0 or infinite;
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
