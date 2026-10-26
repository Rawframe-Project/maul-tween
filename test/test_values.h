// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The value functions as a suite that tests what they compute calls them:
// each call checks its status is mtwn_success and returns what it gives.
// A suite testing refusals calls the library's functions itself.

#ifndef MAUL_TWEEN_TEST_VALUES_H
#define MAUL_TWEEN_TEST_VALUES_H

#include "test_harness.h"

#include "maul-tween/easing.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
#include "maul-tween/spring.h"

#include <math.h>

static inline mtwnSpringMotion SpringMotionAt(const mtwnSpring* spring, double seconds)
{
    mtwnSpringMotion motion = {(double)NAN, (double)NAN, (double)NAN, (double)NAN};
    CHECK(mtwnSpringMotionAt(spring, seconds, &motion) == mtwn_success, "a spring's motion");
    return motion;
}

static inline void SampleSpring(const mtwnSpring* spring, double offset, double velocity,
                                double seconds, double* offsetOut, double* velocityOut)
{
    CHECK(mtwnSampleSpring(spring, offset, velocity, seconds, offsetOut, velocityOut) ==
              mtwn_success,
          "a spring's sample");
}

static inline double SpringSettlingTime(const mtwnSpring* spring, double offset, double velocity,
                                        double offsetTolerance, double velocityTolerance)
{
    double seconds = (double)NAN;
    CHECK(mtwnSpringSettlingTime(spring, offset, velocity, offsetTolerance, velocityTolerance,
                                 &seconds) == mtwn_success,
          "a spring's settling time");
    return seconds;
}

static inline void SampleDecay(const mtwnDecay* decay, double position, double velocity,
                               double seconds, double* positionOut, double* velocityOut)
{
    CHECK(mtwnSampleDecay(decay, position, velocity, seconds, positionOut, velocityOut) ==
              mtwn_success,
          "a decay's sample");
}

static inline double DecayStopTime(const mtwnDecay* decay, double velocity, double stopSpeed)
{
    double seconds = (double)NAN;
    CHECK(mtwnDecayStopTime(decay, velocity, stopSpeed, &seconds) == mtwn_success,
          "a decay's stop time");
    return seconds;
}

static inline double DecayCrossingTime(const mtwnDecay* decay, double position, double velocity,
                                       double target)
{
    double seconds = (double)NAN;
    CHECK(mtwnDecayCrossingTime(decay, position, velocity, target, &seconds) == mtwn_success,
          "a decay's crossing time");
    return seconds;
}

static inline void SampleBoundedDecay(const mtwnBoundedDecay* bounded, double position,
                                      double velocity, double seconds, double* positionOut,
                                      double* velocityOut)
{
    CHECK(mtwnSampleBoundedDecay(bounded, position, velocity, seconds, positionOut, velocityOut) ==
              mtwn_success,
          "a bounded decay's sample");
}

static inline double BoundedDecayStopTime(const mtwnBoundedDecay* bounded, double position,
                                          double velocity, double offsetTolerance, double stopSpeed)
{
    double seconds = (double)NAN;
    CHECK(mtwnBoundedDecayStopTime(bounded, position, velocity, offsetTolerance, stopSpeed,
                                   &seconds) == mtwn_success,
          "a bounded decay's stop time");
    return seconds;
}

static inline double EaseAt(const mtwnEasing* easing, double progress, bool before)
{
    double eased = (double)NAN;
    CHECK(mtwnEase(easing, progress, before, &eased) == mtwn_success, "an easing's sample");
    return eased;
}

static inline double EaseBezierAt(const mtwnEasing* easing, double progress)
{
    double eased = (double)NAN;
    CHECK(mtwnEaseCubicBezier(easing, progress, &eased) == mtwn_success, "a cubic bezier's sample");
    return eased;
}

static inline bool IsMonotonicEasing(const mtwnEasing* easing)
{
    bool monotonic = false;
    CHECK(mtwnIsEasingMonotonic(easing, &monotonic) == mtwn_success, "an easing's monotonicity");
    return monotonic;
}

static inline void SamplePath(const mtwnPath* path, double progress, double valueOut[4],
                              double tangentOut[4])
{
    CHECK(mtwnSamplePath(path, progress, valueOut, tangentOut) == mtwn_success, "a path's sample");
}

static inline void PathDirection(const mtwnPath* path, double progress, double directionOut[4])
{
    CHECK(mtwnPathDirection(path, progress, directionOut) == mtwn_success, "a path's direction");
}

static inline double PathAngle(const mtwnPath* path, double progress)
{
    double angle = (double)NAN;
    CHECK(mtwnPathAngle(path, progress, &angle) == mtwn_success, "a path's angle");
    return angle;
}

static inline mtwnVec4 SampleTrack(const mtwnTrack* track, double progress)
{
    mtwnVec4 value = {NAN, NAN, NAN, NAN};
    CHECK(mtwnSampleTrack(track, progress, &value) == mtwn_success, "a track's sample");
    return value;
}

static inline double SamplePunch(const mtwnPunch* punch, double seconds)
{
    double factor = (double)NAN;
    CHECK(mtwnSamplePunch(punch, seconds, &factor) == mtwn_success, "a punch's sample");
    return factor;
}

static inline double SampleShake(const mtwnShake* shake, uint32_t channel, double seconds)
{
    double factor = (double)NAN;
    CHECK(mtwnSampleShake(shake, channel, seconds, &factor) == mtwn_success, "a shake's sample");
    return factor;
}

static inline double ShakeNoise(const mtwnShake* shake, uint32_t channel, double seconds)
{
    double factor = (double)NAN;
    CHECK(mtwnShakeNoise(shake, channel, seconds, &factor) == mtwn_success, "a shake's noise");
    return factor;
}

static inline double RubberBand(double distance, double dimension, double coefficient)
{
    double offset = (double)NAN;
    CHECK(mtwnRubberBand(distance, dimension, coefficient, &offset) == mtwn_success,
          "a rubber band");
    return offset;
}

static inline double RubberBandInverse(double offset, double dimension, double coefficient)
{
    double drag = (double)NAN;
    CHECK(mtwnRubberBandInverse(offset, dimension, coefficient, &drag) == mtwn_success,
          "a rubber band's inverse");
    return drag;
}

static inline mtwnColor MixColor(const mtwnColorMix* mix, double progress)
{
    mtwnColor color = {NAN, NAN, NAN, NAN};
    CHECK(mtwnMixColor(mix, progress, &color) == mtwn_success, "a color mix's sample");
    return color;
}

static inline mtwnQuat MixRotation(const mtwnRotationMix* mix, double progress)
{
    mtwnQuat rotation = {NAN, NAN, NAN, NAN};
    CHECK(mtwnMixRotation(mix, progress, &rotation) == mtwn_success, "a rotation mix's sample");
    return rotation;
}

static inline double MixScale(const mtwnScaleMix* mix, double progress)
{
    double scale = (double)NAN;
    CHECK(mtwnMixScale(mix, progress, &scale) == mtwn_success, "a scale mix's sample");
    return scale;
}

static inline mtwnZoomView MixZoom(const mtwnZoomMix* mix, double progress)
{
    mtwnZoomView view = {(double)NAN, (double)NAN, (double)NAN};
    CHECK(mtwnMixZoom(mix, progress, &view) == mtwn_success, "a zoom mix's sample");
    return view;
}

static inline double ZoomLength(const mtwnZoomMix* mix)
{
    double length = (double)NAN;
    CHECK(mtwnZoomLength(mix, &length) == mtwn_success, "a zoom mix's length");
    return length;
}

#endif
