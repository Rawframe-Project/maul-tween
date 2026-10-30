#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# The checks, in three tiers. Each tier includes the one before it.
#
#   tools/check.sh changed   the files changed against origin/main: the
#                            source rules, formatting, the documentation
#                            checks, clang-tidy with the assertions compiled
#                            out and live on the changed sources, the
#                            linux-clang-20-asan preset (assertions on), and
#                            the test labels the changes reach. Under two
#                            minutes; run it before every push.
#   tools/check.sh pr        what the pull request check runs on this
#                            machine: every rule, the generators and oracles,
#                            clang-tidy with the assertions compiled out and
#                            live, the code size budgets, the
#                            linux-gcc-14 and linux-clang-20-asan presets, and
#                            a short fuzzing pass.
#   tools/check.sh full      also every other Linux preset (ThreadSanitizer,
#                            shared with the package test, 32-bit x86, Clang
#                            Release), the nightly scenes and longer fuzzing.
#
# The builds are the CI cells' CMake presets (CMakePresets.json), in
# out/<preset>; the other trees go to out/check-*. Needs gcc-14 (with its
# 32-bit libraries for full), clang-20, clang-tidy-20, ninja and the
# clang-format version CI pins: the organization's CI image has them all.
# The org documentation check runs when the organization's .github checkout
# is at ORG_DEFAULTS (default: ../../.github from the repository).
set -euo pipefail

tier=${1:-changed}
case "$tier" in
changed | pr | full) ;;
*)
    echo "usage: tools/check.sh [changed|pr|full]" >&2
    exit 2
    ;;
esac

cd "$(dirname "$0")/.."
root=$PWD
out=$root/out
mkdir -p "$out"
org=${ORG_DEFAULTS:-$root/../../.github}
jobs=${JOBS:-8}

step() { printf '\n== %s\n' "$*"; }

# A tree outside the presets (clang-tidy, fuzzing, code size).
configure() { # name, then cmake arguments
    local name=check-$1
    shift
    cmake -S "$root" -B "$out/$name" -G Ninja -DMAUL_TWEEN_WERROR=ON "$@" >"$out/$name.log"
}

# A CI cell: its configure, build and test presets. More ctest arguments
# (a label filter) follow the preset's name.
preset() {
    local name=$1
    shift
    step "preset $name"
    cmake --preset "$name" >"$out/$name.log"
    python3 tools/check_test_hygiene.py --ctest "out/$name"
    cmake --build --preset "$name" -j "$jobs" >>"$out/$name.log"
    ctest --preset "$name" -j "$jobs" "$@"
}

