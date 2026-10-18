# Changelog

All notable changes to this project are recorded here. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
the project uses [Semantic Versioning](https://semver.org/). Before
1.0.0, any minor release may change the API and the ABI.

## [Unreleased]

### Changed

- `tools/code_size.py --budget` fails a part's budget set more than 10%
  over its size at the last release unless a record raises it.
- The guide's animation on twos states its grid: from the timeline's
  start, a whole number of holds.
- A scale transition's velocity is the held value's times its rate, the
  value at most the largest float: within a float's rounding of before,
  with no case for a rate of 0 (record mtwn-0059).

## [0.6.0] - 2026-10-18

A minor release: scales moved in their logarithm and van Wijk and
Nuij's zoom, rough easings, contexts without records, inertia that
lands or is bounded, a path's direction, a shake's noise on its own
clock, monotone segments, and recipes for jumps, trauma shake, keyframe
handles and animation on twos; each part's size budget restated.

Its sizes (x86-64, GCC 14, `--gc-sections`, by the parts a program
uses): curves 24,985 bytes, values 56,281, transitions 56,601, clips
138,762, everything 155,353. `tools/code_size.py --budget` fails a
later budget set more than 10% over these unless a record raises it
(record mtwn-0055).

### Added

- The guide's animation on twos: a timeline eased by steps() holds its
  children at as many poses, as After Effects' Posterize Time.
- The guide's keyframe handles: a curve editor's segment, a Bezier in
  time and value (Blender's F-curve handles, Unity's weighted
  tangents), as a Bezier segment timed by a cubic-bezier easing.
- `mtwn_segmentMonotone`: a segment through the keyframes in time that
  never passes them in any channel, by Steffen's slopes over the
  offsets (record mtwn-0067), as curve editors' clamped tangents keep
  a value's curve; flat where a channel turns, its speed continuous
  across keyframes of any spacing.
- The guide's update: a value shown on a grid is snapped where the host
  shows it (record mtwn-0066).
- `mtwnShakeNoise`: a shake's noise alone on any clock, for camera
  shake driven by trauma (record mtwn-0065); the guide shows it.
- The guide's jump: DOTween's DOJump as a clip of three tracks, the
  hops added over the height's line, repeating up and down.
- A path's direction (`mtwnPathDirection`) and angle (`mtwnPathAngle`)
  by SVG 2's path directionality, as CSS's `offset-rotate: auto` turns
  along a path: derivatives of 0, steps, coincident keyframes and paths
  of no length included (record mtwn-0064). The values part's size
  budget is 57 kB and every function's 156 kB.
- The guide's scroll-driven playback: a paused player swept to the
  scroll's progress before each update, smoothed by a spring the host
  retargets at each scroll event.
- Transitions' decays bounded and landing (record mtwn-0063):
  `bounded` holds a fling between `lower` and `upper`, the def's spring
  bouncing it back from the bound it reaches; `lands` brings it to rest
  on the def's value at the start velocity. The transitions' size
  budget is 58 kB and the clips' 139 kB.
- Inertia (record mtwn-0063): `mtwnMakeDecayToRest`, the decay that
  rests on a chosen end with the start's velocity, for snapping a
  fling; `mtwnMakeBoundedDecay`, `mtwnSampleBoundedDecay` and
  `mtwnBoundedDecayStopTime`, a decay between bounds that a spring takes
  back from the exact time it reaches one.
- Rough easings (`mtwnMakeRoughEasing`): `linear()` through points
  wandering from a shape by a seed, the same curve on every platform,
  with GSAP's tapers, clamped, rising or an even zig-zag (record
  mtwn-0061). The curves part's size budget is 25 kB, from 24.
- Scales moved in their logarithm, so equal steps multiply by equal
  ratios (`mtwnMakeScaleMix`, `mtwnMixScale`), and van Wijk and Nuij's
  smooth zoom and pan between two views (`mtwnMakeZoomMix`,
  `mtwnMixZoom`, `mtwnZoomLength`), record mtwn-0059. The values part's
  size budget is 54 kB, from 50.
- `mtwn_valueScale`, a target kind moved in its logarithm: eased along
  its scale mix, sprung along the mix's progress (overshooting by a
  ratio), decayed in the logarithm (record mtwn-0059).
- Scale tracks: keyframes of `mtwn_valueScale` move along their
  logarithms, every segment kind and constant speed included;
  `mtwnPath` has a `logarithmic` byte where it had a reserved one.
- Scales in clips: tracks of `mtwn_valueScale`, composed in their
  logarithm (add multiplies, replace mixes geometrically, weights as
  powers), incremental repeats multiplying by the track's ratio.
