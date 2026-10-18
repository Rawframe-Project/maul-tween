# mtwn-0058. The values layer's speed targets, measured

Status: Accepted

## Context

Record mtwn-0043 set targets for transitions, curves and sizes; none
for a color, a rotation, a path or a track. One sample of each was
measured against a rival, in instructions (`perf stat -e
instructions:u`, the difference of two counts over the samples
between), the cases the same on both sides: red to blue mixed in Oklab
and in OkLCh, returned in sRGB (culori 4.0.2); identity to 120 degrees
about (1, 2, 3) by slerp (gl-matrix 3.4.4); eight points sampled at
constant speed, centripetal Catmull-Rom here (GSAP 3.15.0's
MotionPathPlugin); a number keyed 0, 100, 0 with a quadratic out on
each segment (Motion 14.2.0's `interpolate`). The rivals ran under
Node 24.22.

## Decision

Instructions a sample, GCC 14 at `-O2` on x86-64, counted by the
bench's `--count` cases (oklab, oklch, slerp, path, track):

- A color mixed in Oklab at most 1,800, in OkLCh at most 2,050.
  Measured 1,779 and 2,012; culori 3,629 and 3,877.
- A rotation by slerp at most 800. Measured 782; gl-matrix 533, by the
  engine's `acos` and `sin`, which differ between engines. Record
  mtwn-0008's own functions give the same bits on every platform, and
  are the cost of that.
- A path at constant speed at most 2,050, at its accuracy (record
  mtwn-0031). Measured 2,026, from 5,250. GSAP's 538 is not the
  target: its table of lengths puts the point up to 7.7e-4 of the
  length off, so its constant speed speeds and slows. An exact
  method's floor is about two measured lengths a sample, and the path
  is there: 2.1 lengths, 2.3 intervals of 15 points. A quadrature
  compiled for two channels alone reaches 1,713 but adds 1.6 kB, past
  the clips' code budget (record mtwn-0051).
- A track of three keyframes at most 250. Measured 344; Motion 227,
  picking the segment and mixing two numbers where the track goes
  through the path's general segment. Missed: to be fixed.

A target missed is fixed, or changed by a record that says why.

## Consequences

The bench prints a time for each case and counts each alone. Wall
times stay information; instruction counts compare code.

## Amended 2026-10-16: the track's target met

A track's sample built its segment's cubic every time, where a linear
segment needs two values and one mix. A linear segment is now mixed
straight from its keyframes by the expression the cubic gave (the end
exact at 1), the same bits. The track 344 to 237 instructions, the
target met. Clip frames, whose tracks sample the same way, take 7.6%
fewer instructions (the bench's spread players 6.21 to 5.74 million).

## Amended 2026-10-17: the path's target at 2,800

Record mtwn-0031's amendments of this day made the path's lengths
exact where they were not: a turn back along a line inside a segment
had hidden from both rules (a length up to 1.1e-3 off), and the rules
agreeing to 1e-9 left lengths near a sharp minimum of the speed 2.5e-9
of a segment off and random paths 2.5e-10. They now agree to 1e-12, the
tolerance points are placed to, and are split at the speed's minima:
near turns within 6.1e-12, random paths exact to their last bits. The
path's sample, counted as this record counts (GCC 14 at `-O2`), went
from 2,025 instructions before them to 2,727; in the gate's Release
build (`-O3`, which the bench's bounds count) from 1,925 to 2,595. The
minima's search and the split take about 166 of the latter, the
tighter agreement the rest: 2.98 pairs of rules a sample where there
were 2.35.

Tried: the rules' sums taken channel by channel instead of point by
point, the same bits. At `-O3` it takes 2,369 instructions, but GCC
unrolls and vectorizes it into 2.3 kB more and the values part to
51.6 kB of its 50.0 kB budget (record mtwn-0055); at `-O2` it is slower,
2,937. Declined, as a quadrature compiled for two channels alone was
for 15% at 1.6 kB. A test of the minimum's sharpness saved 17
instructions and was no more accurate.

The target is now at most 2,800 instructions a sample at that
accuracy. GSAP's 538 still is not the target: its table puts a point
up to 7.7e-4 of the length off. (This amendment was first published
with the gate's `-O3` counts, 2,595 and a target of 2,650; this
record's targets are `-O2` counts. Counted so, the others hold: Oklab
1,779, OkLCh 2,012, slerp 794 and the track 239.)

## Amended 2026-10-18: counted again after scales, inertia and directions

Counted again (`-O2`) after records mtwn-0059 to mtwn-0065: Oklab
1,779, OkLCh 2,012 and slerp 794, unmoved; the path 2,776 and the track
243, both within their targets. The path's growth since 2,727: 41 from
scale tracks, which a path reads as their logarithms, testing at each
keyframe it loads whether to take one, and which moved the arc length
to a file of its own, so the sample calls it; 2 from a segment of a
length within rounding of none held still; 6 from the direction's
access to segments. A Catmull-Rom segment loaded its two keyframes
twice, once for the segment and once for its tangents, two tests each:
it now takes them from the segment, and the path is 2,754 (2,621 in
the gate's `-O3` build, 2,646 before). The track's 4, the test on the
linear segment's two loads, stays: it is the scale kind's.

The bench's frames, counted at `-O3` against record mtwn-0043's bounds,
had grown about 0.6% (the clips 5.946 to 5.981 million): 4 instructions
a track sample from scale tracks, 3 more where a clip's track moves on
by its iterations, whose scale case made the common return, a track
that does not, dearer, and the test of a context's record capacity
(record mtwn-0062), 3,100 a frame. The moving on is now out of line
behind a test of whether the track moves on at all: the clips 5.886
million, below their count before, and every other frame lower too.

## Amended 2026-10-18: the monotone track's target

Record mtwn-0067's monotone segment is a values case of its own: a
scalar track of five keyframes, 0, 30, 40, 100 and 100 at 0, 0.1, 0.4,
0.5 and 1, sampled as the track is. Its segments first worked all four
channels whatever the path's; they now work its channels only, the
others left 0: 805 instructions to 518 at `-O3`, the same keyframes as
Catmull-Rom taking 547. Counted as this record counts (`-O2`): 551. The
target is at most 600 instructions a sample; the bench's bound 535.

