# mtwn-0046. Randomized invariants, and what they found

Status: Accepted

## Context

Record mtwn-0020 asks for a randomized invariant test of the builder and
the timing model under the sanitizers, beside the references. Each
reference checks what a part computes on cases chosen for it; the
invariants check what every part must keep on inputs nobody chose,
extreme ones among them.

## Decision

- `test/test_invariants.c`, from fixed seeds, under every CI job:
  - **Timing:** 20,000 random timings, a quarter of them with extreme
    values (0, negative, 1e300, infinite, NaN), each accepted one at
    every phase boundary, either way: ranges of every result field, the
    end time from the active duration, the overall progress never going
    back as time goes on.
  - **Builder:** 400 clips of random calls, valid or not: a refused call
    changes nothing, an accepted one adds one node, a finished clip's
    nodes start at finite times and end after them.
  - **Playing:** random clips under random controls, extreme rates
    included: values finite; records ordered, counted, and within the
    host span of the update that made them.
  - **Transitions:** random transitions of every value kind and spec
    replacing each other: values finite, each at rest on its base.
- Three defects it found are fixed, each with a fixed case:
  - With a repeat delay and a negative rate, the active time at the
    active interval's end can round an ulp past the active duration;
    the scaled time then fell below 0, into a repeat delay of iteration
    -1. The scaled time is kept from 0 (the timing oracle too).
  - The repeat delays were counted as ceil(s + n) - floor(s) - 1, which
    for a large iteration start s, its fraction lost in s + n, gave -1
    and a negative active duration. They are counted from the fraction,
    ceil(s - floor(s) + n) - 1: the same for every other start.
  - A rate change keeps the current time as Web Animations does, by the
    start time, so to within the host time's resolution times the rate;
    at a rate of 1e300 the time is lost, and records were stamped before
    their update. A record's host time is kept within its update, and a
    player's rate larger than `MTWN_RATE_LIMIT`, 2^20, is refused as
    misuse with the player unchanged (in its def and by
    `mtwnPlayer_SetRate`): at the limit and 10^7 s of host time the time
    is kept to 2 ms, and no host loses time silently.

## Consequences

The start-time model stays Web Animations' (its tests pin its steps);
a rate past the limit is refused rather than kept to a resolution the
host cannot see.