- `mtwnMeasureTransition`: the distance a transition would move its
  target, in its kind's own measure, for a duration set by speed
  (record mtwn-0060). The size budget of a program taking every
  function is 152 kB, from 146.

### Changed

- A monotone segment works only its path's channels: a one-channel
  monotone track's sample takes 518 instructions, from 805; the bench
  counts it (record mtwn-0058's target, at most 600).
- A path's Catmull-Rom segment loads its keyframes once: a path's
  sample at constant speed takes about 22 fewer instructions.
- A clip's track that does not move on by its iterations costs a test:
  the bench's clip frames take 1.6% fewer instructions.
- A context of `eventCapacity` 0 makes no records and drops none; its
  clips' players skip the sweeps that find crossings (record
  mtwn-0062). Before, every record was made and dropped, counted. A
  repeat frame of 1,000 players on clips of their own falls from 16.6
  to 3.9 million instructions.
- `event.h` states which iteration an iteration record names: the one
  entered going forwards and, going backwards, the one left, current at
  the record's time (its start) with the one before it just after. The
  behavior is unchanged; a test pins it for a backward player and a
  node's negative rate.

### Fixed

- A scale target's velocity stays finite when its value passes the
  floats (a fling of 1e12 read infinity), and a clip track's velocity
  within the floats too (record mtwn-0059).
- At constant speed, a path whose end segment joins coincident
  keyframes went to enormous values just past that end; the segment
  now holds still as a segment of no length does. Keyframes whose
  offsets lie within 2^-52 of each other now jump as equal offsets do,
  where extending them before or after took a color track to NaN
  (record mtwn-0031, amended).
- A scale past the float range (a steep track extended, an easing far
  past 1, an incremental repeat) became infinite or 0; it now holds at
  the largest or smallest positive float (record mtwn-0059, amended).
- `mtwnSpringSettlingTime` returned NaN for two kinds of finite start:
  a velocity bound whose products overflowed with opposite signs (now
  infinite), and a critical spring whose bound peaked at a time rounding
  to nearly 0, where the search began on a slope of 0 (record
  mtwn-0063).
- A path of two or more channels that turns back along a line inside a
  segment measured that segment short (0.4% on a Bezier through 0, 6, 1
  and 5), and its points at constant speed came up to 1% of the length
  off: the arc length's two rules both missed the turn. Each length is
  now split at the speed's minima. Lengths and points at constant speed
  move within the old tolerance elsewhere; a path sample costs about
  20% more instructions.
- A path's lengths were measured to an agreement of 1e-9 between two
  rules, which near a sharp slowing of the speed is about the error
  itself: lengths up to 2.5e-9 of a segment off, where points at
  constant speed are placed to 1e-12. The rules now agree to 1e-12;
  lengths are within about 1e-11, as `path.h` now states, and random
  paths' lengths exact to their last bits. A path sample at constant
  speed costs about 13% more instructions again.

## [0.5.1] - 2026-10-17

A patch: the active start of a timing whose whole iteration start meets
a repeat delay no longer shows an iteration before the first.

### Fixed

- A timing whose whole iteration start's cycles round below it on the
  repeat delays' timeline (an iteration start of 339 with iterations and
  delays of 0.1 s) showed, at its active start, the end of an
  iteration before the first, inside a repeat delay, and the records
  of a timeline nested in it counted its whole first iteration at once;
  the active start is now the first iteration's start (record
  mtwn-0012, amended).

## [0.5.0] - 2026-10-17

The value functions fuzzed with wild parameters and their defects at
extreme inputs fixed; a fit of linear() limited by
`MTWN_MAX_FIT_POINTS`; a fuzz target over the public API.

### Added

- `test/fuzz_api.c`: the public API driven by bytes, the same script on
  a context alone, on a task system and counting every record, which
  must agree bit for bit with values finite and records in order.
  `ctest` runs it on a fixed set of inputs; `MAUL_TWEEN_FUZZ` builds it
  as a libFuzzer target (Clang).
- `test/fuzz_values.c`: the value functions driven by bytes with
  ordinary and wild parameters, run by `ctest` on a fixed set of inputs
  and built as a libFuzzer target with the other.
- `MTWN_MAX_FIT_POINTS` (65536): `mtwnFitLinearEasing` returns
  `mtwn_errorLimit` when a fit needs more points, as for a tolerance
  below the curve's rounding, which before took every segment down to
  2^-24 (about 10^8 points, minutes of work) (record mtwn-0022).
