// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// One check per kind of value struct: whether a maker could have produced
// it. A value struct's fields are the library's, but a host holds it and
// may write any bytes into it, so every public call taking one checks it,
// and so does every path taking one into a context. Both call these, so
// the two cannot drift. Each lives in its kind's module.

#ifndef MAUL_TWEEN_SRC_VALID_H
#define MAUL_TWEEN_SRC_VALID_H

#include "maul-tween/easing.h"
#include "maul-tween/spring.h"

#include <stdbool.h>

// An easing's fields, those its kind reads, as its maker gives them:
// exactly where the maker computes them exactly (the normalizations of the
// curves without parameters, a power's and a back's, the CSS curves' own
// fields), and otherwise in the ranges that keep its sample finite over
// progress 0 to 1 (an elastic, a spring easing). linear()'s points are
// the host's and not read; its sampler is finite for any finite points.
bool mtwnValidEasing(const mtwnEasing* easing);

// A spring's precomputed fields derived again from its frequency and ratio,
// bit for bit, and its reserved word 0.
bool mtwnValidSpring(const mtwnSpring* spring);

// A decay's rate finite and above 0.
bool mtwnValidDecay(const mtwnDecay* decay);

// A bounded decay's decay and spring valid, and its bounds as
// mtwnMakeBoundedDecay takes them.
bool mtwnValidBoundedDecay(const mtwnBoundedDecay* bounded);

#endif
