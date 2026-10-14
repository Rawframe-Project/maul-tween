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
  last, k times, before its own path.
- Numbers, vectors, angles, integers (rounded once, after) and
  rotations; colors and discrete values are refused.
- Velocity is measured on the moved path, so it runs on across a
  boundary.

## Consequences

A loop of a quarter turn spins on; a step of a staircase climbs on.
