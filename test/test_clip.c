// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Clips (records mtwn-0004, mtwn-0013, mtwn-0020, mtwn-0033): one block
// per clip through the host's allocator within the context's clip
// storage, tracks made in it as added and read back through the values
// layer, positions resolved to start times, the states a clip passes, and
// the same bits under a host's flush-to-zero.

#include "context_internal.h"
#include "fpenv.h"
#include "test_harness.h"

#include "maul-tween/clip.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Counter
{
    int allocations;
    int frees;
    uint64_t live;
    bool fail;
} Counter;

static void* CountAlloc(size_t size, size_t alignment, void* context)
{
    (void)alignment;
    Counter* counter = context;
    if (counter->fail)
    {
        return NULL;
    }
    ++counter->allocations;
    counter->live += size;
    return malloc(size);
}

static void CountFree(void* memory, size_t size, size_t alignment, void* context)
{
    (void)alignment;
    Counter* counter = context;
    ++counter->frees;
    counter->live -= size;
    free(memory);
}

static mtwnContext* Make(Counter* counter, uint32_t clips, uint64_t storage)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.allocator = (mtwnAllocator){CountAlloc, CountFree, counter};
    def.clipCapacity = clips;
    def.clipStorageBytes = storage;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    return context;
}

static mtwnKeyframe Key(double offset, float x)
{
    mtwnKeyframe k;
    memset(&k, 0, sizeof k);
    k.offset = offset;
    k.value = (mtwnVec4){x, 0, 0, 0};
    k.easing = mtwnLinearEasing();
    return k;
}

static mtwnPosition At(mtwnPositionKind kind, double offset)
{
    mtwnPosition position;
    memset(&position, 0, sizeof position);
    position.kind = kind;
    position.offset = offset;
    return position;
}

static void TestStorage(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 2, 65536);
    mtwnClipDef def = mtwnDefaultClipDef();
    mtwnClipId a;
    mtwnClipId b;
    mtwnClipId c;
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_success && counter.allocations == 2,
          "one block for the clip");
    uint64_t one = context->clipStorageUsed;
    CHECK(one > 0 && one == mtwnClipAt(context, a.index1)->blockBytes, "counted");
    CHECK(mtwnCreateClip(context, &def, &b) == mtwn_success, "a second");
    CHECK(mtwnCreateClip(context, &def, &c) == mtwn_errorLimit && c.index1 == 0,
          "the pool of clips full");
    CHECK(mtwnDestroyClip(context, a) == mtwn_success && counter.frees == 1 &&
              context->clipStorageUsed == one,
          "destroyed, its storage back");
    CHECK(!mtwnClip_IsValid(context, a) && mtwnClip_IsValid(context, b), "stale and alive");
    CHECK(mtwnDestroyClip(context, a) == mtwn_errorStale, "a stale destroy");
    mtwnDestroyContext(context);
    CHECK(counter.frees == counter.allocations && counter.live == 0,
          "the context frees the clips it still holds");
    // Clip storage is a limit.
    counter = (Counter){0};
    context = Make(&counter, 4, one + one / 2);
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_success, "one fits");
    CHECK(mtwnCreateClip(context, &def, &b) == mtwn_errorLimit && counter.allocations == 2,
          "a second would pass the clip storage, nothing allocated");
    CHECK(mtwnGetContextMisuse(context) == 0, "a limit is not misuse");
    mtwnDestroyContext(context);
    // A failed allocation gives its slot back; a used clip is busy.
    counter = (Counter){0};
    context = Make(&counter, 1, 1 << 20);
    counter.fail = true;
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_errorCapacity && a.index1 == 0,
          "the allocator fails");
    counter.fail = false;
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_success, "the only slot still free");
    mtwnClipAt(context, a.index1)->users = 1;
    CHECK(mtwnDestroyClip(context, a) == mtwn_errorBusy && mtwnClip_IsValid(context, a),
          "a clip a player uses is busy");
    mtwnClipAt(context, a.index1)->users = 0;
    mtwnDestroyContext(context);
}

