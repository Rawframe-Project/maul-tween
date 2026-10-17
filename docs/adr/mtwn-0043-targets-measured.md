# mtwn-0043. Performance and size targets, measured

Status: Accepted

## Context

Record mtwn-0019 set targets before the library's design was complete.
The benchmark (`bench/`) measured the complete library on the same
machine (AMD Ryzen 9 5900X, GCC 14 at `-O2`, one core) after the hot
path of record mtwn-0042. Measured: 64,000 eased number transitions
created in 3.82 ms and updated in 0.722 ms, 0.724 ms with the
changed-target report, 380 bytes each with their target; a cubic
bezier sample 26.8 ns, a spring sample 39.7 ns, Penner's arithmetic
easings 7.6 ns and those of exponentials and sines 41 ns; code (x86-64
text, unused sections dropped) 18.7 kB for a host of easings and
springs, 27.8 kB for every value kind's interpolation, 106.0 kB for
the whole library (15.2, 23.1 and 80.4 kB at `-Os`).

## Decision

On that machine with GCC 14 at `-O2`:

- 64,000 eased number transitions are created in at most 4 ms and
  update in at most 0.75 ms (0.8 ms with the changed-target report),
  at most 384 bytes each with their target.
- A cubic-bezier sample takes at most 30 ns, a Penner easing of
  arithmetic alone at most 10 ns, a spring sample or an easing of
  exponentials and sines at most 45 ns.
- x86-64 code: easing and springs alone at most 20 kB, every value
  kind's interpolation at most 30 kB, the whole library at most 112 kB.

Why the changes: an update reads four cache lines a transition (its
target's base, value and velocity, and the transition's times, ends
and curve), 16 MB for 64,000, which at this machine's cache bandwidth
is near 0.6 ms alone; a 96-byte transition with its target would hold
one kind of value and one curve. Springs and the exponential easings
use the library's own exponential and sine, the same bits under every
C library, several times slower than a C library's. The sizes hold the
library's own elementary functions, about 12 kB, in every host with
curves, and the complete scope: timelines with exact crossings,
players, paths, composition.

A target missed is fixed, or changed by a record that says why.

## Consequences

The benchmark prints every timed target; `bench/baseline.txt` keeps
the numbers of a quiet run, and sizes are checked when a release is
made.

## Amended 2026-10-16: instruction bounds in the gate

A shortcut lost changes no result, so no test sees it. The gate runs
`tools/bench_counts.py` on its GCC 14 Release build: each line of
`bench/counts.txt` bounds a bench layout's frame or a values case's
sample in instructions (`perf stat -e instructions:u`, one frame the
difference of two runs), about 3% above the counts at 0.3.1, with a
clip period where a layout's players must repeat often. With the open
start's shortcut off, the clips at a period of 0.032 s count 12.2
million against a bound of 11.43; measuring a path's length from the
far end, 2,324 against 1,990. A bound is restated by a record when a
change costs more on purpose.

## Amended 2026-10-17: the compiler beside the bounds

The bounds hold for one compiler's code: `bench/counts.txt` names it
on its `compiler` line (GCC 14.2.0, Ubuntu's 14.2.0-4ubuntu2~24.04.1),
the bench prints its own with `--compiler`, and the check names both
when they differ or a bound fails, so a compiler's point release that
moves the counts is seen as the compiler's.

## Amended 2026-10-17: misses beside instructions

Instructions do not show where data lies: with the task hooks on one
worker the bench's players on clips of their own ran 0.8% more
instructions than without them and took about 8% longer, their groups
reading node times apart from each clip's nodes. `bench/counts.txt`
therefore also bounds misses, as a ratio: a `misses` line names a
layout, the layout it is set over, a perf event and the bound, and the
check takes the median of three tries of the one's misses a frame over
the other's, counted side by side. A ratio moves much less with what
else the machine runs than a count: the second-level cache's data
misses with the hooks over without came to 0.89 to 1.06 at a load of
117 to 214 with each clip's own times, 1.18 to 1.33 before, and the
bound is 1.12. The event is Zen's; a machine without it reports the
line and skips it.

Measured again the same day at load 231, the ratio of the build with
each clip's own times reached 1.12 and that of 0.4.0 fell to 1.14 over
five tries of 1,000 frames each: no bound parts them there. The check
therefore takes the median of five tries over 1,000 frames and counts
misses only when the load average is at most one task for each of the
machine's threads, reporting the line as skipped otherwise.
