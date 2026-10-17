# mtwn-0040. Incremental tracks: each iteration moves on

Status: Accepted

## Context

Web Animations Level 2's iteration composite accumulates successive
iterations onto the previous final value; DOTween's and PrimeTween's
incremental loops move on by end minus start each loop. A repeated
move or spin should go on from where it ended.

## Decision

- A track def's `incremental` makes iteration k add k times its last
  keyframe minus its first (keyframes as made: angles unwrapped,
  rotations on their path), k the current iteration, counted from the
  iteration start as Level 2 counts. The path stays continuous for any
  first keyframe, where Level 2's final value would jump unless the
  track starts at zero.
- A rotation turns by the rotation from its first keyframe to its
  last, k times, before its own path: the power is its axis with its
  angle times k, not k products, so iteration 10^6 does not drift.
- Numbers, vectors, angles, integers (rounded once, after) and
  rotations; colors and discrete values are refused.
- Velocity is measured on the moved path, so it runs on across a
  boundary.

## Consequences

A loop of a quarter turn spins on; a step of a staircase climbs on.

## Amended 2026-10-17: endless iterations of no duration refused

A timing of no duration and endless iterations is valid: all its
iterations pass at once, and past them (or before them, for a negative
rate) the current iteration is infinite, as Web Animations counts it.
An incremental track there moved on by infinity times its step: an
infinite value, or NaN for a step of 0. No value is right for it, so an
incremental track with such a timing is refused when it is added, as a
color is; one not moving on, or with endless iterations that take time
(whose iteration is never infinite), is not. The API's fuzz found it.
