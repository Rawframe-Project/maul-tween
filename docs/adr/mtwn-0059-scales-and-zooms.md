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

## Amended 2026-10-17: the scale kind in targets and transitions

`mtwn_valueScale`, one channel above 0, is a kind of target. A
transition eases it along its scale mix; a spring moves the mix's
progress, as for colors and rotations, so it overshoots by a ratio, its
start velocity taken as a rate in the logarithm, v / value, over the
logarithm's distance; a decay coasts in the logarithm, its start
velocity v / value, its rest speed a share of the value a second, its
value the exponential and its velocity the value times the rate. A
target or end of 0 or below is refused, and so is a decay whose stop
rounds to a float's 0 or infinity. Clips' tracks take the kind with
their own amendment; until then a clip has no track of it to bind.

The zoom now lives apart from the scale (`zoom.c`), so a program that
transitions a scale links the scale's mix alone: in one file the zoom
came along and the transitions part went to 54.5 kB, over its 54.0 kB;
apart, it is 52.0 kB (from 50.7) and the clips' 130.3 kB (from 129.1).

## Amended 2026-10-17: scale tracks

A track of `mtwn_valueScale` is a path over its keyframes' logarithms,
taken where the path loads a value or a control (`mtwnPath` marks it in
a byte that was reserved, so its size is the same); the keyframes stay
as the host wrote them, as taking logarithms in place could not be done
twice. A sample is exact at every keyframe, a linear segment's is the
scale's mix, the others' the exponential of their cubic; at constant
speed the lengths are the logarithm's, so equal ratios take equal times.
Values and Bezier controls must be above 0. The check of the flag on
each load costs the path's sample 22 instructions (2,595 to 2,617 in the
gate's build) and the track's 5 (243 of 250 at `-O2`). The path's file
then passed the family's 1,000 lines; its arc length and the inverse
moved to `path_length.c`, the path calling the inverse across files, 25
more: 2,642 in the gate's build, 2,768 of the 2,800 target at `-O2`.

## Amended 2026-10-18: scales in clips

A clip takes tracks of `mtwn_valueScale`, and players compose them in
the logarithm: add multiplies the underlying value by the effect to the
weight's power, a b^w; replace moves toward the effect by that power of
the ratio, a (b / a)^w, exactly b at full weight (and add exactly a b);
velocities compose as rates, v / s, so the result's is its value times
the composed rate. Accumulate is add, as for every kind. An incremental
scale track moves on by the ratio of its last keyframe to its first each
iteration, taken in double from its keyframes. The parts after: clips
132.5 kB of 135.0, everything 145.8 kB of 146.0; the next growth of
either restates its budget.

## Amended 2026-10-18: a scale within the float range

A scale is held as a float, and its logarithm's line leaves the float
range quickly: a steep segment extended back by an easing's overshoot,
an easing far past 1 (`linear()` to 400 takes 1 to 10 to 10^400), an
incremental repeat's ratio to a high power. `fuzz_values` found a track
sampled to infinity from ordinary keyframes. Every place a scale becomes
a float now holds it within the positive floats, the largest past them
and the smallest below, so no target's scale is ever infinite or 0: a
track's sample, a transition's value, the composition, the incremental
repeat.

## Amended 2026-10-18: a velocity within the float range too

A scale's velocity is its value times its logarithm's rate, so a value
past the floats took the velocity with it: `fuzz_api` flung a zoom at
1e12 a second against bounds of 0.875 and read a velocity of infinity
beside the value held at the largest float. A scale's velocity now
becomes a float the way its value does, within the finite floats, and
is 0 at rest however far its value lies; a clip track's velocity, a
difference over a millisecond, which can pass the floats between two
finite values of any kind, likewise.
