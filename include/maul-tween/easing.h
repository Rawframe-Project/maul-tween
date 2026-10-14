// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Easing: curves that map a timing's progress to eased progress (record
// mtwn-0011). An easing is a value made by a function that validates it
// and sampled by mtwnEase with no status to check. Penner's families
// land exactly on 0 and 1; every kind has the same four modes.

#ifndef MAUL_TWEEN_EASING_H
#define MAUL_TWEEN_EASING_H

#include "maul-tween/base.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // What an easing is.
    typedef uint8_t mtwnEasingKind;
    enum
    {
        // The identity: eased progress is progress.
        mtwn_easingLinear = 0,
        // p^k for an exponent k above 0: quad is 2, cubic 3, quart 4,
        // quint 5.
        mtwn_easingPower = 1,
        // 1 - cos(p pi / 2).
        mtwn_easingSine = 2,
        // 2^(10 (p - 1)), normalized to start at 0.
        mtwn_easingExpo = 3,
        // 1 - sqrt(1 - p^2).
        mtwn_easingCirc = 4,
        // p^2 ((s + 1) p - s), dipping below 0 by an overshoot s.
        mtwn_easingBack = 5,
        // A decaying sine, from GSAP's elastic with an amplitude and a
        // period.
        mtwn_easingElastic = 6,
        // Penner's four parabolas.
        mtwn_easingBounce = 7,
        // A function of the host's.
        mtwn_easingHost = 8,
    };

    // How an easing's curve f is applied to progress p. The in mode is
    // the curve as defined; the others are its reflections, the same for
    // every kind.
    typedef uint8_t mtwnEaseMode;
    enum
    {
        // f(p).
        mtwn_easeIn = 0,
        // 1 - f(1 - p): the curve reversed.
        mtwn_easeOut = 1,
        // f(2p) / 2 for p below 1/2, then 1 - f(2 - 2p) / 2: in, then out.
        mtwn_easeInOut = 2,
        // The out mode's curve over the first half, then f's over the
        // second.
        mtwn_easeOutIn = 3,
    };

    // A host's easing curve: eased progress for a progress from 0 to 1.
    typedef double (*mtwnEaseFunction)(void* context, double progress);

    // An easing: a curve and its mode. Make one with a mtwnMake...Easing
    // function or mtwnLinearEasing; its fields are the library's, set by
    // the maker, and a host reads none of them. It is a plain value, copied
    // freely; what it references (a host function's context) stays the
    // host's.
    typedef struct mtwnEasing
    {
        // The curve's numbers, precomputed by its maker.
        double parameters[5];
        // A host function and its context.
        mtwnEaseFunction function;
        void* context;
        // An integer the kind needs: a power's integer exponent, or 0.
        uint32_t count;
        mtwnEasingKind kind;
        mtwnEaseMode mode;
        uint8_t reserved[2];
    } mtwnEasing;

    /// Returns the linear easing, whose eased progress is its progress.
    ///
    /// @return The easing.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API mtwnEasing mtwnLinearEasing(void);

    /// Makes the power easing p^k: quad for k = 2, cubic 3, quart 4, quint
    /// 5. GSAP's power(n) is k = n + 1. Integer exponents up to 8 are
    /// computed by multiplication.
    ///
    /// @param mode        The mode.
    /// @param exponent    k, finite and above 0.
    /// @param easingOut   Receives the easing.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, an
    ///         unknown mode or an exponent outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePowerEasing(mtwnEaseMode mode, double exponent,
                                                           mtwnEasing* easingOut);

    /// Makes one of Penner's easings without parameters: sine, expo, circ
    /// or bounce. Each lands exactly on 0 at 0 and on 1 at 1; expo is
    /// normalized to do so and differs from Penner's by at most 2^-10.
    ///
    /// @param kind       mtwn_easingSine, mtwn_easingExpo, mtwn_easingCirc
    ///                   or mtwn_easingBounce.
    /// @param mode       The mode.
    /// @param easingOut  Receives the easing.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, another
    ///         kind or an unknown mode.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePennerEasing(mtwnEasingKind kind, mtwnEaseMode mode,
                                                            mtwnEasing* easingOut);

    /// Makes the back easing p^2 ((s + 1) p - s), normalized to land on 1.
    /// GSAP's back(s) and Penner's in and out use s as given, 1.70158 by
    /// default; Penner's and tween.js's in-out use 1.525 s, which is this
    /// easing with that s.
    ///
    /// @param mode       The mode.
    /// @param overshoot  s, finite and at least 0; 0 is the cubic.
    /// @param easingOut  Receives the easing.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, an
    ///         unknown mode or an overshoot outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeBackEasing(mtwnEaseMode mode, double overshoot,
                                                          mtwnEasing* easingOut);

    /// Makes the elastic easing, GSAP's elastic(amplitude, period): out is
    /// 1 + a 2^(-10 p) sin((p - c) 2 pi / P) with c = P asin(1 / a) /
    /// (2 pi), normalized to land on 0 and 1, and in its reflection. An
    /// amplitude below 1 is 1 with the period divided by it, as GSAP does.
    /// GSAP's defaults are amplitude 1 with period 0.3, or 0.45 in-out;
    /// Penner's is amplitude 1.
    ///
    /// @param mode       The mode.
    /// @param amplitude  a, finite and above 0.
    /// @param period     P, finite and above 0, in units of progress.
    /// @param easingOut  Receives the easing.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, an
    ///         unknown mode or a parameter outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeElasticEasing(mtwnEaseMode mode, double amplitude,
                                                             double period, mtwnEasing* easingOut);

    /// Makes an easing of a host function, called with progress clamped to
    /// 0 to 1, under the mode's reflections. The library neither checks
    /// nor forces its ends.
    ///
    /// @param mode       The mode.
    /// @param function   The function; it must be safe to call from every
    ///                   thread that samples the easing.
    /// @param context    Passed to the function; may be NULL.
    /// @param easingOut  Receives the easing.
    /// @return mtwn_success; mtwn_errorInvalid for a null function or
    ///         easingOut or an unknown mode.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeHostEasing(mtwnEaseMode mode,
                                                          mtwnEaseFunction function, void* context,
                                                          mtwnEasing* easingOut);

    /// Changes an easing's mode, keeping its curve.
    ///
    /// @param easing  An easing made by this library.
    /// @param mode    The mode.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or an
    ///         unknown mode, leaving the easing as it was.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnSetEaseMode(mtwnEasing* easing, mtwnEaseMode mode);

    /// Tells whether an easing never decreases, as a timeline's easing
    /// must: linear, power, sine, expo and circ in every mode; not back,
    /// elastic or bounce, and not a host function, which the library cannot
    /// inspect.
    ///
    /// @param easing  An easing made by this library.
    /// @return Whether it never decreases.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API bool mtwnIsEasingMonotonic(const mtwnEasing* easing);

    /// Samples an easing. Penner's curves and host functions see progress
    /// clamped to 0 to 1.
    ///
    /// @param easing    An easing made by this library.
    /// @param progress  The progress, finite.
    /// @param before    Whether the timing is in its before phase; it moves
    ///                  only curves that jump at their start.
    /// @return The eased progress, exactly 0 at 0 and 1 at 1 for every
    ///         kind but a host function.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnEase(const mtwnEasing* easing, double progress, bool before);

#ifdef __cplusplus
}
#endif

#endif