# The ctest labels a changed path reaches; "all" when it reaches every
# suite (shared code, the context, the build).
labels_for() {
    case "$1" in
    tools/check_exports.py) echo exports ;;
    *.md | LICENSE | .github/* | tools/check_*.py | tools/build_docs.py | tools/gen_api.py | tools/code_size.py) ;;
    docs/user/* | test/test_guide.c) echo guide ;;
    src/base.c | include/maul-tween/base.h | test/test_base.c) echo base ;;
    src/easing.c | src/css_easing.* | src/easing_internal.h | include/maul-tween/easing.h) echo "easing easing_css rival_easing sample guide fuzz_values fuzz_fit" ;;
    src/rough.c) echo "rough guide" ;;
    src/sample.c | include/maul-tween/sample.h) echo "sample fuzz_fit" ;;
    src/spring.c | src/spring_*.h | include/maul-tween/spring.h) echo "spring rival_spring decay inertia coast guide fuzz_values" ;;
    src/decay.c | src/inertia.c | src/coast.c) echo "decay inertia coast transition guide fuzz_values" ;;
    src/color.c) echo "color rival_color compose track transition fuzz_values" ;;
    src/rotation.c) echo "rotation track transition compose incremental fuzz_values" ;;
    src/scale.c | src/zoom.c) echo "scale track transition compose guide fuzz_values" ;;
    src/signal.c) echo "signal guide fuzz_values" ;;
    src/path.c | src/path_*.c | src/path_*.h | include/maul-tween/path.h) echo "path path_length direction monotone track guide fuzz_values" ;;
    src/timing.c | src/timing_internal.h | include/maul-tween/timing.h) echo "timing exact invariants crossing" ;;
    src/stagger.c | src/stagger_internal.h) echo "stagger invariants guide" ;;
    src/mix_internal.h) echo "mix color rotation signal" ;;
    test/test_*.c) basename "$1" .c | sed 's/^test_//' ;;
    test/fuzz_api.c | test/generated/fuzz_seeds_api.h) echo fuzz_api ;;
    test/fuzz_values.c | test/generated/fuzz_seeds_values.h | test/generated/fuzz_seeds_fit.h) echo "fuzz_values fuzz_fit" ;;
    test/generated/timing_cases.h) echo timing ;;
    test/generated/exact_cases.h) echo exact ;;
    test/generated/zoom_cases.h) echo scale ;;
    test/generated/cycle_cases.h) echo crossing ;;
    test/generated/rival_*.h) echo "rival_easing rival_spring rival_color" ;;
    tools/oracle/* | test/fuzz_regressions/* | bench/* | tools/fuzz_seeds.py) ;;
    samples/*) echo samples ;;
    *) echo all ;;
    esac
}

source_checks() {
    step "source, module, length, documentation and snippet rules"
    python3 tools/check_source.py
    python3 tools/check_modules.py
    python3 tools/check_float_env.py
    python3 tools/check_lengths.py
    python3 tools/check_docs.py
    python3 tools/check_guide.py
    python3 tools/check_test_hygiene.py --self-test
    python3 tools/check_test_hygiene.py
    if [ -f "$org/scripts/docs_check.py" ]; then
        python3 "$org/scripts/docs_check.py"
    else
        echo "skipped the organization's documentation check: no checkout at $org"
    fi
}

fuzz() { # seconds per target
    step "fuzzing, $1 seconds per target"
    configure fuzz -DCMAKE_C_COMPILER=clang-20 -DMAUL_TWEEN_FUZZ=ON -DMAUL_TWEEN_BUILD_TESTS=OFF \
        -DMAUL_TWEEN_BUILD_BENCH=OFF -DMAUL_TWEEN_BUILD_SAMPLES=OFF
    cmake --build "$out/check-fuzz" -j "$jobs" >>"$out/check-fuzz.log"
    for target in api values fit; do
        corpus=$out/corpus/$target
        mkdir -p "$corpus"
        python3 tools/fuzz_seeds.py extract "$target" "$corpus" >/dev/null
        "$out/check-fuzz/fuzz_$target" -max_total_time="$1" -max_len=1024 "$corpus" \
            >"$out/fuzz_$target.log" 2>&1 ||
            {
                tail -30 "$out/fuzz_$target.log"
                exit 1
            }
        echo "fuzz_$target: no finding"
    done
}

# clang-tidy on the files given, as CI's checks job runs it: on the
# Release preset, where the assertions compile out, and with them live.
# Each hides findings the other shows.
tidy() { # files
    local build
    for build in Release Debug; do
        step "clang-tidy, $build, on $# files"
        cmake --preset linux-clang-20 -B "$out/check-tidy-$build" -DCMAKE_BUILD_TYPE=$build \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DMAUL_TWEEN_BUILD_TESTS=OFF \
            -DMAUL_TWEEN_BUILD_BENCH=OFF -DMAUL_TWEEN_BUILD_SAMPLES=OFF >"$out/check-tidy-$build.log"
        printf '%s\n' "$@" | xargs -P "$jobs" -n 1 clang-tidy-20 -p "$out/check-tidy-$build" --quiet \
            --warnings-as-errors='*' 2>/dev/null
    done
}

if [ "$tier" = changed ]; then
    base=$(git merge-base HEAD origin/main 2>/dev/null || echo HEAD)
    mapfile -t changed < <({
        git diff --name-only "$base"
        git ls-files --others --exclude-standard
    } | sort -u)
    if [ ${#changed[@]} -eq 0 ]; then
        echo "nothing changed against origin/main"
        exit 0
    fi
    source_checks
    mapfile -t c_files < <(printf '%s\n' "${changed[@]}" | grep -E '\.[ch]$' || true)
    present=()
    for f in "${c_files[@]+"${c_files[@]}"}"; do [ -f "$f" ] && present+=("$f"); done
    if [ ${#present[@]} -gt 0 ]; then
        step "clang-format on ${#present[@]} changed files"
        clang-format --dry-run -Werror "${present[@]}"
    fi
    # The changed files of the pr tier's set, new ones too; none runs none.
    mapfile -t sources < <(git ls-files --cached --others --exclude-standard 'src/*.c' |
        grep -Fx -f <(printf '%s\n' "${present[@]+"${present[@]}"}") || true)
    if [ ${#sources[@]} -gt 0 ]; then
        tidy "${sources[@]}"
    fi
    labels=""
    for f in "${changed[@]}"; do labels="$labels $(labels_for "$f")"; done
    if [ -z "${labels// /}" ]; then
        echo "no test reached by the changes"
        exit 0
    fi
    if [[ " $labels " == *" all "* ]]; then
        preset linux-clang-20-asan
    else
        regex=$(printf '%s\n' $labels | sort -u | paste -sd '|')
        echo "tests labeled $regex"
        preset linux-clang-20-asan -L "^($regex)\$"
    fi
    exit 0
fi

source_checks

step "clang-format on every file"
git ls-files '*.c' '*.h' | xargs clang-format --dry-run -Werror

step "generated sources and oracle tables match their generators"
python3 tools/gen_math_constants.py --check
python3 tools/gen_color_matrices.py --check
python3 tools/oracle/timing_oracle.py --check
python3 tools/oracle/exact_oracle.py --check
python3 tools/oracle/zoom_oracle.py --check
python3 tools/oracle/cycle_oracle.py --check

mapfile -t sources < <(git ls-files 'src/*.c')
tidy "${sources[@]}"

preset linux-gcc-14

step "code size within the budgets"
configure size -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_BUILD_TYPE=Release -DMAUL_TWEEN_BUILD_TESTS=OFF \
    -DMAUL_TWEEN_BUILD_BENCH=OFF -DMAUL_TWEEN_BUILD_SAMPLES=OFF
cmake --build "$out/check-size" -j "$jobs" >>"$out/check-size.log"
python3 tools/code_size.py "$out/check-size/libmaul-tween.a" --budget

preset linux-clang-20-asan

if [ "$tier" = pr ]; then
    fuzz 10
    exit 0
fi

fuzz 60

for name in linux-clang-20-tsan linux-shared linux-i686-gcc-14 linux-clang-20 linux-clang-20-nightly; do
    preset "$name"
done

step "all checks passed"
