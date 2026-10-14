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
  and Keyser's end tangents; knot gaps under 1e-12 are raised to it,
  so coincident keyframes hold still.
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
