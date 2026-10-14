#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# Writes src/generated/math_constants.h, the constants of the library's
# own math functions (src/math.c, record mtwn-0008): the splits of ln 2
# and pi / 2 for exact argument reduction, the Taylor coefficients of
# the polynomials, and arctangents at multiples of 1/8. Each is computed
# in decimal arithmetic to 80 digits and rounded once to binary64.
#
# usage: gen_math_constants.py [--check]
#   --check  fail when the file differs from what would be written

import decimal
import os
import sys
from fractions import Fraction

D = decimal.Decimal
decimal.getcontext().prec = 80
PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "generated",
                    "math_constants.h")


def pi():
    # Machin's formula: pi = 16 atan(1/5) - 4 atan(1/239).
    def atan_inv(n):
        x = D(1) / n
        x2 = x * x
        term, total, k = x, x, 1
        while True:
            term *= -x2
            add = term / (2 * k + 1)
            if add == 0:
                return total
            total += add
            k += 1
    return 16 * atan_inv(5) - 4 * atan_inv(239)


def atan(x):
    # Halve the argument until it is small, then sum the series.
    halvings = 0
    while abs(x) > D("0.01"):
        x = x / (1 + (1 + x * x).sqrt())
        halvings += 1
    x2 = x * x
    term, total, k = x, x, 1
    while True:
        term *= -x2
        add = term / (2 * k + 1)
        if add == 0:
            break
        total += add
        k += 1
    return total * (2 ** halvings)


def to_double(value):
    # The nearest binary64, ties to even: Python's float of a Fraction
    # is correctly rounded.
    return float(Fraction(value))


def to_bits(value, bits):
    # value rounded to `bits` significant bits, so that products with
    # small integers are exact.
    f = Fraction(value)
    if f == 0:
        return 0.0
    e = 0
    while abs(f) >= 2:
        f /= 2
        e += 1
    while abs(f) < 1:
        f *= 2
        e -= 1
    scaled = f * (2 ** (bits - 1))
    n = round(scaled)
    return float(Fraction(n, 2 ** (bits - 1)) * Fraction(2) ** e)


def split(value, bits):
    hi = to_bits(value, bits)
    return hi, D(value) - D(Fraction(hi).numerator) / D(Fraction(hi).denominator)


def exact(x):
    f = Fraction(x)
    return D(f.numerator) / D(f.denominator)


def factorial(n):
    out = 1
    for i in range(2, n + 1):
        out *= i
    return out


def lines():
    ln2 = D(2).ln()
    pio2 = pi() / 2
    out = []

    def const(name, value, note):
        out.append(f"// {note}")
        out.append(f"#define {name} {value.hex()}")

    ln2hi, rest = split(ln2, 40)
    const("MTWN_LN2_HI", ln2hi, "ln 2 to 40 bits: k times it is exact for |k| < 2^13.")
    const("MTWN_LN2_LO", to_double(rest), "ln 2 minus MTWN_LN2_HI.")
    const("MTWN_INV_LN2", to_double(1 / ln2), "1 / ln 2.")
    ln2_near = to_double(ln2)
    const("MTWN_LN2", ln2_near, "ln 2, the nearest double.")
    const("MTWN_LN2_TAIL", to_double(ln2 - exact(ln2_near)), "ln 2 minus MTWN_LN2.")
    p1, rest = split(pio2, 33)
    p2, rest = split(rest, 33)
    const("MTWN_PIO2_1", p1, "pi / 2 to 33 bits: k times it is exact for |k| < 2^20.")
    const("MTWN_PIO2_2", p2, "The next 33 bits of pi / 2.")
    const("MTWN_PIO2_3", to_double(rest), "The rest of pi / 2.")
    const("MTWN_INV_PIO2", to_double(1 / pio2), "2 / pi.")
    pi_hi = to_double(2 * pio2)
    const("MTWN_PI_HI", pi_hi, "pi.")
    const("MTWN_PI_LO", to_double(2 * pio2 - exact(pi_hi)), "pi minus MTWN_PI_HI.")
    pio2_hi = to_double(pio2)
    const("MTWN_PIO2_HI", pio2_hi, "pi / 2.")
    const("MTWN_PIO2_LO", to_double(pio2 - exact(pio2_hi)), "pi / 2 minus MTWN_PIO2_HI.")

    def table(name, values, note):
        out.append(f"// {note}")
        out.append(f"static const double {name}[{len(values)}] = {{")
        for v in values:
            out.append(f"    {v.hex()},")
        out.append("};")

    table("s_expTaylor", [to_double(D(1) / factorial(n)) for n in range(2, 14)],
          "1 / n! for n from 2 to 13: e^r for |r| <= ln 2 / 2.")
    table("s_exp2Taylor", [to_double(ln2 ** n / factorial(n)) for n in range(1, 14)],
          "(ln 2)^n / n! for n from 1 to 13: 2^f for |f| <= 1/2.")
    table("s_logSeries", [to_double(D(2) / (2 * n + 1)) for n in range(1, 12)],
          "2 / (2n + 1) for n from 1 to 11: the atanh series of the logarithm.")
    table("s_sinTaylor", [to_double(D((-1) ** n) / factorial(2 * n + 1)) for n in range(1, 9)],
          "(-1)^n / (2n + 1)! for n from 1 to 8: sine for |r| <= pi / 4.")
    table("s_cosTaylor", [to_double(D((-1) ** n) / factorial(2 * n)) for n in range(2, 10)],
          "(-1)^n / (2n)! for n from 2 to 9: cosine for |r| <= pi / 4.")
    table("s_atanSeries", [to_double(D((-1) ** n) / (2 * n + 1)) for n in range(1, 9)],
          "(-1)^n / (2n + 1) for n from 1 to 8: arctangent for |u| <= 1/16.")
    table("s_atanEighths", [to_double(atan(D(k) / 8)) for k in range(0, 9)],
          "atan(k / 8) for k from 0 to 8.")
    return out


def render():
    head = [
        "// SPDX-License-Identifier: MIT",
        "// Copyright (c) 2026 Sirac Ozmen",
        "//",
        "// Generated by tools/gen_math_constants.py; do not edit. The constants",
        "// of src/math.c (record mtwn-0008), each computed to 80 digits and",
        "// rounded once to binary64.",
        "// clang-format off",
        "",
        "#ifndef MTWN_MATH_CONSTANTS_H",
        "#define MTWN_MATH_CONSTANTS_H",
        "",
    ]
    return "\n".join(head + lines() + ["", "#endif", ""])


def main():
    text = render()
    if len(sys.argv) > 1 and sys.argv[1] == "--check":
        with open(PATH, encoding="utf-8") as f:
            if f.read() != text:
                print("src/generated/math_constants.h is stale: run tools/gen_math_constants.py")
                return 1
        print("math constants: current")
        return 0
    os.makedirs(os.path.dirname(PATH), exist_ok=True)
    with open(PATH, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote " + os.path.normpath(PATH))
    return 0


if __name__ == "__main__":
    sys.exit(main())
