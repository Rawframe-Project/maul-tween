// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Clips: one block
// per clip through the host's allocator within the context's clip
// storage, tracks made in it as added and read back through the values
// layer, positions resolved to start times, the states a clip passes, and
// the same bits under a host's flush-to-zero.

#include "test_fenv.h"
#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/clip.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"

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
    uint64_t before = counter.live;
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_success && counter.allocations == 2,
          "one block for the clip");
    uint64_t one = 0;
    CHECK(mtwnContext_GetClipBytes(context, &def, &one) == mtwn_success && one > 0 &&
              counter.live - before >= one,
          "its block holds what the def takes");
    CHECK(mtwnCreateClip(context, &def, &b) == mtwn_success, "a second");
    CHECK(mtwnCreateClip(context, &def, &c) == mtwn_errorCapacity && c.index1 == 0,
          "the pool of clips full");
    CHECK(mtwnDestroyClip(context, a) == mtwn_success && counter.frees == 1,
          "destroyed, its block freed");
    CHECK(!mtwnClip_IsValid(context, a) && mtwnClip_IsValid(context, b), "stale and alive");
    CHECK(mtwnDestroyClip(context, a) == mtwn_errorStale, "a stale destroy");
    mtwnDestroyContext(context);
    CHECK(counter.frees == counter.allocations && counter.live == 0,
          "the context frees the clips it still holds");
    // Clip storage is a limit.
    counter = (Counter){0};
    context = Make(&counter, 4, one + one / 2);
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_success, "one fits");
    CHECK(mtwnCreateClip(context, &def, &b) == mtwn_errorCapacity && counter.allocations == 2,
          "a second would pass the clip storage, nothing allocated");
    CHECK(mtwnContext_GetMisuse(context) == 0, "a limit is not misuse");
    mtwnDestroyContext(context);
    // Two that fill it to the byte both fit.
    context = Make(&counter, 4, 2 * one);
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_success &&
              mtwnCreateClip(context, &def, &b) == mtwn_success,
          "the storage filled exactly");
    CHECK(mtwnDestroyClip(context, a) == mtwn_success &&
              mtwnCreateClip(context, &def, &c) == mtwn_success,
          "a destroyed clip's storage back for another");
    mtwnDestroyContext(context);
    // A failed allocation gives its slot back; a used clip is busy.
    counter = (Counter){0};
    context = Make(&counter, 1, 1 << 20);
    counter.fail = true;
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_errorCapacity && a.index1 == 0,
          "the allocator fails");
    counter.fail = false;
    CHECK(mtwnCreateClip(context, &def, &a) == mtwn_success, "the only slot still free");
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = a;
    playerDef.keep = true;
    mtwnPlayerId player = {0, 0};
    CHECK(mtwnClip_Seal(context, a) == mtwn_success &&
              mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success,
          "a player of it");
    CHECK(mtwnDestroyClip(context, a) == mtwn_errorBusy && mtwnClip_IsValid(context, a),
          "a clip a player uses is busy");
    CHECK(mtwnDestroyPlayer(context, player) == mtwn_success &&
              mtwnDestroyClip(context, a) == mtwn_success,
          "and free to go with its player gone");
    mtwnDestroyContext(context);
}

