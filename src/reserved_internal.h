// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The reserved bytes of what a call takes in: each
// must be 0, so a later 1.y may give one a meaning without changing what
// a program asks for; a call refuses one that is not.

#ifndef MAUL_TWEEN_SRC_RESERVED_INTERNAL_H
#define MAUL_TWEEN_SRC_RESERVED_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Whether every byte of a reserved field is 0.
static inline bool mtwnReservedClear(const uint8_t* bytes, size_t size)
{
    uint8_t any = 0;
    for (size_t i = 0; i < size; ++i)
    {
        any |= bytes[i];
    }
    return any == 0;
}

#endif
