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

MTWN_NOINLINE mtwnResult mtwnMakeLinearEasingWork(mtwnLinearPoint* points, uint32_t count,
                                                  mtwnEasing* easingOut);
MTWN_NOINLINE bool mtwnIsEasingMonotonicWork(const mtwnEasing* easing);
MTWN_NOINLINE double mtwnEaseWork(const mtwnEasing* easing, double progress, bool before);
MTWN_NOINLINE mtwnSpringMotion mtwnSpringMotionAtWork(const mtwnSpring* spring, double seconds);
MTWN_NOINLINE void mtwnSampleSpringWork(const mtwnSpring* spring, double offset, double velocity,
                                        double seconds, double* offsetOut, double* velocityOut);
MTWN_NOINLINE double mtwnSpringSettlingTimeWork(const mtwnSpring* spring, double offset,
                                                double velocity, double offsetTolerance,
                                                double velocityTolerance);
MTWN_NOINLINE mtwnResult mtwnMakeDecayWork(double rate, mtwnDecay* decayOut);
MTWN_NOINLINE void mtwnSampleDecayWork(const mtwnDecay* decay, double position, double velocity,
                                       double seconds, double* positionOut, double* velocityOut);
MTWN_NOINLINE double mtwnDecayStopTimeWork(const mtwnDecay* decay, double velocity,
                                           double stopSpeed);
MTWN_NOINLINE double mtwnDecayCrossingTimeWork(const mtwnDecay* decay, double position,
                                               double velocity, double target);
MTWN_NOINLINE void mtwnSampleBoundedDecayWork(const mtwnBoundedDecay* bounded, double position,
                                              double velocity, double seconds, double* positionOut,
                                              double* velocityOut);
MTWN_NOINLINE double mtwnBoundedDecayStopTimeWork(const mtwnBoundedDecay* bounded, double position,
                                                  double velocity, double offsetTolerance,
                                                  double stopSpeed);

#endif // MAUL_TWEEN_SRC_WORK_H
