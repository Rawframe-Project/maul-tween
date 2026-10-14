# mtwn-0021. The easing API: one value type, makers that validate, a mode for every kind

Status: Accepted

## Context

An easing is sampled in the hottest loop of every animation, so the
sample must not carry a status to check; yet its parameters come from
the host and must be validated somewhere. Penner's in, out and in-out
variants and CSS's reverse and mirror modifiers are the same idea named
twice.

## Decision

- `mtwnEasing` is a plain value: a kind, a mode and the kind's
  parameters, precomputed by its maker where that saves work per sample.
  What it references (a host function's context, `linear()` points)
  stays the host's.
- Makers validate and return a status, one per family of parameters:
  power, the parameterless Penner curves, back, elastic, cubic bezier,
  steps, the CSS keywords, `linear()` points and a host function.
  `mtwnLinearEasing` cannot fail.
- One mode applies to every kind: in (the curve as defined), out (its
  reflection `1 - f(1 - p)`), in-out and out-in (the reflected halves).
  It can be changed on a made easing.
- `mtwnEase(easing, progress, before)` returns the eased progress with
  no status. CSS curves follow CSS Easing Level 2 outside 0 to 1;
  Penner's curves and host functions see progress clamped to 0 to 1.
- `mtwnIsEasingMonotonic` tells a timeline whether an easing may
  shape its children's time (record mtwn-0013).

## Consequences

The hot call is a switch and the curve's arithmetic. A host reverses a
bezier as easily as a Penner curve, with one concept to learn.