static void TestPositions(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 5, 1 << 20);
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
          "nothing is read before it is sealed");
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
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
              mtwnClip_Seal(context, timed) == mtwn_success &&
              mtwnClip_GetDuration(context, timed, &duration) == mtwn_success && duration == 3.25,
          "a child's delay, iterations and end delay make its end");
    CHECK(mtwnClip_GetTrack(context, clip, nodes[4], &track) == mtwn_success &&
              track.timing.iterations == 3.0 && track.slot == 1,
          "its timing and slot");
    mtwnClipNodeInfo info;
    CHECK(mtwnClip_GetNode(context, clip, nodes[4], &info) == mtwn_success && info.slot == 1 &&
              info.kind == mtwn_nodeTrack,
          "its node: a track in slot 1");
    // A first child with or after a previous one that is not there counts
    // from the timeline's start.
    static const mtwnPositionKind firsts[2] = {mtwn_positionWithPrevious,
                                               mtwn_positionAfterPrevious};
    int fromStart = 0;
    for (int i = 0; i < 2; ++i)
    {
        mtwnClipId alone;
        CHECK(mtwnCreateClip(context, &clipDef, &alone) == mtwn_success, "a clip");
        def.position = At(firsts[i], 0.25);
        CHECK(mtwnClip_AddTrack(context, alone, &def, &nodes[0]) == mtwn_success &&
                  mtwnClip_Seal(context, alone) == mtwn_success,
              "a first child");
        fromStart += mtwnClip_GetTrack(context, alone, nodes[0], &track) == mtwn_success &&
                     track.start == 0.25;
    }
    CHECK(fromStart == 2, "with or after no previous child: from 0");
    // After the end with an offset: a gap past the children's end.
    mtwnClipId gapped;
    CHECK(mtwnCreateClip(context, &clipDef, &gapped) == mtwn_success, "a clip");
    def = mtwnDefaultClipTrackDef();
    def.keyframes = keys;
    def.keyframeCount = 2;
    CHECK(mtwnClip_AddTrack(context, gapped, &def, &nodes[0]) == mtwn_success, "0 to 1");
    def.position = At(mtwn_positionAfterEnd, 0.5);
    CHECK(mtwnClip_AddTrack(context, gapped, &def, &nodes[1]) == mtwn_success &&
              mtwnClip_Seal(context, gapped) == mtwn_success &&
              mtwnClip_GetTrack(context, gapped, nodes[1], &track) == mtwn_success &&
              track.start == 1.5,
          "half a second after the end: 1.5");
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
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
    mtwnClipTrack track;
    CHECK(mtwnClip_GetTrack(context, clip, node, &track) == mtwn_success, "read");
    mtwnTrack direct;
    double lengths[3];
    CHECK(mtwnMakeTrack(&def.track, given, 3, lengths, NULL, &direct) == mtwn_success, "direct");
    int same = 1;
    for (int k = 0; k <= 50; ++k)
    {
        mtwnVec4 a = SampleTrack(&track.track, k / 50.0);
        mtwnVec4 b = SampleTrack(&direct, k / 50.0);
        same &= memcmp(&a, &b, sizeof a) == 0;
    }
    CHECK(same, "the clip's track samples as the values layer's");
    mtwnVec4 half = SampleTrack(&track.track, 0.5);
    CHECK(fabs((double)half.x - (350 + 40) * pi / 180) <= 1e-5,
          "at constant speed: half of 80 degrees, not the keyframe at 0.5");
    CHECK(mtwnClip_GetTrack(context, clip, MTWN_ROOT_NODE, &track) == mtwn_errorInvalid,
          "the root is no track");
    CHECK(mtwnClip_GetTrack(context, clip, 9, &track) == mtwn_errorInvalid, "no such node");
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorState && node == 0,
          "nothing is added once sealed");
    CHECK(mtwnClip_Seal(context, clip) == mtwn_errorState, "sealed once");
    CHECK(mtwnContext_GetMisuse(context) == 4, "each refusal counted");
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
    clipDef.keyframeCapacity = MTWN_MAX_COUNT + 1;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_errorLimit, "a capacity too large");
    clipDef = mtwnDefaultClipDef();
    clipDef.pointCapacity = MTWN_MAX_COUNT + 1;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_errorLimit,
          "a point capacity too large");
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
    uint64_t misuse = mtwnContext_GetMisuse(context);
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
    CHECK(mtwnContext_GetMisuse(context) == misuse + 10, "each counted");
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
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorCapacity, "the nodes full");
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
    int intact = 1;
    for (int i = 0; i < 3; ++i)
    {
        mtwnClipTrack track;
        intact &= mtwnClip_GetTrack(context, clip, filled[i], &track) == mtwn_success &&
                  SampleTrack(&track.track, 1.0).x == (float)(i + 1);
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
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_errorCapacity,
          "two more do not fit");
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
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
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
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
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
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
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
    uint64_t misuse = mtwnContext_GetMisuse(context);
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &deeper) == mtwn_errorLimit && deeper == 0 &&
              mtwnContext_GetMisuse(context) == misuse + 1,
          "and no deeper: past MTWN_MAX_CLIP_DEPTH, counted as misuse");
    CHECK(mtwnClip_AddMark(context, clip, parent, At(mtwn_positionAbsolute, 0.0), 1, &deeper) ==
              mtwn_success,
          "though instants go in the deepest");
    // A full clip refuses a timeline for room, which is not misuse.
    mtwnClipId full;
    clipDef.nodeCapacity = 0;
    group.parent = MTWN_ROOT_NODE;
    misuse = mtwnContext_GetMisuse(context);
    CHECK(mtwnCreateClip(context, &clipDef, &full) == mtwn_success &&
              mtwnClip_AddTimeline(context, full, &group, &deeper) == mtwn_errorCapacity &&
              deeper == 0 && mtwnContext_GetMisuse(context) == misuse,
          "a clip of no room refuses a timeline, not as misuse");
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
              mtwn_errorCapacity,
          "labels count among the nodes");
    group = mtwnDefaultClipTimelineDef();
    group.cookie = 0;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &node) == mtwn_errorInvalid, "no cookie");
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
    CHECK(mtwnClip_AddMark(context, clip, MTWN_ROOT_NODE, At(mtwn_positionAbsolute, 0.0), 1,
                           &node) == mtwn_errorState,
          "nothing added once sealed");
    // An endless repeat of a timeline played backwards and lasting as its
    // children do: empty it lasts 0, an end to start from, and it is
    // taken; a child giving it a duration leaves it endless, refused.
    mtwnClipId repeated;
    clipDef = mtwnDefaultClipDef();
    CHECK(mtwnCreateClip(context, &clipDef, &repeated) == mtwn_success, "a clip");
    group = mtwnDefaultClipTimelineDef();
    group.autoDuration = true;
    group.timing.playbackRate = -1.0;
    group.timing.iterations = HUGE_VAL;
    CHECK(mtwnClip_AddTimeline(context, repeated, &group, &backward) == mtwn_success,
          "an endless repeat of nothing, backwards");
    track.parent = backward;
    track.timing.iterations = 1.0;
    CHECK(mtwnClip_AddTrack(context, repeated, &track, &node) == mtwn_errorInvalid,
          "a child would make it endless");
    mtwnClipTimelineDef inner = mtwnDefaultClipTimelineDef();
    inner.parent = backward;
    inner.autoDuration = false;
    inner.timing.duration = 1.0;
    CHECK(mtwnClip_AddTimeline(context, repeated, &inner, &node) == mtwn_errorInvalid,
          "a timeline child too");
    mtwnClipNodeInfo info;
    CHECK(mtwnClip_Seal(context, repeated) == mtwn_success &&
              mtwnClip_GetNode(context, repeated, backward, &info) == mtwn_success &&
              info.timing.duration == 0.0,
          "the refused children left it lasting 0");
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

