// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Paths and tracks (records mtwn-0016, mtwn-0031 and mtwn-0032): keyframe
// curves of one to four channels, segment by segment linear, step, cubic
// Bezier or Catmull-Rom (centripetal by default), timed by their keyframes
// or traveled at constant speed by arc length; and tracks, which give
// every value kind its keyframes. Both reference the caller's arrays and
// allocate nothing.

#ifndef MAUL_TWEEN_PATH_H
#define MAUL_TWEEN_PATH_H

#include "maul-tween/base.h"
#include "maul-tween/easing.h"
#include "maul-tween/interpolate.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // How a segment goes from its keyframe to the next.
    typedef uint8_t mtwnSegmentKind;
    enum
    {
        // A straight line.
        mtwn_segmentLinear = 0,
        // Holds the keyframe's value until the next keyframe.
        mtwn_segmentStep = 1,
        // A cubic Bezier through the keyframe's out control and the next
        // keyframe's in control.
        mtwn_segmentBezier = 2,
        // Catmull-Rom through the neighbors, with the path's knot exponent.
        mtwn_segmentCatmullRom = 3,
    };

    // A keyframe: a value at an offset, and the segment that starts there.
    typedef struct mtwnKeyframe
    {
        // The value, in the path's channels.
        mtwnVec4 value;
        // A Bezier segment's controls: in is the second control of the
        // segment ending here, out the first of the segment starting here.
        mtwnVec4 in;
        mtwnVec4 out;
        // Where in the path's progress, from 0 to 1.
        double offset;
        // The easing of the segment that starts here, and its kind.
        mtwnEasing easing;
        mtwnSegmentKind kind;
        uint8_t reserved[7];
    } mtwnKeyframe;

    // A path over keyframes. Make one with mtwnMakePath; it references the
    // keyframes and lengths, which must outlive it and not change.
    typedef struct mtwnPath
    {
        const mtwnKeyframe* keyframes;
        const double* lengths;
        double alpha;
        uint32_t count;
        uint8_t channels;
        uint8_t reserved[3];
    } mtwnPath;

    /// Makes a path. With lengths it travels at constant speed: the
    /// cumulative arc length at each keyframe is measured into them, and a
    /// progress is a share of the whole length, the keyframes' offsets and
    /// easings no longer timing it.
    ///
    /// @param keyframes  At least 2: offsets finite, not decreasing, the
    ///                   first 0 and the last 1; values finite in the path's
    ///                   channels, and Bezier controls too; known kinds.
    /// @param count      The number of keyframes.
    /// @param channels   From 1 to 4.
    /// @param alpha      Catmull-Rom's knot exponent from 0 to 1: 0.5
    ///                   centripetal (no cusps or self-intersections within
    ///                   a segment), 0 uniform, 1 chordal.
    /// @param lengthsOut Count doubles for constant speed, or NULL. Every
    ///                   segment but the last keyframe's then may not be a
    ///                   step, which has no length to travel.
    /// @param pathOut    Receives the path.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or an
    ///         argument outside the above.
    /// @par Thread safety
    /// Safe from any thread; the lengths are used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePath(const mtwnKeyframe* keyframes, uint32_t count,
                                                    uint8_t channels, double alpha,
                                                    double* lengthsOut, mtwnPath* pathOut);

    /// Samples a path: the segment a progress falls in (the last keyframe
    /// at or below it; past the ends the first or last segment extended),
    /// its local progress eased by the segment's easing, then its kind; or
    /// at constant speed the point at that share of the length.
    ///
    /// @param path        A path made by this library.
    /// @param progress    The progress, finite.
    /// @param valueOut    Receives the path's channels, 0 past them.
    /// @param tangentOut  Receives the segment's derivative by its own
    ///                    parameter there, not normalized, for orienting
    ///                    along the path; may be NULL.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API void mtwnSamplePath(const mtwnPath* path, double progress, double valueOut[4],
                                 double tangentOut[4]);

    // A segment's mix, made when a track of colors or rotations is made.
    typedef union mtwnSegmentMix
    {
        mtwnColorMix color;
        mtwnRotationMix rotation;
    } mtwnSegmentMix;

    // What a track holds and how: start from mtwnDefaultTrackDef.
    typedef struct mtwnTrackDef
    {
        // Catmull-Rom's knot exponent, from 0 to 1.
        double alpha;
        mtwnValueKind kind;
        // For colors: the space segments mix in, and the hue method.
        mtwnColorSpace colorSpace;
        mtwnHueMethod hueMethod;
        // For angles: how each keyframe is moved by whole turns from the one
        // before.
        mtwnAngleMethod angleMethod;
        // For rotations: the way each segment turns.
        mtwnRotationPath rotationPath;
        uint8_t reserved[3];
    } mtwnTrackDef;

    /// Makes a track def for a kind: centripetal Catmull-Rom, colors in
    /// Oklab with the shorter hue, angles the shorter way, rotations the
    /// shortest.
    ///
    /// @param kind  The kind.
    /// @return The def.
    /// @par Thread safety
    /// Safe from any thread.
    static inline mtwnTrackDef mtwnDefaultTrackDef(mtwnValueKind kind)
    {
#ifdef __cplusplus
        mtwnTrackDef def = {};
#else
    mtwnTrackDef def = {0};
#endif
        def.alpha = 0.5;
        def.kind = kind;
        def.colorSpace = mtwn_colorOklab;
        def.hueMethod = mtwn_hueShorter;
        def.angleMethod = mtwn_angleShorter;
        def.rotationPath = mtwn_rotationShortest;
        return def;
    }

    // A track: keyframes of one value kind. Make one with mtwnMakeTrack; it
    // references the keyframes, lengths and mixes, which must outlive it
    // and not change.
    typedef struct mtwnTrack
    {
        mtwnPath path;
        const mtwnSegmentMix* mixes;
        mtwnValueKind kind;
        uint8_t reserved[7];
    } mtwnTrack;

    /// Makes a track. Numbers and vectors are their path; integers the path
    /// rounded to nearest, ties away from zero; discrete values step at an
    /// eased 1/2 of a linear segment, or hold through a step. Angles are
    /// unwrapped in place, each keyframe (with its controls) moved by the
    /// method's whole turns from the one before. Colors mix linear segments
    /// in the def's space and hold through steps. Rotations slerp linear
    /// segments, hold through steps, and take Bezier and Catmull-Rom per
    /// component, normalized after; with the shortest path the keyframes
    /// and controls are first flipped in place into the hemisphere of the
    /// one before. On failure the keyframes are as they were.
    ///
    /// @param def         The def: a known kind, alpha from 0 to 1, known
    ///                    choices for its kind.
    /// @param keyframes   As mtwnMakePath takes them, in the kind's channels;
    ///                    a rotation not all zero; a color's alpha from 0 to
    ///                    1. Colors and discrete values take linear and step
    ///                    segments only.
    /// @param count       The number of keyframes.
    /// @param lengthsOut  Count doubles for constant speed, or NULL; only
    ///                    numbers, vectors, angles and integers take it.
    /// @param mixesOut    Count - 1 mixes for colors and rotations, else
    ///                    NULL or unused.
    /// @param trackOut    Receives the track.
    /// @return mtwn_success; mtwn_errorInvalid for a null pointer or an
    ///         argument outside the above.
    /// @par Thread safety
    /// Safe from any thread; the keyframes are used by one thread at a time.
    MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeTrack(const mtwnTrackDef* def,
                                                     mtwnKeyframe* keyframes, uint32_t count,
                                                     double* lengthsOut, mtwnSegmentMix* mixesOut,
                                                     mtwnTrack* trackOut);

    /// Samples a track: its value at a progress, as mtwnSamplePath finds the
    /// segment and eases it.
    ///
    /// @param track     A track made by this library.
    /// @param progress  The progress, finite.
    /// @return The value, the kind's channels used and the rest 0.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API mtwnVec4 mtwnSampleTrack(const mtwnTrack* track, double progress);

#ifdef __cplusplus
}
#endif

#endif
