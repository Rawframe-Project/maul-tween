// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A host's floating-point setting for the tests, set and read without the
// library's own code: the rounding mode through <fenv.h>, and flush-to-
// zero (with denormals-are-zero on x86) through each architecture's
// control register. A test plays a host that runs its threads with flush-
// to-zero and rounding toward zero, and checks every call leaves the
// setting as it found it.

#ifndef MAUL_TWEEN_TEST_FENV_H
#define MAUL_TWEEN_TEST_FENV_H

#include <fenv.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(_MSC_VER) && !defined(__clang__) && (defined(_M_X64) || defined(_M_IX86))
#include <xmmintrin.h>
#elif defined(_MSC_VER) && defined(_M_ARM64)
#include <intrin.h>
#endif

typedef struct TestFloatSetting
{
    int rounding;
    bool flush;
} TestFloatSetting;

// MXCSR's flush-to-zero and denormals-are-zero bits; FPCR's FZ.
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define TEST_FLUSH_BITS ((1u << 15) | (1u << 6))
#define TEST_HAS_FLUSH  1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define TEST_FLUSH_BITS (1ull << 24)
#define TEST_HAS_FLUSH  1
#else
#define TEST_HAS_FLUSH 0
#endif

#if TEST_HAS_FLUSH
// Volatile asm with a memory clobber where the compiler allows it, so a
// read is never reused across a call into the library.
static inline uint64_t TestControlRead(void)
{
#if defined(__x86_64__) || defined(__i386__)
    uint32_t value = 0;
    __asm__ volatile("stmxcsr %0" : "=m"(value) : : "memory");
    return value;
#elif defined(_M_X64) || defined(_M_IX86)
    return _mm_getcsr();
#elif defined(__aarch64__)
    uint64_t value = 0;
    __asm__ volatile("mrs %0, fpcr" : "=r"(value) : : "memory");
    return value;
#else
    return (uint64_t)_ReadStatusReg(ARM64_SYSREG(3, 3, 4, 4, 0));
#endif
}

static inline void TestControlWrite(uint64_t value)
{
#if defined(__x86_64__) || defined(__i386__)
    uint32_t control = (uint32_t)value;
    __asm__ volatile("ldmxcsr %0" : : "m"(control) : "memory");
#elif defined(_M_X64) || defined(_M_IX86)
    _mm_setcsr((unsigned int)value);
#elif defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" : : "r"(value) : "memory");
#else
    _WriteStatusReg(ARM64_SYSREG(3, 3, 4, 4, 0), (__int64)value);
#endif
}
#endif

// The thread's setting as it is.
static inline TestFloatSetting TestFloatGet(void)
{
    TestFloatSetting setting = {fegetround(), false};
#if TEST_HAS_FLUSH
    setting.flush = (TestControlRead() & TEST_FLUSH_BITS) == TEST_FLUSH_BITS;
#endif
    return setting;
}

// Sets the thread's rounding mode and flush-to-zero.
static inline void TestFloatSet(TestFloatSetting setting)
{
    (void)fesetround(setting.rounding);
#if TEST_HAS_FLUSH
    uint64_t control = TestControlRead();
    TestControlWrite(setting.flush ? control | TEST_FLUSH_BITS
                                   : control & ~(uint64_t)TEST_FLUSH_BITS);
#endif
}

static inline bool TestFloatSame(TestFloatSetting a, TestFloatSetting b)
{
    return a.rounding == b.rounding && a.flush == b.flush;
}

// The hostile host: flush-to-zero and rounding toward zero where the
// platform takes them, else the setting as it is.
static inline TestFloatSetting TestFloatHostile(void)
{
    TestFloatSetting setting = TestFloatGet();
#ifdef FE_TOWARDZERO
    int saved = fegetround();
    if (fesetround(FE_TOWARDZERO) == 0 && fegetround() == FE_TOWARDZERO)
    {
        setting.rounding = FE_TOWARDZERO;
    }
    (void)fesetround(saved);
#endif
    setting.flush = TEST_HAS_FLUSH != 0;
    return setting;
}

#endif // MAUL_TWEEN_TEST_FENV_H
