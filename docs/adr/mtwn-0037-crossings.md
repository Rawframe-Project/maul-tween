# mtwn-0037. Crossings: exact counts through every ancestor's iterations

Status: Accepted

## Context

Records mtwn-0004 and mtwn-0013 make events counted records: a sweep
of a player's time crosses marks and node boundaries, through repeated,
reversed and delayed ancestors, and a stall over a fast endless repeat
must count, not walk, its crossings.

## Decision

- Each timeline's child time runs along a path: its iterations in the
  order its rate takes them, each from its start to its end, both ends
  covered; repeat delays hold the path. A point below is crossed once
  each time the path covers it. Where an iteration's end and the next
  one's start share a time (no repeat delay, or going backwards) both
  are reached at that time.
- The count of crossings a level has reached by a local time is the
  constants it keeps for its first, full forward, full reversed and
  last iterations, times their numbers, plus the iteration it reaches
  partly, whose moving end is counted at the level below. A count is
  one walk down the chain; the constants are kept bottom up once per
  point counted.
- A sweep forwards counts (previous, current], backwards [current,
  previous), so a point at an update's exact time is crossed once, in
  the update that reaches it; a sweep from no time takes its own start.
  Each crossing's direction is the point's own (its coordinate rising
  or falling).
- What is counted: an instant (a mark, a node's active start or end),
  or the iteration starts of a node after its first.
- The first and last crossing's times are found by bisection on the
  count, to the double.
- Timelines nest at most `MTWN_MAX_CLIP_DEPTH` (32) below the root, so
  a count's chain fits a fixed scratch.

## Consequences

Counts are exact and independent of the update rate; an oracle that
samples the timing itself and splits the sweep wherever anything jumps
agrees on random nested clips.
