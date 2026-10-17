# mtwn-0059. Scales and zooms

Status: Accepted

## Context

A scale mixed linearly from 1 to 100 seems to slow down: each step adds
the same amount, so its ratio falls. GSAP answers with ExpoScaleEase,
`expoScale(start, end, ease)`, an easing bent so the value moves as
start (end / start)^ease(p); it must be given the tween's own ends. A
zoom with a pan has a second problem: zooming and panning together
loses the eye. van Wijk and Nuij ("Smooth and efficient zooming and
panning", IEEE InfoVis 2003) found the optimal path between two views
(a centre and a width) in a space where a pan's cost grows with the
width, with rho trading zoom against pan (sqrt 2 by their measure).
d3-interpolate's `interpolateZoom` implements it, and Mapbox's and
Leaflet's `flyTo` follow it (Mapbox's curve 1.42, the mean of the
paper's user study).

## Decision

A scale is a value moved in its logarithm, not an easing: an easing
that knows its ends breaks when a transition is retargeted, when values
are composed and on a track of several keyframes, where a value kind
lets any easing apply as it is (GSAP's `expoScale(s, e, ease)` is a
scale mix eased by that ease). The values layer has it first:

- `mtwnMakeScaleMix` and `mtwnMixScale`: from (to / from)^p, the ratio
  kept as a difference of logarithms so it cannot overflow, a sample one
  exponential, the ends exact; scales finite and above 0, else refused.
- `mtwnMakeZoomMix`, `mtwnMixZoom` and `mtwnZoomLength`: van Wijk and
  Nuij's path, its length S for a duration. Written to keep its digits
  where the paper's form loses them: u(s) as w0 sinh(rho s) /
  (rho^2 cosh(rho s + r0)) in place of a difference that cancels near
  0, r = -asinh(b) by its stable form, the terms in units of w0 so views
  of any scale take the same path, and equal centres as a geometric
  zoom of length |log(w1 / w0)| / rho, the limit of a pan going to 0.

The scale as a kind of every target, transition, track and clip
follows by its own amendments.

## Consequences

Against 60-digit decimals of the paper's own formulas
(`tools/oracle/zoom_oracle.py`, 240 view pairs at three scales, ten
progresses, five rhos): views within 1.3e-14, lengths 1.4e-13 relative,
scales 1.3e-13 across 1e-300 to 1e300. d3's `interpolateZoom`, written
again in doubles on the same cases, is up to 0.67 off: its threshold for
equal centres is an absolute 1e-12 on the squared pan, so small views
lose their pan, and zooming in with equal centres gives a negative
length (and duration).

The values part (record mtwn-0055) grows 2.9 kB, nearly all of it
`scale.c`, to 52.2 kB; its budget is restated from 50.0 to 54.0 kB by
this record.
