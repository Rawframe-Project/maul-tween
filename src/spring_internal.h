// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What other modules may ask of a spring's layout, which lives in
// src/spring.c.

#ifndef MAUL_TWEEN_SRC_SPRING_INTERNAL_H
#define MAUL_TWEEN_SRC_SPRING_INTERNAL_H

#include "maul-tween/spring.h"

#include <stdbool.h>

// Whether a spring looks made by this library: a finite frequency above 0,
// a finite damping ratio from 0, a known regime.
bool mtwnIsSpring(const mtwnSpring* spring);

#endif
