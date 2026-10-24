// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The pool of generation-checked slots, through its internal functions
// over slots the test owns: fresh slots in order, last in first out
// reuse, stale ids, a full pool, and a slot at the last generation
// retired. A slot's generation is read from its memory, so a test seeds
// one a step from the end instead of 2^32 reuses.

#include "pool.h"
#include "test_harness.h"

#include <stdint.h>
#include <string.h>

typedef struct Slot
{
    mtwnSlotHeader header;
    uint64_t payload;
} Slot;

static mtwnPool Make(Slot* slots, uint32_t capacity)
{
    memset(slots, 0, sizeof(Slot) * capacity);
    mtwnPool pool;
    memset(&pool, 0, sizeof pool);
    pool.slots = (unsigned char*)slots;
    pool.stride = (uint32_t)sizeof(Slot);
    pool.capacity = capacity;
    return pool;
}

static void TestReuse(void)
{
    Slot slots[3];
    mtwnPool pool = Make(slots, 3);
    uint32_t a = mtwnPoolAcquire(&pool);
    uint32_t b = mtwnPoolAcquire(&pool);
    CHECK(a == 1 && b == 2 && slots[0].header.generation == 1, "fresh slots in order");
    slots[0].payload = 7;
    mtwnPoolRelease(&pool, a);
    CHECK(mtwnPoolFind(&pool, a, 1) == NULL, "a released slot's id is stale");
    uint32_t again = mtwnPoolAcquire(&pool);
    CHECK(again == a && slots[0].header.generation == 2 && slots[0].payload == 0,
          "reused last in first out, zeroed, its generation advanced");
    CHECK(mtwnPoolFind(&pool, a, 2) == &slots[0].header && mtwnPoolFind(&pool, a, 1) == NULL,
          "the new id found, the old one not");
    CHECK(mtwnPoolAcquire(&pool) == 3 && mtwnPoolAcquire(&pool) == 0, "then full");
    CHECK(mtwnPoolFind(&pool, 0, 0) == NULL && mtwnPoolFind(&pool, 4, 1) == NULL,
          "no slot 0, none past the capacity");
}

static void TestRetire(void)
{
    Slot slots[2];
    mtwnPool pool = Make(slots, 2);
    // A step from the last generation.
    slots[0].header.generation = UINT32_MAX - 1;
    uint32_t a = mtwnPoolAcquire(&pool);
    CHECK(a == 1 && slots[0].header.generation == UINT32_MAX &&
              mtwnPoolFind(&pool, a, UINT32_MAX) == &slots[0].header,
          "the last generation handed out");
    mtwnPoolRelease(&pool, a);
    CHECK(pool.free == 0 && mtwnPoolFind(&pool, a, UINT32_MAX) == NULL,
          "released, it is retired, not free");
    uint32_t b = mtwnPoolAcquire(&pool);
    CHECK(b == 2, "the next slot taken instead");
    CHECK(mtwnPoolAcquire(&pool) == 0, "and the retired one does not count as free");
    mtwnPoolRelease(&pool, b);
    CHECK(mtwnPoolAcquire(&pool) == b, "a slot short of the end is reused as before");
}

int main(void)
{
    TestReuse();
    TestRetire();
    return s_failures == 0 ? 0 : 1;
}
