// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Allocation through the host's allocator, the C library's when it is
// zeroed (record mtwn-0006).

#ifndef MAUL_TWEEN_SRC_ALLOCATOR_H
#define MAUL_TWEEN_SRC_ALLOCATOR_H

#include "maul-tween/base.h"

#include <stddef.h>

// Allocates bytes aligned to alignment, a power of two; NULL when the
// allocator fails, or for a zeroed allocator and an alignment past
// max_align_t's.
void* mtwnAllocate(const mtwnAllocator* allocator, size_t bytes, size_t alignment);

// Frees what mtwnAllocate returned for the same size and alignment. NULL
// is ignored.
void mtwnRelease(const mtwnAllocator* allocator, void* memory, size_t bytes, size_t alignment);

#endif
