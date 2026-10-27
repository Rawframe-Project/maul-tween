// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context and its targets:
// the def's checks, one block through the host's allocator, the
// sharing's capacities in its bytes, ids that go
// stale and are reused last in first out, misuse counted, and values
// stored to the bit.

#include "test_harness.h"

#include "maul-tween/context.h"
#include "maul-tween/event.h"
#include "maul-tween/target.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Counter
{
    int allocations;
    int frees;
    size_t bytes;
    size_t alignment;
    bool fail;
} Counter;

static void* CountAlloc(size_t size, size_t alignment, void* context)
{
    Counter* counter = context;
    if (counter->fail)
    {
        return NULL;
    }
    ++counter->allocations;
    counter->bytes = size;
    counter->alignment = alignment;
    return malloc(size);
}

static void CountFree(void* memory, size_t size, size_t alignment, void* context)
{
    Counter* counter = context;
    ++counter->frees;
    CHECK(size == counter->bytes && alignment == counter->alignment,
          "freed with the size and alignment it was allocated with");
    free(memory);
}

static void TestCreate(void)
{
    Counter counter = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    def.allocator = (mtwnAllocator){CountAlloc, CountFree, &counter};
    def.targetCapacity = 100;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success && context != NULL, "made");
    CHECK(counter.allocations == 1, "one block");
    CHECK(mtwnContext_GetMemoryBytes(context) == counter.bytes, "the block the size reported");
    uint64_t hundred = counter.bytes;
    CHECK(mtwnGetContextMisuse(context) == 0, "no misuse yet");
    mtwnDestroyContext(context);
    CHECK(counter.frees == 1, "one free");
    def.targetCapacity = 200;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success &&
              mtwnContext_GetMemoryBytes(context) > hundred,
          "and grows with the targets it holds");
    mtwnDestroyContext(context);
    mtwnDestroyContext(NULL);
    counter.fail = true;
    context = (mtwnContext*)&counter;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorCapacity && context == NULL,
          "a failing allocator");
    mtwnContextDef plain = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(&plain, &context) == mtwn_success, "the C library's allocator");
    mtwnDestroyContext(context);
}

static void TestDefRefusals(void)
{
    mtwnContext* context = NULL;
    mtwnContextDef def = mtwnDefaultContextDef();
    CHECK(mtwnCreateContext(NULL, &context) == mtwn_errorInvalid, "null def");
    CHECK(mtwnCreateContext(&def, NULL) == mtwn_errorInvalid, "null out");
    def.cookie = 0;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "no cookie");
    def.version = 0;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "the cookie before the version");
    def = mtwnDefaultContextDef();
    def.version = MTWN_ABI_VERSION + 1;
    def.targetCapacity = UINT32_MAX;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorVersion,
          "another patch version, before the rest is read");
    def = mtwnDefaultContextDef();
    def.version = MTWN_ABI_VERSION + (1u << 8);
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorVersion, "another minor version");
    def = mtwnDefaultContextDef();
    def.version = 0;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorVersion, "a zeroed version");
    def.version = MTWN_ABI_VERSION & 0x7FFFFFFFu;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorVersion,
          "the version without its top bit");
    // The family's encoding of whatever release the headers name: the top
    // bit, the major in bits 16 to 30, and below them the minor and patch
    // in 8 bits each before 1.0, the minor alone from 1.0.
    uint32_t low = MTWN_VERSION_MAJOR == 0 ? (MTWN_VERSION_MINOR << 8) | MTWN_VERSION_PATCH
                                           : MTWN_VERSION_MINOR;
    CHECK((MTWN_ABI_VERSION >> 31) == 1u &&
              ((MTWN_ABI_VERSION >> 16) & 0x7FFFu) == MTWN_VERSION_MAJOR &&
              (MTWN_ABI_VERSION & 0xFFFFu) == low,
          "the stamp encodes the headers' version");
    def = mtwnDefaultContextDef();
    def.allocator.alloc = CountAlloc;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "an allocator of one function");
    uint32_t* counts[] = {&def.targetCapacity,     &def.playerCapacity,
                          &def.clipCapacity,       &def.eventCapacity,
                          &def.sharedNodeCapacity, &def.sharedRecordCapacity,
                          &def.pointsPerTransition};
    // Past its maximum is refused before the allocator is asked, so a
    // missing check reads as a refused allocation, not a huge block.
    Counter refusing = {.fail = true};
    for (int i = 0; i < 7; ++i)
    {
        def = mtwnDefaultContextDef();
        uint32_t* field = (uint32_t*)((char*)&def + ((char*)counts[i] - (char*)&def));
        *field = MTWN_MAX_COUNT + 1;
        def.allocator = (mtwnAllocator){CountAlloc, CountFree, &refusing};
        CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "a count past its maximum");
        def.allocator = mtwnDefaultContextDef().allocator;
        *field = 0;
        CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a count of 0");
        mtwnDestroyContext(context);
    }
    def = mtwnDefaultContextDef();
    def.allocator = (mtwnAllocator){CountAlloc, CountFree, &refusing};
    def.targetCapacity = MTWN_MAX_COUNT;
    def.clipStorageBytes = MTWN_MAX_CLIP_STORAGE;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorCapacity,
          "the maxima themselves are accepted, then the allocator asked");
    def = mtwnDefaultContextDef();
    def.clipStorageBytes = (uint64_t)MTWN_MAX_CLIP_STORAGE + 1;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "clip storage past its maximum");
}

