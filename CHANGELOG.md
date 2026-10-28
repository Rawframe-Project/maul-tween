# Changelog

All notable changes to this project are recorded here, grouped as
Added, Changed, Fixed, Removed and Security. The project uses
[Semantic Versioning](https://semver.org/). Before 1.0.0, any release
may change the API and the ABI; a context def built against other headers is
refused with `mtwn_errorVersion`.

## [0.10.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.9.0...v0.10.0) (2026-10-28)


### ⚠ BREAKING CHANGES

* the results take the Maul family's values. mtwn_errorStale is -3 (was -5), mtwn_errorState -6 (was -7), mtwn_errorVersion -7 (was -4), mtwn_errorLimit -9 (was -3) and mtwn_errorBusy -10 (was -6). A full pool or a room the def set now refuses with mtwn_errorCapacity, not mtwn_errorLimit: targets, players, clips, bindings, clip storage, a clip's nodes, keyframes and linear() points, and a transition's points. An argument past MTWN_MAX_COUNT, MTWN_MAX_CLIP_STORAGE, MTWN_RATE_LIMIT or MTWN_SEEK_LIMIT, an infinite seek time or share included, now refuses with mtwn_errorLimit, not mtwn_errorInvalid. A call back in from the host's code now refuses with mtwn_errorBusy, not mtwn_errorState. A transition's player given to a clip's control or mtwnPlayer_GetState, and mtwnPlayer_SeekProgress on an endless clip, now refuse with mtwn_errorInvalid, not mtwn_errorState. A timeline past MTWN_MAX_CLIP_DEPTH now counts one misuse. Closes #123 Refs Rawframe-Project/.github#78
* the public structs have no reserved fields. Removed: the reserved member of mtwnContextDef, mtwnClipDef, mtwnClipTrackDef, mtwnClipTimelineDef, mtwnPlayerDef, mtwnPosition, mtwnBinding, mtwnTiming, mtwnStagger, mtwnEasing, mtwnSpring, mtwnKeyframe, mtwnPath, mtwnTrackDef, mtwnTrack, mtwnColorMix, mtwnClipNodeInfo, mtwnPlayerState, mtwnTimingResult, mtwnTargetState and mtwnEvent. Their layouts change. No call refuses a struct for a reserved byte any longer: a host that set one compiles no more, and one that used positional initializers drops the reserved member. Closes #121 Refs Rawframe-Project/.github#73
* **player:** a clip's player takes a rate other than 0 only from 1 / MTWN_RATE_LIMIT (2^-20) to MTWN_RATE_LIMIT in size, and every time it seeks to, and every clip's finite end, at most MTWN_SEEK_LIMIT (2^1000) in size. Newly refused with mtwn_errorInvalid, counted as misuse: by mtwnCreatePlayer, a def whose rate is other than 0 and below 2^-20 in size; by mtwnPlayer_SetRate, such a rate; by mtwnPlayer_Seek and mtwnPlayer_Sweep, a finite time past MTWN_SEEK_LIMIT in size; by mtwnPlayer_SeekProgress, a finite share whose time, the share times the duration, is past it; by mtwnPlayer_SeekLabel, a label of the root whose start is past it; by mtwnClip_AddTrack and mtwnClip_AddTimeline, a node that would take its clip's finite end past it.

### Added

* **player:** publish the rate's floor as MTWN_MIN_RATE, from v0.0.33 ([#126](https://github.com/Rawframe-Project/maul-tween/issues/126)) ([ff47df8](https://github.com/Rawframe-Project/maul-tween/commit/ff47df8f0b95ae1663b3885b09b23a207b3e09c5))


### Fixed

* **player:** bound rates and seeks so every start time is finite ([#119](https://github.com/Rawframe-Project/maul-tween/issues/119)) ([dc77b5a](https://github.com/Rawframe-Project/maul-tween/commit/dc77b5a41e479ebd32c58bfbf987d3cc6917f900)), closes [#115](https://github.com/Rawframe-Project/maul-tween/issues/115)
* **timing:** keep the iteration where a start's cycles pass the doubles ([#117](https://github.com/Rawframe-Project/maul-tween/issues/117)) ([9951a78](https://github.com/Rawframe-Project/maul-tween/commit/9951a781c0fd5e0b784d92a3f0ced3f60a3e2df3)), closes [#116](https://github.com/Rawframe-Project/maul-tween/issues/116) [#102](https://github.com/Rawframe-Project/maul-tween/issues/102)


### Changed

* remove the reserved fields from the public structs ([#122](https://github.com/Rawframe-Project/maul-tween/issues/122)) ([5c0c38e](https://github.com/Rawframe-Project/maul-tween/commit/5c0c38e64a22093f3a461291c093de98ea216c48))
* take the family's result numbering and meanings ([#124](https://github.com/Rawframe-Project/maul-tween/issues/124)) ([52e6789](https://github.com/Rawframe-Project/maul-tween/commit/52e6789da55a7983c3f33ec400dd0f6fd1ca5a71))

## [0.9.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.8.0...v0.9.0) (2026-10-27)

### Changed

- **Breaking:** `mtwnComputeTiming(timing, localTime, backwards, resultOut)` returns `mtwnResult` (`MTWN_NODISCARD`) where it returned `void`. It refuses with `mtwn_errorInvalid`, leaving the result as it was, a null timing or result, a timing `mtwnValidateTiming` refuses (a playback rate of 0, nonzero padding, an easing of an unknown kind or with fields no maker gives), and a local time that is NaN or negative infinity. A caller takes the returned status; a refused call leaves the result as it was. Every call it still takes gives the same bits ([#87](https://github.com/Rawframe-Project/maul-tween/pull/87)).
- On WebAssembly the float environment's entries compile away, so each wrapper compiles to its worker alone: the web build's library is about 8 KB smaller, with the same results. Other targets are unchanged ([#108](https://github.com/Rawframe-Project/maul-tween/pull/108)).

### Fixed

- `mtwnComputeTiming`, and the clips' timelines and tracks that compute the same way, keep the iterations at their end where rounding lost them. An active duration that rounds to 0, or a repeat delay's cycle that swallows the active time, no longer leaves the overall progress at the start with the iteration before it. A start so large that adding the iterations rounds back to it no longer holds the iteration before the start's. A repeat delay no longer gives NaN where an effect is due. A clip's values, directions and event iterations follow ([#100](https://github.com/Rawframe-Project/maul-tween/pull/100)).
- `mtwnSpringSettlingTime` no longer returns a time before the motion settles when a product of the start with a coefficient underflows, for a spring with a subnormal `w0^2`, or when one bound's time is NaN. A start already settled on a spring of subnormal rate gets 0 where it got infinity. A spring transition that ends by it ends at the new time. Elsewhere a time moves only within rounding, where a tolerance lies at a bound's peak, and never early ([#90](https://github.com/Rawframe-Project/maul-tween/pull/90)).
- An undamped spring with an infinite offset tolerance no longer stops a build with assertions in `mtwnSpringSettlingTime`; every settling time keeps its bits ([#95](https://github.com/Rawframe-Project/maul-tween/pull/95)).
- `mtwnDecayCrossingTime`, `mtwnDecayStopTime` and `mtwnSampleDecay` stay within the doubles on the way to results that lie within them, where they used to give infinity, 0 or a value with few digits. Every input that worked before keeps its bits ([#92](https://github.com/Rawframe-Project/maul-tween/pull/92)).

## [0.8.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.7.0...v0.8.0) (2026-10-26)

### Changed

- **Breaking: punches, shakes and the rubber band check their
  arguments.** `mtwnSamplePunch`, `mtwnSampleShake` and
  `mtwnShakeNoise` return `mtwnResult` and give their factor through
  the new `factorOut`. Each refuses with `mtwn_errorInvalid` a null
  pointer, a punch or shake whose fields its maker would refuse, and a
  NaN time; an infinite time stays taken. A shake of 2^53 lattice
  points or more over its duration is refused, where a forged one of
  2^64 or more cast an index past `uint64_t`'s range, which was
  undefined behavior in every build. `mtwnRubberBand` and
  `mtwnRubberBandInverse` return `mtwnResult` and give their result
  through the new `offsetOut` and `dragOut`. They refuse with
  `mtwn_errorInvalid` a null out, a dimension or coefficient not above
  0, and a NaN drag or offset, exactly the inputs that gave 0 before.
  Every output is unchanged on refusal, and every input still taken
  gives the same bits. A call costs up to about 11 ns more
  ([#76](https://github.com/Rawframe-Project/maul-tween/pull/76)).
- **Breaking: color, rotation, scale and zoom mixes are checked where
  they are sampled.** `mtwnMixColor`, `mtwnMixRotation`,
  `mtwnMixScale` and `mtwnMixZoom` return `mtwnResult` and give their
  value through the new `colorOut`, `rotationOut`, `scaleOut` and
  `viewOut`; `mtwnZoomLength` returns `mtwnResult` and gives the length
  through `lengthOut`. Each refuses with `mtwn_errorInvalid` a null
  pointer and a mix whose fields are outside their ranges, and the
  samplers also refuse a progress that is not finite.
  `mtwnSampleTrack` refuses a segment whose color or rotation mix,
  where it reads one, fails that check. A mix that
  passes gives no undefined behavior and no NaN, but is not promised to
  equal what a maker would give: a color or rotation is finite, held
  within the floats, and a scale or zoom reaches infinity or 0 only
  past the doubles' range. Every output is unchanged on refusal. A
  rotation, here and wherever transitions, tracks and layers slerp, is
  held within the finite floats; a maker's rotation changes no bits
  except where a float would have been infinite. Results for a maker's
  mix are otherwise unchanged. A checked call costs about 4 to 17 ns
  more. Transitions, layers and clips pay nothing per sample beyond the
  rotation's hold; `mtwnSampleTrack` pays the check of the segment's
  mix it reads. This completes the mixes, punches and shakes part of
  [#60](https://github.com/Rawframe-Project/maul-tween/issues/60); its
  timing part is still to come
  ([#82](https://github.com/Rawframe-Project/maul-tween/pull/82)).

### Fixed

- `mtwnMixZoom` no longer gives a NaN centre for a maker's mix far
  outside progress 0 to 1. Past about 710 along the path the centre
  settles on its limit, as the header says the path extends, and the
  width is unchanged everywhere. Results change only where a field was
  NaN or infinite before; no finite field changes a bit. A sample
  costs up to about 3 ns more
  ([#81](https://github.com/Rawframe-Project/maul-tween/pull/81)).

## [0.7.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.6.0...v0.7.0) (2026-10-25)

### Changed

- **Breaking: paths and tracks are checked where they are sampled.**
  `mtwnSamplePath`, `mtwnPathDirection`, `mtwnPathAngle` and
  `mtwnSampleTrack` return `mtwnResult`, and every output is unchanged
  on refusal: `mtwnPathAngle` gives its angle through the new
  `angleOut`, and `mtwnSampleTrack` its value through the new
  `valueOut`. Each refuses with `mtwn_errorInvalid` a null struct or
  output, a progress that is not finite, a path or track whose fields
  no maker gives, and the segment it reads unless its kind is known (at
  constant speed, not a step) and, when its keyframes time it, its
  easing is a maker's. Keyframes or lengths the host changes after
  making give an unspecified value, but nothing is read outside their
  arrays; what a keyframe's easing references, such as `linear()`'s
  points and their count, stays the host's. At constant speed a
  keyframe changed to NaN no longer makes one sample cost about 600,000
  evaluations. A sample costs 1 to 13 ns more. Results for every input
  still taken are unchanged. This is the path and track part of
  [#60](https://github.com/Rawframe-Project/maul-tween/issues/60)
  ([#70](https://github.com/Rawframe-Project/maul-tween/pull/70)).
- **Breaking: a spring easing whose squared frequency is infinite is
  refused.** `mtwnMakeSpringEasing` refuses with `mtwn_errorInvalid` a
  spring whose natural frequency, in radians a second, times the
  duration reaches 2^512 (about 1.34e154); it used to make such an
  easing from a critical spring, which eased finite, and already
  refused an overdamped one. `mtwnEase` and every call taking an easing
  refuse a spring easing whose squared frequency is infinite, which a
  host-written one could have and which eased NaN. Every easing still
  made gives the same bits
  ([#73](https://github.com/Rawframe-Project/maul-tween/pull/73)).

## [0.6.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.5.0...v0.6.0) (2026-10-25)

### Changed

- **Breaking: `linear()` points are copied where an easing enters a
  context.** A transition, track or timeline eased by `linear()` copies
  the points into the context, so the host's array may change or go once
  the call returns. `mtwnContextDef` gains `pointsPerTransition` and
  `mtwnClipDef` gains `pointCapacity`, both 64 by default, which changes
  both defs' layout; a host with no `linear()` transitions may set
  `pointsPerTransition` to 0 and pay nothing. A transition with more
  points than `pointsPerTransition`, and a track or timeline whose
  points pass the clip's `pointCapacity`, are refused with
  `mtwn_errorLimit`. Points the host changed from their maker's
  canonical form (an input or output not finite, or an input below the
  one before) are refused with `mtwn_errorInvalid`. A refusal leaves the
  running transition as it was. `mtwnGrowContext` moves each running
  transition's points and refuses a smaller `pointsPerTransition`. The
  default context grows by 256 KiB, the default clip by 1 KiB, and the
  default clip storage from 208 KiB to 224 KiB. A start pays about 1 ns
  a point for the check and the copy; an update costs the same as
  before. With 0.5.0 this completes the easing part of
  [#60](https://github.com/Rawframe-Project/maul-tween/issues/60)
  ([#68](https://github.com/Rawframe-Project/maul-tween/pull/68)).
- **Breaking: every easing entering the library is checked.**
  `mtwnValidateTiming` (and so `mtwnComputeTiming`), `mtwnMakePath`,
  `mtwnMakeTrack`, the stagger functions and the clips refuse an easing
  no maker produced, as the easing calls already do. Results for every
  input still taken are unchanged
  ([#68](https://github.com/Rawframe-Project/maul-tween/pull/68)).
- On 32-bit x86 the library is built with SSE2 math
  (`-msse2 -mfpmath=sse`), so it needs a processor with SSE2, and every
  build now leaves trapping math at the compiler's default, from the
  family's compiler flags. The GCC builds, 64-bit and 32-bit x86, pass
  every suite unchanged, the determinism hash and the oracle tables
  included
  ([#67](https://github.com/Rawframe-Project/maul-tween/pull/67)).

## [0.5.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.4.0...v0.5.0) (2026-10-25)

### Changed

- **Breaking: easings are checked wherever a call takes one.**
  `mtwnEase`, `mtwnEaseCubicBezier` and `mtwnIsEasingMonotonic` return
  an `mtwnResult` and give their results through pointers, unchanged on
  refusal. They, `mtwnSetEaseMode`, `mtwnSampleEasing`,
  `mtwnFitLinearEasing`, `mtwnMakeRoughEasing` (its shape) and a
  transition's start and measure refuse an easing whose fields are not
  as its maker sets them. `mtwnEase` and `mtwnEaseCubicBezier` refuse a
  progress that is not finite, and `mtwnEaseCubicBezier` refuses an
  easing of another kind instead of returning NaN. Results for every
  input still taken are unchanged, but for the fixes below. A sample
  through `mtwnEase` costs about 2 to 12 ns more; a transition checks
  its easing once, at start.
  This is the second part of
  [#60](https://github.com/Rawframe-Project/maul-tween/issues/60)
  ([#64](https://github.com/Rawframe-Project/maul-tween/pull/64)).
- **Breaking: makers refuse easings that could not stay finite.**
  `mtwnMakeElasticEasing` refuses an amplitude past 512, and a period
  below 2^-10, after an amplitude below 1 has stretched it.
  `mtwnMakeCubicBezier` refuses a y past 2^1000 in magnitude.
  `mtwnMakeSpringEasing` refuses a spring that oscillates more than
  once in 2^-10 of the duration, and one whose eased progress could pass
  the doubles' range
  ([#64](https://github.com/Rawframe-Project/maul-tween/pull/64)).

### Fixed

- CSS curves are finite for every finite progress, in every mode. Where
  a cubic bezier's end tangent, a `linear()` extension or a step far
  past its end gave infinity or NaN, it now gives the largest finite
  double of its sign, or a flat tangent's end. `linear()` with inputs
  spanning more than the doubles interpolates instead of giving its
  first output. The free `linear()` sampler is finite for any finite
  points, so points the host changes after making the easing give an
  unspecified but finite value. Every result that was already finite
  keeps its bits
  ([#64](https://github.com/Rawframe-Project/maul-tween/pull/64)).

## [0.4.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.3.0...v0.4.0) (2026-10-25)

### Changed

- **Breaking: springs and decays are checked wherever a call takes
  one.** `mtwnSpringMotionAt`, `mtwnSampleSpring`,
  `mtwnSpringSettlingTime`, `mtwnSampleDecay`, `mtwnDecayStopTime`,
  `mtwnDecayCrossingTime`, `mtwnSampleBoundedDecay` and
  `mtwnBoundedDecayStopTime` return an `mtwnResult` and give their
  results through pointers, unchanged on refusal. They refuse a spring,
  decay or bounded decay no maker produced, a start that is not finite,
  a time below 0 or NaN, and a tolerance or stop speed not above 0,
  which used to give infinity. `mtwnMakeBoundedDecay` and
  `mtwnMakeSpringEasing` refuse a decay or spring no maker produced,
  and spring transitions and bounded decay transitions refuse a spring
  whose derived fields or regime differ from what its frequency and
  ratio make. Results for every input still taken are unchanged. A
  sample costs a few nanoseconds more (5 to 8 ns for a spring, 2 to
  4 ns for a decay, 15 to 21 ns for a bounded decay); a transition
  checks once, at start. This is the first part of
  [#60](https://github.com/Rawframe-Project/maul-tween/issues/60)
  ([#62](https://github.com/Rawframe-Project/maul-tween/pull/62)).

## [0.3.0](https://github.com/Rawframe-Project/maul-tween/compare/v0.2.1...v0.3.0) (2026-10-25)

### Changed

- **Breaking:** `mtwnValidateTiming` refuses timings whose active
  duration, end time or iteration cycle would overflow a double, and so
  do `mtwnClip_AddTrack` and `mtwnClip_AddTimeline`, which check a
  node's timing, with `mtwn_errorInvalid`. `timing.h` already promised
  that a negative rate needs a finite active duration, whose end a
  backward play starts from. 0.2.1 checked only iterations times
  duration, so it accepted backward timings whose repeat delays or small
  rate overflowed the active duration, and those gave NaN iterations.
  Newly refused: finite iterations and duration whose active duration
  overflows, played forwards, which became endless; an end time (delay
  plus active duration plus end delay) past a double, which became
  endless the same way; a duration plus repeat delay past a double,
  whose iterations came out NaN; and a child that would give a timeline
  lasting as long as its children such a timing, which leaves the clip
  as it was. Endless timings played forwards stay valid: infinite
  iterations or an infinite duration with a positive rate. Infinitely
  many iterations of nothing, or none of an infinite duration, last 0
  and are checked as finite ones are. Keep a timing's sums within a
  double; every timing within one validates and computes as before.
- Code is larger where `timing.h` links: values 60,185 bytes (from
  59,673), clips 151,836 (151,388), and the whole library 169,666
  (169,218), as `tools/code_size.py` measures them.

## [0.2.1](https://github.com/Rawframe-Project/maul-tween/compare/v0.2.0...v0.2.1) (2026-10-25)

### Fixed

- A target that a clip's track wrote at full weight and an eased
  transition of numbers then wrote over in the same update reported a
  wrong velocity, hundreds of times too large: the track's, worked out
  against the transition's value. `mtwnTarget_GetState`, a transition
  started on the target (such as a spring taking its velocity) and a
  clip layer composed over it now get the transition's velocity. The
  value was always right
  ([#50](https://github.com/Rawframe-Project/maul-tween/pull/50)).

## [0.2.0](https://github.com/Rawframe-Project/maul-tween/releases/tag/v0.2.0) (2026-10-24)

### Changed

- **Breaking:** a spring whose start overflows its settling bound now
  gets its true settling time, where 0.1.0 gave infinity. This covers a
  coefficient that overflows, two products that overflow with opposite
  signs, and, for a slow spring, a bound that overflows on the way.
  `mtwnSpringSettlingTime` and `mtwnBoundedDecayStopTime` return the
  true, finite time. A spring transition from such a start, or a
  bounded decay handing over to a spring, ends at that time: the end
  value becomes the target's base and an end record is written, where
  before it ran until replaced or stopped. An absurdly stiff spring from
  far out really settles almost at once. Every result that did not
  overflow keeps its bits.
- Code is larger by 320 to 384 bytes a part: curves 27,545 bytes (from
  27,161), values 59,673 (59,289), transitions 62,937 (62,617), clips
  151,388 (151,004), and the whole library 169,218 (168,834), as
  `tools/code_size.py` measures them. An ordinary `mtwnSpringSettlingTime`
  takes 3,204 instructions where it took 3,169.
- The user guide's clips, transitions and players pages open with a
  list of their sections.

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
