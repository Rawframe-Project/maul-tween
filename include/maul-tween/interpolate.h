// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Interpolation of values that are not plain numbers (records mtwn-0014,
// mtwn-0015, mtwn-0016, mtwn-0017, mtwn-0024 to mtwn-0026): colors in a
// named space as CSS Color 4 interpolates them, angles with CSS's hue
// methods, rotations by slerp, integers and discrete values, and punch
// and shake, the signals a host adds to a value.

#ifndef MAUL_TWEEN_INTERPOLATE_H
#define MAUL_TWEEN_INTERPOLATE_H

#include "maul-tween/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A color: gamma-encoded sRGB with straight alpha, as CSS's rgb(),
    // each channel nominally from 0 to 1. Channels outside that range are
    // extended sRGB, for wide-gamut hosts; alpha is from 0 to 1.
    typedef struct mtwnColor
    {
        float red;
        float green;
        float blue;
        float alpha;
    } mtwnColor;

    // A space colors interpolate in, and its coordinates.
    typedef uint8_t mtwnColorSpace;
    enum
    {
        // Oklab: lightness, a, b. CSS's default.
        mtwn_colorOklab = 0,
        // OkLCh: lightness, chroma, hue in degrees.
        mtwn_colorOklch = 1,
        // Linear sRGB: red, green, blue in linear light, as light mixes.
        mtwn_colorLinearSrgb = 2,
        // Gamma sRGB: the channels as given, as legacy web content mixes.
        mtwn_colorSrgb = 3,
    };

    // How OkLCh hues interpolate, CSS Color 4's four methods.
    typedef uint8_t mtwnHueMethod;
    enum
    {
        // The shorter arc. CSS's default.
        mtwn_hueShorter = 0,
        // The longer arc.
        mtwn_hueLonger = 1,
        // Always increasing.
        mtwn_hueIncreasing = 2,
        // Always decreasing.
        mtwn_hueDecreasing = 3,
    };

    // Two colors ready to interpolate: converted to their space and
    // premultiplied. Make one with mtwnMakeColorMix; its fields are the
    // library's.
    typedef struct mtwnColorMix
    {
        double from[4];
        double to[4];
        mtwnColor fromColor;
        mtwnColor toColor;
        mtwnColorSpace space;
        uint8_t reserved[7];
    } mtwnColorMix;

    /// Makes the interpolation between two colors in a space, following CSS
    /// Color 4: a powerless OkLCh hue (chroma at most 0.000004) takes the
    /// other color's, the hues are fixed up by the method, and the colors
    /// are premultiplied, the hue excepted.
    ///
    /// @param from    The color at progress 0: finite, alpha from 0 to 1.
    /// @param to      The color at progress 1, likewise.
    /// @param space   The space to interpolate in.
    /// @param hue     The hue method, read for OkLCh only.
    /// @param mixOut  Receives the mix.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, an
    ///         unknown space or method, or a color outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeColorMix(mtwnColor from, mtwnColor to,
                                                        mtwnColorSpace space, mtwnHueMethod hue,
                                                        mtwnColorMix* mixOut);

    /// Samples a mix: its colors exactly at 0 and 1, between them the
    /// premultiplied coordinates interpolated and divided by the
    /// interpolated alpha, or kept as they are where that alpha is 0 or
    /// below. Progress outside 0 to 1 extends the line, with alpha clamped
    /// to 0 to 1; the channels are not clamped.
    ///
    /// @param mix       A mix made by this library.
    /// @param progress  The progress, finite.
    /// @return The color.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API mtwnColor mtwnMixColor(const mtwnColorMix* mix, double progress);

    /// Converts a color to a space's coordinates and its alpha, for a host
    /// that springs or composes in the space.
    ///
    /// @param color           Finite, alpha from 0 to 1.
    /// @param space           The space.
    /// @param coordinatesOut  Receives the three coordinates and the
    ///                        alpha; an OkLCh hue in degrees from 0 to 360,
    ///                        NaN where it is powerless.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, an
    ///         unknown space or a color outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnColorToSpace(mtwnColor color, mtwnColorSpace space,
                                                        double coordinatesOut[4]);

    /// Converts a space's coordinates and an alpha back to a color.
    ///
    /// @param coordinates  The three coordinates and the alpha; a NaN OkLCh
    ///                     hue counts as 0, as CSS's missing hue does.
    /// @param space        The space.
    /// @param colorOut     Receives the color, alpha clamped to 0 to 1.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, an
    ///         unknown space, or a coordinate or alpha not finite, a NaN
    ///         hue excepted.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnColorFromSpace(const double coordinates[4],
                                                          mtwnColorSpace space,
                                                          mtwnColor* colorOut);

    // How an angle interpolates: raw, or by CSS's hue methods with a
    // period of a full turn.
    typedef uint8_t mtwnAngleMethod;
    enum
    {
        // As given, turns kept: 0 to 4 pi turns twice.
        mtwn_angleRaw = 0,
        // The shorter arc, at most half a turn.
        mtwn_angleShorter = 1,
        // The longer arc; a full turn between equal angles.
        mtwn_angleLonger = 2,
        // Always increasing, less than a turn.
        mtwn_angleIncreasing = 3,
        // Always decreasing, less than a turn.
        mtwn_angleDecreasing = 4,
    };

    /// Interpolates an angle in radians: the end at progress 1 is moved by
    /// whole turns as the method asks, then the two are interpolated
    /// linearly. At 0 the result is from exactly, and at 1 to exactly
    /// when no turn was added.
    ///
    /// @param from      The angle at progress 0, finite.
    /// @param to        The angle at progress 1, finite.
    /// @param method    The method; an unknown one is raw.
    /// @param progress  The progress, finite; outside 0 to 1 extends.
    /// @return The angle.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnMixAngle(double from, double to, mtwnAngleMethod method, double progress);

    // A rotation as a unit quaternion (x, y, z, w): the axis times the sine
    // of half the angle, then its cosine. q and -q are the same rotation.
    typedef struct mtwnQuat
    {
        float x;
        float y;
        float z;
        float w;
    } mtwnQuat;

    // Which way a rotation turns between its ends.
    typedef uint8_t mtwnRotationPath;
    enum
    {
        // The shorter way, at most half a turn.
        mtwn_rotationShortest = 0,
        // The other way, the rest of the turn.
        mtwn_rotationLongest = 1,
    };

    // Two rotations ready to interpolate. Make one with
    // mtwnMakeRotationMix; its fields are the library's.
    typedef struct mtwnRotationMix
    {
        double from[4];
        double to[4];
        double angle;
        mtwnQuat fromRotation;
        mtwnQuat toRotation;
    } mtwnRotationMix;

    /// Makes the spherical interpolation between two rotations, at constant
    /// angular speed, the shortest way or the longest.
    ///
    /// @param from    The rotation at progress 0: finite, not zero; it is
    ///                normalized.
    /// @param to      The rotation at progress 1, likewise.
    /// @param path    Which way.
    /// @param mixOut  Receives the mix.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer, an
    ///         unknown path, a rotation outside the above, or the longest
    ///         way between rotations so close (|q0 + q1| under 1e-6 once
    ///         the path's sign is taken) that it turns a full circle about
    ///         no defined axis.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeRotationMix(mtwnQuat from, mtwnQuat to,
                                                           mtwnRotationPath path,
                                                           mtwnRotationMix* mixOut);

    /// Samples a rotation mix: its rotations normalized exactly at 0 and 1
    /// (the second negated when the path needed it), between and beyond
    /// them slerp, of unit length to a float's rounding.
    ///
    /// @param mix       A mix made by this library.
    /// @param progress  The progress, finite.
    /// @return The rotation.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API mtwnQuat mtwnMixRotation(const mtwnRotationMix* mix, double progress);

    /// Interpolates an integer: the interpolation in double, rounded to
    /// nearest with ties away from zero, so it passes through every integer
    /// between. Exact at the ends, and between them for values within 2^53.
    ///
    /// @param from      The value at progress 0.
    /// @param to        The value at progress 1.
    /// @param progress  The progress, finite; outside 0 to 1 extends,
    ///                  clamped to the type's range.
    /// @return The value.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API int64_t mtwnMixInteger(int64_t from, int64_t to, double progress);

    /// Interpolates a discrete value the host indexes (an enumeration, a
    /// flag): from below progress 1/2, to from it, as Web Animations
    /// steps values it cannot interpolate.
    ///
    /// @param from      The value at progress 0.
    /// @param to        The value at progress 1.
    /// @param progress  The progress.
    /// @return from or to.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API uint32_t mtwnMixDiscrete(uint32_t from, uint32_t to, double progress);

    // A punch: a sine fading out over a duration, sin(2 pi f t) (1 - t /
    // duration)^exponent. Make one with mtwnMakePunch.
    typedef struct mtwnPunch
    {
        double frequency;
        double duration;
        double exponent;
    } mtwnPunch;

    // A shake: gradient noise fading out over the same envelope, the same
    // for a seed on every platform. Make one with mtwnMakeShake.
    typedef struct mtwnShake
    {
        double frequency;
        double duration;
        double exponent;
        uint64_t seed;
    } mtwnShake;

    /// Makes a punch.
    ///
    /// @param frequency  Cycles a second, finite and above 0 (DOTween's
    ///                   vibrato over the duration).
    /// @param duration   Seconds, finite and above 0.
    /// @param exponent   The envelope's, finite and above 0: 1 fades
    ///                   linearly, above 1 sooner, below 1 later.
    /// @param punchOut   Receives the punch.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or a
    ///         parameter outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePunch(double frequency, double duration,
                                                     double exponent, mtwnPunch* punchOut);

    /// Samples a punch: a factor from -1 to 1 the host scales by each
    /// channel's amplitude and adds to the value; exactly 0 at 0, at the
    /// duration and outside them.
    ///
    /// @param punch    A punch made by this library.
    /// @param seconds  The time since its start.
    /// @return The factor.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnSamplePunch(const mtwnPunch* punch, double seconds);

    /// Makes a shake.
    ///
    /// @param frequency  Lattice points a second, finite and above 0: how
    ///                   often the noise changes direction.
    /// @param duration   Seconds, finite and above 0.
    /// @param exponent   The envelope's, finite and above 0.
    /// @param seed       Any value; the same seed shakes the same way.
    /// @param shakeOut   Receives the shake.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or a
    ///         parameter outside the above.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeShake(double frequency, double duration,
                                                     double exponent, uint64_t seed,
                                                     mtwnShake* shakeOut);

    /// Samples a shake for one channel: a factor from -1 to 1, each
    /// channel its own noise; exactly 0 at 0, at the duration and outside
    /// them.
    ///
    /// @param shake    A shake made by this library.
    /// @param channel  Which channel, any value.
    /// @param seconds  The time since its start.
    /// @return The factor.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API double mtwnSampleShake(const mtwnShake* shake, uint32_t channel, double seconds);

#ifdef __cplusplus
}
#endif

#endif
