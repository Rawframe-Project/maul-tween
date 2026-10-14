// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Paths (records mtwn-0016 and mtwn-0031): keyframe curves of one to four
// channels, segment by segment linear, step, cubic Bezier or Catmull-Rom
// (centripetal by default), timed by their keyframes or traveled at
// constant speed by arc length. A path references the caller's keyframes
// and lengths and allocates nothing.

#ifndef MAUL_TWEEN_PATH_H
#define MAUL_TWEEN_PATH_H

#include "maul-tween/base.h"
#include "maul-tween/easing.h"

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

#ifdef __cplusplus
}
#endif

#endif
