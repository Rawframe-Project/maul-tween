# Maul Tween

Maul Tween is a C library of easing, springs, tweens and timelines driven by the host's clock: curves and value interpolation with no owner, and a context of targets that transitions and clip players write. It is part of the Maul family; the family rules are in [docs/internals/maul-conventions.md](docs/internals/maul-conventions.md), and [docs/internals/overview.md](docs/internals/overview.md) explains how the parts fit.

It never compromises on:

1. **Same bits everywhere.** The same calls at the same host times give the same values, velocities and event records on every platform, compiler and host floating-point environment.
2. **Host time and memory.** Nothing runs by itself or reads a clock; a context is one block from the host's allocator, an update never allocates, and nothing grows unless the host asks.
3. **Hostile input.** Every argument is validated and refused with a status in every build; a refused call changes nothing, and a call on a context counts the refusal as misuse.
4. **Exact events.** Every crossing between two updates is counted, however far time moved, and none is dropped silently.
5. **Small to link.** A program of transitions links no clip code, and each part stays within its code budget in `tools/code_size.py`.

## Commands

```sh
tools/check.sh changed                           # before every push: rules, formatting, the tests the changes reach (under 2 min)
tools/check.sh pr                                # what the pull request check runs (under 10 min)
tools/check.sh full                              # every Linux preset, the nightly scenes, longer fuzzing (under 20 min)
cmake --preset linux-clang-20-asan && cmake --build --preset linux-clang-20-asan
ctest --preset linux-clang-20-asan -L player     # one suite, by label, assertions on
```

The server's default `gcc` is 13, which the family refuses; use `gcc-14` or `clang-20`. Run the `pr` and `full` tiers, full builds and fuzzing through the workspace's heavy-job wrapper on the shared machine. Each suite is `test/test_<label>.c`, labeled with its name; the large random scenes also carry `nightly` and run only in the `linux-clang-20-nightly` preset ([checks.md](docs/operations/checks.md)).

## Ways to hurt yourself

- **Killing by pattern.** Never `pkill -f`, `pgrep | kill` or `kill` a PID found by matching a name or path: other sessions build and fuzz on the same machine with the same binary names. Kill only a PID you started.
- **Endless fuzzing.** A libFuzzer target runs until it finds something. Always pass `-max_total_time`, and stop what you started.
- **Editing generated files.** `src/generated/` and `test/generated/` are written by the generators and oracles in `tools/` ([development.md](docs/operations/development.md#generated-files)). Change the generator and run it.
- **Editing shared family files.** These are synced from the canonical copies in the organization's `.github` repository and must not differ: `docs/internals/maul-conventions.md`; `.clang-format`, `.clang-tidy`, `.editorconfig`, `.gitattributes`, `.gitignore`; `cmake/CompilerFlags.cmake`, `cmake/Config.cmake.in`, `cmake/Install.cmake`, `cmake/Library.cmake`, `cmake/ReadVersion.cmake`, `cmake/Testing.cmake`, `cmake/pkg-config.pc.in`, `cmake/HeaderCheck/CMakeLists.txt`; `bench/CMakeLists.txt`; `test/test_harness.h`, `test/test_math.c`, `test/test_math_float.c`; `src/elementary.c`, `src/elementary.h`, `src/elementary_float.c`, `src/elementary_float.h`, `src/generated/math_constants.h`, `src/generated/math_constants_float.h`; and `tools/build_docs.py`, `check_docs.py`, `check_guide.py`, `check_lengths.py`, `check_modules.py`, `check_source.py`, `gen_api.py`, `gen_math_constants.py`. Change the canonical copy first.

## Invariants

- **Floating-point environment:** every public function that computes runs inside the switch to the library's environment (`src/fpenv.c`). Only the `<math.h>` functions in `tools/libm-allowed.txt` may be called; use the library's own elementary functions for the rest.
- **Determinism:** `test/test_determinism.c` holds one hash of a scenario of every part: every value kind, clips and events, and samples of paths, scales, the zoom, punch, shake, springs, easings, fits, rough easings and decays. A change that moves any bit of a result changes it, and the pull request says why.
- **Generated sources:** `src/generated/` (math constants, color matrices) and `test/generated/` (oracle tables, fuzz seeds), each with its generator in `tools/`. CI checks the Python generators' output with `--check`.
- **Modules:** `tools/modules.txt` lists which module may include which. The clips' players reach the update only through the function table their first player installs, so `context`, `target`, `transition` and `update` never include clip code.
- **Defs:** every def comes from its inline default, which sets its cookie; only `mtwnContextDef` also carries `MTWN_ABI_VERSION`, checked after the cookie. Reserved fields are 0 in and out. A new field changes the def's layout and so the ABI.
- **Outputs on failure:** a call on a context clears its outputs before it checks anything; a call without one leaves them as they were. Every out-parameter's header says which.
- **Busy context:** every function taking the context non-const enters and leaves its busy state, so host code calling back in is refused.
- **Assertions:** `MTWN_ASSERT` (`src/invariant.h`) checks internal invariants only. It traps in builds without `NDEBUG` (Debug, sanitizers, fuzzing) and compiles out of Release, whose update loop must not pay for it. Never assert on caller input; validate and return a status.
- **Limits:** `MTWN_MAX_COUNT`, `MTWN_MAX_CLIP_STORAGE`, `MTWN_MAX_FIT_POINTS`, `MTWN_RATE_LIMIT` and `MTWN_MAX_CLIP_DEPTH` are part of the API.

## Testing

Follow the [repository standard's Tests section](https://github.com/Rawframe-Project/.github/blob/main/docs/repository-standard.md#tests); in this repository:

- Test through the public headers. A white-box suite includes `src/` only for an internal function no public call reaches exactly (the elementary functions, the mix, the crossing count, the pool); never test internal state or sizes. Switches and counts only a test needs (every record counted, what the sharing kept) go through `src/test_hooks.h`, a library of its own that is never exported or installed, so a test reads no library struct.
- Exactness is checked against an independent reference: the long double formulas, the oracles in `tools/oracle/`, or the rivals' committed outputs. A bound comes from the definitions, not from what the code happens to give.
- Implementations that must agree (serial and on tasks, sharing on and off, the sweep's shortcuts on and off) are checked by running both on twin contexts and comparing bits.
- Every C snippet of `docs/user/` appears unchanged in `test/test_guide.c`; `tools/check_guide.py` enforces it.
- A fuzz finding becomes a regression input in `test/fuzz_regressions/<target>/` and, where it shows a rule, a test in its module's suite.

## Documentation

Follow the [repository standard's Documentation section](https://github.com/Rawframe-Project/.github/blob/main/docs/repository-standard.md#documentation). A change in what a caller must do or can rely on updates the page of `docs/user/` for that part and, if it is in a snippet, the matching code in `test/test_guide.c`. The API reference is generated from the headers into the documentation site; never commit it.

## Pull requests

Titles are conventional commits. Scopes: `easing`, `spring`, `value`, `path`, `timing`, `context`, `transition`, `clip`, `player`, `event`, `math`, `bench`, `build`, `family`. Example: `fix(player): keep the current time when a paused player's rate changes`.

A change of results (any bit of a value, velocity or record) says so in its pull request and names who sees it, since hosts may replay or compare frames across versions.
