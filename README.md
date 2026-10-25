# Maul Tween

Easing, springs, tweens and timelines in C, driven by the host's clock, with no dependencies and the same results on every platform.

[![CI](https://github.com/Rawframe-Project/maul-tween/actions/workflows/ci.yml/badge.svg)](https://github.com/Rawframe-Project/maul-tween/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/github/license/Rawframe-Project/maul-tween)](LICENSE)
[![Release](https://img.shields.io/github/v/release/Rawframe-Project/maul-tween)](https://github.com/Rawframe-Project/maul-tween/releases)
[![OpenSSF Scorecard](https://api.scorecard.dev/projects/github.com/Rawframe-Project/maul-tween/badge)](https://scorecard.dev/viewer/?uri=github.com/Rawframe-Project/maul-tween)

Interfaces move: a button lifts on hover and settles back when the pointer leaves halfway, a list scrolls on after a fling and bounces at its end, a card's parts slide in one after another, a character breathes while a hit plays over it. Each needs a curve, a value that knows how fast it is moving, and a clock the program controls.

Maul Tween gives games, engines and applications:

- **Easing:** Penner's curves landing exactly on their ends, CSS Easing Level 2 entire (`cubic-bezier`, `steps`, `linear()` with points), rough curves from a seed, and `linear()` points fitted to any curve for a browser.
- **Physics:** damped springs in closed form that keep their velocity when retargeted, set by stiffness or by duration and bounce; decay for flings, landing on a chosen end or bouncing back from bounds; the rubber band; punch and seeded shake.
- **Values:** numbers and vectors, colors interpolated in Oklab or another space as CSS Color 4 does, angles, rotations, integers, discrete values, scales moved by ratios, van Wijk and Nuij's smooth zoom and pan, and keyframe paths at constant speed with their direction.
- **Transitions:** one call moves a value to a new one, from where it is and as fast as it was going.
- **Timelines:** Web Animations' timing on every node of nested timelines, with positions, labels and marks; players that play, pause, reverse, seek and change rate; layers composed by replace and add with weights; events as counted records the host reads; staggers; reduced motion.

The values layer needs no setup and allocates nothing. Everything else lives in one context the host creates with its own allocator and fixed capacities; nothing is allocated after that, an update allocates nothing, and the library starts no thread but can run its players on the host's task system.

## Quick start

It builds with CMake 3.25 or newer and GCC 14 or newer, Clang 19 or newer, or Apple Clang. On Linux and macOS:

```sh
git clone https://github.com/Rawframe-Project/maul-tween.git
cd maul-tween
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
./build/sample_button
```

Ubuntu 24.04's default compiler is GCC 13, which the build refuses ("GCC 14 or newer is required"). Install GCC 14, with Git and CMake on a system that lacks them, and configure again in a fresh build directory, since `build/` keeps the compiler it first found; then go on from `cmake --build build`:

```sh
sudo apt install git cmake gcc-14
rm -rf build && CC=gcc-14 cmake -B build -DCMAKE_BUILD_TYPE=Release
```

On Windows the library builds with `clang-cl`, and on the web with Emscripten; [Getting started](docs/user/getting-started.md) lists what each needs.

Install it with `cmake --install build --prefix <prefix>` and use it from CMake:

```cmake
find_package(maul-tween REQUIRED CONFIG)
target_link_libraries(app PRIVATE maul-tween::maul-tween)
```

or through pkg-config as `maul-tween`. [Getting started](docs/user/getting-started.md) covers the model the API follows.

## Platforms

Linux with GCC 14 or Clang 19 and newer on x86-64, 32-bit x86 (with SSE2 arithmetic) and AArch64, macOS with Apple Clang, Windows with `clang-cl`, and WebAssembly with Emscripten; CI builds and tests each. The public headers are C17 and C++17, so any C or C++ program can include them.

## Documentation

- [The documentation site](https://rawframe-project.github.io/maul-tween/), with the API reference generated from the headers.
- [User documentation](docs/README.md): one page per part of the library.
- [How it works](docs/internals/overview.md) and [how to develop it](docs/operations/development.md).
- [Changelog](CHANGELOG.md).

Maul Tween belongs to the Maul family of standalone C libraries, which share one set of conventions.

## Contributing

Issues are welcome. Pull requests are accepted for issues a maintainer has approved; see [CONTRIBUTING.md](https://github.com/Rawframe-Project/.github/blob/main/CONTRIBUTING.md). The project is developed with AI coding agents, directed and reviewed by its maintainers; outside contributions follow the [AI policy](https://github.com/Rawframe-Project/.github/blob/main/AI_POLICY.md).

## License

MIT; see [LICENSE](LICENSE).