// The block's bytes for a def, its sharing's capacities set.
static uint64_t BytesSharing(bool tasks, uint32_t nodes, uint32_t records);

// The sharing's capacities: 80 bytes a node and a record's, an mtwnEvent
// and a double (72 bytes on a 64-bit target), in each work, the serial one
// and with a task system each of the 16 groups'.
static void TestSharingCapacities(void)
{
    uint64_t record = sizeof(mtwnEvent) + sizeof(double);
    uint64_t work = 128u * 80u + 64u * record;
    uint64_t whole = BytesSharing(false, 128, 64);
    CHECK(whole - BytesSharing(false, 0, 0) == work && (sizeof(void*) != 8 || work == 14848),
          "the default sharing, 14,848 bytes a work on a 64-bit target");
    CHECK(whole - BytesSharing(false, 0, 64) == 128u * 80u &&
              whole - BytesSharing(false, 128, 0) == 64u * record,
          "each kind's own");
    CHECK(BytesSharing(true, 128, 64) - BytesSharing(true, 0, 0) == 17u * work,
          "and every group's with a task system");
}

static void* RunNow(mtwnTaskFn* task, int32_t itemCount, int32_t minRange, void* taskContext,
                    void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext);
    return NULL;
}

static void FinishNone(void* handle, void* userContext)
{
    (void)handle;
    (void)userContext;
}

static uint64_t BytesSharing(bool tasks, uint32_t nodes, uint32_t records)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = tasks ? RunNow : NULL;
    def.finishTask = tasks ? FinishNone : NULL;
    def.sharedNodeCapacity = nodes;
    def.sharedRecordCapacity = records;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "made");
    uint64_t bytes = mtwnContext_GetMemoryBytes(context);
    CHECK(mtwnContext_GetSerialAdvances(context) == 0, "no advance left serial yet");
    mtwnDestroyContext(context);
    return bytes;
}

// Each group's levels hold a chain of the deepest clip and the node
// below it, apart from the next group's and from the groups' node times
// after them: groups on threads at once would otherwise write each
// other's levels, which no sanitizer sees inside one block.
static mtwnContext* Make(uint32_t targets)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.targetCapacity = targets;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "made");
    return context;
}

static bool SameBits(mtwnVec4 a, mtwnVec4 b)
{
    return memcmp(&a, &b, sizeof a) == 0;
}

