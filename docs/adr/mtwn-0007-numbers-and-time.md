# mtwn-0007. Float values, double computation, integer host time

Status: Accepted

## Context

Lockstep games, replays and frame tests need the same bits everywhere. A
`float` time in seconds has 7.8 ms resolution after a day and 62.5 ms
after a week; summing frame deltas drifts and depends on the frame
sequence.

## Decision

- Values are binary32 at the API and in targets, computed in binary64 up
  to one rounding into the target.
- The update takes `uint64_t` nanoseconds on a monotonic clock;
  durations and local times are binary64 seconds. A player's local time
  is the integer difference between the host time and its start,
  converted once and multiplied by its rate; a change of rate, a pause
  or a seek rebases the start, so nothing is accumulated.
- Results are bit-exact on every platform (record mtwn-0001).

## Consequences

Local time is exact for 104 days whatever the host's uptime, a value
carries one rounding, and the same calls at the same times give the same
bits.