- `mtwnFitLinearEasing` refuses (`mtwn_errorInvalid`) an easing that is
  infinite or NaN at a point it would keep, as a host function or a
  linear() overflowing between wide points can be, instead of returning
  points `mtwnMakeLinearEasing` refuses (record mtwn-0022).

### Fixed

- The back easing with an overshoot from 2^53 was NaN and infinite
  everywhere: its end, (s + 1) - s, rounded to 0. It is now computed as
  p^2 (p + s (p - 1)), exactly 1 at the end for any overshoot and
  closer to the exact curve (within 1.1e-16 for 1.70158, from 4.6e-16)
  (record mtwn-0011).
- linear() between outputs of opposite signs near the double range
  (1e308 and -1e308) overflowed their difference, -inf at the second
  point; such a span is now interpolated as a weighted sum (record
  mtwn-0011).
- A spring's motion was NaN far past its envelope's underflow, where
  its phase was beyond the sine's range, and -inf or NaN for a spring so
  stiff that its phase passed 2^52 radians early on; a phase that holds
  no angle is now reduced by 2 pi first (an infinite one to 0), so the
  motion stays bounded and an underflowed envelope brings it to rest
  (records mtwn-0010 and mtwn-0023).
- A constant-speed path could take minutes for one sample: where a
  segment's speed falls to 0 at an end as (1 - u)^2 (a Bezier with both
  handles on that end), the length's two quadrature orders never agree
  in the rounding there, and a span 2^-40 of the segment from the end
  split until the stack was full (about 2^32 rule pairs). One length
  now takes at most 1024 (the tests' take at most 17) (record
  mtwn-0031).
- A rotation sampled far past its ends was NaN: a curved segment's
  cubic overflowed before it was normalized (now scaled by its largest
  component first), and `mtwnMixRotation` far out took the sine of a
  phase out of range (now taken at the progress less the phase's whole
  turns, a unit rotation) (records mtwn-0025 and mtwn-0031).
- A punch over a long span, or fast enough that its cycles overflowed,
  sampled NaN; its phase is now reduced in cycles, exactly, before the
  sine (record mtwn-0026).
- A shake with 2^53 or more lattice points over its duration converted
  an out-of-range double to an integer; it is refused (record
  mtwn-0026).
- The rubber band was NaN with an infinite dimension or coefficient,
  and its inverse with an infinite dimension (records mtwn-0010 and
  mtwn-0023).
- An incremental track with endless iterations of no duration is
  refused when added: all its iterations pass at once, and past them
  its value moved on by an infinite iteration, to infinity or NaN
  (record mtwn-0040, amended).

## [0.4.1] - 2026-10-17

A patch: values beside repeat delays and a negative rate's start, and
records past repeat delays, fixed.

### Fixed

- A record could be lost where a crossing fell exactly at a frame's end
  inside a timeline with a repeat delay whose times are off a binary
  grid (a reversed 0.2 s timeline held a crossing at 1.28125 s): the
  exact count placed it in the next frame, while the shortcut that
  skips a point the frame before ended on took it for counted. The
  count's progress within an iteration past a repeat delay is now formed
  as the timing model forms it, so both see the same child time (record
  mtwn-0042, amended).
- With a repeat delay, a time where an iteration's progress rounded to
  its end showed the next iteration's start for that one time before
  the delay held the end: iterations of 0.1 s with delays of 0.1 s did
  at 0.5 s, a value jumping to the keyframes' start for a frame. A time
  whose quotient by the cycle rounded up past an iteration's start was
  an ulp below it, out of the delay. Both now hold the end (record
  mtwn-0012, amended).
- A crossing past a repeat delay could be recorded at a time and
  iteration two ulps from where counting every record put it: the count
  fell again an ulp past an iteration's start, so the search for the
  time ended on another edge when it started from a guess. The count
  now rounds as the timing model does and never falls (record
  mtwn-0037, amended).
- With a negative rate, a time a double after the active start (where
  the active time is lost from the active duration) showed an iteration
  past the last at its start for that one time; the end is now held
  wherever the overall progress reaches it (record mtwn-0012, amended).
- With the alternate-reverse direction, iterations from 2^53 on all ran
  forwards: their parity was taken of the iteration plus one, which
  rounds back to it there. It is now the iteration's own, negated.
- A record's count of 2^64 - 2048, the largest double below 2^64, was
  given as UINT64_MAX; counts below 2^64 are now exact and only those of
  2^64 or more are UINT64_MAX, as `mtwnEvent` now says.

### Changed