static void TestIds(void)
{
    mtwnContext* context = Make(3);
    mtwnVec4 one = {1.0f, 0.0f, 0.0f, 0.0f};
    mtwnTargetId a;
    mtwnTargetId b;
    mtwnTargetId c;
    mtwnTargetId d = {7, 7};
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, one, &a) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, one, &b) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, one, &c) == mtwn_success,
          "three");
    CHECK(a.index1 == 1 && b.index1 == 2 && c.index1 == 3 && a.generation == 1, "in order, from 1");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, one, &d) == mtwn_errorLimit &&
              d.index1 == 0 && d.generation == 0,
          "the capacity reached");
    CHECK(mtwnGetContextMisuse(context) == 0, "a full pool is not misuse");
    CHECK(mtwnDestroyTarget(context, b) == mtwn_success &&
              mtwnDestroyTarget(context, a) == mtwn_success,
          "destroyed");
    CHECK(!mtwnTarget_IsValid(context, a) && !mtwnTarget_IsValid(context, b) &&
              mtwnTarget_IsValid(context, c),
          "stale and alive");
    CHECK(mtwnGetContextMisuse(context) == 0, "validity queries are not misuse");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, one, &d) == mtwn_success && d.index1 == 1 &&
              d.generation == 2,
          "the last freed slot first, its generation advanced");
    mtwnTargetId e;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, one, &e) == mtwn_success && e.index1 == 2 &&
              e.generation == 2,
          "then the one before");
    CHECK(!mtwnTarget_IsValid(context, a), "the old id stays stale");
    mtwnVec4 value;
    CHECK(mtwnTarget_GetValue(context, a, &value) == mtwn_errorStale &&
              SameBits(value, (mtwnVec4){0}),
          "a stale read");
    CHECK(mtwnDestroyTarget(context, a) == mtwn_errorStale, "a stale destroy");
    CHECK(mtwnTarget_SetBase(context, (mtwnTargetId){0, 0}, one) == mtwn_errorStale, "the null id");
    CHECK(mtwnTarget_GetValue(context, (mtwnTargetId){99, 1}, &value) == mtwn_errorStale,
          "an index past the pool");
    CHECK(mtwnGetContextMisuse(context) == 4, "each counted as misuse");
    CHECK(!mtwnTarget_IsValid(NULL, c), "no context");
    mtwnDestroyContext(context);
}

static void TestValues(void)
{
    mtwnContext* context = Make(16);
    float tiny = FLT_TRUE_MIN;
    mtwnVec4 given = {tiny, -0.0f, 3.5f, -7.25f};
    static const uint8_t channels[] = {1, 2, 3, 4, 4, 1, 4, 1, 1};
    for (mtwnValueKind kind = 0; kind <= mtwn_valueDiscrete; ++kind)
    {
        mtwnTargetId id;
        CHECK(mtwnCreateTarget(context, kind, given, &id) == mtwn_success, "each kind");
        mtwnTargetState state;
        CHECK(mtwnTarget_GetState(context, id, &state) == mtwn_success, "read");
        float in[4] = {given.x, given.y, given.z, given.w};
        float expected[4] = {0};
        memcpy(expected, in, channels[kind] * sizeof(float));
        mtwnVec4 want = {expected[0], expected[1], expected[2], expected[3]};
        CHECK(SameBits(state.base, want) && SameBits(state.value, want) &&
                  SameBits(state.velocity, (mtwnVec4){0}) && state.kind == kind,
              "the kind's channels to the bit, a subnormal and -0 included, the rest 0, at rest");
        CHECK(mtwnDestroyTarget(context, id) == mtwn_success, "freed");
    }
    mtwnTargetId id;
    CHECK(mtwnCreateTarget(context, 10, given, &id) == mtwn_errorInvalid, "an unknown kind");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){NAN, 0, 0, 0}, &id) ==
              mtwn_errorInvalid,
          "NaN");
    CHECK(mtwnCreateTarget(context, mtwn_valueVec2, (mtwnVec4){0, -INFINITY, 0, 0}, &id) ==
              mtwn_errorInvalid,
          "infinity");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){1, NAN, NAN, NAN}, &id) ==
              mtwn_success,
          "channels past the kind's are not read");
    CHECK(mtwnCreateTarget(context, mtwn_valueRotation, (mtwnVec4){-0.0f, 0, 0, 0}, &id) ==
              mtwn_errorInvalid,
          "a zero rotation");
    CHECK(mtwnCreateTarget(context, mtwn_valueRotation, (mtwnVec4){0, 0, 0, 3.0f}, &id) ==
              mtwn_success,
          "a rotation not of unit length, stored as given");
    mtwnVec4 value;
    CHECK(mtwnTarget_GetValue(context, id, &value) == mtwn_success && value.w == 3.0f, "as given");
    CHECK(mtwnTarget_SetBase(context, id, (mtwnVec4){0, 0, 0, 0}) == mtwn_errorInvalid,
          "set refuses what create does");
    CHECK(mtwnTarget_SetBase(context, id, (mtwnVec4){0, 1, 0, 0}) == mtwn_success &&
              mtwnTarget_GetValue(context, id, &value) == mtwn_success && value.y == 1.0f &&
              value.w == 0.0f,
          "a target no animation writes shows its base at once");
    mtwnTargetId changed[16];
    uint32_t total = 0;
    bool reported = false;
    CHECK(mtwnUpdate(context, 0, changed, 16, &total) == mtwn_success, "updated");
    for (uint32_t i = 0; i < total && i < 16; ++i)
    {
        reported = reported || changed[i].index1 == id.index1;
    }
    CHECK(reported, "and is in the next report");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, given, NULL) == mtwn_errorInvalid,
          "null out");
    CHECK(mtwnCreateTarget(NULL, mtwn_valueScalar, given, &id) == mtwn_errorInvalid, "no context");
    CHECK(mtwnTarget_GetValue(context, id, NULL) == mtwn_errorInvalid, "null value out");
    CHECK(mtwnTarget_GetState(context, id, NULL) == mtwn_errorInvalid, "null state out");
    CHECK(mtwnGetContextMisuse(context) == 8, "every refusal counted, none without a context");
    mtwnDestroyContext(context);
}

