# mtwn-0009. Cubic-bezier easing by safeguarded Newton

Status: Accepted

## Context

CSS leaves the solver's method and accuracy open. Measured on eight
curves, 32-step bisection takes 171.7 ns per sample with a worst error
of 2.7e-10 on smooth curves; Newton with a fixed count fails near flat
points.

## Decision

Newton on x(t) = x from t = x, inside a bracket every step narrows,
bisecting whenever a step would leave it or the slope is not positive;
it stops when the error in x is at most 1e-14 or the bracket is under
2^-52, after at most 64 steps. 0 and 1 map exactly to 0 and 1, and
inputs outside [0, 1] extrapolate along the end tangents as CSS Easing
Level 2 specifies.

## Consequences

Measured, 24.6 ns per sample with a worst error of 1.3e-13 on smooth
curves, and never worse than bisection on degenerate ones, where a flat
point sets a floor of about 2e-7 for every method. A stop on a tolerance
is still bit-exact: the same input takes the same steps.