static void TestPositions(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 10.0f)};
    mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
    def.keyframes = keys;
    def.keyframeCount = 2;
    mtwnNode nodes[5];
    // 0 to 1; then 1 to 2.5 after the end; with it at 1.25; after the
    // previous one's end at 2.75 to 3.75; then absolute at -0.5.
    def.position = At(mtwn_positionAbsolute, 0.0);
    CHECK(mtwnClip_AddTrack(context, clip, &def, &nodes[0]) == mtwn_success && nodes[0] == 2,
          "the first child is node 2");
    def.position = At(mtwn_positionAfterEnd, 0.0);
    def.timing.duration = 1.5;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &nodes[1]) == mtwn_success, "after the end");
    def.position = At(mtwn_positionWithPrevious, 0.25);
    def.timing.duration = 1.0;
    def.slot = 1;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &nodes[2]) == mtwn_success, "with the previous");
    def.position = At(mtwn_positionAfterPrevious, 0.5);
    CHECK(mtwnClip_AddTrack(context, clip, &def, &nodes[3]) == mtwn_success, "after the previous");
    def.position = At(mtwn_positionAbsolute, -0.5);
    def.timing.delay = 0.25;
    def.timing.iterations = 3.0;
    def.timing.endDelay = 0.5;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &nodes[4]) == mtwn_success, "before 0");
    mtwnClipTrack track;
    CHECK(mtwnClip_GetTrack(context, clip, nodes[0], &track) == mtwn_errorState,
          "nothing is read before it is finished");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    static const double starts[5] = {0.0, 1.0, 1.25, 2.75, -0.5};
    int placed = 1;
    for (int i = 0; i < 5; ++i)
    {
        placed &= mtwnClip_GetTrack(context, clip, nodes[i], &track) == mtwn_success &&
                  track.start == starts[i] && track.parent == MTWN_ROOT_NODE;
    }
    CHECK(placed, "every position resolved to its start");
    double duration = 0.0;
    CHECK(mtwnClip_GetDuration(context, clip, &duration) == mtwn_success && duration == 3.75,
          "the root lasts to its latest child's end");
    // The last child: -0.5 + 0.25 + 3 + 0.5 = 3.25, inside 3.75; alone it
    // makes the root's end.
    mtwnClipId timed;
    CHECK(mtwnCreateClip(context, &clipDef, &timed) == mtwn_success, "another clip");
    CHECK(mtwnClip_AddTrack(context, timed, &def, &nodes[0]) == mtwn_success &&
              mtwnClip_Finish(context, timed) == mtwn_success &&
              mtwnClip_GetDuration(context, timed, &duration) == mtwn_success && duration == 3.25,
          "a child's delay, iterations and end delay make its end");
    CHECK(mtwnClip_GetTrack(context, clip, nodes[4], &track) == mtwn_success &&
              track.timing.iterations == 3.0 && track.slot == 1,
          "its timing and slot");
    mtwnDestroyContext(context);
}

static void TestTracks(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    double pi = 3.141592653589793;
    mtwnKeyframe angles[3] = {Key(0.0, (float)(350 * pi / 180)), Key(0.5, (float)(10 * pi / 180)),
                              Key(1.0, (float)(70 * pi / 180))};
    mtwnKeyframe given[3];
    memcpy(given, angles, sizeof angles);
    mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
    def.track = mtwnDefaultTrackDef(mtwn_valueAngle);
    def.keyframes = angles;
    def.keyframeCount = 3;
    def.constantSpeed = true;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_success, "an angle track");
    CHECK(memcmp(angles, given, sizeof angles) == 0, "the caller's keyframes untouched");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnClipTrack track;
    CHECK(mtwnClip_GetTrack(context, clip, node, &track) == mtwn_success, "read");
    mtwnTrack direct;
    double lengths[3];
    CHECK(mtwnMakeTrack(&def.track, given, 3, lengths, NULL, &direct) == mtwn_success, "direct");
    int same = 1;
    for (int k = 0; k <= 50; ++k)
    {
        mtwnVec4 a = mtwnSampleTrack(&track.track, k / 50.0);
        mtwnVec4 b = mtwnSampleTrack(&direct, k / 50.0);
        same &= memcmp(&a, &b, sizeof a) == 0;
    }
    CHECK(same, "the clip's track samples as the values layer's");
    mtwnVec4 half = mtwnSampleTrack(&track.track, 0.5);
    CHECK(fabs((double)half.x - (350 + 40) * pi / 180) <= 1e-5,
          "at constant speed: half of 80 degrees, not the keyframe at 0.5");
    CHECK(mtwnClip_GetTrack(context, clip, MTWN_ROOT_NODE, &track) == mtwn_errorInvalid,
          "the root is no track");
    CHECK(mtwnClip_GetTrack(context, clip, 9, &track) == mtwn_errorInvalid, "no such node");
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorState && node == 0,
          "nothing is added once finished");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_errorState, "finished once");
    CHECK(mtwnGetContextMisuse(context) == 4, "each refusal counted");
    mtwnDestroyContext(context);
}

