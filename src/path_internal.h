// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A path's sample with its work counted, for its tests; its segments as
// cubics, for its direction.

#ifndef MAUL_TWEEN_SRC_PATH_INTERNAL_H
#define MAUL_TWEEN_SRC_PATH_INTERNAL_H

#include "path_length.h"

#include "maul-tween/path.h"

#include <stdint.h>

// mtwnSamplePath, adding to *lengths, when not NULL, each arc length it
// measures (at constant speed, finding the parameter at a length).
void mtwnSamplePathCounting(const mtwnPath* path, double progress, double valueOut[4],
                            double tangentOut[4], uint32_t* lengths);

// The segment a progress falls in, its parameter there and its cubic, as
// the path's sample finds them.
void mtwnLocatePath(const mtwnPath* path, double progress, uint32_t* segmentOut, double* uOut,
                    mtwnCubic* cubic);

// Segment i's cubic.
void mtwnPathSegmentCubic(const mtwnPath* path, uint32_t i, mtwnCubic* cubic);

#endif // MAUL_TWEEN_SRC_PATH_INTERNAL_H
