# mtwn-0067. Monotone segments

Status: Accepted

## Context

Curve editors keep a value's curve through keyframes from overshooting
them: Unity's default tangent, ClampedAuto, adjusts "to prevent the
curve from overshooting the target value"; Blender's Auto Clamped
handles lie flat at an extremum and within the neighbours' values,
measured in time; d3's `curveMonotoneX` takes Steffen's slopes ("A
simple method for monotonic interpolation in one dimension", Astron.
Astrophys. 239, 1990), which keep the curve monotone between monotone
data, with extrema only at the data and a continuous first derivative.

A path's Catmull-Rom segment is centripetal by default, which cannot
turn back within a segment (Yuksel, Schaefer and Keyser, Computer-Aided
Design 43(7), 2011): one channel never passes its keyframes. Several
channels can, each alone (measured: up to 15% of the segment's chord),
and its tangents are in each segment's own progress, so across
keyframes of uneven spacing its speed in time jumps (0, 1, 2 at 0, 0.1
and 1: a slope of 10 into the middle keyframe, 1.11 out of it).

## Decision

`mtwn_segmentMonotone`: each channel a Hermite cubic whose slopes in
time are Steffen's, the keyframes' offsets the time. Between secants a
and b over widths ha and hb, the slope is `(sign a + sign b) min(|a|,
|b|, |p| / 2)` with `p = (a hb + b ha) / (ha + hb)`: 0 where the
channel turns or holds, never more than twice the lesser secant, so
the cubic stays monotone (Fritsch and Carlson, SIAM J. Numer. Anal.
17(2), 1980). At the path's ends, and beside a keyframe pair of no
width (a jump), d3's one-sided slope `(3 s - t) / 2`, of curvature 0;
between two keyframes alone, the line. A segment's tangents are its
slopes times its width, so under a linear easing the speed is
continuous across keyframes of any spacing.

It is a kind per segment, as editors set a tangent mode per key. Scales
take it in their logarithm, constant speed and directions as any
cubic; colors and discrete values refuse it, as they refuse
Catmull-Rom. Catmull-Rom stays the curve for a point's path, smooth in
space.

## Consequences

`test_monotone` checks every channel between its keyframes on 2,000
random paths of uneven widths and jumps, Steffen's slope continuous
across a keyframe, the ends' slopes, flat turns, a line, scales, and
refusals; `fuzz_values` checks the range on every monotone segment it
makes. Each channel's work is out of line (unrolled for four channels
it took 1.8 kB): the values part grows 1.0 kB, within its budget, and
a Catmull-Rom path's sample 5 instructions at `-O3`, from the kind's
case.
