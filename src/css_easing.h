// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CSS Easing Level 2's curves, for
// src/easing.c: cubic-bezier with its end tangents, steps with the
// before flag, and linear() with its canonicalization.

#ifndef MAUL_TWEEN_SRC_CSS_EASING_H
#define MAUL_TWEEN_SRC_CSS_EASING_H

#include "maul-tween/easing.h"

// cubic-bezier(control[0], control[1], control[2], control[3]) at any
// finite progress x, both control x from 0 to 1.
double mtwnSampleCubicBezier(const double control[4], double x);

// steps(count, position) at any finite progress x.
double mtwnSampleSteps(uint32_t count, mtwnStepPosition position, double x, bool before);

// linear() through canonical points at any finite progress x.
double mtwnSampleLinearPoints(const mtwnLinearPoint* points, uint32_t count, double x);

// Validates count points (at least 2, finite outputs, inputs finite or
// NaN) and canonicalizes them in place as CSS does: true, or false with
// the points untouched.
bool mtwnCanonicalizeLinearPoints(mtwnLinearPoint* points, uint32_t count);

#endif // MAUL_TWEEN_SRC_CSS_EASING_H
