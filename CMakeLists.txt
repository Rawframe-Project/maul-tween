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
    src/compose.c
    src/color.c
    src/context.c
    src/crossing.c
    src/css_easing.c
    src/decay.c
    src/easing.c
    src/fpenv.c
    src/rotation.c
    src/path.c
    src/player.c
    src/player_slot.c
    src/pool.c
    src/runs.c
    src/sample.c
    src/signal.c
    src/spring.c
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
    maul_add_test(sample)
    maul_add_test(spring)
    maul_add_test(decay)
    maul_add_test(color)
    maul_add_test(rotation)
    maul_add_test(signal)
    maul_add_test(timing)
    maul_add_test(context WHITEBOX)
    maul_add_test(transition WHITEBOX)
    maul_add_test(path)
    maul_add_test(track)
    maul_add_test(clip WHITEBOX)
    maul_add_test(player WHITEBOX)
    maul_add_test(clip_play)
    maul_add_test(crossing WHITEBOX)
    maul_add_test(event)
    maul_add_test(compose WHITEBOX)
    maul_add_test(incremental)
    maul_add_test(grow)
    maul_add_test(determinism)
    maul_add_test(guide)
    maul_add_test(invariants WHITEBOX)
    maul_add_test(math WHITEBOX)
    maul_add_test(math_float WHITEBOX)
    maul_add_header_tests()
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
