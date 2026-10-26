#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# A reference for scale and zoom mixes, worked out in
# 60-digit decimals from each input double taken exactly:
#
# - a scale from a to b at progress p is a (b / a)^p;
# - a zoom is van Wijk and Nuij's path ("Smooth and efficient zooming and
#   panning", 2003) as the paper writes it: with d the pan's distance,
#   b_i = (w1^2 - w0^2 +- rho^4 d^2) / (2 w_i rho^2 d),
#   r_i = log(sqrt(b_i^2 + 1) - b_i), S = (r1 - r0) / rho, and at s = p S
#   u(s) = w0 / rho^2 (cosh r0 tanh(rho s + r0) - sinh r0),
#   w(s) = w0 cosh r0 / cosh(rho s + r0);
#   with equal centres S = |log(w1 / w0)| / rho and w = w0 (w1 / w0)^p.
#
# The library computes u another way (without the cancellation near 0)
# and in units of w0; at 60 digits the paper's form loses nothing that
# matters. Far past the ends, out to the doubles' largest progress, the
# paper's u cancels by up to e^(2 |r0|), so those cases take 800 digits.
# It writes test/generated/zoom_cases.h, inputs from its own SplitMix64.
#
# usage: zoom_oracle.py [--check]

import math
import os
import sys
from decimal import Decimal as D, getcontext, localcontext

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "test",
                    "generated", "zoom_cases.h")

getcontext().prec = 60
MASK = 2 ** 64 - 1
PROGRESS = [0.001, 0.1, 0.25, 1.0 / 3.0, 0.5, 0.75, 0.9, 0.999, -0.25, 1.25]
RHOS = [math.sqrt(2.0), 1.42, 0.5, 3.0, 0.05]
FAR_PROGRESS = [16.0, 1e3, 1e4, 1e10, 1e300, sys.float_info.max,
                -16.0, -1e3, -1e4, -1e300, -sys.float_info.max]
# Views whose paths are followed far past their ends: the example of #78,
# a zoom in, a small rho, a pan along x whose share passes the doubles'
# range, and far centres.
FAR_VIEWS = [
    ((0.0, 0.0, 1.0), (10.0, 0.0, 2.0), math.sqrt(2.0)),
    ((0.0, 0.0, 1.0), (100.0, 50.0, 0.01), 1.42),
    ((3.0, 2.0, 8.0), (-5.0, 7.0, 0.125), 0.5),
    ((0.0, 0.0, 1.0), (2.0 ** -500, 0.0, 2.0 ** 33), 1.0),
    ((1e6, -1e6, 1e3), (2e6, 0.0, 1e5), 3.0),
]
# Where tanh is +-1 and 1 / cosh is 0 to every digit kept.
FLAT = D(5000)


class SplitMix64:
    def __init__(self, seed):
        self.state = seed & MASK

    def next(self):
        self.state = (self.state + 0x9E3779B97F4A7C15) & MASK
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
        return z ^ (z >> 31)

    def unit(self):
        return (self.next() >> 11) / 9007199254740992.0

    def pick(self, values):
        return values[self.next() % len(values)]


def cosh(x):
    if abs(x) > FLAT * 20:
        return D("Infinity")
    e = x.exp()
    return (e + 1 / e) / 2


def sinh(x):
    e = x.exp()
    return (e - 1 / e) / 2


def tanh(x):
    if abs(x) > FLAT:
        return D(1) if x > 0 else D(-1)
    e = (2 * x).exp()
    return (e - 1) / (e + 1)


def scale_at(a, b, p):
    a, b, p = D(a), D(b), D(p)
    return a * ((b / a).ln() * p).exp()