- With the host's task system, a clip with one player is sampled in
  its own node times beside its nodes, not in its group's: on one
  worker the bench's players on clips of their own fetched twice the
  cache lines from the shared cache that the update without the hooks
  did, and now about 10% more (record mtwn-0056).
- With the host's task system, a clip with one player runs in its
  group however many nodes it has; only a clip of more than 128 nodes
  with other players is left to the calling thread (record mtwn-0056).
- The count check bounds the second-level cache misses of the bench's
  players with the task hooks on one worker over those without them,
  the median of five tries, so the misses that composing in the groups
  once doubled cannot come back unseen; above a load of one task a
  thread, where the ratio moves too much, it reports the line skipped.
- The bench prints the compiler it was built with (`--compiler`), and
  the count check names it beside the one `bench/counts.txt` was set
  with when they differ or a bound fails.

## [0.4.0] - 2026-10-17

The clip players on the host's task system cheaper and their memory
the host's to set: on one worker the hooks cost 0.8% more instructions
than none for the bench's 1,000 players (5.5% before), and 1.3% with a
tenth of them made and freed each frame (9.3%); the sharing's
capacities are in the context def, 0 turning it off, and a count of
the players left serial is read from the context. The def grew by 8
bytes, so programs are built again against these headers. The gate
holds the bench's frames to instruction bounds.

### Added

- `mtwnContextDef.sharedNodeCapacity` and `sharedRecordCapacity`
  (defaults 128 and 64): the most nodes a clip may have for players in
  step to share its sample, and the most records a sweep keeps to
  share; 0 shares none and holds nothing for it (14,848 bytes a work
  at the defaults, 17 works with a task system). `mtwnGrowContext`
  takes any (record mtwn-0057).
- `mtwnContext_GetSerialAdvances`: the clip players' advances a context
  with a task system ran on the calling thread for want of a group's
  room (record mtwn-0056).

### Changed

- The context def grows by 8 bytes, so the ABI version is 0.4:
  programs are built again against these headers.
- With the host's task system, a group takes the target one player
  alone writes from its binding without looking it up, as the writers
  were just counted: on one worker the bench's 1,000 players on clips
  of their own cost 0.6% more instructions than without the hooks
  (5.5% before), the frame where all repeat 0.1% (2.7%); without the
  hooks 0.4% fewer (record mtwn-0056).
- A player under reduced motion not yet swept sweeps twice in its first
  update, so a group takes it only with room for twice the records.
- The bench's `--count` layouts take `-tasks`, run in groups on the
  calling thread, and the gate bounds them too.
- With the host's task system, who writes each target is kept as
  players are made and freed and transitions start and end, instead of
  counted again over every binding in the next update: the bench's
  1,000 players with a tenth made and freed each frame cost 1.3% more
  instructions with the hooks than without (9.3% before); a steady
  frame 0.8% (record mtwn-0056). The bench's `--count` takes `churn`.
- Clip players share a track result or a sweep only when the time,
  rate and ends match the last one's bits, not only their values (0
  equals -0; record mtwn-0057): no result found to differ yet, so the
  sharing's same-bits rule rests on its inputs alone.
- A count of whole iterations of 0 adds nothing to a record's count
  instead of multiplying a constant that may be infinite (record
  mtwn-0037).
- The bench's `--count` takes a clip period, and the gate holds its
  frames and samples to the instruction bounds in `bench/counts.txt`
  (`tools/bench_counts.py`, record mtwn-0043): a shortcut lost changes
  no result, only the count.

## [0.3.1] - 2026-10-16

Clip players faster again, and their records right at the edges: in
the bench, 1,000 players in step from 6.02 to 1.75 million instructions
a frame, sharing their work, and 1,000 on clips of their own all
repeating in one frame from 25.9 to 16.5 million; paths at constant
speed 2.6 times faster and exact for one channel; with the host's task
system, a target one player alone writes composed in its group. Counts
no longer depend on where frames fall, and a start's or end's iteration
is right where a timeline repeats. No public layout changes: programs
built against 0.3.0 headers run as they are.

### Changed

- Clip players in step share their work: a player of the same clip at
  the same time, rate and direction as the last in its work takes that
  one's track results and records instead of working them out, with
  the same bits (record mtwn-0057). The bench's 1,000 players in step
  from 6.02 to 1.75 million instructions a frame, the frame where they
  all repeat from 25.8 to 2.14 million; players out of step cost 0.4%
  more. A context holds 14.5 KB more for it, and as much again for
  each of the 16 groups when given a task system.
- Records are counted with fewer walks down a clip's levels: constants
  a level's direction never reads are left unwalked, equal ones copied,
  and a clip's endless root, passing its time through, left out
  (record mtwn-0037, amended). The bench's 1,000 players on clips of
  their own, all repeating in one frame, from 25.9 to 22.7 million
  instructions.
