// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The value functions as a suite that tests what they compute calls them:
// each call checks its status is mtwn_success and returns what it gives.
// A suite testing refusals calls the library's functions itself.

#ifndef MAUL_TWEEN_TEST_VALUES_H
#define MAUL_TWEEN_TEST_VALUES_H

#include "test_harness.h"

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

#endif