// A timeline lasting as long as its children takes a timing its def
// would be refused with: a child that would give it a cycle or an active
// duration past a double is refused, and leaves it as it was.
static void TestOverflowingChildren(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 4, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = true;
    group.timing.repeatDelay = 1e308;
    group.timing.iterations = 2.0;
    mtwnNode repeating;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &repeating) == mtwn_success,
          "repeating with a long delay, lasting as its children");
    group.timing.repeatDelay = 0.0;
    group.timing.iterations = 1e300;
    mtwnNode many;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &many) == mtwn_success,
          "1e300 iterations, lasting as its children");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1e308;
    track.parent = repeating;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "1e308 s and a 1e308 s delay: a cycle past a double");
    track.timing.duration = 1e300;
    track.parent = many;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "1e300 iterations of 1e300 s: an active duration past a double");
    track.timing.duration = 1.0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
          "1e300 iterations of 1 s");
    // Inside a timeline of its own duration, so the clip's end stays
    // within MTWN_SEEK_LIMIT.
    group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = 1.0;
    mtwnNode fixed;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &fixed) == mtwn_success, "a timeline of 1 s");
    group.autoDuration = true;
    group.timing.duration = 0.0;
    group.timing.delay = 1e308;
    group.parent = fixed;
    mtwnNode late;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &late) == mtwn_success,
          "a 1e308 s delay, lasting as its children");
    track.timing.duration = 1e308;
    track.parent = late;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorInvalid,
          "a 1e308 s child after it: an end past a double");
    mtwnClipNodeInfo info;
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success &&
              mtwnClip_GetNode(context, clip, repeating, &info) == mtwn_success &&
              info.timing.duration == 0.0,
          "the refused child left it lasting 0");
    mtwnDestroyContext(context);
}

