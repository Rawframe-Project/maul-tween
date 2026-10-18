# Maul Tween

Easing, springs, tweens and timelines for C and C++ programs, driven by
the time the host passes in. Written in C23 with public headers any C17
or C++17 program can include, with no dependency and an MIT license.

It covers:

- **Easing:** Penner's curves landing exactly on their ends, and CSS
  Easing Level 2 entire: `cubic-bezier`, `steps`, `linear()` with
  points; rough easings drawn from a seed.
- **Physics:** damped springs in closed form that keep their velocity
  when retargeted, set by stiffness and damping or by duration and
  bounce; decay for flings, landing on a chosen end or bounced back
  from bounds; rubber banding; punch and seeded shake.
- **Values:** scalars, vectors, colors interpolated in Oklab or another
  named space, rotations as quaternions, angles, integers, discrete
  values, scales moved geometrically, van Wijk and Nuij's smooth zoom
  and pan, keyframe tracks and paths at constant speed, with their
  direction for turning along them.
- **Timelines:** Web Animations' timing on every node (delays, fill,
  direction, fractional and infinite iterations), positions and labels,
  nesting, players with play, pause, reverse, seek and rate, events as
  records the host reads.

The curves need no setup and allocate nothing. Everything else lives
in a context the host creates with its allocator and fixed capacities;
nothing is allocated afterwards, and results are identical on every
platform. CI builds and tests it on x86-64 and 32-bit x86 (with SSE2
arithmetic), AArch64 and WebAssembly; x87's excess precision is refused
at build time.

## Status

1.0.3 (2026-10-19) holds the whole library: Penner's families and CSS
Easing Level 2, in four modes, with even tables and fitted CSS linear()
points, and rough easings from a seed; springs in closed form, as
easings too, decay, inertia landing on an end or within bounds, and the
rubber band; colors as CSS Color 4 interpolates them; angles, rotations,
integers, discrete values, scales in their logarithm, van Wijk and
Nuij's zoom, punch and shake; Web Animations' timing with playback rate
and repeat delay; staggers over a row or a grid; a context in one block
holding targets, and transitions advanced by an update that guards the
floating-point environment; keyframe paths, Catmull-Rom, monotone or
Bezier, at constant speed if asked, with their direction, and tracks of
every value kind over them; clips holding tracks on nested timelines
with labels and marks; players controlling clips by Web Animations'
procedures; events as counted records in a bounded queue; players
layered by replace and add with weights; reduced motion; a context grown
only when the host asks; clips' players run on the host's task system if
it gives one. From 1.0.0 the source interface only grows within 1.x and
results are the same bits on every platform (record mtwn-0072). 1.0.2,
the day before, had the same features and gave a number held at a bound
of the floats a velocity where a spring or decay moved past them or
players added past them. 1.0.1, earlier that day, had the same features
and refused an angle track whose Bezier controls its unwrap moved past
the floats' range, leaving its keyframes rewritten. 1.0.0, earlier that
day, the first stable release, had the same features, gave NaN for a
rubber band of an endless coefficient and no drag, ended an unbounded
number decay stopping past the floats' range infinite, and moved an
incremental number track whose step passed the floats' range to the
bound. 0.7.0, earlier that day, had the same features, named the misuse
count `mtwnContext_GetMisuseCount`, and promised none of this; 0.6.1,
earlier that day, had the same features, took inputs whose reserved
bytes were not 0, mixed values by the weighted sum, up to 2^-14 of the
result off far past ends one ulp apart, and was not tested on 32-bit
x86; 0.6.0, earlier that day, had the same features, took a call back in
from the host's code into the context it was serving, let a number eased
past the floats' range turn infinite and an extreme mix of equal ends
move; 0.5.1, on 2026-10-17, had neither scales, zooms, rough easings,
inertia's ends and bounds, a path's direction nor monotone segments, and
made every record even in a context that wanted none; 0.4.0, earlier
that day, had the same features, values that could jump to an
iteration's start for one time beside a repeat delay or a negative
rate's start, and records that could be lost or two ulps off past a
repeat delay; 0.3.1, the day before, had the same features, the
sharing's memory fixed and the task hooks costing up to 9% on one
worker; 0.3.0, earlier that day, had slower clip players and records
that could miss a crossing where a frame landed on a held time's edge;
0.2.1, on 2026-10-15, ran them on the calling thread only and several
times slower; 0.1.0 lacked staggers and reduced motion. The library has
124 public functions.

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
- `exact_oracle.py`: the timing model's iterations and the crossings'
  counts in exact rationals.