static void TestRefusals(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(NULL, &clipDef, &clip) == mtwn_errorInvalid, "no context");
    CHECK(mtwnCreateClip(context, NULL, &clip) == mtwn_errorInvalid, "no def");
    clipDef.cookie = 0;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_errorInvalid, "no cookie");
    clipDef = mtwnDefaultClipDef();
    clipDef.version = MTWN_ABI_VERSION + 1;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_errorVersion, "another version");
    clipDef = mtwnDefaultClipDef();
    clipDef.keyframeCapacity = MTWN_MAX_COUNT + 1;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_errorInvalid, "a capacity too large");
    clipDef = mtwnDefaultClipDef();
    clipDef.nodeCapacity = 1;
    clipDef.keyframeCapacity = 3;
    clipDef.slotCapacity = 2;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a tiny clip");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
    def.keyframes = keys;
    def.keyframeCount = 2;
    mtwnNode node;
    uint64_t misuse = mtwnGetContextMisuse(context);
    def.parent = 2;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "no such parent");
    def.parent = MTWN_ROOT_NODE;
    def.slot = 2;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "a slot past it");
    def.slot = 0;
    def.position.kind = 9;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "a position kind");
    def.position.kind = mtwn_positionAbsolute;
    def.slot = 2;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "a slot past it");
    def.slot = 0;
    def.position.kind = 9;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "a position kind");
    def.position = At(mtwn_positionAbsolute, (double)NAN);
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "a NaN offset");
    def.position = At(mtwn_positionAbsolute, 0.0);
    def.timing.duration = -1.0;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "a bad timing");
    def.timing.duration = 1.0;
    def.keyframes = NULL;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "no keyframes");
    def.keyframes = keys;
    keys[1].offset = 0.5;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid,
          "keyframes the track refuses");
    keys[1].offset = 1.0;
    def.cookie = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid, "no cookie");
    def.cookie = MTWN_CLIP_TRACK_DEF_COOKIE;
    def.version = MTWN_ABI_VERSION + 1;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorVersion, "a version");
    def.version = MTWN_ABI_VERSION;
    CHECK(mtwnGetContextMisuse(context) == misuse + 11, "each counted");
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_success, "then it fits");
    def.parent = node;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid,
          "a track is no parent");
    mtwnDestroyContext(context);
    // Filled to its node capacity, every track intact.
    counter = (Counter){0};
    context = Make(&counter, 4, 1 << 20);
    clipDef = mtwnDefaultClipDef();
    clipDef.nodeCapacity = 3;
    clipDef.keyframeCapacity = 64;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip of 3 nodes");
    def = mtwnDefaultClipTrackDef();
    def.keyframes = keys;
    def.keyframeCount = 2;
    mtwnNode filled[3];
    for (int i = 0; i < 3; ++i)
    {
        keys[1].value.x = (float)(i + 1);
        CHECK(mtwnClip_AddTrack(context, clip, &def, &filled[i]) == mtwn_success, "added");
    }
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorLimit, "the nodes full");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    int intact = 1;
    for (int i = 0; i < 3; ++i)
    {
        mtwnClipTrack track;
        intact &= mtwnClip_GetTrack(context, clip, filled[i], &track) == mtwn_success &&
                  mtwnSampleTrack(&track.track, 1.0).x == (float)(i + 1);
    }
    CHECK(intact, "every track intact, the storage holding them all");
    keys[1].value.x = 1.0f;
    mtwnDestroyContext(context);
    // The keyframes and a slot's kind.
    counter = (Counter){0};
    context = Make(&counter, 4, 1 << 20);
    clipDef = mtwnDefaultClipDef();
    clipDef.keyframeCapacity = 3;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    def.slot = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_success, "two keyframes");
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorLimit, "two more do not fit");
    clipDef.keyframeCapacity = 8;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "another");
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_success, "a scalar on slot 0");
    def.track = mtwnDefaultTrackDef(mtwn_valueInteger);
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorInvalid,
          "an integer on the same slot");
    CHECK(mtwnClip_AddTrack(context, (mtwnClipId){9, 1}, &def, &node) == mtwn_errorStale,
          "a stale clip");
    mtwnDestroyContext(context);
}

