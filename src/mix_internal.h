// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The mix of two values (record mtwn-0070).

#ifndef MAUL_TWEEN_SRC_MIX_INTERNAL_H
#define MAUL_TWEEN_SRC_MIX_INTERNAL_H

// The mix of a and b at t, (1 - t) a + t b: exact at t = 0 and t = 1,
// which a + t (b - a) is not, and a for a = b at any t. Far past its ends
// the weighted sum cancels: its error is an ulp of t a, beside a result of
// at least t |b - a|, so it matters only where the ends are the same,
// whose value it would lose past t = 2^53.
static inline double mtwnMix(double a, double b, double t)
{
    return a == b ? a : (1.0 - t) * a + t * b;
}

// The mix of a and b at t where the ends return before it (a color's):
// a + t (b - a), a for a = b at any finite t without a test, and the
// cheapest, its channels one vector; not exact at t = 1, which never
// comes here. culori mixes so.
static inline double mtwnMixInside(double a, double b, double t)
{
    return a + t * (b - a);
}

#endif
