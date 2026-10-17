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

## Amended 2026-10-17: a position the rounding steps back

With a repeat delay the position is the quotient of the scaled time by
the cycle and the progress within it, and both round. A progress that
rounds to the iteration's end (`k + within / d` rounding up to `k + 1`)
showed the next iteration's start for one time before the delay held
the end: iterations of 0.1 s with delays of 0.1 s showed it at 0.5 s, a
time frames of a tenth land on, a value jumping to the keyframes' start
for a frame. A quotient rounded up past an iteration's start left the
progress an ulp below it, out of the delay before. The first now holds
the end, as the delay does an ulp later; the second is in that delay.
Over 3,000 random timings, every double within 40 of each iteration's
start and end now goes one way; there were 1,173 steps back. Where the
times are too large for an ulp to be one, before the first iteration,
nothing changes. The rounding at the active end of a negative rate,
which follows the specification computed in doubles, is a separate
matter, decided the same day (below).

## Amended 2026-10-17: an overall progress rounded to the end

A negative rate begins at the iterations' end, and its scaled time is
the active duration less the active time: an active time below half an
ulp of the active duration is lost from it, so a double after the
active start the overall progress is the end exactly while the active
time is not 0. The end hold asked for an active time of exactly 0 (the
active duration for a positive rate), so there an iteration past the
last showed its start: with a delay of 0.171875 s, one double after it,
iteration 5 of 5 at progress 0, the keyframes' start for that time.

The end is now held, in the phases it lies in (active or after for a
positive rate, before or active for a negative one), also where the
overall progress is the iterations' end, unless the iterations add
nothing to a start too large to keep them. Blink holds the end where
the active time is within a microsecond of the active duration
(`IsWithinAnimationTimeTolerance` in `timing_calculations.cc`), which
covers this rounding; Gecko compares exactly, on integer ticks that do
not lose a small active time this way. The rule is the crossing
count's: it already held the end where the progress reached it, so the
two agree bit for bit. No Level 1 case moves, the oracle's random cases
being dyadic; the oracle states the same rule and keeps the two
rounding cases of this record as regressions.
