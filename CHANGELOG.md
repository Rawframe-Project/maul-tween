# Changelog

All notable changes to this project are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/). Before
1.0.0, any minor release may change the API and the ABI.

## [Unreleased]

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
