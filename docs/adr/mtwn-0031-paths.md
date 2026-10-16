# mtwn-0031. Paths: keyframe curves in the values layer

Status: Accepted

## Context

Record mtwn-0016 gives tracks keyframes with linear, step, cubic Bezier
and Catmull-Rom segments, centripetal by default, and constant speed by
arc length. Record mtwn-0003 keeps every curve in the values layer, so
a host can bake a path with no context.

## Decision

- `mtwnKeyframe` holds a value of up to four channels, its offset in
  [0, 1], the kind and easing of the segment it starts, and Bezier
  controls (`out` starts a segment, `in` ends one). Offsets do not
  decrease, start at 0 and end at 1; equal offsets jump.
- `mtwnMakePath` references the caller's keyframes and, for constant
  speed, a caller array it fills with the cumulative arc length at each
  keyframe. Nothing is allocated.
- A progress picks the last keyframe at or below it (the end segments
  extend outside [0, 1]), eases the segment's local progress by its
  easing and evaluates its kind. A step holds until the next keyframe.
- Catmull-Rom takes a knot exponent alpha from 0 to 1 (0.5
  centripetal, 0 uniform, 1 chordal), reflects the ends' missing
  neighbors, and is evaluated as a cubic Hermite with Yuksel, Schaefer
  and Keyser's end tangents; each knot gap is raised to 1e-12 of the
  largest of the segment's three, so the curve is the same at any
  scale and coincident keyframes hold still.
- Arc length is adaptive Gauss-Legendre (orders 5 and 10 agreeing to
  1e-9 relative, a fixed stack bounding the splitting). At constant
  speed a progress is a share of the whole length; the segment is found
  by binary search and the parameter by safeguarded Newton. Step
  segments are refused there.
- `mtwnSamplePath` also gives the segment's tangent by its own
  parameter, for orienting along the path.

## Consequences

The tests find the Hermite form within 1.6e-15 of Barry and Goldman's
pyramid in long double for alpha 0, 1/2 and 1, arc length within 1e-15
relative of an extrapolated polyline, and constant speed within 6e-12
on lines whose parameter is uneven.

## Amended 2026-10-16: the length and the parameter, faster and exact

- Arc length is adaptive Gauss-Kronrod: QUADPACK's 15-point rule
  against its 7-point Gauss rule, agreeing to 1e-9 relative. About an
  interval's middle the derivative is a quadratic, so each pair of
  abscissas shares its even part.
- A path of one channel has its total variation for a length, summed
  between the turns where its derivative is 0 (the roots by the stable
  quadratic formula): exact, where a quadrature misses the kink each
  turn puts in |x'|. A number turning twice had points at constant
  speed 1.1e-4 of its length off.
- The parameter at a length starts from the length measured from the
  segment's nearer end and takes Halley's steps (the speed and its
  first two derivatives), in the bracket as before. A step under 1e-4
  in the parameter, where the speed is at least 1e-3 of the segment's
  length, adds the length's Taylor series to h^3 instead of measuring.
- Summing each Newton step's measured length had drifted from the
  length measured afresh: on a random curve of two channels a point
  came 1.6e-7 off. Kronrod's pieces, fewer and far more accurate, keep
  the sum within the tolerance; a test checks each share's point
  against the parameter found again by Simpson's rule in long double.

The bench's path through eight points (record mtwn-0058): 5,250 to
2,026 instructions a sample, GCC 14 at `-O2`; the library's code
0.6 kB smaller.