// A clip's finite end is at most MTWN_SEEK_LIMIT, so a player's start
// time stays finite: a node that would take it past is refused, and an
// endless one is taken.
static void TestEndLimit(void)
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
    track.timing.duration = MTWN_SEEK_LIMIT;
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
          "a track ending at the limit");
    track.timing.duration = nextafter(MTWN_SEEK_LIMIT, HUGE_VAL);
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorLimit && node == 0,
          "one ending an ulp past it, refused");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.timing.delay = 1e308;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &node) == mtwn_errorLimit,
          "a timeline whose delay takes the end past it, refused");
    track.position = (mtwnPosition){0};
    track.position.kind = mtwn_positionAbsolute;
    track.position.offset = MTWN_SEEK_LIMIT;
    track.timing.duration = 1.0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
          "a track of 1 s from the limit, whose end rounds to it");
    track.timing.duration = MTWN_SEEK_LIMIT * 0x1p-52;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorLimit,
          "one lasting an ulp of the limit, ending past it, refused");
    track.position.offset = 0.0;
    track.timing.duration = 1.0;
    track.timing.iterations = HUGE_VAL;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
          "an endless track, taken");
    double duration = 0.0;
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success &&
              mtwnClip_GetDuration(context, clip, &duration) == mtwn_success &&
              duration == HUGE_VAL,
          "the clip endless");
    CHECK(mtwnContext_GetMisuse(context) == 3, "each refusal counted");
    mtwnDestroyContext(context);
}

// A curved track's lengths and samples, hashed, under a host setting.
static uint64_t Scenario(TestFloatSetting host, int* restoredOut)
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
    TestFloatSetting saved = TestFloatGet();
    TestFloatSet(host);
    mtwnNode node;
    int restored = mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_success &&
                   TestFloatSame(TestFloatGet(), host);
    TestFloatSet(saved);
    // The lengths measured under the host's setting, as the sealed
    // clip's track reports them, and its samples at constant speed.
    uint64_t hash = 0xcbf29ce484222325u;
    mtwnClipTrack track;
    bool read = mtwnClip_Seal(context, clip) == mtwn_success &&
                mtwnClip_GetTrack(context, clip, node, &track) == mtwn_success;
    CHECK(read, "the sealed clip's track");
    // The lengths the track reports, and its samples.
    for (uint32_t k = 0; read && track.track.path.lengths != NULL && k < track.track.path.count;
         ++k)
    {
        unsigned char bytes[sizeof(double)];
        memcpy(bytes, &track.track.path.lengths[k], sizeof bytes);
        for (size_t b = 0; b < sizeof bytes; ++b)
        {
            hash = (hash ^ bytes[b]) * 0x100000001b3u;
        }
    }
    for (uint32_t k = 0; read && k <= 16; ++k)
    {
        mtwnVec4 sample = SampleTrack(&track.track, 0.0625 * (double)k);
        unsigned char bytes[sizeof sample];
        memcpy(bytes, &sample, sizeof bytes);
        for (size_t b = 0; b < sizeof bytes; ++b)
        {
            hash = (hash ^ bytes[b]) * 0x100000001b3u;
        }
    }
    mtwnDestroyContext(context);
    *restoredOut = restored;
    return hash;
}

static void TestFloatEnvironment(void)
{
    int restored = 0;
    uint64_t plain = Scenario(TestFloatGet(), &restored);
    CHECK(restored, "the default kept");
    for (int h = 0; h < TestFloatHostileCount(); ++h)
    {
        int hostileRestored = 0;
        uint64_t hostile = Scenario(TestFloatHostileAt(h), &hostileRestored);
        CHECK(hostileRestored, "the host's setting left as it was");
        CHECK(plain == hostile, "the same lengths under every rounding mode, with flush-to-zero "
                                "and denormals-are-zero off and on");
    }
}