- Paths at constant speed find the parameter at a length 2.6 times
  faster: lengths by Gauss-Kronrod, the parameter by Halley's steps
  from the segment's nearer end, a short last step by the length's
  Taylor series (record mtwn-0031, amended). The bench's path through
  eight points from 5,250 to 2,026 instructions a sample.
- A linear segment is sampled straight from its keyframes, without
  making its cubic, the same bits (record mtwn-0058, amended): the
  bench's track of three keyframes from 344 to 237 instructions a
  sample, clip frames 7.6% fewer.
- A record of one crossing is searched once, its last its first
  (record mtwn-0053, amended): the bench's 1,000 players on clips of
  their own, all repeating in one frame, from 22.5 to 17.75 million
  instructions.
- A frame starting exactly on an iteration's boundary leaves out what it
  cannot cross, and a crossing's search starts from a guess just past
  the sweep's end (records mtwn-0042 and mtwn-0053, amended): the
  bench's 1,000 players on clips of their own, repeating every other
  frame, from 15.2 to 11.7 million instructions a frame on average.
- A start or end record takes the node's iteration from its own timing,
  worked out when its clip is finished, without walking the timelines
  above (record mtwn-0038, amended): the bench's 1,000 players on clips
  of their own, repeating every other frame, from 11.8 to 11.0 million
  instructions a frame on average, their repeat frame at 1 s from 18.1
  to 16.5 million. A clip's node holds 16 bytes more.
- With the host's task system, a clip player's target that no other
  player and no transition writes is composed in the player's group,
  in place, instead of on the calling thread from values staged on
  other cores (record mtwn-0056, amended): the serial rest of the
  bench's update from about 100 to 220 microseconds to 27 to 38 at 1, 8
  and 16 workers, measured under load. The context holds about 12 KB
  more with the hooks at the default def; without them the bench's
  frames cost up to 0.9% more instructions. The clips' size budget is
  127 kB and the whole library's 138 kB until 0.4.0 (record mtwn-0055,
  amended).

### Added

- The bench times and counts one sample each of a color mix in Oklab
  and in OkLCh, a slerp, a path at constant speed and a track, against
  targets of their own (record mtwn-0058).

### Fixed

- A path of one channel has its total variation for a length, exact:
  a quadrature missed the kink each turn puts in its speed, and a
  number turning twice had points at constant speed 1.1e-4 of its
  length off.
- At constant speed the lengths of the parameter's steps, summed, no
  longer drift from the length measured afresh: on a random curve a
  point came 1.6e-7 off.
- A frame ending exactly where a held time starts to move, or on a
  timeline's iteration boundary, recorded what lies at the held time:
  a track at the start of a delayed timeline did not start in the frame
  landing on the delay's end, and a timeline held after its end, played
  backwards, did not end in the frame landing on its end (record
  mtwn-0038, amended). Counts no longer depend on where frames fall;
  clip frames cost about 2% more.
- A record of one crossing could report a last iteration one before its
  first, its two searches landing a double apart through nested
  timelines' rounding.
- A start or end record where a timeline above repeats could report the
  iteration on the repeat's far side: a track ending where its timeline
  repeats reported its first iteration at its end, and a track's start
  crossed backwards there its second.

## [0.3.0] - 2026-10-16

Clip players on the host's task system, and clip players several times
faster: the average frame of the bench's 1,000 looping players from
22.5 to 6.0 million instructions, its repeat frame from 1,060 to 25.8
million. The context def's layout changes, so programs built against
0.2 headers are refused with mtwn_errorVersion until rebuilt.

### Added

- `mtwnContext_GetPlayingOnCount` and `mtwnPlayerState.playsOnReduced`:
  the clips' players an update leaves running under reduced motion
  though not essential (an infinite end or a rate of 0), so the host can
  find them and meet the preference itself (record mtwn-0049, amended).
- `mtwnContextDef` takes a task system, Maul VFX's hook pair
  (`enqueueTask`, `finishTask`, `userTaskContext`): `mtwnUpdate` then
  advances the clip players in up to 16 groups on the host's threads,
  staging what they make, and composes them in creation order after.
  Every output is the same bits as a serial update's (record
  mtwn-0056). The def's layout changes.

### Changed

- A random stagger's order is drawn without bias: a place was drawn by
  one multiply, which favoured some places by up to count / 2^32; the
  rare draws that would do so are drawn again (Lemire's method). A
  seed's order changes only where such a draw comes up.
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
