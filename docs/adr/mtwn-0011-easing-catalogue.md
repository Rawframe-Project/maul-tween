# mtwn-0011. The easing catalogue, landing exactly

Status: Accepted

## Context

Penner's equations do not all reach their ends (expo is 2^-10 at 0), and
each library parameterizes back and elastic its own way.

## Decision

- Penner families: power with any exponent, sine, expo, circ, back
  (overshoot, default 1.70158), elastic (amplitude and period in GSAP's
  form, period 0.3, or 0.45 for in-out), bounce. Each comes as in, out,
  in-out and out-in, reflections of its in curve.
- A curve g not exact at both ends is used as `(g(p) - g(0)) / (g(1) -
  g(0))`, which IEEE arithmetic makes exactly 0 and 1 at the ends with
  no special case.
- CSS Easing Level 2 entire: `linear`, `linear()` with points, `cubic-
  bezier` (record mtwn-0009), the four keywords, `steps` with every jump
  position and the before flag.
- A host function; reverse and mirror for any easing.
- Each parameter's mapping to GSAP, Penner's and tween.js's is
  documented; Penner's in-out back is ours with an overshoot of `1.525
  s`.

## Consequences

Every easing is continuous and exact at its ends, and an animation
written for another library can be matched by its parameters.

## Amended 2026-10-17: back computed as p^2 (p + s (p - 1))

Back's curve p^2 ((s + 1) p - s) ends at (s + 1) - s, which is 0 once
s + 1 rounds to s (from 2^53), so its normalization divided by 0 and
every value was NaN or infinite. The same curve as p^2 (p + s (p - 1))
is exactly 1 at the end for any s and has no cancellation there: for
GSAP's 1.70158 it is within 1.1e-16 of the exact curve, where the first
form missed by up to 4.6e-16. The values' fuzz found it.