// The builder's limits at their edges: a track taking
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
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_errorCapacity,
          "three more past the last refused");
    track.keyframes = two;
    track.keyframeCount = 2;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
          "two taking the last fit");
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
    mtwnClipNodeInfo info;
    // The last node is the last added: the root, then two tracks.
    uint32_t nodes = node;
    CHECK(mtwnClip_GetNode(context, clip, 0, &info) == mtwn_errorInvalid &&
              mtwnClip_GetNode(context, clip, nodes + 1, &info) == mtwn_errorInvalid &&
              mtwnClip_GetNode(context, clip, nodes, &info) == mtwn_success,
          "node 0 and past the last refused, the last read");
    mtwnClipDef largest = mtwnDefaultClipDef();
    largest.keyframeCapacity = MTWN_MAX_COUNT;
    uint64_t bytes = 0;
    CHECK(mtwnContext_GetClipBytes(context, &largest, &bytes) == mtwn_success &&
              bytes > MTWN_MAX_COUNT,
          "a capacity of MTWN_MAX_COUNT sized");
    largest.keyframeCapacity = MTWN_MAX_COUNT + 1;
    CHECK(mtwnContext_GetClipBytes(context, &largest, &bytes) == mtwn_errorLimit,
          "one past it refused");
    mtwnDestroyContext(context);
}

static void TestDefaults(void)
{
    // The default context holds as many default clips as it has clip
    // slots, and mtwnContext_GetClipBytes gives what each counts against it.
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "the default context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    uint64_t bytes = 0;
    CHECK(mtwnContext_GetClipBytes(context, &clipDef, &bytes) == mtwn_success && bytes > 0 &&
              bytes <= 14336,
          "a default clip within 14 KiB");
    bool made = true;
    for (uint32_t i = 0; i < def.clipCapacity; ++i)
    {
        mtwnClipId clip;
        made = made && mtwnCreateClip(context, &clipDef, &clip) == mtwn_success;
    }
    CHECK(made, "every clip slot filled with a default clip");
    // Each counts what mtwnContext_GetClipBytes says: a byte less storage than
    // all of them refuses the last.
    mtwnContextDef tight = def;
    tight.clipStorageBytes = bytes * def.clipCapacity - 1;
    mtwnContext* short_ = NULL;
    CHECK(mtwnCreateContext(&tight, &short_) == mtwn_success, "a context a byte short");
    uint32_t fitted = 0;
    mtwnClipId clip;
    while (fitted < def.clipCapacity && mtwnCreateClip(short_, &clipDef, &clip) == mtwn_success)
    {
        ++fitted;
    }
    CHECK(fitted == def.clipCapacity - 1 &&
              mtwnCreateClip(short_, &clipDef, &clip) == mtwn_errorCapacity,
          "each counted as said");
    mtwnDestroyContext(short_);
    // Bigger defs take more; bad defs are refused, each a misuse, and no
    // def is read without a context.
    clipDef.keyframeCapacity *= 2;
    uint64_t more = 0;
    CHECK(mtwnContext_GetClipBytes(context, &clipDef, &more) == mtwn_success && more > bytes,
          "more keyframes");
    uint64_t misuse = mtwnContext_GetMisuse(context);
    CHECK(mtwnContext_GetClipBytes(context, NULL, &more) == mtwn_errorInvalid &&
              mtwnContext_GetClipBytes(context, &clipDef, NULL) == mtwn_errorInvalid,
          "null arguments refused");
    clipDef.cookie = 0;
    CHECK(mtwnContext_GetClipBytes(context, &clipDef, &more) == mtwn_errorInvalid && more == 0,
          "a def without its cookie");
    clipDef = mtwnDefaultClipDef();
    clipDef.slotCapacity = MTWN_MAX_COUNT + 1;
    CHECK(mtwnContext_GetClipBytes(context, &clipDef, &more) == mtwn_errorLimit,
          "a count past the limit");
    CHECK(mtwnContext_GetMisuse(context) == misuse + 4, "each counted");
    CHECK(mtwnContext_GetClipBytes(NULL, &clipDef, &more) == mtwn_errorInvalid && more == 0,
          "no context, no def read");
    mtwnDestroyContext(context);
}

