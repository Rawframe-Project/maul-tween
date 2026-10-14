// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context's layout, shared by the modules that work in it (record
// mtwn-0028). The context and its pools live in one block.

#ifndef MAUL_TWEEN_SRC_CONTEXT_INTERNAL_H
#define MAUL_TWEEN_SRC_CONTEXT_INTERNAL_H

#include "maul-tween/context.h"
#include "maul-tween/target.h"

#include <stdint.h>

// A target slot: 64 bytes. A free slot keeps its generation, which the
// next target in it advances, and the index1 of the next free slot.
typedef struct mtwnTargetSlot
{
    mtwnVec4 base;
    mtwnVec4 value;
    mtwnVec4 velocity;
    uint32_t generation;
    uint32_t nextFree;
    mtwnValueKind kind;
    uint8_t flags;
    uint8_t reserved[6];
} mtwnTargetSlot;

// The slot holds a live target.
#define MTWN_TARGET_ALIVE 1u
// The target changed since the last update reported it.
#define MTWN_TARGET_CHANGED 2u

struct mtwnContext
{
    mtwnAllocator allocator;
    mtwnContextDef def;
    // The block holding the context and its pools, and its size.
    uint64_t blockBytes;
    uint64_t misuse;
    mtwnTargetSlot* targets;
    // Slots used so far; past it every slot is fresh.
    uint32_t targetHigh;
    // The index1 of the first free slot below targetHigh, 0 when none.
    uint32_t targetFree;
    uint32_t targetsAlive;
    uint32_t reserved;
};

#endif