static mtwnPosition AtLabel(uint32_t key, double offset)
{
    mtwnPosition position = At(mtwn_positionAtLabel, offset);
    position.label = key;
    return position;
}

static void TestTimelines(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.position = At(mtwn_positionAbsolute, 1.0);
    mtwnNode t;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &t) == mtwn_success && t == 2,
          "a timeline at 1");
    mtwnNode a;
    track.parent = t;
    track.timing.duration = 2.0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &a) == mtwn_success, "a 2 s track in it");
    mtwnNode b;
    track.parent = MTWN_ROOT_NODE;
    track.slot = 1;
    track.timing.duration = 1.0;
    track.position = At(mtwn_positionAfterPrevious, 0.0);
    CHECK(mtwnClip_AddTrack(context, clip, &track, &b) == mtwn_success,
          "after the timeline, which ends at 3");
    mtwnNode c;
    track.parent = t;
    track.position = At(mtwn_positionAfterEnd, 0.0);
    CHECK(mtwnClip_AddTrack(context, clip, &track, &c) == mtwn_success,
          "the timeline grows to 3 s, ending at 4");
    // A fixed duration does not follow its children.
    mtwnClipTimelineDef fixed = mtwnDefaultClipTimelineDef();
    fixed.autoDuration = false;
    fixed.timing.duration = 0.5;
    fixed.timing.iterations = 2.0;
    fixed.timing.delay = 0.25;
    mtwnNode f;
    CHECK(mtwnClip_AddTimeline(context, clip, &fixed, &f) == mtwn_success, "fixed at 0");
    track.parent = f;
    track.slot = 2;
    track.timing.duration = 9.0;
    track.position = At(mtwn_positionAbsolute, 0.0);
    mtwnNode inside;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &inside) == mtwn_success, "a long child");
    // Labels name times; a position at one; not a previous child.
    mtwnNode label;
    CHECK(mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, At(mtwn_positionAbsolute, 2.5), 7,
                            &label) == mtwn_success,
          "a label at 2.5");
    CHECK(mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, At(mtwn_positionAbsolute, 9.0), 7,
                            &label) == mtwn_errorInvalid,
          "its key once in its parent");
    mtwnNode otherLabel;
    CHECK(mtwnClip_AddLabel(context, clip, t, At(mtwn_positionAbsolute, 0.5), 7, &otherLabel) ==
              mtwn_success,
          "the same key in another timeline");
    mtwnNode d;
    track.parent = MTWN_ROOT_NODE;
    track.slot = 3;
    track.timing.duration = 0.25;
    track.position = AtLabel(7, 0.5);
    CHECK(mtwnClip_AddTrack(context, clip, &track, &d) == mtwn_success, "at the label plus 0.5");
    mtwnNode late;
    CHECK(mtwnClip_AddLabel(context, clip, MTWN_ROOT_NODE, At(mtwn_positionAbsolute, 50.0), 8,
                            &late) == mtwn_success,
          "a label past the end");
    mtwnNode e;
    track.position = At(mtwn_positionWithPrevious, 0.0);
    track.slot = 4;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &e) == mtwn_success,
          "with the previous track, not the label");
    track.position = AtLabel(99, 0.0);
    mtwnNode refused;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &refused) == mtwn_errorInvalid && refused == 0,
          "no such label");
    mtwnNode mark;
    CHECK(mtwnClip_AddMark(context, clip, t, At(mtwn_positionAbsolute, 1.5), 7, &mark) ==
                  mtwn_success &&
              mtwnClip_AddMark(context, clip, t, At(mtwn_positionAbsolute, 2.0), 7, &mark) ==
                  mtwn_success,
          "marks may share a key");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnClipNodeInfo info;
    double seconds = 0.0;
    CHECK(mtwnClip_GetNode(context, clip, b, &info) == mtwn_success && info.start == 3.0,
          "a sibling keeps the start it was given");
    CHECK(mtwnClip_GetNode(context, clip, t, &info) == mtwn_success && info.start == 1.0 &&
              info.end == 4.0 && info.timing.duration == 3.0 && info.kind == mtwn_nodeTimeline,
          "the timeline's end and duration follow its children");
    CHECK(mtwnClip_GetNode(context, clip, c, &info) == mtwn_success && info.start == 2.0 &&
              info.parent == t,
          "after its timeline's end so far");
    CHECK(mtwnClip_GetNode(context, clip, f, &info) == mtwn_success && info.end == 1.25 &&
              info.timing.duration == 0.5,
          "a fixed duration keeps its end: 0.25 + 2 x 0.5");
    CHECK(mtwnClip_GetNode(context, clip, d, &info) == mtwn_success && info.start == 3.0,
          "at the label plus 0.5");
    CHECK(mtwnClip_GetNode(context, clip, e, &info) == mtwn_success && info.start == 3.0,
          "with the previous track");
    CHECK(mtwnClip_GetNode(context, clip, mark, &info) == mtwn_success &&
              info.kind == mtwn_nodeMark && info.key == 7 && info.start == 2.0 && info.end == 2.0,
          "a mark, an instant with its key");
    CHECK(mtwnClip_FindLabel(context, clip, MTWN_ROOT_NODE, 7, &seconds) == mtwn_success &&
              seconds == 2.5,
          "the label found by its key");
    CHECK(mtwnClip_FindLabel(context, clip, t, 7, &seconds) == mtwn_success && seconds == 0.5,
          "and the other in its timeline");
    CHECK(mtwnClip_FindLabel(context, clip, MTWN_ROOT_NODE, 9, &seconds) == mtwn_errorInvalid,
          "no such label");
    CHECK(mtwnClip_FindLabel(context, clip, 999, 7, &seconds) == mtwn_errorInvalid &&
              mtwnClip_FindLabel(context, clip, a, 7, &seconds) == mtwn_errorInvalid,
          "a parent that is no timeline of the clip");
    double duration = 0.0;
    CHECK(mtwnClip_GetDuration(context, clip, &duration) == mtwn_success && duration == 4.0,
          "labels and marks lengthen nothing");
    CHECK(mtwnClip_GetNode(context, clip, MTWN_ROOT_NODE, &info) == mtwn_success &&
              info.end == 4.0 && info.kind == mtwn_nodeTimeline && info.timing.duration == 4.0,
          "the root");
    // The children in the order they were added.
    const mtwnClipSlot* slot = mtwnClipAt(context, clip.index1);
    uint32_t order[16];
    uint32_t n = 0;
    for (uint32_t child = slot->nodes[t - 1].firstChild; child != 0 && n < 16;
         child = slot->nodes[child - 1].nextSibling)
    {
        order[n++] = child;
    }
    CHECK(n == 5 && order[0] == a && order[1] == c && order[2] == otherLabel && order[4] == mark,
          "children chained in the order added");
    mtwnDestroyContext(context);
}

