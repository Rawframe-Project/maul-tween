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
