#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# A reference for the timing model's discrete answers and the crossings'
# counts that cannot share their rounding (records mtwn-0012 and
# mtwn-0037): every input is a double, taken as the exact rational it
# is, and every answer is worked out with fractions. Durations, delays
# and iteration starts are not dyadic (0.1, 1/3, whole starts to 10^6),
# so the library's doubles round where these do not. It writes
# test/generated/exact_cases.h, inputs from its own SplitMix64:
#
# - timings at local times inside the active interval: the phase, the
#   current iteration and whether the time lies in a repeat delay;
#   within 2^-40 (relative) of an iteration's boundary either neighbour
#   is right, and everywhere the iteration lies in the timing's own;
# - a mark one or two timelines deep, and a node's iteration starts
#   alone or one timeline deep, swept: the crossings each way. Sweeps
#   with a counted point within 2^-38 of their ends are left out, but
#   for dyadic timings whose boundaries are exact in doubles too, whose
#   sweeps end exactly on an iteration start: there a sweep counts a
#   point in (from, to] forwards and [to, from) backwards.
#
# Timings whose start's fraction and count sum within rounding of a
# whole number are left out: there the doubles' sum, not the exact one,
# is the iterations' end the author wrote (0.3 and 3.7 make 4 in
# doubles, 4.0000000000000000666 exactly), and so the repeat delays'
# number.
#
# usage: exact_oracle.py [--check]

import os
import sys
from fractions import Fraction as F

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "test",
                    "generated", "exact_cases.h")

MASK = 2 ** 64 - 1


