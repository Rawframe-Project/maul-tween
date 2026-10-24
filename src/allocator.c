// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The one place a zeroed allocator reaches the C library.
// The library asks it for max_align_t's alignment at most,
// which malloc serves.

#include "allocator.h"

#include <stdlib.h>

void* mtwnAllocate(const mtwnAllocator* allocator, size_t bytes, size_t alignment)
{
    if (allocator->alloc != NULL)
    {
        return allocator->alloc(bytes, alignment, allocator->context);
    }
    return malloc(bytes);
}

void mtwnRelease(const mtwnAllocator* allocator, void* memory, size_t bytes, size_t alignment)
{
    if (memory == NULL)
    {
        return;
    }
    if (allocator->free != NULL)
    {
        allocator->free(memory, bytes, alignment, allocator->context);
        return;
    }
    free(memory);
}
