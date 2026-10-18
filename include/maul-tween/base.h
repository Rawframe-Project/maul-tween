// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The base of the Maul Tween API: the library version, the export and
// attribute macros, the result codes every fallible function returns,
// the allocator a context takes, and the four-channel values and their
// kinds that targets and tracks hold.

#ifndef MAUL_TWEEN_BASE_H
#define MAUL_TWEEN_BASE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// The library version. CMake reads it from here.
#define MTWN_VERSION_MAJOR 1
#define MTWN_VERSION_MINOR 0
#define MTWN_VERSION_PATCH 2

// The major and minor version in one number: a def built against
// headers of another major or minor version is refused.
#define MTWN_ABI_VERSION (((uint32_t)MTWN_VERSION_MAJOR << 16) | (uint32_t)MTWN_VERSION_MINOR)

// MTWN_API marks the public functions: dllexport or dllimport in a
// shared Windows build (maul_tween_EXPORTS is defined while building
// the library), default visibility in a shared build elsewhere.
#if defined(MAUL_TWEEN_SHARED) && defined(_WIN32)
#if defined(maul_tween_EXPORTS)
#define MTWN_API __declspec(dllexport) extern
#else
#define MTWN_API __declspec(dllimport) extern
#endif
#elif defined(MAUL_TWEEN_SHARED) && (defined(__GNUC__) || defined(__clang__))
#define MTWN_API __attribute__((visibility("default"))) extern
#else
#define MTWN_API extern
#endif

// MTWN_NODISCARD marks a function whose result must be read: every
// function that returns a status. The attribute is standard in C23 and
// C++17 and left out for older dialects. MSVC keeps __cplusplus at
// 199711L unless /Zc:__cplusplus is given, so _MSVC_LANG is read too.
#if (defined(__cplusplus) && __cplusplus >= 201703L) ||                                            \
    (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define MTWN_NODISCARD [[nodiscard]]
#elif !defined(__cplusplus) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MTWN_NODISCARD [[nodiscard]]
#else
#define MTWN_NODISCARD
#endif

    // The status a fallible function returns. Zero is success, positive
    // values are outcomes that are not errors, negative values are errors.
    // The type has a fixed width so that result structs have one layout in
    // C and C++.
    typedef int32_t mtwnResult;
    enum
    {
        // The call did what was asked.
        mtwn_success = 0,
        // An argument is invalid: a null pointer where one is required, a
        // value out of range, a def not made by its default function.
        mtwn_errorInvalid = -1,
        // A caller buffer is too small for the result, or the allocator
        // failed.
        mtwn_errorCapacity = -2,
        // A named limit the caller set was reached: the work it bounds was
        // not done.
        mtwn_errorLimit = -3,
        // A def was built against headers of another major or minor
        // version.
        mtwn_errorVersion = -4,
        // An id names an object that has been destroyed since it was
        // handed out.
        mtwn_errorStale = -5,
        // The object is in use by another and cannot be destroyed or
        // changed now.
        mtwn_errorBusy = -6,
        // The call is not allowed in the object's current state.
        mtwn_errorState = -7,
    };

    // A version: major, minor and patch.
    typedef struct mtwnVersion
    {
        uint16_t major;
        uint16_t minor;
        uint16_t patch;
    } mtwnVersion;

    // The allocator a context takes in its def and keeps for its lifetime.
    // Alignment is a power of two. A zeroed allocator means the C library's
    // functions, which serve alignments up to that of max_align_t. Its
    // functions call no function of the context they serve (record
    // mtwn-0068).
    typedef struct mtwnAllocator
    {
        void* (*alloc)(size_t size, size_t alignment, void* context);
        void (*free)(void* memory, size_t size, size_t alignment, void* context);
        void* context;
    } mtwnAllocator;

    // A value of up to four channels, binary32: what a target holds. Its
    // kind says which channels it uses and what they mean.
    typedef struct mtwnVec4
    {
        float x;
        float y;
        float z;
        float w;
    } mtwnVec4;

    // What a value is, a target's or a track's: the channels it uses and how
    // it is interpolated and composed.
    typedef uint8_t mtwnValueKind;
    enum
    {
        // One number, in x.
        mtwn_valueScalar = 0,
        // Two numbers, x and y.
        mtwn_valueVec2 = 1,
        // Three numbers, x to z.
        mtwn_valueVec3 = 2,
        // Four numbers.
        mtwn_valueVec4 = 3,
        // A color: red, green, blue and alpha in x to w, sRGB as CSS gives
        // it, interpolated in the space a transition or track names.
        mtwn_valueColor = 4,
        // An angle in radians, in x.
        mtwn_valueAngle = 5,
        // A rotation as a quaternion: x, y, z, then w.
        mtwn_valueRotation = 6,
        // An integer, in x: interpolations round to nearest.
        mtwn_valueInteger = 7,
        // A discrete value the host indexes, in x: it steps at progress 1/2.
        mtwn_valueDiscrete = 8,
        // A scale, in x, above 0: moved in its logarithm, so equal steps
        // multiply it by equal ratios (record mtwn-0059).
        mtwn_valueScale = 9,
    };

    /// Returns the version of the library that was linked, which may differ
    /// from the MTWN_VERSION macros a program was compiled with.
    ///
    /// @return The library version.
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API mtwnVersion mtwnGetVersion(void);

    /// Returns the name of a result code, for diagnostics.
    ///
    /// @param result  Any value; an unknown one is named as such.
    /// @return A static, NUL-terminated string such as "mtwn_errorCapacity".
    /// @par Thread safety
    /// Safe from any thread.
    MTWN_API const char* mtwnResultName(mtwnResult result);

#ifdef __cplusplus
}
#endif

#endif
