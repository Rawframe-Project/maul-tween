# mtwn-0001. Library profile

Status: Accepted

## Context

Every Maul library states in one record what its domain adds to the
family rulebook (family record 0005). Maul Tween computes animated
values from the host's time: easing curves, springs and decay,
interpolation of scalars, vectors, colors and rotations, keyframe
tracks and paths, timelines that place and repeat them, and players
that run them on values the host reads. It must stay small enough for
a host that moves one button, fast enough for one that moves tens of
thousands of values a frame, and give the same results wherever it
runs.

## Decision

- **Determinism:** bit-exact. The same inputs at the same host times
  give the same bits on every platform, compiler and architecture, for
  every value and velocity the curves return, every target after an
  update and every event record. Values are binary32 at the API and
  computed in binary64 up to one rounding; host time is integer
  nanoseconds and local times are computed from it, never summed from
  frame deltas; only the `<math.h>` functions IEEE 754 rounds exactly
  are called (`tools/libm-allowed.txt`), and the library writes its own
  exponential, logarithm, trigonometry and powers.
- **Threads:** none. The curve and interpolation functions have no
  owner and are safe from any thread; a context is used by one thread
  at a time.
- **Memory:** the curve and interpolation functions allocate nothing.
  The one owner object is the context, which takes the host's
  allocator and every capacity in its def and allocates its pools when
  it is created; a clip's storage is allocated when the clip is
  created. Nothing is allocated by an update, and nothing grows by
  itself.
- **Platform dependencies:** none beyond the C standard library's
  `<stddef.h>`, `<stdint.h>`, `<string.h>` and the exactly rounded
  functions of `<math.h>`. No operating system call: no clock, file or
  thread.
- **Commit areas:** `api`, `bench`, `build`, `ci`, `clip`, `color`,
  `context`, `docs`, `easing`, `event`, `math`, `path`, `player`,
  `rotation`, `sample`, `spring`, `target`, `tests`, `timeline`,
  `timing`, `tools`, `transition`, `value`.

## Consequences

A host links only what it calls: the curves alone need no context. Two
machines given the same calls at the same times agree to the bit, so
replays, lockstep games and frame tests work across platforms, and the
host decides when time passes and where every byte lives.

## Amended 2026-10-17: release commits

A release is one commit `release: X.Y.Z`, as the family's conventions
(section 15, the release checklist) fix it for every library; the
commit areas above are for every other commit. 0.5.0 and 0.5.1 were
committed as `api: version X.Y.Z`, outside that rule; the releases
from 0.5.2 on follow it, with the library's own steps in
`docs/releasing.md`.
