cmake_minimum_required(VERSION 3.25)

# The library's identity. The family's CMake modules and tools read these.
set(MAUL_LIBRARY maul-tween)
set(MAUL_API_PREFIX mtwn)
set(MAUL_MACRO_PREFIX MTWN)
set(MAUL_TITLE "Maul Tween")
set(MAUL_PREFIX MAUL_TWEEN)

include(cmake/ReadVersion.cmake)
maul_read_version(${CMAKE_CURRENT_SOURCE_DIR}/include/maul-tween/base.h MTWN MAUL_VERSION)
project(maul-tween
    VERSION ${MAUL_VERSION}
    DESCRIPTION "Easing, springs, tweens and timelines driven by host time"
    LANGUAGES C)

option(MAUL_TWEEN_BUILD_TESTS "Build the test suites" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_TWEEN_BUILD_BENCH "Build the benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_TWEEN_BUILD_SAMPLES "Build the samples" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_TWEEN_BUILD_SHARED "Build a shared library instead of a static one" OFF)
option(MAUL_TWEEN_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})
option(MAUL_TWEEN_WERROR "Treat compiler warnings as errors" OFF)
option(MAUL_TWEEN_SANITIZE "Instrument with AddressSanitizer and UBSan" OFF)
option(MAUL_TWEEN_TSAN "Instrument with ThreadSanitizer" OFF)
option(MAUL_TWEEN_COVERAGE "Instrument for gcov coverage" OFF)
option(MAUL_TWEEN_FUZZ "Build the libFuzzer target (Clang)" OFF)

# Every executable (tests, benchmarks, tools) lands in the top of the
# build tree, so scripts and CI find them in one place.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

include(GNUInstallDirs)
include(cmake/CompilerFlags.cmake)
include(cmake/Library.cmake)
include(cmake/Testing.cmake)

if(MAUL_TWEEN_BUILD_SHARED)
    set(MAUL_LIBRARY_KIND SHARED)
else()
    set(MAUL_LIBRARY_KIND STATIC)
endif()
add_library(maul-tween ${MAUL_LIBRARY_KIND}
    src/allocator.c
    src/base.c
    src/clip.c
    src/clip_sample.c
    src/clip_tasks.c
    src/compose.c
    src/coast.c
    src/color.c
    src/context.c
    src/crossing.c
    src/css_easing.c
    src/decay.c
    src/easing.c
    src/fpenv.c
    src/inertia.c
    src/rotation.c
    src/rough.c
    src/scale.c
    src/zoom.c
    src/path.c
    src/path_direction.c
    src/path_length.c
    src/player.c
    src/player_slot.c
    src/pool.c
    src/runs.c
    src/sample.c
    src/signal.c
    src/spring.c
    src/stagger.c
    src/target.c
    src/timing.c
    src/transition.c
    src/update.c
    src/elementary.c
    src/event.c
    src/sweep.c
    src/elementary_float.c)
maul_configure_library(maul-tween)
# The exactly rounded functions the library takes from libm
# (tools/libm-allowed.txt, record mtwn-0001).
if(NOT WIN32 AND NOT APPLE)
    target_link_libraries(maul-tween PUBLIC m)
endif()

