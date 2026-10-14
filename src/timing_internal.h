// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What the timing shares with the crossings (record mtwn-0037).

#ifndef MAUL_TWEEN_SRC_TIMING_INTERNAL_H
#define MAUL_TWEEN_SRC_TIMING_INTERNAL_H

#include "maul-tween/timing.h"

#include <stdbool.h>

// Whether an iteration runs forwards under a direction (Web Animations
// Level 1's directed progress).
bool mtwnIterationForwards(mtwnDirection direction, double iteration);

// Whether a timing has a repeat delay that takes time.
bool mtwnHasRepeatDelay(const mtwnTiming* timing);

#endif
