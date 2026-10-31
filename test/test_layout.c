// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Layouts: the size and every field's offset of each def, of the structs
// a def holds and of a binding and an event record, on the three data
// models CI builds: 64-bit, 32-bit x86 (doubles aligned to 4 in a struct)
// and WebAssembly (32-bit, doubles aligned to 8). A change in any of them
// changes the ABI, and the pull request that makes it says so.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/path.h"
#include "maul-tween/player.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <stddef.h>
#include <stdint.h>

typedef struct Pinned
{
    const char* name;
    size_t value;
    // 64-bit, 32-bit with doubles at 4, 32-bit with doubles at 8.
    size_t expected[3];
} Pinned;

typedef struct DoubleAfterChar
{
    char c;
    double d;
} DoubleAfterChar;

static int DataModel(void)
{
    if (sizeof(void*) == 8)
    {
        return 0;
    }
    return offsetof(DoubleAfterChar, d) == 4 ? 1 : 2;
}

int main(void)
{
    static const Pinned s_pinned[] = {
        {"mtwnContextDef", sizeof(mtwnContextDef), {96, 72, 72}},
        {"mtwnContextDef.cookie", offsetof(mtwnContextDef, cookie), {0, 0, 0}},
        {"mtwnContextDef.version", offsetof(mtwnContextDef, version), {4, 4, 4}},
        {"mtwnContextDef.allocator", offsetof(mtwnContextDef, allocator), {8, 8, 8}},
        {"mtwnContextDef.enqueueTask", offsetof(mtwnContextDef, enqueueTask), {32, 20, 20}},
        {"mtwnContextDef.finishTask", offsetof(mtwnContextDef, finishTask), {40, 24, 24}},
        {"mtwnContextDef.userTaskContext", offsetof(mtwnContextDef, userTaskContext), {48, 28, 28}},
        {"mtwnContextDef.targetCapacity", offsetof(mtwnContextDef, targetCapacity), {56, 32, 32}},
        {"mtwnContextDef.playerCapacity", offsetof(mtwnContextDef, playerCapacity), {60, 36, 36}},
        {"mtwnContextDef.clipCapacity", offsetof(mtwnContextDef, clipCapacity), {64, 40, 40}},
        {"mtwnContextDef.eventCapacity", offsetof(mtwnContextDef, eventCapacity), {68, 44, 44}},
        {"mtwnContextDef.bindingCapacity", offsetof(mtwnContextDef, bindingCapacity), {72, 48, 48}},
        {"mtwnContextDef.sharedNodeCapacity",
         offsetof(mtwnContextDef, sharedNodeCapacity),
         {76, 52, 52}},
        {"mtwnContextDef.sharedRecordCapacity",
         offsetof(mtwnContextDef, sharedRecordCapacity),
         {80, 56, 56}},
        {"mtwnContextDef.pointsPerTransition",
         offsetof(mtwnContextDef, pointsPerTransition),
         {84, 60, 60}},
        {"mtwnContextDef.clipStorageBytes",
         offsetof(mtwnContextDef, clipStorageBytes),
         {88, 64, 64}},
        {"mtwnClipDef", sizeof(mtwnClipDef), {20, 20, 20}},
        {"mtwnClipDef.cookie", offsetof(mtwnClipDef, cookie), {0, 0, 0}},
        {"mtwnClipDef.nodeCapacity", offsetof(mtwnClipDef, nodeCapacity), {4, 4, 4}},
        {"mtwnClipDef.keyframeCapacity", offsetof(mtwnClipDef, keyframeCapacity), {8, 8, 8}},
        {"mtwnClipDef.slotCapacity", offsetof(mtwnClipDef, slotCapacity), {12, 12, 12}},
        {"mtwnClipDef.pointCapacity", offsetof(mtwnClipDef, pointCapacity), {16, 16, 16}},
        {"mtwnClipTrackDef", sizeof(mtwnClipTrackDef), {232, 204, 216}},
        {"mtwnClipTrackDef.cookie", offsetof(mtwnClipTrackDef, cookie), {0, 0, 0}},
        {"mtwnClipTrackDef.parent", offsetof(mtwnClipTrackDef, parent), {4, 4, 4}},
        {"mtwnClipTrackDef.slot", offsetof(mtwnClipTrackDef, slot), {8, 8, 8}},
        {"mtwnClipTrackDef.keyframeCount", offsetof(mtwnClipTrackDef, keyframeCount), {12, 12, 12}},
        {"mtwnClipTrackDef.position", offsetof(mtwnClipTrackDef, position), {16, 16, 16}},
        {"mtwnClipTrackDef.track", offsetof(mtwnClipTrackDef, track), {32, 32, 32}},
        {"mtwnClipTrackDef.timing", offsetof(mtwnClipTrackDef, timing), {64, 60, 64}},
        {"mtwnClipTrackDef.keyframes", offsetof(mtwnClipTrackDef, keyframes), {216, 196, 208}},
        {"mtwnClipTrackDef.constantSpeed",
         offsetof(mtwnClipTrackDef, constantSpeed),
         {224, 200, 212}},
        {"mtwnClipTrackDef.incremental", offsetof(mtwnClipTrackDef, incremental), {225, 201, 213}},
        {"mtwnClipTimelineDef", sizeof(mtwnClipTimelineDef), {184, 164, 176}},
        {"mtwnClipTimelineDef.cookie", offsetof(mtwnClipTimelineDef, cookie), {0, 0, 0}},
        {"mtwnClipTimelineDef.parent", offsetof(mtwnClipTimelineDef, parent), {4, 4, 4}},
        {"mtwnClipTimelineDef.autoDuration",
         offsetof(mtwnClipTimelineDef, autoDuration),
         {8, 8, 8}},
        {"mtwnClipTimelineDef.position", offsetof(mtwnClipTimelineDef, position), {16, 12, 16}},
        {"mtwnClipTimelineDef.timing", offsetof(mtwnClipTimelineDef, timing), {32, 28, 32}},
        {"mtwnPosition", sizeof(mtwnPosition), {16, 16, 16}},
        {"mtwnPosition.offset", offsetof(mtwnPosition, offset), {0, 0, 0}},
        {"mtwnPosition.label", offsetof(mtwnPosition, label), {8, 8, 8}},
        {"mtwnPosition.kind", offsetof(mtwnPosition, kind), {12, 12, 12}},
        {"mtwnClipPlayerDef", sizeof(mtwnClipPlayerDef), {48, 36, 40}},
        {"mtwnClipPlayerDef.cookie", offsetof(mtwnClipPlayerDef, cookie), {0, 0, 0}},
        {"mtwnClipPlayerDef.clip", offsetof(mtwnClipPlayerDef, clip), {4, 4, 4}},
        {"mtwnClipPlayerDef.bindingCount", offsetof(mtwnClipPlayerDef, bindingCount), {12, 12, 12}},
        {"mtwnClipPlayerDef.bindings", offsetof(mtwnClipPlayerDef, bindings), {16, 16, 16}},
        {"mtwnClipPlayerDef.play", offsetof(mtwnClipPlayerDef, play), {24, 20, 20}},
        {"mtwnClipPlayerDef.keep", offsetof(mtwnClipPlayerDef, keep), {25, 21, 21}},
        {"mtwnClipPlayerDef.essential", offsetof(mtwnClipPlayerDef, essential), {26, 22, 22}},
        {"mtwnClipPlayerDef.rate", offsetof(mtwnClipPlayerDef, rate), {32, 24, 24}},
        {"mtwnClipPlayerDef.weight", offsetof(mtwnClipPlayerDef, weight), {40, 32, 32}},
        {"mtwnBinding", sizeof(mtwnBinding), {20, 20, 20}},
        {"mtwnBinding.slot", offsetof(mtwnBinding, slot), {0, 0, 0}},
        {"mtwnBinding.composite", offsetof(mtwnBinding, composite), {4, 4, 4}},
        {"mtwnBinding.target", offsetof(mtwnBinding, target), {8, 8, 8}},
        {"mtwnBinding.weight", offsetof(mtwnBinding, weight), {16, 16, 16}},
        {"mtwnTransitionDef", sizeof(mtwnTransitionDef), {280, 256, 272}},
        {"mtwnTransitionDef.cookie", offsetof(mtwnTransitionDef, cookie), {0, 0, 0}},
        {"mtwnTransitionDef.target", offsetof(mtwnTransitionDef, target), {4, 4, 4}},
        {"mtwnTransitionDef.colorSpace", offsetof(mtwnTransitionDef, colorSpace), {12, 12, 12}},
        {"mtwnTransitionDef.hueMethod", offsetof(mtwnTransitionDef, hueMethod), {16, 16, 16}},
        {"mtwnTransitionDef.angleMethod", offsetof(mtwnTransitionDef, angleMethod), {20, 20, 20}},
        {"mtwnTransitionDef.rotationPath", offsetof(mtwnTransitionDef, rotationPath), {24, 24, 24}},
        {"mtwnTransitionDef.value", offsetof(mtwnTransitionDef, value), {28, 28, 28}},
        {"mtwnTransitionDef.velocity", offsetof(mtwnTransitionDef, velocity), {44, 44, 44}},
        {"mtwnTransitionDef.delay", offsetof(mtwnTransitionDef, delay), {64, 60, 64}},
        {"mtwnTransitionDef.duration", offsetof(mtwnTransitionDef, duration), {72, 68, 72}},
        {"mtwnTransitionDef.easing", offsetof(mtwnTransitionDef, easing), {80, 76, 80}},
        {"mtwnTransitionDef.spring", offsetof(mtwnTransitionDef, spring), {160, 144, 152}},
        {"mtwnTransitionDef.decay", offsetof(mtwnTransitionDef, decay), {208, 188, 200}},
        {"mtwnTransitionDef.lower", offsetof(mtwnTransitionDef, lower), {216, 196, 208}},
        {"mtwnTransitionDef.upper", offsetof(mtwnTransitionDef, upper), {232, 212, 224}},
        {"mtwnTransitionDef.restOffset", offsetof(mtwnTransitionDef, restOffset), {248, 228, 240}},
        {"mtwnTransitionDef.restSpeed", offsetof(mtwnTransitionDef, restSpeed), {256, 236, 248}},
        {"mtwnTransitionDef.kind", offsetof(mtwnTransitionDef, kind), {264, 244, 256}},
        {"mtwnTransitionDef.velocitySource",
         offsetof(mtwnTransitionDef, velocitySource),
         {268, 248, 260}},
        {"mtwnTransitionDef.reverses", offsetof(mtwnTransitionDef, reverses), {272, 252, 264}},
        {"mtwnTransitionDef.essential", offsetof(mtwnTransitionDef, essential), {273, 253, 265}},
        {"mtwnTransitionDef.bounded", offsetof(mtwnTransitionDef, bounded), {274, 254, 266}},
        {"mtwnTransitionDef.lands", offsetof(mtwnTransitionDef, lands), {275, 255, 267}},
        {"mtwnTrackDef", sizeof(mtwnTrackDef), {32, 28, 32}},
        {"mtwnTrackDef.alpha", offsetof(mtwnTrackDef, alpha), {0, 0, 0}},
        {"mtwnTrackDef.kind", offsetof(mtwnTrackDef, kind), {8, 8, 8}},
        {"mtwnTrackDef.colorSpace", offsetof(mtwnTrackDef, colorSpace), {12, 12, 12}},
        {"mtwnTrackDef.hueMethod", offsetof(mtwnTrackDef, hueMethod), {16, 16, 16}},
        {"mtwnTrackDef.angleMethod", offsetof(mtwnTrackDef, angleMethod), {20, 20, 20}},
        {"mtwnTrackDef.rotationPath", offsetof(mtwnTrackDef, rotationPath), {24, 24, 24}},
        {"mtwnTiming", sizeof(mtwnTiming), {152, 136, 144}},
        {"mtwnTiming.delay", offsetof(mtwnTiming, delay), {0, 0, 0}},
        {"mtwnTiming.endDelay", offsetof(mtwnTiming, endDelay), {8, 8, 8}},
        {"mtwnTiming.iterationStart", offsetof(mtwnTiming, iterationStart), {16, 16, 16}},
        {"mtwnTiming.iterations", offsetof(mtwnTiming, iterations), {24, 24, 24}},
        {"mtwnTiming.duration", offsetof(mtwnTiming, duration), {32, 32, 32}},
        {"mtwnTiming.repeatDelay", offsetof(mtwnTiming, repeatDelay), {40, 40, 40}},
        {"mtwnTiming.playbackRate", offsetof(mtwnTiming, playbackRate), {48, 48, 48}},
        {"mtwnTiming.easing", offsetof(mtwnTiming, easing), {56, 56, 56}},
        {"mtwnTiming.fill", offsetof(mtwnTiming, fill), {136, 124, 128}},
        {"mtwnTiming.direction", offsetof(mtwnTiming, direction), {140, 128, 132}},
        {"mtwnTiming.alternateEasing", offsetof(mtwnTiming, alternateEasing), {144, 132, 136}},
        {"mtwnRoughDef", sizeof(mtwnRoughDef), {104, 92, 96}},
        {"mtwnRoughDef.shape", offsetof(mtwnRoughDef, shape), {0, 0, 0}},
        {"mtwnRoughDef.strength", offsetof(mtwnRoughDef, strength), {80, 68, 72}},
        {"mtwnRoughDef.seed", offsetof(mtwnRoughDef, seed), {88, 76, 80}},
        {"mtwnRoughDef.taper", offsetof(mtwnRoughDef, taper), {96, 84, 88}},
        {"mtwnRoughDef.flags", offsetof(mtwnRoughDef, flags), {100, 88, 92}},
        {"mtwnEvent", sizeof(mtwnEvent), {64, 64, 64}},
        {"mtwnEvent.hostTime", offsetof(mtwnEvent, hostTime), {0, 0, 0}},
        {"mtwnEvent.clipTime", offsetof(mtwnEvent, clipTime), {8, 8, 8}},
        {"mtwnEvent.firstIteration", offsetof(mtwnEvent, firstIteration), {16, 16, 16}},
        {"mtwnEvent.lastIteration", offsetof(mtwnEvent, lastIteration), {24, 24, 24}},
        {"mtwnEvent.count", offsetof(mtwnEvent, count), {32, 32, 32}},
        {"mtwnEvent.player", offsetof(mtwnEvent, player), {40, 40, 40}},
        {"mtwnEvent.node", offsetof(mtwnEvent, node), {48, 48, 48}},
        {"mtwnEvent.key", offsetof(mtwnEvent, key), {52, 52, 52}},
        {"mtwnEvent.kind", offsetof(mtwnEvent, kind), {56, 56, 56}},
        {"mtwnEvent.backwards", offsetof(mtwnEvent, backwards), {60, 60, 60}},
    };
    int model = DataModel();
    uint32_t wrong = 0;
    for (size_t i = 0; i < sizeof s_pinned / sizeof s_pinned[0]; ++i)
    {
        if (s_pinned[i].value != s_pinned[i].expected[model])
        {
            printf("%s: %zu, pinned %zu\n", s_pinned[i].name, s_pinned[i].value,
                   s_pinned[i].expected[model]);
            wrong += 1;
        }
    }
    CHECK(wrong == 0, "every size and offset as pinned");
    return s_failures == 0 ? 0 : 1;
}