static void TestReadMany(void)
{
    mtwnContext* context = Make(8);
    mtwnTargetId ids[4];
    for (int i = 0; i < 3; ++i)
    {
        CHECK(mtwnCreateTarget(context, mtwn_valueVec2, (mtwnVec4){(float)i, (float)-i, 0, 0},
                               &ids[i]) == mtwn_success,
              "made");
    }
    ids[3] = ids[1];
    mtwnVec4 values[4];
    CHECK(mtwnReadTargets(context, ids, 4, values) == mtwn_success, "read");
    CHECK(values[0].x == 0.0f && values[2].x == 2.0f && values[2].y == -2.0f && values[3].x == 1.0f,
          "in the order of the ids, repeats allowed");
    CHECK(mtwnDestroyTarget(context, ids[1]) == mtwn_success, "freed");
    CHECK(mtwnReadTargets(context, ids, 4, values) == mtwn_errorStale, "a stale id among them");
    CHECK(values[1].x == 0.0f && values[3].x == 0.0f && values[2].x == 2.0f,
          "stale ones zero, the others read");
    CHECK(mtwnGetContextMisuse(context) == 2, "each stale id counted");
    CHECK(mtwnReadTargets(context, NULL, 0, NULL) == mtwn_success, "nothing to read");
    CHECK(mtwnReadTargets(context, NULL, 1, values) == mtwn_errorInvalid, "null ids");
    mtwnDestroyContext(context);
}

// Reading targets with what it needs missing is misuse, counted; with no
// context, refused.
static void TestReadRefusals(void)
{
    mtwnContext* context = Make(2);
    mtwnVec4 values[1];
    mtwnTargetId ids[1] = {{0, 0}};
    uint64_t misuse = mtwnGetContextMisuse(context);
    CHECK(mtwnReadTargets(context, NULL, 1, values) == mtwn_errorInvalid &&
              mtwnReadTargets(context, ids, 1, NULL) == mtwn_errorInvalid &&
              mtwnGetContextMisuse(context) == misuse + 2,
          "no ids or no room: misuse, counted");
    CHECK(mtwnReadTargets(NULL, ids, 1, values) == mtwn_errorInvalid, "no context refused");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestReadRefusals();
    TestCreate();
    TestDefRefusals();
    TestSharingCapacities();
    TestIds();
    TestValues();
    TestReadMany();
    return s_failures == 0 ? 0 : 1;
}