def zoom(v0, v1, rho):
    """The path's length and its view at a progress, as a function."""
    x0, y0, w0 = (D(c) for c in v0)
    x1, y1, w1 = (D(c) for c in v1)
    rho = D(rho)
    dx, dy = x1 - x0, y1 - y0
    d = (dx * dx + dy * dy).sqrt()
    if d == 0:
        length = abs((w1 / w0).ln()) / rho

        def at(p):
            return x0, y0, w0 * ((w1 / w0).ln() * D(p)).exp()
        return length, at
    rr = rho * rho
    b0 = (w1 * w1 - w0 * w0 + rr * rr * d * d) / (2 * w0 * rr * d)
    b1 = (w1 * w1 - w0 * w0 - rr * rr * d * d) / (2 * w1 * rr * d)
    r0 = ((b0 * b0 + 1).sqrt() - b0).ln()
    r1 = ((b1 * b1 + 1).sqrt() - b1).ln()
    length = (r1 - r0) / rho

    def at(p):
        s = D(p) * length
        u = w0 / rr * (cosh(r0) * tanh(rho * s + r0) - sinh(r0))
        w = w0 * cosh(r0) / cosh(rho * s + r0)
        return x0 + u / d * dx, y0 + u / d * dy, w
    return length, at


def number(x):
    return float(x).hex()


def views(g):
    """Pairs of views at several scales and shapes, rho for each."""
    out = []
    for n in range(60):
        base = g.pick([1e-6, 1.0, 1e6])
        shape = n % 6
        x0 = (g.unit() - 0.5) * 2000.0 * base
        y0 = (g.unit() - 0.5) * 2000.0 * base
        w0 = base * 10.0 ** (g.unit() * 6.0 - 3.0)
        w1 = base * 10.0 ** (g.unit() * 6.0 - 3.0)
        if shape == 0:
            x1, y1 = x0, y0  # a zoom alone
        elif shape == 1:
            x1, y1 = x0 + w0 * 1e-9, y0  # a pan far under the width
        elif shape == 2:
            x1, y1, w1 = x0 + w0 * (g.unit() + 0.1), y0 - w0 * g.unit(), w0  # a pan alone
        else:
            x1 = (g.unit() - 0.5) * 2000.0 * base
            y1 = (g.unit() - 0.5) * 2000.0 * base
        out.append(((x0, y0, w0), (x1, y1, w1), g.pick(RHOS)))
    return out


def render():
    g = SplitMix64(0x5CA1E200)
    lines = [
        "// SPDX-License-Identifier: MIT",
        "// Copyright (c) 2026 Sirac Ozmen",
        "//",
        "// Generated by tools/oracle/zoom_oracle.py; do not edit. Scale and zoom",
        "// mixes worked out in 60-digit decimals.",
        "// clang-format off",
        "",
        "#ifndef MTWN_ZOOM_CASES_H",
        "#define MTWN_ZOOM_CASES_H",
        "",
        "static const ScaleCase s_scaleCases[] = {",
    ]
    kept = 0
    while kept < 200:
        a = 10.0 ** (g.unit() * 600.0 - 300.0)
        b = 10.0 ** (g.unit() * 600.0 - 300.0) if g.next() % 4 else a * (1.0 + g.unit())
        p = g.pick(PROGRESS)
        value = scale_at(a, b, p)
        if not D("1e-300") < value < D("1e300"):
            continue  # extrapolated past the doubles' normal range
        lines.append(f"    {{{number(a)}, {number(b)}, {number(p)}, {number(value)}}},")
        kept += 1
    lines += ["};", "", "static const ZoomCase s_zoomCases[] = {"]
    for v0, v1, rho in views(g):
        length, at = zoom(v0, v1, rho)
        for p in PROGRESS:
            x, y, w = at(p)
            values = list(v0) + list(v1) + [rho, p, x, y, w, length]
            lines.append("    {" + ", ".join(number(v) for v in values) + "},")
    lines += ["};", "", "static const ZoomCase s_zoomFarCases[] = {"]
    with localcontext() as far:
        far.prec = 800
        for v0, v1, rho in FAR_VIEWS:
            length, at = zoom(v0, v1, rho)
            for p in FAR_PROGRESS:
                x, y, w = at(p)
                values = list(v0) + list(v1) + [rho, p, x, y, w, length]
                lines.append("    {" + ", ".join(number(v) for v in values) + "},")
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines)


def main():
    text = render()
    if len(sys.argv) > 1 and sys.argv[1] == "--check":
        current = open(PATH, encoding="utf-8").read() if os.path.exists(PATH) else ""
        if current != text:
            print("test/generated/zoom_cases.h is stale: run tools/oracle/zoom_oracle.py")
            return 1
        print("zoom cases: current")
        return 0
    with open(PATH, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote " + os.path.normpath(PATH))
    return 0


if __name__ == "__main__":
    sys.exit(main())
