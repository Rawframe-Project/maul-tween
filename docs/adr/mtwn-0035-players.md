# mtwn-0035. Players: Web Animations' procedures on the host's clock

Status: Accepted

## Context

Record mtwn-0012 makes players Web Animations' animations, synchronous:
the host's update is the only clock, and no call waits for a promise.
Level 1's procedures answer every playback edge, but some of them wait
for a ready time a browser gives.

## Decision

- `mtwnPlayerDef` (cookie, ABI version) names a finished clip, its
  bindings (slot to target, copied; unbound slots play on nothing),
  the playback rate, whether it plays at once and whether it is kept
  at its end. `mtwnCreatePlayer` holds the clip busy until the player
  is freed. Bindings live in the context, in its `bindingCapacity`, as
  runs taken first fit and coalesced when given back.
- Times are seconds; the timeline time is the last update's host time,
  the effect end the clip's duration. Current, hold, start and
  previous current times, and the play states idle, running, paused and
  finished, are Level 1's.
- The ready time Level 1 waits for is the next update's: a play that
  needs a start time, a rate change on a running player and a reverse
  take effect there, the old rate running until then. Pause, seek (by
  time, progress or root label), finish and cancel take effect at
  once, at the last update's time, as do rate changes on paused,
  finished or idle players, keeping the current time. Reverse is
  refused, with nothing changed, when it would play an infinite clip
  backwards from its end; finish for a rate of 0 or an infinite clip
  forwards. The finished state is judged by the effective rate.
- Transitions are players too; the controls refuse them. Every player
  is on a list in creation order, the composite order the update walks.

## Consequences

A play or reverse called between updates starts where the host next
samples, as a transition does, and matches Level 1's behaviour in a
browser whose next frame is the next update.
