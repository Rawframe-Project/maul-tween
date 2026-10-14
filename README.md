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

Not released. The design is decided; the curves come first, and
the easings are in: Penner's families and CSS Easing Level 2, in four
modes, with even tables and fitted CSS linear() points; springs in
closed form, as easings too, decay and the rubber band; colors as CSS
Color 4 interpolates them; angles, rotations, integers, discrete
values, punch and shake; Web Animations' timing with playback rate
and repeat delay; a context in one block holding targets. The library
has 59 public functions.

## Building

Requirements: CMake 3.25 and GCC 14 or Clang 19 or newer; on Windows,
`clang-cl` (the Visual Studio component "C++ Clang tools for Windows").

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Design

The rules every Maul library follows are in `docs/conventions.md` and
`docs/adr/`; the records particular to this library are listed in
`docs/adr/mtwn.md`.

## License

MIT; see `LICENSE`.
