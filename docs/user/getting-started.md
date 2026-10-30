# Getting started

Maul Tween is a C library. It builds with CMake, installs as a CMake package and a pkg-config module, and needs nothing but a C compiler and the C standard library.

## Requirements

- CMake 3.25 or newer.
- GCC 14 or Clang 19 or newer. On Windows, `clang-cl`: the Visual Studio component "C++ Clang tools for Windows". MSVC's own `cl` compiles the public headers but not the library.
- x86-64, 32-bit x86 with SSE2 arithmetic, AArch64 or WebAssembly (Emscripten). A build whose float arithmetic does not round to float, such as x87, is refused at compile time, since its results would differ.

The public headers are plain C17 and C++17, so programs that include them need neither a C23 compiler nor C at all.

## Build and install

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
cmake --install build --prefix /path/to/prefix
```

Then, from CMake:

```cmake
find_package(maul-tween REQUIRED CONFIG)
target_link_libraries(app PRIVATE maul-tween::maul-tween)
```

or through pkg-config, `pkg-config --cflags --libs maul-tween`. A project can also add the source tree with `add_subdirectory`; tests, samples and benchmarks are then off by default. `MAUL_TWEEN_BUILD_SHARED=ON` builds a shared library instead of a static one.

[`samples/minimal/`](../../samples/minimal) is a complete project that uses only the installed package. The other samples build with the library (`-DMAUL_TWEEN_BUILD_SAMPLES=OFF` turns them off), and each returns 0 when what it shows holds:

- `sample_button`: a button's hover and press states as transitions interrupted midway, its scale on a spring, its lift and fill eased.
- `sample_sequence`: a card's entrance as a clip, with a mark where a sound plays, read as events, then reversed.
- `sample_layers`: an endless idle clip with a hit clip added over it, and a wheel turned on by an incremental track.
- `sample_grid`: a grid of tiles staggered from its center, played with motion and under reduced motion.

## The model

- **Host time.** Nothing runs by itself. The host calls `mtwnContext_Update` with its own clock in nanoseconds, never going back, and every transition, clip and event follows that time. A paused game, a slowed one or a replay is the host's choice of clock.
- **Two layers.** Easings, springs, decays, the interpolation of every value kind, paths, timing and staggers are plain functions on values, with no context and no allocation (`easing.h`, `sample.h`, `spring.h`, `interpolate.h`, `path.h`, `timing.h`), safe from any thread. The context (`context.h`, `target.h`, `transition.h`, `clip.h`, `player.h`, `event.h`) holds targets, transitions, clips and players and is used by one thread at a time.
- **Memory.** A context is one block from the host's allocator, sized by its def; each clip is one more block, counted against the context's clip storage. Nothing else is allocated, an update never allocates, and nothing grows unless the host calls `mtwnContext_Grow`.
- **Same results everywhere.** The library computes in its own floating-point environment (round to nearest, subnormals kept), whatever the host's is, with its own exponential, logarithm and trigonometry, so the same calls at the same times give the same bits on every platform and compiler.

## Results, defs and ids

Every fallible function returns an `mtwnResult`: `mtwn_success` (0), or a negative error. `mtwnGetResultName` names any result for logs, and the functions are marked `[[nodiscard]]` where the compiler supports it.

| Result | Value | Meaning |
|---|---|---|
| `mtwn_errorInvalid` | -1 | An argument a correct program never passes: a null pointer, a value outside its documented range, a def not made by its default function. |
| `mtwn_errorCapacity` | -2 | Storage is too small: a capacity the host set is full (targets, players, clips, bindings, a clip's nodes, keyframes or `linear()` points, a transition's points), a caller's buffer, or the allocator. More room would succeed. |
| `mtwn_errorStale` | -3 | An id names no live object: one destroyed since it was handed out, one never handed out, or the null id. |
| `mtwn_errorState` | -6 | The call is not allowed in the object's state, which another call changes: a clip not sealed or already sealed, a player control its rate or time does not allow. |
| `mtwn_errorVersion` | -7 | A context def was built against headers of another ABI version. |
| `mtwn_errorLimit` | -9 | An argument passes a bound published as a named constant: `MTWN_MAX_COUNT`, `MTWN_MAX_CLIP_STORAGE`, `MTWN_MAX_CLIP_DEPTH`, `MTWN_MAX_FIT_POINTS`, `MTWN_MIN_RATE`, `MTWN_RATE_LIMIT`, `MTWN_SEEK_LIMIT`. More room would not help. |
| `mtwn_errorBusy` | -10 | The object is in use, and the same call succeeds once that ends: a call back in from the host's code the context runs, a clip a player plays until the player is freed. |

The values are the Maul family's, the same in every library, so a log of a raw value reads one way.

A refused call changes nothing. A context counts the calls refused for an invalid argument, one past a named bound, a stale target or clip id, a call the object's state does not allow (`mtwn_errorState`) or a call back in (`mtwnContext_GetMisuse`), so a host can assert the count stays 0. A refusal that depends on what the library does is not misuse: a full capacity, a stale player id (a player may free itself at its end), and a clip a player still holds.

Contexts, clips, tracks, timelines, transitions and players are made from defs. Each def comes from its `mtwnDefault...Def` function, which is inline and sets the def's cookie; a def without it is refused with `mtwn_errorInvalid`. The context def's default also stamps the ABI version of the headers the program includes, and a library of another one refuses it with `mtwn_errorVersion`: before 1.0 that is any other release. Every other def is used under a context that passed, so it carries only its cookie. Always start from the default and set what differs.

Targets, players and clips are named by ids of an index and a generation. An id of something destroyed is stale: a call given it returns `mtwn_errorStale` and never touches whatever took its slot. The zero id is null; it and an id never handed out get `mtwn_errorStale` too.

A call on a context clears its outputs when it fails (a null id, a zero value, a count of 0), so a program that ignores a status reads nothing stale. A call without a context, such as an easing maker, leaves its outputs as they were.

## Calls back in

The host's code that the library runs (a host easing, the allocator, the task system's functions) must not change the context it serves. A call that would is refused with `mtwn_errorBusy` and counted as misuse, and `mtwnDestroyContext` called that way returns without destroying. A call that only reads, taking the context `const` (a target's value, a player's state, a clip's nodes), is not refused: it reads the context as the call in progress has left it so far. `mtwnTarget_GetState` is the exception, refused like a change, since a target's velocity may sample its easing and so run the host's code again. One exception: while an update runs clip players on the host's task system, a read of targets or players from the host's code is refused with `mtwn_errorBusy`, since another thread may be writing them. A context is used by one thread at a time, its reads too.

## Compatibility

Before 1.0.0 any release may change the API and the ABI, and the changelog says how. From 1.0.0 Maul Tween ships minor and patch releases of 1 only, and keeps this promise:

- **What it covers.** Every function, type, enum value, macro and struct field declared in the headers under `include/maul-tween/`, by name, signature and meaning. A program that builds against 1.y builds against any later 1.z and means the same. Functions, types, values and fields are only added, a field only at the end of its struct.
- **What it leaves out.** The fields of the structs a maker fills, which their headers call the library's: `mtwnEasing`, `mtwnSpring`, `mtwnColorMix`, `mtwnRotationMix`, `mtwnScaleMix`, `mtwnZoomMix`, `mtwnPath` and `mtwnTrack`. A host makes, copies and passes them but reads none of their fields, so a minor release may change them. The reasons `mtwnContext_GetSerialAdvanceCount`'s header gives for an advance left serial may change too; what the count counts does not.
- **The binary interface** holds within a minor version. `MTWN_ABI_VERSION` is the major and minor version, and so is a shared library's SOVERSION. A context def stamped by another version's headers is refused with `mtwn_errorVersion`, so a program is rebuilt for a new minor release.
- **Results** are the same bits on every platform within a release. A release that changes any bit of a value, velocity or event record says so in its changelog, with who sees it.
- **Stored data:** Maul Tween reads and writes no bytes of its own, so it has no format to keep.

## Where to go next

- [Easing](easing.md): curves from progress to eased progress, CSS's and Penner's, sampling and fitting `linear()`.
- [Springs, decay and signals](physics.md): springs, flings, bounds and snapping, punch and shake.
- [Values and paths](values.md): colors, angles, rotations, scales, zoom, keyframe paths and tracks.
- [Transitions](transitions.md): a context, targets, transitions and the update.
- [Clips and timelines](clips.md): tracks on nested timelines, positions, labels and marks, staggers.
- [Players, events and layers](players.md): playing clips, event records, composing players over each other.
- [Threads and capacity](threads.md): the host's task system, players in step, growing a context.
