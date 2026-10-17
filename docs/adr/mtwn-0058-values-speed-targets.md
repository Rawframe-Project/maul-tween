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

## Amended 2026-10-17: the path's target at 2,650

Record mtwn-0031's amendments of this day made the path's lengths
exact where they were not: a turn back along a line inside a segment
had hidden from both rules (a length up to 1.1e-3 off), and the rules
agreeing to 1e-9 left lengths near a sharp minimum of the speed 2.5e-9
of a segment off and random paths 2.5e-10. They now agree to 1e-12, the
tolerance points are placed to, and are split at the speed's minima:
near turns within 6.1e-12, random paths exact to their last bits. The
path's sample went from 1,925 instructions before them (2,026 when
this record set 2,050) to 2,595: the
minima's search and the split about 166, the tighter agreement the
rest, 2.98 pairs of rules a sample where there were 2.35.

Tried: the rules' sums taken channel by channel instead of point by
point, the same bits, 2,369 instructions, but GCC at `-O3` unrolls and
vectorizes it into 2.3 kB more and the values part to 51.6 kB of its
50.0 kB budget (record mtwn-0055); declined, as a quadrature compiled
for two channels alone was for 15% at 1.6 kB. A test of the minimum's
sharpness saved 17 instructions and was no more accurate.

The target is now at most 2,650 instructions a sample at that
accuracy. GSAP's 538 still is not the target: its table puts a point
up to 7.7e-4 of the length off.