static void TestNesting(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    // Root > outer at 1 > inner at 2 > a 3 s track: the end climbs every
    // level, 1 + 2 + 3 = 6.
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.position = At(mtwn_positionAbsolute, 1.0);
    mtwnNode outer;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &outer) == mtwn_success, "outer");
    group.parent = outer;
    group.position = At(mtwn_positionAbsolute, 2.0);
    mtwnNode inner;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &inner) == mtwn_success, "inner");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.parent = inner;
    track.timing.duration = 3.0;
    mtwnNode leaf;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &leaf) == mtwn_success, "a leaf");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnClipNodeInfo info;
    double duration = 0.0;
    CHECK(mtwnClip_GetNode(context, clip, outer, &info) == mtwn_success && info.end == 6.0 &&
              mtwnClip_GetDuration(context, clip, &duration) == mtwn_success && duration == 6.0,
          "the leaf's end climbs every level to the root");
    mtwnDestroyContext(context);
}

static void TestEndless(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.0;
    track.timing.iterations = HUGE_VAL;
    // A set duration clips an endless child.
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = 2.0;
    mtwnNode fixed;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &fixed) == mtwn_success, "a fixed 2 s");
    track.parent = fixed;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "an endless child");
    track.parent = MTWN_ROOT_NODE;
    track.timing.iterations = 1.0;
    track.position = At(mtwn_positionAfterEnd, 0.0);
    mtwnNode after;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &after) == mtwn_success,
          "after the clipped timeline, at 2");
    // One sized by its children becomes endless with it.
    group = mtwnDefaultClipTimelineDef();
    group.position = At(mtwn_positionAfterEnd, 0.0);
    mtwnNode open;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &open) == mtwn_success, "at 3");
    track.parent = open;
    track.position = At(mtwn_positionAbsolute, 0.0);
    track.timing.iterations = HUGE_VAL;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "an endless child");
    track.timing.iterations = 1.0;
    track.position = At(mtwn_positionAfterEnd, 0.0);
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "nothing after its end");
    track.position = At(mtwn_positionAfterPrevious, 0.0);
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "nor after the endless child");
    track.parent = MTWN_ROOT_NODE;
    track.position = At(mtwn_positionAfterEnd, 0.0);
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "nor after the endless clip");
    CHECK(mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, At(mtwn_positionAfterPrevious, 0.0), 1,
                           &node) == mtwn_errorInvalid,
          "a mark neither");
    track.position = At(mtwn_positionWithPrevious, 0.5);
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
          "with the endless timeline's start");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnClipNodeInfo info;
    double duration = 0.0;
    CHECK(mtwnClip_GetNode(context, clip, fixed, &info) == mtwn_success && info.end == 2.0 &&
              mtwnClip_GetNode(context, clip, after, &info) == mtwn_success && info.start == 2.0,
          "the set duration holds");
    CHECK(mtwnClip_GetNode(context, clip, open, &info) == mtwn_success && info.start == 3.0 &&
              isinf(info.end) && isinf(info.timing.duration),
          "the sized one endless");
    CHECK(mtwnClip_GetNode(context, clip, node, &info) == mtwn_success && info.start == 3.5,
          "with its start plus 0.5");
    CHECK(mtwnClip_GetDuration(context, clip, &duration) == mtwn_success && isinf(duration),
          "an endless clip");
    mtwnDestroyContext(context);
}

