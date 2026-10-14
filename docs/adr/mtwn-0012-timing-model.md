# mtwn-0012. Web Animations timing on every node; synchronous players

Status: Accepted

## Context

Web Animations Level 1 specifies phases, fill, direction, iteration
start, fractional and infinite iterations and the boundary cases
exactly; Level 2 extends it to groups. Users of other libraries also
rely on a delay between repeats and on a yoyo whose shape repeats on the
way back.

## Decision

- Every clip node computes Web Animations Level 1's steps exactly, with
  a repeat delay between iterations (not after the last), a choice of
  yoyo shape (time running back, the default, or the shape repeating), a
  finite nonzero playback rate per node and incremental accumulation for
  tracks.
- Groups pass their transformed time to their children (Level 2) and
  last as long as their latest child unless a duration is set.
- A group's easing must never turn back: back, elastic, bounce,
  `linear()` with a decreasing output, `cubic-bezier` with y outside [0,
  1] and host functions are refused on groups when a clip is built.
- A player has a start time, a hold time and a signed rate, states idle,
  running, paused and finished, and `play`, `pause`, `reverse` with
  Level 1's auto-rewind, `finish`, `cancel`, `seek` and a rate change
  that keeps the current time. Every call takes effect at once.

## Consequences

Timing matches the browsers' model where they overlap, and the one
restriction keeps every event crossing exact.
