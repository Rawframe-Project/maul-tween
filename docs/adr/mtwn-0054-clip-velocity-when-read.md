# mtwn-0054. A clip track's velocity worked out when read

Status: Accepted

## Context

A running clip player's velocity is its values' change over the last
millisecond (mtwn-0036). To get it, every track was sampled twice per
frame, at the frame's time and a millisecond before: the timing steps,
the keyframe search and the path, all twice. Hardly any frame's
velocity is read: only `mtwnTarget_GetState`, a transition inheriting
it, or a player composing over the target reads it. mtwn-0042 already
defers an eased transition's velocity until it is read.

## Decision

- A track of numbers (not an integer) that replaces its target at full
  weight writes its value and leaves its velocity to be worked out when
  read. Replacing at full weight composes to the track's own value
  exactly, so the target's value is what the velocity needs.
- The target's stale velocity field keeps what the second sample needs:
  the track's parent's time a moment ago, whether time ran backwards in
  it, and which track of which clip. It keeps no state of the player's,
  so a rate change, a seek or the player's going cannot change the
  answer.
- A read works the velocity out with the code the eager path uses, from
  the track alone.
- Setting a target's base first stores the velocity, since the base
  replaces the value it is worked from. A clip's destruction stores the
  velocities left on it. No path reaches that case today; the check
  keeps a later path from reading a freed clip.
- Every other track (an integer, another composite, another weight, a
  discrete value, a color, a rotation) computes its velocity as before.

## Consequences

A steady frame of 1,000 players of a looping four-track clip costs 5.9
million instructions instead of 8.0. Velocities read are the same bits.
`test_clip_velocity` checks them against the values layer in every case
above: after a rate change and a seek, under an additive player, at half
weight, for an integer, played backwards, and with the earlier time
exactly at a track's end. Each mutant of the conditions fails it or
another test.
