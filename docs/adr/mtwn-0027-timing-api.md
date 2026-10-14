# mtwn-0027. The timing API: Web Animations' steps, rate, repeat delay

Status: Accepted

## Context

Record mtwn-0012 chose Web Animations Level 1's timing model with Level
2's effect playback rate, a repeat delay and two ways for reverse
iterations to ease. This record fixes how they meet in code, where
Level 1's terms leave room.

## Decision

- `mtwnTiming` is a value holding the effect timing and the additions;
  `mtwnDefaultTiming` gives Level 1's defaults, `mtwnValidateTiming`
  checks one, and `mtwnComputeTiming` returns the phase, active
  duration, end time, active time, overall and simple progress, the
  current iteration, the directed and transformed progress, the before
  flag and whether the time lies in a repeat delay. Unresolved values
  are NaN; times are seconds in double.
- Level 1's steps as written. Level 2's rate divides the active
  duration and scales the overall progress, from the far end when
  negative, which needs a finite active duration (refused otherwise).
  The before flag takes "going forwards" as the iteration running
  forwards exactly when the rate is positive.
- A repeat delay r, above 0 with a finite duration d above 0, gives
  each iteration but the last d + r of active time; inside a delay the
  progress holds at its iteration's end. Without one the arithmetic is
  Level 1's exactly.
- A reverse iteration eases with time running back, `ease(1 - p)`, the
  default, or with the shape repeated, `1 - ease(p)`.
- `tools/oracle/timing_oracle.py` computes the steps again in Python
  from the specifications' text and writes the expected results of
  1600 cases on dyadic inputs; the tests match them to the bit, and CI
  checks the cases are current.

## Consequences

The timing of a node is a pure function of its timing and local time,
the same on every platform; groups (record mtwn-0012) pass their
transformed time to it.