// A clip copies the linear() points of a track's timing and keyframes as
// they are added, within its point capacity: the host may then change or
// drop its own, and the clip samples as the easings did when added, to the
// bit. Points past the capacity are refused with mtwn_errorLimit, points
// the host put out of order with mtwn_errorInvalid, and a timeline's
// easing must not decrease in the points the clip keeps.
static void TestLinearPointsCopied(void)
{
    Counter counter = {0};
    mtwnContext* context = Make(&counter, 3, 1 << 20);
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.pointCapacity = 8;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnLinearPoint segment[3] = {{0.0, 0.0}, {0.4, 0.8}, {1.0, 1.0}};
    mtwnLinearPoint timing[3] = {{0.0, 0.0}, {0.5, 0.2}, {1.0, 1.0}};
    mtwnLinearPoint keptSegment[3];
    mtwnLinearPoint keptTiming[3];
    memcpy(keptSegment, segment, sizeof segment);
    memcpy(keptTiming, timing, sizeof timing);
    mtwnEasing easings[4];
    CHECK(mtwnMakeLinearPointsEasing(segment, 3, &easings[0]) == mtwn_success &&
              mtwnMakeLinearPointsEasing(timing, 3, &easings[1]) == mtwn_success &&
              mtwnMakeLinearPointsEasing(keptSegment, 3, &easings[2]) == mtwn_success &&
              mtwnMakeLinearPointsEasing(keptTiming, 3, &easings[3]) == mtwn_success,
          "linear() easings and copies of them");
    mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 10.0f)};
    keys[0].easing = easings[0];
    mtwnKeyframe given[2];
    memcpy(given, keys, sizeof keys);
    given[0].easing = easings[2];
    mtwnClipTrackDef def = mtwnDefaultClipTrackDef();
    def.keyframes = keys;
    def.keyframeCount = 2;
    def.timing.duration = 1.0;
    def.timing.easing = easings[1];
    mtwnNode node;
    CHECK(mtwnClip_AddTrack(context, clip, &def, &node) == mtwn_success,
          "a track eased by linear() in its timing and its segment");
    memset(segment, 0xFF, sizeof segment);
    memset(timing, 0xFF, sizeof timing);
    mtwnClipTrack track;
    mtwnTrack direct;
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success &&
              mtwnClip_GetTrack(context, clip, node, &track) == mtwn_success &&
              mtwnMakeTrack(&def.track, given, 2, NULL, NULL, &direct) == mtwn_success,
          "read, and the same track made from the copies");
    int same = 0;
    for (int k = 0; k <= 20; ++k)
    {
        mtwnVec4 a = SampleTrack(&track.track, k / 20.0);
        mtwnVec4 b = SampleTrack(&direct, k / 20.0);
        double eased = EaseAt(&track.timing.easing, k / 20.0, false);
        same += memcmp(&a, &b, sizeof a) == 0 && eased == EaseAt(&easings[3], k / 20.0, false);
    }
    CHECK(same == 21, "sampled by the clip's copies, the host's points gone");
    mtwnClipId small;
    clipDef.pointCapacity = 5;
    CHECK(mtwnCreateClip(context, &clipDef, &small) == mtwn_success, "a clip of 5 points");
    def.timing.easing = easings[3];
    keys[0].easing = easings[2];
    uint64_t misuse = mtwnContext_GetMisuse(context);
    CHECK(mtwnClip_AddTrack(context, small, &def, &node) == mtwn_errorCapacity && node == 0 &&
              mtwnContext_GetMisuse(context) == misuse,
          "6 points past 5 refused, not misuse");
    keptTiming[1].input = 1.5;
    CHECK(mtwnClip_AddTrack(context, small, &def, &node) == mtwn_errorInvalid &&
              mtwnContext_GetMisuse(context) == misuse + 1,
          "points the host put out of order refused as misuse");
    keptTiming[1].input = 0.5;
    def.timing.easing = mtwnLinearEasing();
    CHECK(mtwnClip_AddTrack(context, small, &def, &node) == mtwn_success,
          "3 points fit, the clip unchanged by the refusals");
    mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
    timeline.timing.easing = easings[3];
    keptTiming[1].output = -0.5;
    CHECK(mtwnClip_AddTimeline(context, small, &timeline, &node) == mtwn_errorInvalid,
          "a timeline whose points the host made decrease refused");
    keptTiming[1].output = 0.2;
    CHECK(mtwnClip_AddTimeline(context, small, &timeline, &node) == mtwn_errorCapacity,
          "a timeline's points past what is left refused");
    // A timeline's copies are kept: a track added after takes the points
    // after them.
    mtwnClipId third;
    clipDef.pointCapacity = 6;
    CHECK(mtwnCreateClip(context, &clipDef, &third) == mtwn_success, "a clip of 6 points");
    mtwnNode group;
    CHECK(mtwnClip_AddTimeline(context, third, &timeline, &group) == mtwn_success, "a timeline");
    def.timing.easing = mtwnLinearEasing();
    keys[0].easing = easings[2];
    keptSegment[1].output = 0.25;
    mtwnEasing other;
    CHECK(mtwnMakeLinearPointsEasing(keptSegment, 3, &other) == mtwn_success, "other points");
    keys[0].easing = other;
    CHECK(mtwnClip_AddTrack(context, third, &def, &node) == mtwn_success &&
              mtwnClip_Seal(context, third) == mtwn_success,
          "a track after it");
    mtwnClipNodeInfo info;
    CHECK(mtwnClip_GetNode(context, third, group, &info) == mtwn_success &&
              EaseAt(&info.timing.easing, 0.5, false) == EaseAt(&easings[3], 0.5, false),
          "the timeline eases by its own copy still");
    mtwnDestroyContext(context);
}

