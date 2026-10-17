// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A scale as the float a target holds (record mtwn-0059): within the
// positive floats, the largest for one past them and the smallest for one
// below, so that no extrapolation, overshoot or repeat leaves a scale
// infinite or 0.

#ifndef MAUL_TWEEN_SRC_SCALE_INTERNAL_H
#define MAUL_TWEEN_SRC_SCALE_INTERNAL_H

static inline float mtwnScaleFloat(double scale)
{
    return scale >= 0x1.fffffep127 ? 0x1.fffffep127f
                                   : (scale <= 0x1p-149 ? 0x1p-149f : (float)scale);
}

#endif
