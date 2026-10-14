// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Interpolation of values that are not plain numbers (records mtwn-0014,
// mtwn-0015 and mtwn-0024): colors in a named space as CSS Color 4
// interpolates them.

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

#ifdef __cplusplus
}
#endif

#endif
