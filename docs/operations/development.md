# Development

How to build, test and change Maul Tween as a maintainer. Building and installing as a user is in [getting-started.md](../user/getting-started.md); what the checks run and their time budgets is in [checks.md](checks.md), and how a release is made in [release.md](release.md).

## Toolchain

- CMake 3.25 or newer and Ninja.
- GCC 14 and Clang 20 (`clang-20`, `clang-tidy-20`), with the sanitizer and libFuzzer runtimes (`libclang-rt-20-dev` on Debian and Ubuntu). GCC 13 is refused by `cmake/CompilerFlags.cmake`.
- `clang-format` at the version CI pins: `pip install clang-format==22.1.5`. Another version formats differently.
- Python 3 for the checks, the generators, the oracles and the documentation site (`pip install markdown==3.6` for the site only).
- Node 22 for the rival oracle only, and Emscripten for web builds.

## Build options for development

| Option | Effect |
|---|---|
| `MAUL_TWEEN_WERROR` | Warnings as errors, as CI builds. |
| `MAUL_TWEEN_SANITIZE` | AddressSanitizer and UBSan. |
| `MAUL_TWEEN_TSAN` | ThreadSanitizer. |
| `MAUL_TWEEN_COVERAGE` | gcov instrumentation. |
| `MAUL_TWEEN_FUZZ` | The libFuzzer targets, `fuzz_api`, `fuzz_values` and `fuzz_fit` (Clang only). |
| `MAUL_TWEEN_BUILD_TESTS`, `_BENCH`, `_SAMPLES` | On when the project is built on its own. |

`MTWN_ASSERT` is on in every build without `NDEBUG` (Debug, the sanitizer builds and the fuzz builds) and traps with `__builtin_trap`, so a debugger or a core dump stops at the failing check.

## Running one test

Every suite is `test/test_<name>.c`, one executable and one CTest test named and labeled `<name>`:

```sh
cmake --preset linux-clang-20-asan
cmake --build --preset linux-clang-20-asan
ctest --preset linux-clang-20-asan -L player
./out/linux-clang-20-asan/test_player
```

The presets in `CMakePresets.json` are the CI cells, each built in `out/<preset>`; the test presets leave out the tests labeled `nightly`, except `linux-clang-20-nightly`, which runs them all. A suite with nightly scenes takes their name as its argument (`./out/linux-clang-20-asan/test_sweep twins`). A suite prints one line per failed check, with what broke and its file and line. White-box suites include `src/` and are not built in shared builds. The fuzz targets' seeds and a fixed set of random inputs run in CTest as `fuzz_api`, `fuzz_values` and `fuzz_fit`.

## Generated files

Never edit these by hand; change the generator and run it.

| Files | Generator | Regenerate | CI checks |
|---|---|---|---|
| `src/generated/math_constants.h`, `math_constants_float.h` | `tools/gen_math_constants.py` (family file) | `python3 tools/gen_math_constants.py` | `--check` |
| `src/generated/color_matrices.h` | `tools/gen_color_matrices.py`, from CSS Color 4's `conversions.js` in 80-digit decimal | `python3 tools/gen_color_matrices.py` | `--check` |
| `test/generated/timing_cases.h` | `tools/oracle/timing_oracle.py`, Web Animations' steps in Python | `python3 tools/oracle/timing_oracle.py` | `--check` |
| `test/generated/exact_cases.h` | `tools/oracle/exact_oracle.py`, timing and crossing counts in exact rationals | `python3 tools/oracle/exact_oracle.py` | `--check` |
| `test/generated/zoom_cases.h` | `tools/oracle/zoom_oracle.py`, scales and the zoom in 60-digit decimals | `python3 tools/oracle/zoom_oracle.py` | `--check` |
| `test/generated/rival_easing.h`, `rival_spring.h`, `rival_color.h` | `tools/oracle/rivals.mjs`: GSAP 3.15.0, tween.js 25.0.0, Motion's springs and CSS Color 4's `conversions.js` under Node | `node tools/oracle/rivals.mjs` | by hand, `--check` |
| `test/generated/fuzz_seeds_<target>.h` | `tools/fuzz_seeds.py`, a minimized corpus | `python3 tools/fuzz_seeds.py write <target> <corpus>` | no |

`rivals.mjs` fetches each package by its pinned version and refuses it unless its SHA-512 is the pinned one; nothing it runs is kept in the repository or linked into the library. It needs the network, so CI does not run it.

## Code size

`tools/code_size.py` measures the code a program links, by the parts it uses: a program taking the address of every public function of a part's headers, linked against a Release static library with unused sections dropped, its `.text` less an empty program's.

```sh
cmake --preset linux-gcc-14 && cmake --build --preset linux-gcc-14
python3 tools/code_size.py out/linux-gcc-14/libmaul-tween.a --budget
python3 tools/code_size.py out/linux-gcc-14/libmaul-tween.a --map clips   # what links a part in
```

The parts are the curves (easing, sampling, springs), the values (interpolation, paths, timing), the transitions (a context, targets, transitions) and the clips (clips, players, events). Each has a budget in the script, about 6% over its size when the budget was set and moved since only by what a pull request states, for GCC 14 on x86-64; about 12 kB of each is the library's own elementary functions. `--budget` fails past one. A budget is raised only with a reason given in the pull request.

## Benchmarks

`out/<build>/maul-tween_bench` measures, on fixed inputs: 64,000 eased number transitions created and updated (with and without the changed-target report), one sample of a cubic bezier, a spring and Penner's easings, 1,000 clip players of four tracks in a repeating timeline, and one sample of each values case. It prints the best of five runs, and with an earlier run's output saved to a file as its argument, each result's ratio to it. Timings are never a CI gate. To measure a change, build the base and the change in Release with the same compiler, run each pinned to one core (`taskset -c 2`) on a quiet machine several times, and give both numbers in the pull request. `--count` runs one case for an instruction counter (`perf stat -e instructions`), which is steadier than time on a shared machine.

## Fuzzing

```sh
cmake -B out/fuzz -G Ninja -DCMAKE_C_COMPILER=clang-20 -DMAUL_TWEEN_FUZZ=ON
cmake --build out/fuzz
python3 tools/fuzz_seeds.py extract api out/corpus-api
./out/fuzz/fuzz_api -max_total_time=300 out/corpus-api
```

`fuzz_api` drives the public API from bytes (clips built node by node, players under controls, transitions and updates) on a context alone, on a task system and counting every record, which must agree bit for bit. `fuzz_values` drives the values layer with ordinary and wild parameters (huge, tiny, infinite, NaN): whatever a function accepts, its results are finite, or at least not NaN where the inputs had none. `fuzz_fit` fits easings to `linear()` at drawn tolerances, kept apart because a fit at its limit costs a thousand times the rest.

Always pass `-max_total_time`, and stop what you started. A crash becomes a test: the minimized input (`-minimize_crash=1`) goes into `test/fuzz_regressions/<target>/` as a `.bin` file. `fuzz_seeds.py write` adds those files to every corpus it records, and so to the seeds header the target's CTest replay runs, so no merge drops them.

## Shared family files

The files listed in [AGENTS.md](../../AGENTS.md) are synced from the organization's `.github` repository. A change to one is made there first, then taken here with `python3 /home/rawframe-project/.github/maul/family_sync.py --write maul-tween`; `--check maul-tween` reports any difference.
