# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API and the ABI; a context def built against other headers is
refused with `mtwn_errorVersion`.

## [0.0.1](https://github.com/Rawframe-Project/maul-tween/compare/v0.0.1...v0.0.1) (2026-10-24)


### ⚠ BREAKING CHANGES

* **context:** against the withdrawn 1.0 releases, `mtwnGetClipBytes` now takes the context as its first argument; the transition, clip, clip track, clip timeline and player defs lose their `version` field, the transition def's value methods move after `target`, the player def's `bindingCount` before `bindings`, the clip track def's `keyframeCount` after `slot`, and the player, clip track and timeline defs' reserved bytes grow. Only the context def carries the ABI version, which holds the whole release before 1.0.

### Fixed

* **context:** stamp only the context def with the family's ABI version ([#4](https://github.com/Rawframe-Project/maul-tween/issues/4)) ([4b30709](https://github.com/Rawframe-Project/maul-tween/commit/4b307098c069a9211daa7c5bb2e7688c32c8ece8))

## [0.0.1](https://github.com/Rawframe-Project/maul-tween/releases/tag/v0.0.1)

The first release: easing, springs, tweens and timelines driven by the
host's clock, with the same results on every platform.

### Added

- Easings: Penner's power, sine, expo, circ, back, elastic and bounce
  curves landing exactly on 0 and 1, CSS Easing Level 2's keywords,
  `cubic-bezier()`, `steps()` with every jump position and `linear()`
  with points, rough easings from a seed, springs as easings and host
  functions, each in the in, out, in-out and out-in modes, with even
  tables and `linear()` points fitted to a tolerance.
- Springs in closed form, made from stiffness and damping, a frequency
  and damping ratio, or a perceptual duration and bounce, with settling
  times; decay from a rate or iOS's deceleration, landing on a chosen
  end or held between bounds by a spring; iOS's rubber band; punch and
  seeded shake, with the shake's noise alone for trauma-driven shakes.
- Interpolation of numbers and vectors, colors in Oklab, OkLCh, linear
  sRGB or sRGB as CSS Color 4 interpolates them, angles with CSS's hue
  methods, rotations by slerp, integers, discrete values, scales in
  their logarithm, and van Wijk and Nuij's smooth zoom and pan.
- Keyframe paths and tracks of every value kind: linear, step, Bezier,
  centripetal Catmull-Rom and monotone segments, at constant speed if
  asked, with the path's direction and angle.
- Web Animations' timing with a playback rate per node, a repeat delay
  and a choice of how reverse iterations ease; staggers over a row or a
  grid from any origin.
- A context of targets in one block from the host's allocator, with
  fixed capacities, grown only when the host asks, and transitions by an
  easing, a spring or a decay that start from a target's current value
  and velocity, shortened on reversal as CSS Transitions does, with
  durations set from a measured distance.
- Clips of tracks on nested timelines with positions, labels and marks;
  players with Web Animations' play, pause, reverse, finish, cancel,
  seek and rate; layers composed by replace and add with weights;
  incremental tracks; events as counted records in a bounded queue.
- Reduced motion, under which what can finish finishes at once and
  what is marked essential plays on.
- Clips' players run on the host's task system with the same results
  as on one thread, and players of one clip in step share their work.
- Results that are the same bits on x86-64, 32-bit x86 with SSE2,
  AArch64 and WebAssembly, whatever the host's floating-point
  environment.
- A static or shared library, a CMake package and a pkg-config module;
  Linux, macOS, Windows with `clang-cl`, and WebAssembly.

### Changed

- Against the withdrawn 1.0 releases: sine and cosine are within 1 ulp
  for every finite argument, so results change where an argument
  reaches 1.5 * 2^20 radians, which were bounded but inexact before:
  an underdamped spring sampled that far into its oscillation, an
  elastic easing of a very short period, and an OkLCh color mix, a
  sine easing or a slerp extrapolated that far.
- Breaking against the withdrawn 1.0 releases: `mtwnGetClipBytes` now
  takes the context as its first argument; the transition, clip, clip
  track, clip timeline and player defs lose their `version` field, the
  transition def's value methods move after `target`, the player def's
  `bindingCount` before `bindings`, the clip track def's `keyframeCount`
  after `slot`, and the player, clip track and timeline defs' reserved
  bytes grow. Only the context def carries the ABI version, which holds
  the whole release before 1.0.
