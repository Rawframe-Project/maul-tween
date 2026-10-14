# mtwn-0045. Shortened reversals, as CSS Transitions shortens them

Status: Accepted

## Context

CSS Transitions Level 1 shortens a transition that reverses one still
running: a hover left a quarter of the way in returns in a quarter of
the time, not the whole of it. A user interface built on CSS's
behaviour needs the same, and with a plain retarget the way back
always takes the full duration.

## Decision

- A transition def's `reverses` asks for CSS's rule, for an eased
  transition. It is off by default, so a duration means its duration
  unless asked.
- Every eased transition keeps CSS's reversing-adjusted start value
  (where it began) and reversing shortening factor (1).
- When one with `reverses` replaces an eased transition and its end, as
  given, is that one's reversing-adjusted start (equal channel by
  channel, as CSS compares computed values: 0 and -0 alike; values are
  finite, so NaN never compares), the factor is the
  absolute value of that one's eased progress times its factor plus one
  minus its factor, at most 1; the new duration is its own times the
  factor, its reversing-adjusted start is that one's end, and its
  factor is kept. Otherwise it starts with a factor of 1.
- The two share the slot's room with the start velocity, which an
  eased transition does not use.

## Consequences

A host that follows CSS sets `reverses` on its transitions; springs and
decays carry velocity instead, and never shorten.
