# mtwn-0064. A path's direction

Status: Accepted

## Context

Something moving along a path turns with it: CSS Motion Path's
`offset-rotate: auto` rotates by the path's direction at the offset
position (`reverse` adds 180 degrees, an angle adds itself), GSAP's
MotionPath has `autoRotate`, SVG's `animateMotion` `rotate="auto"`.
The direction is SVG 2's path directionality: the curve's direction
within a segment, the later segment's at a boundary, a zero-length
segment's from the nearest segment with a length before it (at its
start) or after it (at its end), +x for a path of no length, and at a
cusp a direction between the incoming and outgoing. `mtwnSamplePath`
gave a segment's derivative, which is 0 where a Bezier control sits on
its end, where keyframes coincide and where a step holds; and the
host's atan2 of it differs between C libraries.

## Decision

`mtwnPathDirection` gives the direction at a progress as a unit vector
in the path's channels:

- within a segment, the first of its first, second and third
  derivatives that is not 0, normalized: where the first vanishes (a
  control on its end, a cusp) its limit, the outgoing direction;
- at a keyframe the later segment's, as the path's sample takes it;
- a step, or a segment whose derivatives are within its values'
  rounding (coincident keyframes, a Catmull-Rom between them whose
  tangents are rounding), takes the direction of the nearest segment
  before it with a length, at its end, else after it, at its start;
- a path of no length points along its first channel.

`mtwnPathAngle` is atan2(y, x) of the first two channels by the
library's own arctangent, the same bits everywhere. A scale's direction
is in its logarithm. A 3D orientation is a look-at from this direction
and an up vector, a rotation's question rather than a path's: not
built.

## Consequences

`test_direction` checks two lines (their directions, the later one's
at the keyframe, extended past the start, at constant speed), a control
on the start (the second derivative's way) and both controls there (the
third's), a step holding the line before's heading, coincident
keyframes taking the line after's, a held Catmull-Rom start with and
without constant speed (its own derivatives point at (-1, 0), rounding
noise), a point, and one channel backwards (pi).

The direction's code is 1.4 kB (its derivatives taken in one pass over
the channels; at first, an order at a time, 1.9 kB): the values part
from 53.5 to 55.1 kB and every function from 151.5 to 153.2 kB. The
values' budget is restated from 54 to 57 kB and everything's from 152
to 156 kB. The path's sample now calls its segment's location rather
than having it inlined: 2,646 instructions at -O3, of a bound of 2,675.
