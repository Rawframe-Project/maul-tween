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
  count over the doubles in their order as 64-bit integers: exact to
  the double, at most 64 halvings, each one count.
- Timelines nest at most `MTWN_MAX_CLIP_DEPTH` (32) below the root, so
  a count's chain fits a fixed scratch.

## Consequences

Counts are exact and independent of the update rate; an oracle that
samples the timing itself and splits the sweep wherever anything jumps
agrees on random nested clips.

## Amended 2026-10-16: walks left out where nothing differs

A frame where many players on clips of their own cross at once costs
the walks of every record, most of them in the preparation of its
levels. Two kinds of walk are left out, the counts unchanged:

- A level whose iterations all run one way never reaches a start of
  the other way's, and the other way's iterations enter its counts
  times none: their constants stay 0, unwalked. A constant of the same
  inputs as one already worked out (the first iteration's start and
  traversal, when it starts at 0, are a full forward one's) is copied,
  the same bits.
- An outermost level passing its time through unchanged (endless, from
  0, without delay, at rate 1, forwards), as the root of a clip with an
  endless child is, is left out of a record's counts and searches when
  the level or leaf below has nothing before its start: below it, the
  counts are the same, and none at or before 0. A negative delay below
  can put something there, and then the level stays.

The bench's 1,000 players on clips of their own, all repeating in one
frame: 25.9 to 22.7 million instructions, 3.8 times the average frame.
A random test compares counts and found times with and without the
level left out.

## Amended 2026-10-16: a zero count adds nothing

A count of whole iterations of 0 adds no term instead of multiplying
its constant: a constant may be infinite (an endless point level), and
0 times infinity is not 0. No test reaches such a pair yet; the term
skipped costs nothing measured.

## Amended 2026-10-17: a count that never falls

A crossing's time is found by halving the doubles between two times on
whether the count reaches one, which is sound only if the count never
falls as the time grows. Past a repeat delay it fell: a quotient by the
cycle rounded up past an iteration's start left a progress just below 0
in that iteration, counted as reached, and the exact 0 an ulp later, at
a start not yet passed, as not. A search with a guess and one without
then ended on different edges, and the shortcuts' sweep recorded a
crossing's time and iteration two ulps from what counting every record
did. The count's position is now rounded as the timing model's
(mtwn-0012, amended the same day), and over 400 random timings every
double within 40 of each iteration's start and end counts one way.