static void TestDepth(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.nodeCapacity = 64;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    mtwnNode parent = MTWN_ROOT_NODE;
    bool nested = true;
    for (uint32_t depth = 1; depth <= MTWN_MAX_CLIP_DEPTH; ++depth)
    {
        group.parent = parent;
        nested = nested && mtwnClip_AddTimeline(context, clip, &group, &parent) == mtwn_success;
    }
    CHECK(nested, "timelines nest to the deepest");
    group.parent = parent;
    mtwnNode deeper;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &deeper) == mtwn_errorLimit && deeper == 0,
          "and no deeper");
    CHECK(mtwnClip_AddMark(context, clip, parent, At(mtwn_positionAbsolute, 0.0), 1, &deeper) ==
              mtwn_success,
          "though instants go in the deepest");
    mtwnDestroyContext(context);
}

static void TestTimelineRefusals(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.nodeCapacity = 3;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    CHECK(mtwnMakeBackEasing(mtwn_easeOut, 1.70158, &group.timing.easing) == mtwn_success, "back");
    mtwnNode node;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &node) == mtwn_errorInvalid,
          "a timeline's easing turns back");
    group = mtwnDefaultClipTimelineDef();
    group.timing.playbackRate = -1.0;
    mtwnNode backward;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &backward) == mtwn_success,
          "a timeline played backwards");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.parent = backward;
    track.timing.iterations = HUGE_VAL;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "an endless child would leave it no end to start from");
    track.timing.iterations = 2.0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a child that ends");
    CHECK(mtwnClip_AddMark(context, clip, node, At(mtwn_positionAbsolute, 0.0), 1, &node) ==
              mtwn_errorInvalid,
          "a mark needs a timeline for a parent");
    CHECK(mtwnClip_AddMark(context, clip, backward, At(9, 0.0), 1, &node) == mtwn_errorInvalid,
          "a position kind");
    CHECK(mtwnClip_AddMark(context, clip, backward, At(mtwn_positionAbsolute, 0.0), 1, &node) ==
              mtwn_success,
          "the third node");
    CHECK(mtwnClip_AddLabel(context, clip, backward, At(mtwn_positionAbsolute, 0.0), 1, &node) ==
              mtwn_errorLimit,
          "labels count among the nodes");
    group = mtwnDefaultClipTimelineDef();
    group.cookie = 0;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &node) == mtwn_errorInvalid, "no cookie");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    CHECK(mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, At(mtwn_positionAbsolute, 0.0), 1,
                           &node) == mtwn_errorState,
          "nothing added once finished");
    mtwnDestroyContext(context);
}

