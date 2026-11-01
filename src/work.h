// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The workers of the public functions the library also calls itself:
// each public function is a wrapper that enters the floating-point
// environment around its worker (src/fpenv.h), and the library, already
// inside an entry, calls the worker directly.

#ifndef MAUL_TWEEN_SRC_WORK_H
#define MAUL_TWEEN_SRC_WORK_H

#include "fpenv.h"

#include "maul-tween/clip.h"
#include "maul-tween/easing.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
#include "maul-tween/sample.h"
#include "maul-tween/spring.h"
#include "maul-tween/timing.h"

MTWN_NOINLINE mtwnResult mtwnMakeLinearPointsEasingWork(mtwnLinearPoint* points, uint32_t count,
                                                        mtwnEasing* easingOut);
MTWN_NOINLINE bool mtwnIsEasingMonotonicWork(const mtwnEasing* easing);
MTWN_NOINLINE double mtwnSampleEasingWork(const mtwnEasing* easing, double progress, bool before);
// mtwnSampleEasingWork as a timeline's easing: a cubic bezier never
// decreasing to the bit (mtwnSampleMonotoneCubicBezier), so a timeline's
// child time never runs back inside an iteration, as the crossings assume.
MTWN_NOINLINE double mtwnSampleTimelineEasingWork(const mtwnEasing* easing, double progress,
                                                  bool before);
MTWN_NOINLINE mtwnSpringMotion mtwnComputeSpringMotionWork(const mtwnSpring* spring,
                                                           double seconds);
MTWN_NOINLINE void mtwnSampleSpringWork(const mtwnSpring* spring, double offset, double velocity,
                                        double seconds, double* offsetOut, double* velocityOut);
MTWN_NOINLINE double mtwnComputeSpringSettlingTimeWork(const mtwnSpring* spring, double offset,
                                                       double velocity, double offsetTolerance,
                                                       double velocityTolerance);
MTWN_NOINLINE mtwnResult mtwnMakeDecayWork(double rate, mtwnDecay* decayOut);
MTWN_NOINLINE void mtwnSampleDecayWork(const mtwnDecay* decay, double position, double velocity,
                                       double seconds, double* positionOut, double* velocityOut);
MTWN_NOINLINE double mtwnComputeDecayStopTimeWork(const mtwnDecay* decay, double velocity,
                                                  double stopSpeed);
MTWN_NOINLINE double mtwnComputeDecayCrossingTimeWork(const mtwnDecay* decay, double position,
                                                      double velocity, double target);
MTWN_NOINLINE void mtwnSampleBoundedDecayWork(const mtwnBoundedDecay* bounded, double position,
                                              double velocity, double seconds, double* positionOut,
                                              double* velocityOut);
MTWN_NOINLINE double mtwnComputeBoundedDecayStopTimeWork(const mtwnBoundedDecay* bounded,
                                                         double position, double velocity,
                                                         double offsetTolerance, double stopSpeed);
MTWN_NOINLINE mtwnResult mtwnMakeColorMixWork(mtwnColor from, mtwnColor to, mtwnColorSpace space,
                                              mtwnHueMethod hue, mtwnColorMix* mixOut);
MTWN_NOINLINE mtwnColor mtwnMixColorWork(const mtwnColorMix* mix, double progress);
MTWN_NOINLINE mtwnResult mtwnConvertColorToSpaceWork(mtwnColor color, mtwnColorSpace space,
                                                     double coordinatesOut[4]);
// Whether an angle method is one of the five. mtwnMixAngleWork takes any
// other as raw, so every call that takes one from the host checks it first.
static inline bool mtwnIsAngleMethod(mtwnAngleMethod method)
{
    return (uint32_t)method <= mtwn_angleDecreasing;
}

MTWN_NOINLINE double mtwnMixAngleWork(double from, double to, mtwnAngleMethod method,
                                      double progress);
MTWN_NOINLINE mtwnResult mtwnMakeRotationMixWork(mtwnQuat from, mtwnQuat to, mtwnRotationPath path,
                                                 mtwnRotationMix* mixOut);
MTWN_NOINLINE mtwnQuat mtwnMixRotationWork(const mtwnRotationMix* mix, double progress);
MTWN_NOINLINE mtwnResult mtwnMakeScaleMixWork(double from, double to, mtwnScaleMix* mixOut);
MTWN_NOINLINE double mtwnMixScaleWork(const mtwnScaleMix* mix, double progress);
MTWN_NOINLINE mtwnResult mtwnMakeTrackWork(mtwnKeyframe* keyframes, uint32_t count,
                                           const mtwnTrackDef* def, double* lengthsOut,
                                           mtwnSegmentMix* mixesOut, mtwnTrack* trackOut);
MTWN_NOINLINE mtwnVec4 mtwnSampleTrackWork(const mtwnTrack* track, double progress);
MTWN_NOINLINE mtwnResult mtwnValidateTimingWork(const mtwnTiming* timing);
// The timing at a local time, its easing sampled by the sampler given; a
// worker of its own, which the two below call, so a program of timings
// links no timeline's curve and a timeline's costs no call more.
typedef double mtwnEasingSampler(const mtwnEasing* easing, double progress, bool before);
MTWN_NOINLINE void mtwnComputeTimingWith(const mtwnTiming* timing, double localTime, bool backwards,
                                         mtwnEasingSampler* sample, mtwnTimingResult* resultOut);

static inline void mtwnComputeTimingWork(const mtwnTiming* timing, double localTime, bool backwards,
                                         mtwnTimingResult* resultOut)
{
    mtwnComputeTimingWith(timing, localTime, backwards, mtwnSampleEasingWork, resultOut);
}

// mtwnComputeTimingWork for a timeline, its easing sampled by
// mtwnSampleTimelineEasingWork.
static inline void mtwnComputeTimelineTimingWork(const mtwnTiming* timing, double localTime,
                                                 bool backwards, mtwnTimingResult* resultOut)
{
    mtwnComputeTimingWith(timing, localTime, backwards, mtwnSampleTimelineEasingWork, resultOut);
}

#endif // MAUL_TWEEN_SRC_WORK_H