class SplitMix64:
    def __init__(self, seed):
        self.state = seed & MASK

    def next(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & MASK
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
        return z ^ (z >> 31)

    def pick(self, values):
        return values[self.next() % len(values)]

    def unit(self):
        return (self.next() >> 11) / 9007199254740992.0

    def uniform(self, lo, hi):
        return lo + (hi - lo) * self.unit()

    def below(self, n):
        return self.next() % n


def floor_f(x):
    return x.numerator // x.denominator


def ceil_f(x):
    return -((-x.numerator) // x.denominator)


class Level:
    """A timing as exact rationals: its path on the cycle timeline."""

    def __init__(self, delay, start, n, d, r, rate, direction):
        self.raw = (delay, start, n, d, r, rate, direction)
        self.D, self.R, self.N, self.S = F(d), F(r), F(n), F(start)
        self.rate, self.delay, self.direction = F(rate), F(delay), direction
        self.first = floor_f(self.S)
        self.fraction = self.S - self.first
        delays = (ceil_f(self.fraction + self.N) - 1) * self.R if r > 0 else 0
        self.length = self.N * self.D + delays
        self.active = self.length / abs(self.rate)
        self.cycle = self.D + self.R
        self.pos0 = self.first * self.cycle + self.fraction * self.D
        self.pos_end = self.pos0 + self.length
        self.last = ceil_f(self.S + self.N) - 1
        self.tol = max(abs(self.pos_end), 1) * F(1, 2 ** 38)

    def rounding_decides(self):
        total = self.fraction + self.N
        return abs(total - round(total)) <= total * F(1, 2 ** 40)

    def forwards(self, k):
        return [True, False, k % 2 == 0, k % 2 == 1][self.direction]

    def position(self, t):
        a = min(max(t - self.delay, F(0)), self.active)
        scaled = a * self.rate if self.rate > 0 else (self.active - a) * -self.rate
        return self.pos0 + scaled

    def local_of(self, p):
        a = (p - self.pos0) / abs(self.rate)
        return self.delay + (a if self.rate > 0 else self.active - a)

    def span(self, lo, hi):
        return max(self.first, floor_f(lo / self.cycle) - 1), \
            min(self.last, floor_f(hi / self.cycle) + 1)

    def count_instant(self, m, t1, t2):
        """A mark at child time m, the local time swept t1 to t2."""
        p1, p2 = self.position(t1), self.position(t2)
        up, lo, hi = p2 > p1, min(p1, p2), max(p1, p2)
        same = other = 0
        first, last = self.span(lo, hi)
        for k in range(first, last + 1):
            fwd = self.forwards(k)
            q = k * self.cycle + (m if fwd else self.D - m)
            if q < self.pos0 - self.tol or q > self.pos_end + self.tol:
                continue
            if min(abs(q - self.pos0), abs(q - self.pos_end), abs(q - p1),
                   abs(q - p2)) <= self.tol:
                return None
            if lo < q < hi:
                if fwd == up:
                    same += 1
                else:
                    other += 1
        return same, other

    def pieces(self, t1, t2):
        """The child time's monotone pieces as the local time sweeps."""
        p1, p2 = self.position(t1), self.position(t2)
        up, lo, hi = p2 > p1, min(p1, p2), max(p1, p2)
        first, last = self.span(lo, hi)
        out = []
        ks = range(first, last + 1) if up else range(last, first - 1, -1)
        for k in ks:
            a = max(lo, k * self.cycle, self.pos0)
            b = min(hi, k * self.cycle + self.D, self.pos_end)
            if a >= b:
                continue
            if self.forwards(k):
                ca, cb = a - k * self.cycle, b - k * self.cycle
            else:
                ca, cb = self.D - (a - k * self.cycle), self.D - (b - k * self.cycle)
            out.append((ca, cb) if up else (cb, ca))
        return out

    def starts(self):
        """The local times of the iteration starts after the first."""
        return [self.local_of(j * self.cycle) for j in range(self.first + 1, self.last + 1)]

    def count_starts(self, u1, u2, exact):
        up, lo, hi = u2 > u1, min(u1, u2), max(u1, u2)
        same = other = 0
        for u in self.starts():
            if exact:
                # (from, to] forwards, [to, from) backwards.
                hit = (u1 < u <= u2) if up else (u2 <= u < u1)
            else:
                tol = max(abs(u), abs(self.active), 1) * F(1, 2 ** 38)
                if abs(u - u1) <= tol or abs(u - u2) <= tol:
                    return None
                hit = lo < u < hi
            if hit:
                if up:
                    same += 1
                else:
                    other += 1
        return same, other


def random_timing(g, scale, big):
    d = g.pick([0.1, 0.3, 0.7, 1 / 3, 0.2, 1.0, 0.75, g.uniform(0.05, 2.0)]) * scale
    r = g.pick([0.0, 0.1, 0.05, 1 / 3, g.uniform(0.001, 1.0)]) * scale
    n = g.pick([1.0, 2.0, 2.5, 3.7, float(1 + g.below(6)), g.uniform(0.3, 6.0)])
    starts = [0.0, 0.3, 1.3, 2.0]
    if big:
        starts += [339.0, 933.0, float(g.below(10 ** 6)), g.uniform(0.0, 1e6)]
    start = g.pick(starts)
    rate = g.pick([1.0, 2.0, 0.5, -1.0, -1.5, -0.25])
    delay = g.pick([0.0, 0.1 * scale, g.uniform(0.0, 1.0) * scale])
    return [delay, start, n, d, r, rate, g.below(4)]


def dyadic_timing(g):
    return [g.pick([0.0, 0.5, 1.0]), g.pick([0.0, 1.0, 2.0, 3.0, 0.5]),
            g.pick([1.0, 2.0, 3.0, 2.5, 4.0]), g.pick([0.25, 0.5, 1.0, 2.0]),
            g.pick([0.0, 0.25, 0.5]), g.pick([1.0, 2.0, 0.5, -1.0, -2.0]), g.below(4)]


def timing_cases(g, count):
    out = []
    while len(out) < count:
        level = Level(*random_timing(g, 1.0, True))
        if level.rounding_decides():
            continue
        locals_ = [level.delay + level.active * F(g.unit())]
        if level.rate > 0:
            locals_.append(level.delay + F(1e-15))
        for local in locals_:
            local = float(local)
            a = F(local) - level.delay
            if not 0 < a < level.active:
                continue
            scaled = a * level.rate if level.rate > 0 else (level.active - a) * -level.rate
            if level.R > 0:
                pos = scaled + level.pos0
                k = floor_f(pos / level.cycle)
                within = pos - k * level.cycle
                in_delay = int(within >= level.D)
                edges = [k * level.cycle, k * level.cycle + level.D, (k + 1) * level.cycle]
                dist = min(abs(pos - e) for e in edges)
                tol = max(abs(pos), 1) * F(1, 2 ** 40)
            else:
                overall = scaled / level.D + level.S
                k = floor_f(overall)
                in_delay = 0
                dist = min(overall - k, k + 1 - overall)
                tol = max(abs(overall), 1) * F(1, 2 ** 40)
            k = min(k, level.last)
            if dist <= tol:
                lo, hi, in_delay = max(k - 1, level.first), min(k + 1, level.last), -1
            else:
                lo = hi = k
            out.append((level.raw, local, lo, hi, in_delay))
    return out[:count]


def count_cases(g, count):
    out = []
    while len(out) < count:
        kind = g.below(5)
        if kind == 0:
            # A mark in one timeline.
            level = Level(*random_timing(g, 1.0, True))
            if level.rounding_decides():
                continue
            m = float(level.D) * g.uniform(0.05, 0.95)
            span = float(level.active) * 1.2
            t1 = float(level.delay) - 0.1 * float(level.active) + g.unit() * span
            t2 = t1 + (g.unit() - 0.5) * span
            got = level.count_instant(F(m), F(t1), F(t2))
            if got is not None:
                out.append((1, level.raw, 0.0, None, 0, m, None, t1, t2) + got)
        elif kind == 1:
            # A mark two timelines deep.
            parent = Level(*random_timing(g, 4.0, True))
            child = Level(*random_timing(g, 1.0, False))
            if parent.rounding_decides() or child.rounding_decides():
                continue
            child_start = float(parent.D) * g.uniform(-0.2, 0.8)
            m = float(child.D) * g.uniform(0.05, 0.95)
            span = float(parent.active) * 1.2
            t1 = float(parent.delay) - 0.1 * float(parent.active) + g.unit() * span
            t2 = t1 + (g.unit() - 0.5) * span
            same = other = 0
            for ca, cb in parent.pieces(F(t1), F(t2)):
                got = child.count_instant(F(m), ca - F(child_start), cb - F(child_start))
                if got is None:
                    break
                same, other = same + got[0], other + got[1]
            else:
                out.append((2, parent.raw, child_start, child.raw, 0, m, None, t1, t2,
                            same, other))
        elif kind in (2, 3):
            # A node's iteration starts, alone or one timeline deep.
            deep = kind == 3
            parent = Level(*random_timing(g, 4.0, True)) if deep else None
            node = Level(*random_timing(g, 1.0, True))
            if (parent and parent.rounding_decides()) or node.rounding_decides():
                continue
            if node.last - node.first > 10 ** 4:
                continue
            at = (float(parent.D) if parent else 1.0) * g.uniform(-0.2, 0.6)
            span = (float(parent.active) if parent else float(node.active + node.delay)) * 1.3
            base = float(parent.delay) if parent else at
            t1 = base - 0.1 * span + g.unit() * span
            t2 = t1 + (g.unit() - 0.5) * span
            pieces = parent.pieces(F(t1), F(t2)) if parent else [(F(t1), F(t2))]
            same = other = 0
            for ca, cb in pieces:
                got = node.count_starts(ca - F(at), cb - F(at), False)
                if got is None:
                    break
                same, other = same + got[0], other + got[1]
            else:
                out.append((1 if deep else 0, parent.raw if parent else None, 0.0, None, 1,
                            at, node.raw, t1, t2, same, other))
        else:
            # Dyadic: a sweep ending exactly on an iteration start.
            node = Level(*dyadic_timing(g))
            starts = node.starts()
            if not starts:
                continue
            at = g.pick([0.0, 0.5, -0.5])
            b = g.pick(starts)
            far = g.pick(starts + [node.delay - 1, node.delay + node.active + 1,
                                   b + F(1, 4), b - F(1, 4)])
            t1, t2 = (far, b) if g.below(2) == 0 else (b, far)
            same, other = node.count_starts(t1, t2, True)
            out.append((0, None, 0.0, None, 1, at, node.raw, float(t1 + F(at)),
                        float(t2 + F(at)), same, other))
    return out[:count]


def number(value):
    return float(value).hex()


def timing_text(raw):
    if raw is None:
        return "{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0}"
    delay, start, n, d, r, rate, direction = raw
    return "{" + ", ".join(number(x) for x in (delay, start, n, d, r, rate)) + \
        f", {direction}}}"


def render():
    g = SplitMix64(0x4D54574E45584143)
    lines = [
        "// SPDX-License-Identifier: MIT",
        "// Copyright (c) 2026 Sirac Ozmen",
        "//",
        "// Generated by tools/oracle/exact_oracle.py; do not edit. Timings and",
        "// sweeps with their answers worked out in exact rationals.",
        "// clang-format off",
        "",
        "#ifndef MTWN_EXACT_CASES_H",
        "#define MTWN_EXACT_CASES_H",
        "",
        "static const ExactTimingCase s_exactTimings[] = {",
    ]
    for raw, local, lo, hi, in_delay in timing_cases(g, 1600):
        lines.append(f"    {{{timing_text(raw)}, {number(local)}, {number(lo)}, {number(hi)}, "
                     f"{in_delay}}},")
    lines += ["};", "", "static const ExactCountCase s_exactCounts[] = {"]
    for levels, outer, child_start, inner, leaf_kind, at, leaf, t1, t2, same, other in \
            count_cases(g, 2000):
        lines.append(f"    {{{levels}, {timing_text(outer)}, {number(child_start)}, "
                     f"{timing_text(inner)}, {leaf_kind}, {number(at)}, {timing_text(leaf)}, "
                     f"{number(t1)}, {number(t2)}, {same}, {other}}},")
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines)


def main():
    text = render()
    if len(sys.argv) > 1 and sys.argv[1] == "--check":
        current = open(PATH, encoding="utf-8").read() if os.path.exists(PATH) else ""
        if current != text:
            print("test/generated/exact_cases.h is stale: run tools/oracle/exact_oracle.py")
            return 1
        print("exact cases: current")
        return 0
    with open(PATH, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote " + os.path.normpath(PATH))
    return 0


if __name__ == "__main__":
    sys.exit(main())
