# Checks

What verifies a change, locally and in CI, and the time each may take.

## Locally

`tools/check.sh` runs the checks in three tiers; each includes the one before it. Its builds are the CI cells' presets from `CMakePresets.json`, in `out/<preset>`; the other trees go to `out/check-*`, each with its log beside it.

| Tier | Runs | Budget |
|---|---|---|
| `changed` | The source, module, length, API documentation and snippet rules, the organization's documentation check, `clang-format` on the changed C files, and the `linux-clang-20-asan` preset (assertions on) with the CTest labels the changed files reach. | under 2 minutes |
| `pr` | Every rule on every file, the generators and oracles with `--check`, clang-tidy, the `linux-gcc-14` preset, the code size budgets, the `linux-clang-20-asan` preset, and 10 seconds of fuzzing per target. | under 10 minutes |
| `full` | Also 60 seconds per fuzz target and every other Linux preset: ThreadSanitizer, the shared library with the package test, 32-bit x86, Clang Release, and `linux-clang-20-nightly`, which runs the nightly scenes too. | under 20 minutes |

Run `changed` before every push. On the shared development machine, run `pr` and `full` through the workspace's heavy-job wrapper.

## In CI

`.github/workflows/ci.yml` runs on every pull request, on every push to `main` and every night. The Linux jobs run in the organization's CI image, pinned by digest, and install nothing. The ruleset requires only its `required` job, which passes when every other job passed or was skipped, and the title check.

| Job | What it guards |
|---|---|
| `changes` | Whether a pull request changes anything beyond documentation; when it does not, the build jobs are skipped. |
| `docs` | The organization's documentation check: allowed files, the em dash, record numbers, page sizes, links. |
| `zizmor` | The workflows: injection, persisted credentials, permissions, unpinned actions. |
| `checks` | `clang-format`, the family's source rules, the module graph, lengths, API documentation, documentation snippets, that the documentation site builds, that the Python generators and oracles reproduce `src/generated/` and `test/generated/`, clang-tidy, and each part's code size against its budget. It runs on documentation changes too. |
| `build` | One call of the organization's `cpp.yml` per preset: GCC 14 (which builds every part: the library, every test, the samples and the bench) and Clang 20 in Release, Clang with AddressSanitizer and UBSan (each test repeated up to three times on a pull request, so a flaky one fails before it merges), Clang with ThreadSanitizer, the shared library with the package test, 32-bit x86 with SSE2, AArch64 on an Arm runner, macOS and `clang-cl` on Windows. All build with warnings as errors and leave out the tests labeled `nightly`. ccache is restored on every run and saved only from `main`. |
| `nightly` | On `main` and at night only: the `linux-clang-20-nightly` preset, every test with the large random scenes. |
| `msvc-headers` | Every public header compiled by MSVC's `cl` as C17 and C++17 with `/W4 /WX`. |
| `fuzz` | Each fuzz target from its committed seeds and the corpus the Actions cache carries: 30 seconds on a pull request, 2 minutes on `main`, 10 minutes at night. On `main` and at night the corpus is minimized with the fuzzer's merge and saved. A finding uploads the logs and the crashing input. |
| `web` | The library and its tests under Emscripten and Node, and the size of the static library. The image has no Emscripten, so this job uses the runner and the emsdk action. |

`pr-title.yml` checks that the pull request title is a conventional commit with a scope from `AGENTS.md`, and `pr-gate.yml` closes a pull request from outside the organization unless it closes an issue labeled `accepted`. `codeql.yml` runs CodeQL on every pull request and on `main`, building with GCC 14 (an advanced setup, since the default one builds with the runner's GCC 13). `pages.yml` publishes the documentation site from `main`. `scorecard.yml` grades the repository's security practices weekly for the README badge, and Dependabot proposes updates of the pinned actions once a week in one pull request.

## Tests

Each suite in `test/` is one executable, registered with CTest under its name and labeled with it; the fuzz targets' replays are `fuzz_api`, `fuzz_values` and `fuzz_fit`, the samples are labeled `samples`, the package test `package`, and `exports`, in Linux shared builds, checks that the library exports the public functions and nothing else. No test reports itself skipped; a test preset that finds no test fails, and so does a CI cell where a test is skipped or not run.

The large random scenes carry the label `nightly` as well as their suite's, and the pull request presets leave them out: `clip_tasks_twins` and `sweep_twins` play random scripts on twin contexts (serial and on tasks, shortcuts on and off), `clip_step_crowd` runs the bench's crowd of players in step with the sharing on and off, and `crossing_oracle_0` to `_2` take every one of the crossing oracle's 3,000 trials. The pull request check runs each suite's fixed cases and a thirtieth of the oracle's trials, and the fuzz job compares the serial, tasked and record-counting contexts on every input.

## Budgets

Over a budget, tests are pruned or merged before anything else is added. The pull request check takes about 5 minutes of wall time, most of it the slowest build cell and the fuzz job; the run on `main` adds the nightly cell.
