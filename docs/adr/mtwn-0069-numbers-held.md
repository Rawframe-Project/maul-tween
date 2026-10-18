# mtwn-0069. Numbers held within the floats

Status: Accepted

## Context

A target holds binary32 values, and the animations that drive it compute
in double. A number's value passes the largest float whenever an easing's
output passes 1 far (a back or elastic easing, a cubic-bezier's y,
`linear()` points, a host's curve) or a sum grows (an incremental repeat,
the add composite). Measured at 270eacc, from 0 to 3e38 under a back out
of 100: a scalar, a 2D vector and an integer target went infinite, value
and velocity, on 60 of 61 frames; a clip track's segment, an endless
incremental repeat and two adding players did the same; an angle and a
color did not. A velocity taken from two infinite samples was NaN, as a
scale's had been (mtwn-0059).

CSS Values and Units 4 converts a value an implementation cannot hold
"to the closest value supported by the implementation" (section 5) and
clamps "only ... the final result of applying all interpolation-related
operations" (section 3.1). This library's scales are held at the largest
float already (mtwn-0059).

## Decision

A number is held at the nearest finite float, ±0x1.fffffep127, past the
range, and is unchanged within it, so no in-range result changes a bit.
Each value a target or a sample takes is held where it becomes a float:
the transitions' values, eased and sampled; a track's sample, so the
values layer gives what a target would; the composition; the incremental
repeat. A value's four channels are converted together and then held
together, which is the same as holding the doubles first (a double past
the largest float converts to an infinity, held at the largest) and
compiles to a packed conversion, a minimum and a maximum. A velocity is
taken from the values held, in double, and is a float within the finite
floats, so a number held at a bound is still.

Clamping at each conversion, not once where a target is written, keeps
the sums before it in double, where they do not overflow, and leaves no
infinity to subtract from another (infinity minus infinity is NaN, which
no later clamp undoes).

## Consequences

The cost, measured against 270eacc (gcc 14): a track's sample 243 to 249
instructions at `-O2`, within mtwn-0058's target of 250 (260 when each
channel was held as a double before converting, which lost the packed
conversion); the bench's clip frame 0.4% more; 1.4 kB of code.

No target's value or velocity is infinite or NaN from any easing or sum
of finite values. `test_transition`, `test_clip_play` and `test_track`
hold each path probed; `fuzz_api` eases a transition drawn below 0, and
a clip track's first key drawn below 0, by a host curve that peaks near
1.5e44, so its invariant (every value and velocity finite) reaches these
paths; with the helper removed it fails. A host easing returning a
non-finite output remains the host's: the library neither checks nor
forces its ends.

## Amended 2026-10-18: where a value is held, and an oracle

Three values were not held where they became floats: an unbounded
number decay's end past the floats (infinite, found fuzzing), an
incremental track's step past them (infinite, every later iteration at
the bound, a finite value no fuzzer's check sees) and an angle track's
Bezier controls moved by the unwrap past them (infinite, the track
refused). Each is now computed in double and held where it becomes a
float (1.0.1 and after).

Players composing onto one target compose each onto the value the layer
before wrote, a float the target holds, so a sum that leaves the floats
and comes back is held between layers: a base of 3e38 adding 3e38 and
then -3e38 ends at the largest float less 3e38, not at 3e38. Carrying a
double per target through an update would cost 32 bytes a target and the
composition's path for sums no float target's animation reaches; the
layer's value as the target holds it is what Web Animations composites
the next effect onto, its underlying value.

`test_bounds` checks each place against an oracle: inputs near the
bounds (a transition eased past its ends, three-keyframe clip tracks, an
incremental repeat, two players adding) against the same arithmetic in
double from the inputs, held once where a target takes it. A value at
the bound where the oracle is not, or short of it where the oracle is,
fails; with the incremental step rounded to a float again, as in 1.0.0,
it fails.
