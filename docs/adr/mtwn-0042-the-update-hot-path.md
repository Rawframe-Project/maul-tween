# mtwn-0042. The update's hot path, the same bits

Status: Accepted

## Context

The benchmark of the library's targets (`bench/`) found the update of
64,000 eased transitions several times slower than its target, and a
clip's player's update spending most of its time counting crossings no
sweep had reached. Every speedup must keep every output bit for bit.

## Decision

- The clips' players have a chain of their own, so the passes over them
  before and after an update skip the transitions.
- A player slot is a union of a clip's player and a transition, a
  transition's one curve by its spec: 312 bytes, the fields every update
  reads in its first three cache lines.
- An integer power easing already normal skips the division by its
  range (the same bits) and runs inline in the update.
- A transition of numbers eased (not a color, rotation or discrete
  value, not a spring or decay) writes its value each update and leaves
  its velocity to be computed when read: by `mtwnTarget_GetState`, by a
  transition inheriting it, or by a clip's player composing over it, at
  the last update's time in the library's floating-point environment,
  by the same arithmetic. It is stored before the transition ends.
- A node's events are not counted when its parent's child time moved
  one way in one iteration of every timeline above, both ends off those
  iterations' boundaries by more than a margin of 1e-6, and the instant
  (or for iterations the active interval) lies outside the span covered
  by more than the margin: its counts are then 0. Each node's active
  duration is kept when its clip is finished.

## Consequences

The scenario hash of every target's state and every event record is
unchanged, and so are the events of 1,200,000 random clips of nested
timelines under random controls. Timings are in the commit that made
them and in `bench/baseline.txt`.