// Only a timeline played backwards needs its children to end: one played
// forwards slower than real time takes an endless child.
static void TestSlowEndless(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.nodeCapacity = 2;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.timing.playbackRate = 0.5;
    mtwnNode slow;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &slow) == mtwn_success, "a slow timeline");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.parent = slow;
    track.timing.iterations = HUGE_VAL;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
          "an endless child of a timeline played forwards at half speed");
    mtwnDestroyContext(context);
}

#if defined(__x86_64__) || defined(_M_X64)
static const uint64_t s_hostSetting = 0x1f80u | (1u << 15) | (1u << 6) | (3u << 13);
#elif defined(__aarch64__) || defined(_M_ARM64)
static const uint64_t s_hostSetting = (1ull << 24) | (3ull << 22);
#else
static const uint64_t s_hostSetting = 0;
#endif

// A curved track's lengths and samples, hashed, under a host setting.
static uint64_t Scenario(uint64_t host, int* restoredOut)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[4] = {Key(0.0, 0x1p-130f), Key(0.3, 2.0f), Key(0.7, -1.0f), Key(1.0, 3.0f)};
    for (int i = 0; i < 4; ++i)
    {
        keys[i].kind = mtwn_segmentCatmullRom;
        keys[i].value.y = (float)i * 0.3f;
    }
    mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
    def.track = mtwnDefaultTrackDef(mtwn_valueVec2);
    def.keyframes = keys;
    def.keyframeCount = 4;
    def.constantSpeed = true;
    uint64_t saved = mtwnFloatControlRead();
    mtwnFloatControlWrite(host);
    mtwnNode node;
    int restored = mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_success &&
                   mtwnFloatControlRead() == host;
    mtwnFloatControlWrite(saved);
    uint64_t hash = 0xcbf29ce484222325u;
    const mtwnClipSlot* slot = mtwnClipAt(context, clip.index1);
    unsigned char bytes[4 * sizeof(double)];
    memcpy(bytes, slot->lengths, sizeof bytes);
    for (size_t b = 0; b < sizeof bytes; ++b)
    {
        hash = (hash ^ bytes[b]) * 0x100000001b3u;
    }
    mtwnDestroyContext(context);
    *restoredOut = restored;
    return hash;
}

static void TestFloatEnvironment(void)
{
    int restored = 0;
    uint64_t plain = Scenario(mtwnFloatControlRead(), &restored);
    CHECK(restored, "the default kept");
    int hostileRestored = 0;
    uint64_t hostile =
        Scenario(s_hostSetting == 0 ? mtwnFloatControlRead() : s_hostSetting, &hostileRestored);
    CHECK(hostileRestored, "the host's setting left as it was");
    CHECK(plain == hostile, "the same lengths under flush-to-zero and rounding toward zero");
}

