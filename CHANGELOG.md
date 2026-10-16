# Changelog

All notable changes to this project are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/). Before
1.0.0, any minor release may change the API and the ABI.

## [Unreleased]

### Changed

- A clip player's update no longer counts a repeating timeline's or
  track's iterations when its time stayed inside one iteration: about
  a fifth fewer instructions in a steady frame of a looping clip. The
  events are unchanged.
- A record's crossing time is searched from a guess: the last crossing
  the sweep found, or the record's own first. The frame where a looping
  clip repeats costs a sixth of what it did (record mtwn-0053). Where
  the count's rounding flips it more than once within a few doubles, a
  record's time can be another of those doubles, the same on every
  platform.
- A steady frame of a clip's player is cheaper again, with the same
  bits: the timing steps take a fraction without `fmod` and write
  their result in place, a node's event leaves are made only when
  counted, and a path evaluates only its own channels and no tangent
  unless asked. 11.0 to 9.4 million instructions a steady frame of
  1,000 four-track players.
- Tracks in a row with the same parent, start and timing share one
  timing result per sample: 9.4 to 8.0 million instructions a steady
  frame of the same players. The values are the same bits.
- A clip track of numbers replacing its target at full weight leaves its
  velocity to be worked out when read, as an eased transition's is
  (record mtwn-0054): 8.0 to 5.9 million instructions a steady frame of
  the same players. Velocities read are the same bits.
- A sweep's records cost less at a repeat: a track sharing the one
  before it's timing takes that track's records for its own node, and a
  record's first search starts where its parent, moving steadily,
  reached the leaf. The frame where a looping clip of 1,000 players
  repeats costs 40 million instructions instead of 170. The times are
  where the count flips, as before (record mtwn-0053).
- The crossing count takes a finished node's active duration as its clip
  keeps it, where it worked it out again at every step of a count: the
  repeat frame 37.6 to 33.4 million instructions, the same bits.
- A record's counts remember their walks: the searches' fixed ends are
  walked once instead of at every test. The repeat frame 33.4 to 27.3
  million instructions, the same bits.
- A prepared level keeps its iterations' shape for the counts, and its
  preparation writes every count below, so a chain needs no clearing:
  the repeat frame 27.3 to 25.6 million instructions, the same bits.
- `mtwnPlayer_SetRate`'s documentation says what a lower rate does to a
  finished player: it plays again from the time the new rate gives, as
  Web Animations' steps and Firefox have it (record mtwn-0052). The
  behavior is unchanged.
- The clips part's size budget is 118 kB and the whole library's 128 kB
  until 0.3.0, paying for the clip players' speed since 0.2.1 (about
  5.7 kB) and their coming task hooks (record mtwn-0055). The README's
  size table shows today's sizes.

## [0.2.1] - 2026-10-15

Three defects the randomized invariants found once they covered 0.2.0's
additions.

### Fixed

- `mtwnPlayer_Finish` holds the player exactly at its end. The start
  rebased by end / rate could round its time just short of the end,
  leaving it running until the next update.
- Under reduced motion, a player is judged by the state the update
  leaves: a finished player whose rate was lowered, released from its
  hold by that update, now finishes in it too.
- `mtwnComputeStagger` refuses a point beyond 2^20 and a spread that is
  not finite, writing nothing. Before, they gave NaN offsets from finite
  inputs.

## [0.2.0] - 2026-10-15

The two parts of the library's scope that 0.1.0 left out: its notes
said it held the whole library, and it did not.

### Added

- Staggers: `mtwnComputeStagger` spreads offsets over a row or a grid
  from the first or last item, the centre, the edges, an index, a
  point, or a seeded order, eased (record mtwn-0048). The scope of
  0.1.0 included them and the release lacked them.
- Reduced motion: `mtwnContext_SetReducedMotion` finishes every
  transition, and every running clip's player with a finite end, at
  its next update, its records at that update's time; `essential` in
  the transition and player defs keeps one playing (record mtwn-0049).
  Also in 0.1.0's scope and missing from it.

## [0.1.0] - 2026-10-15

The first release: the whole library, from the curves to clips,
players and events.

### Added

- Easing: Penner's families in four modes (in, out, in-out, out-in),
  integer and real powers, back and elastic with their parameters;
  CSS Easing Level 2 entire (the keywords, `cubic-bezier()`, `steps()`,
  `linear()` with points); host easings; sampling into even tables and
  CSS `linear()` points fitted to any easing.
- Physics: damped springs in closed form by stiffness and damping, by
  frequency and damping ratio or by duration and bounce, their
  settling times, and springs as easings; decay by rate or by iOS's
  deceleration, its stop and crossing times; the rubber band and its
  inverse; punch and seeded shake.
- Values: colors interpolated as CSS Color 4 does (Oklab, OkLCh,
  linear sRGB, sRGB, premultiplied, the four hue methods), angles by
  method, rotations as quaternions by the shortest or longest path,
  integers, discrete values.
- Timing: Web Animations' timing computation with Level 2's playback
  rate, a repeat delay and the alternate easing.
- A context in one block from the host's allocator, with fixed
  capacities and grown only when asked (`mtwnGrowContext`); targets of
  every value kind; transitions by easing, spring or decay with a
  delay, the start velocity given or taken from the target, and CSS
  Transitions' shortened reversal (`reverses`); an update from the
  host's nanoseconds that reports the targets it changed and guards
  the floating-point environment.
- Paths of linear, step, Catmull-Rom and Bezier segments, at constant
  speed if asked, and keyframe tracks of every value kind over them.
- Clips: nested timelines placed by positions and labels, marks,
  tracks bound to slots, automatic durations, incremental tracks;
  `mtwnGetClipBytes` sizes one.
- Players by Web Animations' procedures: play, pause, reverse,
  finish, cancel, seek by time, progress or label, sweep, rate (up to
  `MTWN_RATE_LIMIT`) and weight; players composed by replace and add,
  per binding, in creation order.
- Events: starts, iterations, ends, marks and cancels as counted
  records in a bounded queue, each crossing found exactly, with its
  host time.
- The same results bit for bit under GCC, Clang, clang-cl and
  WebAssembly: the library's own elementary functions, a determinism
  hash in the tests.
- A guide, samples, the API reference, a benchmark with its baseline,
  and `tools/code_size.py` for the code each part links.
