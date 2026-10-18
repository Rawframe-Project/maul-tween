// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A scale as the float a target holds (record mtwn-0059): within the
// positive floats, the largest for one past them and the smallest for one
// below, so that no extrapolation, overshoot or repeat leaves a scale
// infinite or 0. A velocity likewise within the finite floats: a scale's,
// its value times its rate, passes them with its value, and any track's,
// a difference over a millisecond, can pass them between finite values.

#ifndef MAUL_TWEEN_SRC_SCALE_INTERNAL_H
#define MAUL_TWEEN_SRC_SCALE_INTERNAL_H

static inline float mtwnScaleFloat(double scale)
{
    return scale >= 0x1.fffffep127 ? 0x1.fffffep127f
                                   : (scale <= 0x1p-149 ? 0x1p-149f : (float)scale);
}

// A scale as the target holds it, still in double: within the floats'
// range, its bounds past it. In range it is unchanged, bit for bit.
static inline double mtwnScaleHeld(double scale)
{
    return scale >= 0x1.fffffep127 ? 0x1.fffffep127 : (scale <= 0x1p-149 ? 0x1p-149 : scale);
}

static inline float mtwnVelocityFloat(double velocity)
{
    return velocity >= 0x1.fffffep127
               ? 0x1.fffffep127f
               : (velocity <= -0x1.fffffep127 ? -0x1.fffffep127f : (float)velocity);
}

#endif