// The builder's limits at their edges, found by mutants: a track taking
// the last keyframes fits, a capacity of MTWN_MAX_COUNT is one, and a
// node outside the clip is refused.
static void TestLimitEdges(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 2, 1u << 16);
    mtwnClipDef def = mtwnDefaultClipDef();
    def.keyframeCapacity = 4;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &def, &clip) == mtwn_success, "a clip of 4 keyframes");
    mtwnKeyframe two[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnKeyframe three[3] = {Key(0.0, 0.0f), Key(0.5, 1.0f), Key(1.0, 0.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = two;
    track.keyframeCount = 2;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "two keyframes");
    track.keyframes = three;
    track.keyframeCount = 3;
    track.slot = 1;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorLimit,
          "three more past the last refused");
    track.keyframes = two;
    track.keyframeCount = 2;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
          "two taking the last fit");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnClipNodeInfo info;
    uint32_t nodes = mtwnClipAt(context, clip.index1)->nodeCount;
    CHECK(mtwnClip_GetNode(context, clip, 0, &info) == mtwn_errorInvalid &&
              mtwnClip_GetNode(context, clip, nodes + 1, &info) == mtwn_errorInvalid &&
              mtwnClip_GetNode(context, clip, nodes, &info) == mtwn_success,
          "node 0 and past the last refused, the last read");
    mtwnDestroyContext(context);

    mtwnClipDef largest = mtwnDefaultClipDef();
    largest.keyframeCapacity = MTWN_MAX_COUNT;
    uint64_t bytes = 0;
    CHECK(mtwnGetClipBytes(&largest, &bytes) == mtwn_success && bytes > MTWN_MAX_COUNT,
          "a capacity of MTWN_MAX_COUNT sized");
    largest.keyframeCapacity = MTWN_MAX_COUNT + 1;
    CHECK(mtwnGetClipBytes(&largest, &bytes) == mtwn_errorInvalid, "one past it refused");
}

static void TestDefaults(void)
{
    // The default context holds as many default clips as it has clip
    // slots, and mtwnGetClipBytes gives what each counts against it.
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "the default context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    uint64_t bytes = 0;
    CHECK(mtwnGetClipBytes(&clipDef, &bytes) == mtwn_success && bytes > 0 && bytes <= 13312,
          "a default clip within 13 KiB");
    bool made = true;
    for (uint32_t i = 0; i < def.clipCapacity; ++i)
    {
        mtwnClipId clip;
        made = made && mtwnCreateClip(context, &clipDef, &clip) == mtwn_success;
    }
    CHECK(made, "every clip slot filled with a default clip");
    CHECK(context->clipStorageUsed == bytes * def.clipCapacity, "each counted as said");
    mtwnDestroyContext(context);
    // Bigger defs take more; bad defs are refused.
    clipDef.keyframeCapacity *= 2;
    uint64_t more = 0;
    CHECK(mtwnGetClipBytes(&clipDef, &more) == mtwn_success && more > bytes, "more keyframes");
    CHECK(mtwnGetClipBytes(NULL, &more) == mtwn_errorInvalid &&
              mtwnGetClipBytes(&clipDef, NULL) == mtwn_errorInvalid,
          "null arguments refused");
    clipDef.version += 1;
    CHECK(mtwnGetClipBytes(&clipDef, &more) == mtwn_errorVersion && more == 0,
          "another version refused");
    clipDef = mtwnDefaultClipDef();
    clipDef.cookie = 0;
    CHECK(mtwnGetClipBytes(&clipDef, &more) == mtwn_errorInvalid, "a def without its cookie");
    clipDef = mtwnDefaultClipDef();
    clipDef.slotCapacity = MTWN_MAX_COUNT + 1;
    CHECK(mtwnGetClipBytes(&clipDef, &more) == mtwn_errorInvalid, "a count past the limit");
}

int main(void)
{
    TestLimitEdges();
    TestDefaults();
    TestStorage();
    TestPositions();
    TestTracks();
    TestRefusals();
    TestTimelines();
    TestNesting();
    TestEndless();
    TestDepth();
    TestTimelineRefusals();
    TestSlowEndless();
    TestFloatEnvironment();
    return s_failures == 0 ? 0 : 1;
}
