# Overview

Maul Tween computes animated values from the host's time: easing curves, springs and decay, the interpolation of every value kind, keyframe paths and tracks, timelines that place and repeat them, and players that run them on values the host reads. It follows the family rules in [maul-conventions.md](maul-conventions.md); this page states what the library's domain adds and how the parts fit.

## Library profile

- **Determinism:** bit-exact. The same calls at the same host times give the same bits on every platform, compiler and architecture: every value and velocity a curve returns, every target after an update, every event record. See [numbers.md](numbers.md).
- **Threads:** none started. The values layer has no owner and is safe from any thread; a context is used by one thread at a time, and may hand its clips' players to the host's task system ([update.md](update.md#the-hosts-task-system)).
- **Memory:** the values layer allocates nothing. The one owner object is the context, one block from the host's allocator holding every capacity its def names; each clip's storage is one more block. An update never allocates and nothing grows by itself ([memory-and-contract.md](memory-and-contract.md)).
- **Stale ids:** by kind. A target or a clip goes only when the program destroys it, so a correct program does not use its id afterwards: a stale target or clip id is refused with `mtwn_errorStale` and counted as misuse. A player may free itself at an end the update decides (a transition at its end, a clip's player not kept), so an id it once had outlives it by design: a stale player id the context handed out, a kept player's too, is refused with `mtwn_errorStale` and counts nothing. A player id never handed out, the null id among them, counts one: no player freeing itself makes it. The `_IsValid` queries count none. A host that needs a player after its end keeps it (`keep` in its def).
- **Calls back in:** none allowed. The host's code the library runs calls no function of the context it serves ([memory-and-contract.md](memory-and-contract.md#calls-back-in)).
- **Platform dependencies:** the C standard library only: `malloc` and `free` behind a zeroed allocator ([`src/allocator.c`](../../src/allocator.c)), the `<math.h>` functions IEEE 754 rounds exactly (listed in `tools/libm-allowed.txt`), `<stdatomic.h>` for the misuse count, and the compiler's intrinsics for the floating-point control register ([`src/fpenv.c`](../../src/fpenv.c)). No clock, file or thread call.

## Scope

In: easing (Penner's families, CSS Easing Level 2 entire, rough curves, host functions), springs, decay, inertia and the rubber band, punch and shake; scalars, vectors, colors, angles, rotations, integers, discrete values and scales; keyframe paths and tracks at constant speed; Web Animations' timing on every node of nested timelines with positions, labels and marks; players, composition, events as records, staggers and reduced motion.

Out: skeletal animation and particles, finding or setting properties of host objects, parsing CSS or any string syntax, a clock or a frame loop, physics beyond one-dimensional springs and decay per channel, scroll and gesture policy, and any file format. A host that moves one button links none of what a timeline needs.

## Two layers

```text
values layer (no owner, any thread)          context (one thread at a time)
-----------------------------------          ------------------------------
easing.c css_easing.c rough.c sample.c       context.c  target.c  pool.c
spring.c decay.c inertia.c                   transition.c  update.c  coast.c
color.c rotation.c scale.c zoom.c signal.c   clip.c  clip_read.c  clip_sample.c  compose.c
path.c path_length.c path_direction.c        player.c  player_control.c  player_slot.c  runs.c
timing.c stagger.c                           clip_tasks.c  sweep.c  crossing.c  event.c
             \_______ elementary.c, fpenv.c, mix and bounds helpers _______/
```

The values layer is plain structs and functions: a maker validates a value once and returns a status, and its sample takes the value and returns a result with nothing to check. The public headers of this layer include only `base.h` and each other. The context holds targets, transitions, clips and players, and is advanced by `mtwnContext_Update` with the host's time.

`tools/modules.txt` declares which module may include which, and `tools/check_modules.py` enforces it.

## Targets, clips and players

- A **target** is one animated value of a fixed kind: the host's base value, the current value animations compose over it, and its velocity. Animations write targets; the host reads them by id.
- A **clip** is an immutable description built once: tracks of keyframes on nested timelines, labels and marks. Its tracks address slots, so one clip plays on many sets of targets, and a sealed clip can be read from any thread.
- A **player** plays a sealed clip with its slots bound to targets, by Web Animations' procedures; per binding it has a composite operation and a weight. Players compose onto each target in creation order, from its base.
- A **transition** is the short path: a player holding one curve inline, started from the target's current value and velocity, replacing the transition already on the target.

A player not kept is freed at its end, first committing what it fills forward into its targets' bases, as `commitStyles()` keeps a value. A clip a player uses cannot be destroyed.

The clips' players reach the update through a table of three functions that the first clip's player installs in its context, so a program of transitions alone references, and links, none of the clip code; see [update.md](update.md#linking-only-what-is-used).

## Where each part is explained

- [numbers.md](numbers.md): the floating-point environment, the library's own elementary functions, float values and double computation, values held within the floats.
- [easing.md](easing.md): the easing value, exact ends, the cubic-bezier solver, steps, `linear()`, the fit.
- [physics.md](physics.md): springs in closed form, settling times, decay, inertia, punch and shake.
- [values.md](values.md): colors, angles and rotations, scales and zoom, paths, arc length, tracks.
- [timing.md](timing.md): Web Animations' timing model, rates, repeat delays, infinite times.
- [clips.md](clips.md): building a clip, positions, ends carried up, sampling and composition.
- [events.md](events.md): crossings counted exactly, the sweep, the queue.
- [update.md](update.md): the hot path, the host's task system, players in step.
- [memory-and-contract.md](memory-and-contract.md): the block, ids, defs and versions, refusals, outputs on failure, calls back in.
- [glossary.md](glossary.md).

## Testing

Every part is checked against a reference made another way: long double formulas for the math and the curves, `tools/oracle/` scripts that recompute Web Animations' timing and the crossing counts in exact rationals and the zoom in 60-digit decimals, the rival libraries run under Node, and a determinism scenario whose hash is one constant on every platform. Fuzz targets drive the public API and the values layer and compare implementations that must agree. See [development.md](../operations/development.md).
