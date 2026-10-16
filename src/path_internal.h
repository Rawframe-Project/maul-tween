// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A path's sample with its work counted, for its tests.

#ifndef MAUL_TWEEN_SRC_PATH_INTERNAL_H
#define MAUL_TWEEN_SRC_PATH_INTERNAL_H

#include "maul-tween/path.h"

#include <stdint.h>

// mtwnSamplePath, adding to *lengths, when not NULL, each arc length it
// measures (at constant speed, finding the parameter at a length).
void mtwnSamplePathCounting(const mtwnPath* path, double progress, double valueOut[4],
                            double tangentOut[4], uint32_t* lengths);

#endif // MAUL_TWEEN_SRC_PATH_INTERNAL_H