// Plays a clip of one track per linear() timing easing, each on its own
// slot and target, the tracks alike but for those easings; returns the
// last target's value at 0.3 seconds.
static float LastEasedBy(const mtwnEasing* easings, uint32_t count)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnBinding bindings[2];
    mtwnTargetId target = {0, 0};
    for (uint32_t i = 0; i < count; ++i)
    {
        mtwnKeyframe keys[2] = {Key(0.0, 0.0f), Key(1.0, 10.0f)};
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.slot = i;
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 1.0;
        track.timing.easing = easings[i];
        mtwnNode node;
        CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success &&
                  mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success,
              "a track");
        bindings[i] = mtwnMakeBinding(i, target);
    }
    mtwnPlayerDef playerDef = mtwnDefaultPlayerDef();
    playerDef.clip = clip;
    playerDef.bindings = bindings;
    playerDef.bindingCount = count;
    mtwnPlayerId player;
    mtwnVec4 value = {0};
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success &&
              mtwnCreatePlayer(context, &playerDef, &player) == mtwn_success &&
              mtwnContext_Update(context, 0, NULL, 0, NULL) == mtwn_success &&
              mtwnContext_Update(context, 300000000u, NULL, 0, NULL) == mtwn_success &&
              mtwnTarget_GetValue(context, target, &value) == mtwn_success,
          "played");
    mtwnDestroyContext(context);
    return value.x;
}

// Tracks alike but for their linear() timing easings' points do not share
// one timing's results: each eases by its own copy.
static void TestLinearPointsNotShared(void)
{
    mtwnLinearPoint first[3] = {{0.0, 0.0}, {0.5, 0.1}, {1.0, 1.0}};
    mtwnLinearPoint second[3] = {{0.0, 0.0}, {0.5, 0.9}, {1.0, 1.0}};
    mtwnEasing easings[2];
    CHECK(mtwnMakeLinearPointsEasing(first, 3, &easings[0]) == mtwn_success &&
              mtwnMakeLinearPointsEasing(second, 3, &easings[1]) == mtwn_success,
          "two easings of as many points");
    float alone = LastEasedBy(&easings[1], 1);
    float after = LastEasedBy(easings, 2);
    CHECK(memcmp(&alone, &after, sizeof alone) == 0 && alone > 5.0f,
          "the second track eased by its own points");
}

int main(void)
{
    TestLinearPointsCopied();
    TestLinearPointsNotShared();
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
    TestOverflowingChildren();
    TestEndLimit();
    TestFloatEnvironment();
    return s_failures == 0 ? 0 : 1;
}
