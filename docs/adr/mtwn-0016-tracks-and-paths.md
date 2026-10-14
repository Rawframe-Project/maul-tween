# mtwn-0016. Keyframe tracks, centripetal Catmull-Rom, constant speed

Status: Accepted

## Context

Uniform Catmull-Rom curves can form cusps and loops within a segment;
the centripetal form cannot (Yuksel, Schaefer and Keyser, 2011). Moving
along a path at constant speed needs its arc length.

## Decision

- A track has two or more keyframes at offsets in [0, 1], each segment
  with its easing and its kind: linear, step, cubic Bezier with control
  points, or Catmull-Rom, centripetal by default with uniform and
  chordal by parameter.
- On request, a track moves at constant speed: its arc length is
  integrated by adaptive Gauss-Legendre quadrature to 1e-9 relative when
  the clip is built, and a sample finds its segment by binary search and
  its parameter by safeguarded Newton. Tangents are available.
- Integers round to nearest with ties away from zero; discrete values
  take the end value from progress 1/2, as Web Animations does.

## Consequences

Paths have no surprise loops, constant speed costs nothing per frame
beyond a search, and integer and discrete values follow the web's rules.
