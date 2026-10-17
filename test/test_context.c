// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The context and its targets (records mtwn-0004, mtwn-0006, mtwn-0028):
// the def's checks, one block through the host's allocator, the
// sharing's capacities in its bytes, ids that go
// stale and are reused last in first out, misuse counted, and values
// stored to the bit.

#include "context_internal.h"
#include "test_harness.h"

#include "maul-tween/context.h"
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
    CHECK(mtwnContext_GetMemoryBytes(context) == counter.bytes &&
              counter.bytes >= 100 * sizeof(mtwnTargetSlot),
          "the block holds the targets and is the size reported");
    CHECK(mtwnContext_GetMisuseCount(context) == 0, "no misuse yet");
    mtwnDestroyContext(context);
    CHECK(counter.frees == 1, "one free");
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
    def = mtwnDefaultContextDef();
    def.version = MTWN_ABI_VERSION + 1;
    def.targetCapacity = UINT32_MAX;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorVersion,
          "another minor version, before the rest is read");
    def = mtwnDefaultContextDef();
    def.version = MTWN_ABI_VERSION + (1u << 16);
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorVersion, "another major version");
    def = mtwnDefaultContextDef();
    def.allocator.alloc = CountAlloc;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "an allocator of one function");
    uint32_t* counts[] = {&def.targetCapacity, &def.playerCapacity,     &def.clipCapacity,
                          &def.eventCapacity,  &def.sharedNodeCapacity, &def.sharedRecordCapacity};
    for (int i = 0; i < 6; ++i)
    {
        def = mtwnDefaultContextDef();
        uint32_t* field = (uint32_t*)((char*)&def + ((char*)counts[i] - (char*)&def));
        *field = MTWN_MAX_COUNT + 1;
        CHECK(mtwnCreateContext(&def, &context) == mtwn_errorInvalid, "a count past its maximum");
        *field = 0;
        CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a count of 0");
        mtwnDestroyContext(context);
    }
    Counter refusing = {.fail = true};
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

// The sharing's capacities: 80 bytes a node and 72 a record in each work,
// the serial one and with a task system each of the 16 groups'.
static void TestSharingCapacities(void)
{
    uint64_t whole = BytesSharing(false, 128, 64);
    CHECK(whole - BytesSharing(false, 0, 0) == 14848, "the default sharing, 14,848 bytes a work");
    CHECK(whole - BytesSharing(false, 0, 64) == 128u * 80u &&
              whole - BytesSharing(false, 128, 0) == 64u * 72u,
          "each kind's own");
    CHECK(BytesSharing(true, 128, 64) - BytesSharing(true, 0, 0) == 17u * 14848u,
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
static void TestGroupLevels(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = RunNow;
    def.finishTask = FinishNone;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "made");
    bool apart = true;
    for (uint32_t g = 0; g < MTWN_GROUPS; ++g)
    {
        const char* end = (const char*)(context->works[g].levels + MTWN_MAX_CLIP_DEPTH + 2u);
        const char* next = g + 1 < MTWN_GROUPS ? (const char*)context->works[g + 1].levels
                                               : (const char*)context->works[0].times;
        apart = apart && end <= next;
    }
    CHECK(apart, "every group's levels the deepest clip's, apart");
    mtwnDestroyContext(context);
}

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
    CHECK(mtwnContext_GetMisuseCount(context) == 0, "a full pool is not misuse");
    CHECK(mtwnDestroyTarget(context, b) == mtwn_success &&
              mtwnDestroyTarget(context, a) == mtwn_success,
          "destroyed");
    CHECK(!mtwnTarget_IsValid(context, a) && !mtwnTarget_IsValid(context, b) &&
              mtwnTarget_IsValid(context, c),
          "stale and alive");
    CHECK(mtwnContext_GetMisuseCount(context) == 0, "validity queries are not misuse");
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
    CHECK(mtwnContext_GetMisuseCount(context) == 4, "each counted as misuse");
    CHECK(!mtwnTarget_IsValid(NULL, c), "no context");
    mtwnDestroyContext(context);
}

static void TestRetire(void)
{
    mtwnContext* context = Make(2);
    mtwnVec4 one = {1.0f, 0.0f, 0.0f, 0.0f};
    mtwnTargetId a;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, one, &a) == mtwn_success, "made");
    CHECK(mtwnDestroyTarget(context, a) == mtwn_success, "freed");
    mtwnTargetAt(context, 1)->header.generation = UINT32_MAX - 1;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, one, &a) == mtwn_success && a.index1 == 1 &&
              a.generation == UINT32_MAX,
          "the last generation");
    CHECK(mtwnDestroyTarget(context, a) == mtwn_success, "freed at the end");
    mtwnTargetId b;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, one, &b) == mtwn_success && b.index1 == 2,
          "the retired slot is not reused");
    mtwnTargetId c;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, one, &c) == mtwn_errorLimit,
          "and does not count as free");
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
    CHECK(mtwnCreateTarget(context, 9, given, &id) == mtwn_errorInvalid, "an unknown kind");
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
    CHECK((mtwnTargetAt(context, id.index1)->header.flags & MTWN_TARGET_CHANGED) != 0,
          "and is marked for the next report");
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, given, NULL) == mtwn_errorInvalid,
          "null out");
    CHECK(mtwnCreateTarget(NULL, mtwn_valueScalar, given, &id) == mtwn_errorInvalid, "no context");
    CHECK(mtwnTarget_GetValue(context, id, NULL) == mtwn_errorInvalid, "null value out");
    CHECK(mtwnTarget_GetState(context, id, NULL) == mtwn_errorInvalid, "null state out");
    CHECK(mtwnContext_GetMisuseCount(context) == 8,
          "every refusal counted, none without a context");
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
    CHECK(mtwnContext_GetMisuseCount(context) == 2, "each stale id counted");
    CHECK(mtwnReadTargets(context, NULL, 0, NULL) == mtwn_success, "nothing to read");
    CHECK(mtwnReadTargets(context, NULL, 1, values) == mtwn_errorInvalid, "null ids");
    mtwnDestroyContext(context);
}

// Reading targets with what it needs missing is misuse, counted; with no
// context, refused (found by mutants).
static void TestReadRefusals(void)
{
    mtwnContext* context = Make(2);
    mtwnVec4 values[1];
    mtwnTargetId ids[1] = {{0, 0}};
    uint64_t misuse = mtwnContext_GetMisuseCount(context);
    CHECK(mtwnReadTargets(context, NULL, 1, values) == mtwn_errorInvalid &&
              mtwnReadTargets(context, ids, 1, NULL) == mtwn_errorInvalid &&
              mtwnContext_GetMisuseCount(context) == misuse + 2,
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
    TestGroupLevels();
    TestIds();
    TestRetire();
    TestValues();
    TestReadMany();
    return s_failures == 0 ? 0 : 1;
}
