# mtwn-0008. The library writes its own transcendental functions

Status: Accepted

## Context

C libraries round `exp`, `sin`, `pow` and their kin differently, so a
bit-exact library may call only the functions IEEE 754 rounds exactly
(record mtwn-0001).

## Decision

`exp`, `exp2`, `log`, sine and cosine together, `atan2`, `cbrt` and
`pow` are written in one module from the published methods: exact
argument reduction, a minimax polynomial on the reduced range,
reconstruction by `ldexp`. Each is within 1 ulp in binary64 over the
range the library uses, stated beside it and tested against the C
library and a long double reference.

## Consequences

Springs, expo and elastic easing, colors and rotations give the same
bits everywhere, and the float results they round to are almost always
correctly rounded.
