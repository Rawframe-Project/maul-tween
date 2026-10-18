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
