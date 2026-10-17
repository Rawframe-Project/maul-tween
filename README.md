# Maul Tween

Easing, springs, tweens and timelines for C and C++ programs, driven by
the time the host passes in. Written in C23 with public headers any C17
or C++17 program can include, with no dependency and an MIT license.

It covers:

- **Easing:** Penner's curves landing exactly on their ends, and CSS
  Easing Level 2 entire: `cubic-bezier`, `steps`, `linear()` with
  points.
- **Physics:** damped springs in closed form that keep their velocity
  when retargeted, set by stiffness and damping or by duration and
  bounce; decay for flings; rubber banding; punch and seeded shake.
- **Values:** scalars, vectors, colors interpolated in Oklab or another
  named space, rotations as quaternions, angles, integers, discrete
  values, keyframe tracks and paths at constant speed.
- **Timelines:** Web Animations' timing on every node (delays, fill,
  direction, fractional and infinite iterations), positions and labels,
  nesting, players with play, pause, reverse, seek and rate, events as
  records the host reads.

The curves need no setup and allocate nothing. Everything else lives
in a context the host creates with its allocator and fixed capacities;
nothing is allocated afterwards, and results are identical on every
platform.

## Status

0.4.1 (2026-10-17) holds the whole library: Penner's families and
CSS Easing Level 2, in four modes, with even tables and fitted CSS
linear() points; springs in closed form, as easings too, decay and the
rubber band; colors as CSS Color 4 interpolates them; angles,
rotations, integers, discrete values, punch and shake; Web Animations'
timing with playback rate and repeat delay; staggers over a row or a
grid; a context in one block holding targets, and transitions advanced
by an update that guards the floating-point environment; keyframe
paths, Catmull-Rom or Bezier, at constant speed if asked, and tracks
of every value kind over them; clips holding tracks on nested
timelines with labels and marks; players controlling clips by Web
Animations' procedures; events as counted records in a bounded queue;
players layered by replace and add with weights; reduced motion; a
context grown only when the host asks; clips' players run on the
host's task system if it gives one. 0.4.0, earlier that day, had the
same features, values that could jump to an iteration's start for one
time beside a repeat delay or a negative rate's start, and records that
could be lost or two ulps off past a repeat delay; 0.3.1, the day before, had the
same features, the sharing's memory fixed and the task hooks costing up
to 9% on one worker; 0.3.0, earlier that day, had slower clip players
and records that could miss a crossing where a frame landed on a held
time's edge; 0.2.1, on 2026-10-15, ran them on the calling thread only
and several times slower; 0.1.0 lacked staggers and reduced motion. The library has
110 public functions.

## Building

Requirements: CMake 3.25 and GCC 14 or Clang 19 or newer; on Windows,
`clang-cl` (the Visual Studio component "C++ Clang tools for Windows").

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

[The guide](docs/guide.md) walks through the library in the order a
program uses it. The programs in `samples/` show it at work, from a
button's states to layered clips; [the samples page](docs/samples.md)
says what each shows. `docs/api.md` describes every public function.

## Testing

`ctest` runs every test. Some tests check against references made by
other means, kept in `tools/oracle/` and run by hand. Their outputs are
committed under `test/generated/`, and CI checks the library against
them:

- `timing_oracle.py`: Web Animations' timing steps, written again in
  Python.
- `rivals.mjs`: GSAP 3.15.0, tween.js 25.0.0, Motion's springs
  (motion-dom 14.2.0 with motion-utils 14.1.1) and CSS Color 4's
  `conversions.js` (w3c/csswg-drafts at af8661b), run under Node. Each
  is fetched by its pinned version and checked against its pinned hash
  before it runs. None is kept in this repository or linked into the
  library.

## Size

The code a program links, by the parts it uses: x86-64, GCC 14 `-O2`,
the program linked with `--gc-sections`, `.text` of a program taking
every function of the part's headers, an empty program's subtracted.
`python3 tools/code_size.py <the built libmaul-tween.a>` measures it,
and with `--map` shows what links a part in. A program that never makes
a clip's player links none of the players, clips' sampling,
composition, paths or crossings (record mtwn-0047).

| The program uses | Default build | Built with `-ffunction-sections` | Clang 20, default build | Budget |
|---|---|---|---|---|
| curves: easings, sampling, springs, decay, rubber band | 21.8 kB | 21.3 kB | 18.6 kB | 23 kB |
| values: colors, angles, rotations, punch, shake, paths, tracks, timing, staggers | 45.8 kB | 37.2 kB | 39.2 kB | 48 kB |
| transitions: a context, targets and transitions | 49.5 kB | 41.4 kB | 40.9 kB | 52 kB |
| clips: clips, players, events | 125.6 kB | 103.5 kB | 103.8 kB | 132 kB |
| everything | 136.2 kB | 136.0 kB | 114.0 kB | 144 kB |

Each part counts what it calls in the others; about 12 kB of each is
the library's own elementary functions, the same under every C library. The budgets
apply to GCC 14's default build on x86-64 only; the Clang 20 column
(`--cc clang-20` on a Clang Release build) is kept beside them so that
a growth one compiler shows and the other does not is seen as the
compiler's, not the library's (records mtwn-0051 and mtwn-0055).
`--budget` fails past one, and growing past one takes a recorded
decision.

## Design

The rules every Maul library follows are in `docs/conventions.md` and
`docs/adr/`; the records particular to this library are listed in
`docs/adr/mtwn.md`.

## License

MIT; see `LICENSE`.
