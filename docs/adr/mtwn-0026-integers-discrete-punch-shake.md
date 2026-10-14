# mtwn-0026. Integers, discrete values, punch and shake as values

Status: Accepted

## Context

Counters animate through whole numbers, enumerations and flags cannot
interpolate at all, and punch and shake (DOTween's, PrimeTween's,
LitMotion's) are signals added to a value rather than paths between
two. The rivals' shake draws from a global random generator, so it
never repeats the same way twice or on another platform.

## Decision

- `mtwnMixInteger` interpolates `int64_t` in double and rounds to
  nearest, ties away from zero, passing through every integer between;
  exact at the ends and for values within 2^53, clamped to the type's
  range past it.
- `mtwnMixDiscrete` is Web Animations' discrete step: the start below
  progress 1/2, the end from it.
- `mtwnPunch` and `mtwnShake` are made with a frequency, a duration and
  an envelope exponent (1 fades linearly); a shake also with a 64-bit
  seed. A sample gives a factor in [-1, 1] the host scales by each
  channel's amplitude: punch `sin(2 pi f t) (1 - p)^e`, shake Perlin's
  improved gradient noise in one dimension with gradients from
  SplitMix64 of the seed, the channel and the lattice index, under the
  same envelope. Both are exactly 0 at 0, at the duration and outside
  them.

## Consequences

A shake with a given seed is the same bits on every platform, which the
tests pin with kept values; amplitude stays in the host's units.