if(MAUL_TWEEN_BUILD_TESTS)
    enable_testing()
    maul_add_test(base)
    maul_add_test(easing)
    maul_add_test(easing_css)
    maul_add_test(rival_easing)
    maul_add_test(rival_spring)
    maul_add_test(rival_color)
    maul_add_test(sample)
    maul_add_test(spring)
    maul_add_test(decay)
    maul_add_test(inertia)
    maul_add_test(color)
    maul_add_test(rotation)
    maul_add_test(rough)
    maul_add_test(scale)
    maul_add_test(signal)
    maul_add_test(mix WHITEBOX)
    maul_add_test(stagger WHITEBOX)
    maul_add_test(timing)
    maul_add_test(context WHITEBOX)
    maul_add_test(transition WHITEBOX)
    maul_add_test(coast)
    maul_add_test(path)
    maul_add_test(direction)
    maul_add_test(monotone)
    maul_add_test(path_counts WHITEBOX)
    maul_add_test(track)
    maul_add_test(clip WHITEBOX)
    maul_add_test(player WHITEBOX)
    maul_add_test(clip_play)
    maul_add_test(clip_share)
    maul_add_test(clip_tasks WHITEBOX THREADS)
    maul_add_test(clip_step WHITEBOX)
    maul_add_test(clip_velocity)
    maul_add_test(reduced)
    maul_add_test(threads THREADS)
    # The crossing test's oracle trials in three slices beside its exact
    # cases, run side by side; with no argument the program runs all.
    maul_add_test(crossing WHITEBOX MANUAL)
    if(TARGET test_crossing)
        add_test(NAME crossing COMMAND test_crossing exact)
        foreach(slice 0 1 2)
            add_test(NAME crossing_oracle_${slice} COMMAND test_crossing oracle ${slice} 3)
        endforeach()
    endif()
    maul_add_test(exact WHITEBOX)
    maul_add_test(sweep WHITEBOX)
    maul_add_test(event)
    maul_add_test(compose WHITEBOX)
    maul_add_test(incremental)
    maul_add_test(grow)
    maul_add_test(reentry)
    maul_add_test(reserved)
    maul_add_test(bounds)
    maul_add_test(outputs)
    maul_add_test(refusals)
    maul_add_test(determinism)
    maul_add_test(guide)
    maul_add_test(invariants WHITEBOX)
    maul_add_test(math WHITEBOX)
    maul_add_test(math_float WHITEBOX)
    maul_add_header_tests()
    # The fuzz targets' scripts on a fixed set of random inputs.
    if(NOT MAUL_TWEEN_BUILD_SHARED)
        foreach(target IN ITEMS api values)
            add_executable(test_fuzz_${target} ${PROJECT_SOURCE_DIR}/test/fuzz_${target}.c)
            target_link_libraries(test_fuzz_${target} PRIVATE ${PROJECT_NAME})
            target_include_directories(test_fuzz_${target} PRIVATE ${PROJECT_SOURCE_DIR}/src)
            maul_apply_flags(test_fuzz_${target})
            add_test(NAME fuzz_${target} COMMAND test_fuzz_${target})
        endforeach()
        # The fit's target is fuzz_values' easings with their fits.
        add_executable(test_fuzz_fit ${PROJECT_SOURCE_DIR}/test/fuzz_values.c)
        target_link_libraries(test_fuzz_fit PRIVATE ${PROJECT_NAME})
        target_include_directories(test_fuzz_fit PRIVATE ${PROJECT_SOURCE_DIR}/src)
        target_compile_definitions(test_fuzz_fit PRIVATE MTWN_FUZZ_FIT)
        maul_apply_flags(test_fuzz_fit)
        add_test(NAME fuzz_fit COMMAND test_fuzz_fit)
    endif()
endif()

# The fuzz targets, each run as fuzz_<name> <corpus dir>: test/fuzz_api.c
# over scripts of the public API (clips, players, transitions, updates) on
# a context alone, on a task system and counting every record, and
# test/fuzz_values.c over the value functions (easings, springs, decay,
# colors, rotations, signals, paths, staggers) with wild parameters.
if(MAUL_TWEEN_FUZZ)
    # The library is instrumented too: its coverage guides libFuzzer, and
    # the sanitizers watch its code. Undefined behaviour stops the run.
    target_compile_options(maul-tween PRIVATE -fsanitize=fuzzer-no-link,address,undefined
                                              -fno-sanitize-recover=all)
    target_link_options(maul-tween PUBLIC -fsanitize=address,undefined)
    foreach(target IN ITEMS api values fit)
        set(source ${target})
        if(target STREQUAL "fit")
            set(source values)
        endif()
        add_executable(fuzz_${target} test/fuzz_${source}.c)
        if(target STREQUAL "fit")
            target_compile_definitions(fuzz_${target} PRIVATE MTWN_FUZZ_FIT)
        endif()
        target_link_libraries(fuzz_${target} PRIVATE maul-tween)
        target_include_directories(fuzz_${target} PRIVATE ${PROJECT_SOURCE_DIR}/src)
        target_compile_definitions(fuzz_${target} PRIVATE MTWN_FUZZING)
        maul_apply_flags(fuzz_${target})
        target_compile_options(fuzz_${target} PRIVATE -fsanitize=fuzzer,address,undefined
                                                      -fno-sanitize-recover=all)
        target_link_options(fuzz_${target} PRIVATE -fsanitize=fuzzer,address,undefined)
    endforeach()
endif()

if(MAUL_TWEEN_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(MAUL_TWEEN_BUILD_BENCH AND EXISTS ${PROJECT_SOURCE_DIR}/bench/bench_main.c)
    add_subdirectory(bench)
endif()

if(MAUL_TWEEN_INSTALL)
    include(cmake/Install.cmake)
endif()
