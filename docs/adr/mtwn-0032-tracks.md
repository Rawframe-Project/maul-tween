# mtwn-0032. Tracks: every value kind over keyframes

Status: Accepted

## Context

A path (record mtwn-0031) is numbers. A clip's tracks also animate
integers, discrete values, angles, colors and rotations, each with its
own rule between keyframes; record mtwn-0003 puts those rules in the
values layer.

## Decision

- `mtwnMakeTrack` takes a def (the value kind, Catmull-Rom's knot
  exponent, the kind's choices), the keyframes, and caller arrays for
  arc lengths (constant speed) and the segments' mixes (colors,
  rotations); `mtwnSampleTrack` gives the value as four floats.
- Numbers and vectors are the path; integers the path rounded to
  nearest, ties away from zero; discrete values flip at an eased 1/2 of
  a linear segment, as Web Animations steps them, or hold through a
  step.
- Angles are unwrapped in place, each keyframe and its controls moved
  by the method's whole turns from the one before, then are a path.
- Colors mix linear segments in the interpolation space and hold
  through steps; splines are refused, as CSS has none.
- Rotations slerp linear segments with the track's path, hold through
  steps, and take Bezier and Catmull-Rom per component normalized
  after, as glTF does; with the shortest path keyframes and controls
  are first flipped in place into the hemisphere of the one before.
  Every rotation that comes out is of unit length.
- Constant speed for numbers, vectors, angles and integers only. A
  refused track leaves its keyframes as they were.

## Consequences

A clip track is a track with timing; a host can sample one with no
context. glTF's rotation rules mean an imported animation plays as its
source did.
