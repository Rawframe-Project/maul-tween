# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API and the ABI; a context def built against other headers is
refused with `mtwn_errorVersion`.

## [0.2.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.1.1...v0.2.0) (2026-10-24)


### ⚠ BREAKING CHANGES

* **spring:** for a start that overflows a settling bound, mtwnSpringSettlingTime and mtwnBoundedDecayStopTime return the true finite time instead of infinity, and a spring transition from such a start, or a bounded decay handing over to one, ends at that time with an end record instead of running until replaced or stopped. Every other result keeps its bits.

### Fixed

* **spring:** give the true settling time when a start overflows ([#27](https://github.com/Rawframe-Project/maul-tween/issues/27)) ([f30bef8](https://github.com/Rawframe-Project/maul-tween/commit/f30bef87c6da454537edd6734ea125e48bd6eb5d)), closes [#20](https://github.com/Rawframe-Project/maul-tween/issues/20)

## [0.1.1](https://github.com/Rawframe-Project/maul-tween/releases/tag/v0.1.1) (2026-10-24)

### Changed

- The context calls the host's allocator in the host's own
  floating-point environment, not the library's.
- Every public call that computes switches to the library's
  floating-point environment and back, about 45 instructions:
  `mtwnEaseCubicBezier` takes 315 instructions where it took 272,
  `mtwnSampleSpring` 474 where it took 399, and an empty `mtwnUpdate`
  188 where it took 166.
- Code is larger. A program links 0.8 to 4.7 kB more, by the parts it
  uses: curves 27,161 bytes (from 26,345), values 59,289 (57,945),
  transitions 62,617 (61,465), clips 151,004 (147,420), and the whole
  library 168,834 (164,098), as `tools/code_size.py` measures them.
  The wrappers add about 70 bytes each; building with
  `-fno-math-errno` takes back about 0.3 to 1.2 kB of that per part.

### Fixed

- Calls outside the update and the clips' and transitions' work ran in
  the host thread's floating-point environment. The easings, springs,
  decays, interpolation, paths, timing, samplers and fits did, and so
  did several context calls. A host thread with flush-to-zero,
  denormals-are-zero or a rounding mode other than nearest got other
  bits from them than other hosts do. Every public call that computes
  now runs in the library's environment, so those threads get the
  default environment's bits. Hosts in the default environment see no
  change.
- On 32-bit x86, glibc's `sqrt` and `ldexp` compute on the x87
  unit, which rounded by the host's x87 setting: under upward rounding,
  `mtwnMakeZoomMix` gave other bits. The library's environment there
  now also sets the x87 control word (round to nearest, every exception
  masked, 64-bit precision), and the library is built with
  `-fno-math-errno`, so `sqrt` is always the SSE instruction.

## [0.1.0](https://github.com/Rawframe-Project/maul-tween/releases/tag/v0.1.0) (2026-10-24)

### Added

- `mtwnEaseCubicBezier` samples a cubic-bezier easing with the same
  bits as `mtwnEase`. With link-time optimization or unused sections
  dropped, a program that samples cubic beziers only through it links
  none of the other curves.

### Changed

- An underdamped spring whose phase (damped rate times time) is at
  least 2^52 radians while its envelope is not yet 0 now takes its sine
  from the exact argument reduction. There, `mtwnSpringMotionAt`,
  `mtwnSampleSpring` and the spring easings, bounded decays and spring
  transitions built on them return different bits; results below 2^52
  are unchanged.
- Grouped players cost less: with groups, an update of 1,000 players in
  step takes 1.6% more instructions than without them, down from 14.4%.

### Fixed

- A critical or overdamped spring whose start overflows its settling
  bound (stiffness times offset past about 1.8e308) gave a NaN settling
  time. `mtwnSpringSettlingTime` and `mtwnBoundedDecayStopTime` now
  return infinity, as the capped path already did. A spring transition
  from such a start, which ended at once (a path jumped to its target,
  other kinds ended with the other channels), now never ends by
  settling. Where only the offset bound overflowed, the settling time
  was the velocity bound's early finite time; it is now infinity too.

## [0.0.1](https://github.com/Rawframe-Project/maul-tween/releases/tag/v0.0.1) (2026-10-24)

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