- `zoom_oracle.py`: scales and van Wijk and Nuij's zoom and pan in
  60-digit decimals, the path as the paper writes it.
- `rivals.mjs`: GSAP 3.15.0, tween.js 25.0.0, Motion's springs
  (motion-dom 14.2.0 with motion-utils 14.1.1) and CSS Color 4's
  `conversions.js` (w3c/csswg-drafts at af8661b), run under Node. Each
  is fetched by its pinned version and checked against its pinned hash
  before it runs. None is kept in this repository or linked into the
  library.

`test/fuzz_api.c` drives the public API from bytes: clips built node by
node, players under controls, transitions and updates, the same script
on a context alone, on a task system and counting every record, which
must agree bit for bit. `ctest` runs it on a fixed set of inputs; with
Clang, `-DMAUL_TWEEN_FUZZ=ON` builds it as a libFuzzer target, run as
`fuzz_api <corpus directory>` outside the tests. `test/fuzz_values.c`
does the same for the value functions (easings, springs, decay and the
rubber band, colors, rotations, punch and shake, paths, staggers) over
ordinary and wild parameters (huge, tiny, infinite, NaN): whatever a
function accepts, its results are finite, or for wild inputs at least
not NaN where the inputs had none. `fuzz_fit`, built from the same file,
takes its easings and fits each to `linear()` at a drawn tolerance, which
the other leaves out: a fit at its limit costs a thousand times the
rest, and would starve them. Its tolerances are mostly from 2^-2 to
2^-16, one in sixteen one the fit refuses, and one input in sixteen may
run one fit to its limit. Each replays its seeds in `ctest`
before its fixed inputs: one input for each edge a campaign reached,
in `test/generated/fuzz_seeds_<target>.h`. `tools/fuzz_seeds.py extract
<target> <dir>` starts a campaign from them, and `write <target>
<corpus>` records a campaign's corpus, merged with `-merge=1
-use_counters=0`. An input that found a defect is kept in
`test/fuzz_regressions/<target>/` as a `.bin` file, which `write` adds to
every corpus it records, so no merge drops it.

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
| curves: easings, sampling, springs, decay, rubber band | 25.0 kB | 24.4 kB | 20.8 kB | 27 kB |
| values: colors, angles, rotations, punch, shake, paths, tracks, timing, staggers | 56.5 kB | 47.3 kB | 48.7 kB | 60 kB |
| transitions: a context, targets and transitions | 59.8 kB | 51.6 kB | 47.6 kB | 63 kB |
| clips: clips, players, events | 143.9 kB | 114.1 kB | 119.5 kB | 152 kB |
| everything | 160.5 kB | 160.3 kB | 134.3 kB | 169 kB |

Each part counts what it calls in the others; about 12 kB of each is
the library's own elementary functions, the same under every C library. The budgets
apply to GCC 14's default build on x86-64 only; the Clang 20 column
(`--cc clang-20` on a Clang Release build) is kept beside them so that
a growth one compiler shows and the other does not is seen as the
compiler's, not the library's (records mtwn-0051 and mtwn-0055).
`--budget` fails past one, and growing past one takes a recorded
decision. A release follows the family's checklist with this library's own
steps, the sizes and the budgets among them:
[docs/releasing.md](docs/releasing.md).

## Design

The rules every Maul library follows are in `docs/conventions.md` and
`docs/adr/`; the records particular to this library are listed in
`docs/adr/mtwn.md`.

## License

MIT; see `LICENSE`.
