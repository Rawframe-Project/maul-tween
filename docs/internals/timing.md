# Timing

Every track and timeline of a clip, and `mtwnComputeTiming` for a host, computes Web Animations Level 1's timing steps exactly, in the order its sections give them ([`src/timing.c`](../../src/timing.c)). The model is the web's because hosts and designers already think in it (delays, fill, direction, fractional and infinite iterations), and because an exact specification can be checked by an independent implementation.

## What is added to Level 1

- **Playback rate per node,** Level 2's effect playback rate: it divides the active duration and scales the overall progress, from the far end when negative. A negative rate needs a finite active duration, whose end a backward play starts from, and is refused otherwise. A negative rate begins the iterations at their end, so the end holds at the active start (and before it, filling backwards) as it holds at the active end for a positive rate.
- **Repeat delay,** between iterations and not after the last, as GSAP's `repeatDelay` and DOTween's loops give it. A delay `r` above 0 with a finite duration `d` above 0 puts `d + r` of active time into each iteration but the last; inside a delay the progress holds at its iteration's end. The delays a run crosses are counted from the iteration start's fraction, `ceil(s - floor(s) + n) - 1`: written as `ceil(s + n) - floor(s) - 1` it loses the fraction of a large start and goes negative. Without a repeat delay the arithmetic is Level 1's exactly.
- **How reverse iterations ease:** time running back, `ease(1 - p)`, Web Animations' way, in which an ease-in looks like an ease-out on the way back; or the shape repeated, `1 - ease(p)`, as GSAP's yoyo eases.
- **Infinite local time,** the limit of the finite ones: a finite active interval lies behind it, filled as after its end; an endless one is still active there and is never left; an infinite duration's progress is its iteration start there, as at every finite active time. A child of a reversed endless timeline is there, and results at finite times are the same bits.

## Guards against rounding

- The overall progress is kept within the iteration start and its end, so rounding never yields an iteration past the last.
- The scaled time is kept from 0: with a repeat delay and a negative rate, the active time at the active interval's end can round an ulp past the active duration and would otherwise fall into the repeat delay of iteration -1.
- The before flag takes "going forwards" as the iteration running forwards exactly when the rate is positive, so `steps()` gets the flag the specification means.

## Players

A player has a start time, a hold time and a signed rate, the play states idle, running, paused and finished, and Level 1's procedures ([`src/player.c`](../../src/player.c), the controls in [`src/player_control.c`](../../src/player_control.c)): play with auto-rewind, pause, reverse, finish, cancel, seek, and a rate change that keeps the current time. The ready time Level 1 waits for is the next update's: a play that needs a start time, a rate change on a running player and a reverse take effect there, the old rate running until then. Pause, seek, finish and cancel take effect at once, at the last update's time.

A rate change keeps the current time through the start time, so to within an ulp of the host time times the rate. At a rate of 1e300 that is no time at all, and records would be stamped before their update, so a rate larger than `MTWN_RATE_LIMIT` (2^20) is refused: at the limit and 10^7 seconds of host time the time is kept to 2 ms, and no host loses time silently.

The start time, `now - t / rate`, is finite by two more bounds. A rate other than 0 is at least `MTWN_MIN_RATE` (2^-20) in size, and every time a player seeks to, and a clip's finite end, is at most `MTWN_SEEK_LIMIT` (2^1000) in size; a seek past it is refused, and so is a node that would take its clip's end past it. Every time a player holds or reaches is then a seek's, 0, or its clip's end, run on at most 2^20 times the host's time, which `mtwnContext_Update` takes as `uint64_t` nanoseconds, below 2^35 s: below 2^1000 + 2^55. So `t / rate` is below 2^1021 in size, 16 times short of the largest double.

A finished player held at its end that is given a lower rate keeps its start time too, as the specification's steps say, so the next update finds it short of its end and it plays again. Firefox agrees; Chrome keeps a player that played to its end finished. The library follows the specification.

## Checked by an independent implementation

`tools/oracle/timing_oracle.py` computes the steps again in Python, from the specifications' text, on dyadic inputs where the arithmetic is exact, and writes the expected results into `test/generated/timing_cases.h`; the tests match them to the bit. `tools/oracle/exact_oracle.py` computes the discrete answers (phases, iterations, crossing counts) in exact rationals on inputs that are not dyadic, so the library rounds where the reference does not.

With a repeat delay, the position on the cycles, `scaled + floor(start) * cycle + fraction * duration`, can pass the doubles where the cycles it counts do not: 2.5 iterations of 1e308 s. There `mtwnCycleOf` (`src/timing.c`) takes the same steps with every term scaled by a power of 2, which commutes with rounding in the normal range, so the iteration and the time into it are what the steps give with an exponent without bounds. Two cases keep the count infinite, as a position past the doubles always gave: a quotient past the doubles, and a start past 2^53 cycles whose count rounds by more than one. `tools/oracle/cycle_oracle.py` emulates those steps in exact rationals, rounding each to 53 bits, and `test/generated/cycle_cases.h` holds its results.

## Traps

- A timeline's easing must be monotonic: it shapes the time its children see, and the crossing count assumes that time moves one way through each iteration.
- The rate limit, the depth limit and the other `MTWN_MAX_` limits are part of the API: what is accepted stays accepted.
- `mtwnComputeTiming` gives NaN in its result for what a phase leaves undefined (Web Animations' unresolved); a caller tests with `isnan`, never with equality.
