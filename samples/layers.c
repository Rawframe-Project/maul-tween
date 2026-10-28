// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Layered clips: an idle clip breathes a character's scale forever; a
// hit clip, added over it, squashes the scale for 0.3 s and goes when it
// ends; a wheel spins on by an incremental track, a quarter turn every
// half second. A second target plays the idle clip alone as the
// reference. Returns 0 when the hit adds exactly its own value to the
// idle one and leaves nothing behind, and the wheel's turn after three
// seconds is the closed form's.

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int s_failed;

static void Expect(int ok, const char* what)
{
    if (!ok)
    {
        printf("FAIL: %s\n", what);
        s_failed = 1;
    }
}

// A clip of one track on slot 0 through three values, linear between.
static mtwnClipId Clip(mtwnContext* context, mtwnValueKind kind, const mtwnVec4 values[3],
                       double seconds, double iterations, bool incremental)
{
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
    Expect(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[3];
    memset(keys, 0, sizeof keys);
    for (int i = 0; i < 3; ++i)
    {
        keys[i].value = values[i];
        keys[i].offset = 0.5 * i;
        keys[i].easing = mtwnLinearEasing();
    }
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.track.kind = kind;
    track.keyframes = keys;
    track.keyframeCount = 3;
    track.timing.duration = seconds;
    track.timing.iterations = iterations;
    track.incremental = incremental;
    mtwnNode node = 0;
    Expect(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
    Expect(mtwnClip_Seal(context, clip) == mtwn_success, "sealed");
    return clip;
}

static mtwnPlayerId Play(mtwnContext* context, mtwnClipId clip, mtwnTargetId target,
                         mtwnComposite composite, bool keep)
{
    mtwnBinding binding = mtwnBind(0, target);
    binding.composite = composite;
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = &binding;
    def.bindingCount = 1;
    def.keep = keep;
    mtwnPlayerId player = {0, 0};
    Expect(mtwnCreatePlayer(context, &def, &player) == mtwn_success, "a player");
    return player;
}

static mtwnVec4 Value(mtwnContext* context, mtwnTargetId target)
{
    mtwnTargetState state;
    Expect(mtwnTarget_GetState(context, target, &state) == mtwn_success, "read");
    return state.value;
}

int main(void)
{
    mtwnContext* context = NULL;
    mtwnContextDef def = mtwnDefaultContextDef();
    Expect(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    if (s_failed)
    {
        return 1;
    }
    static const mtwnVec4 breath[3] = {{1.0f, 0, 0, 0}, {1.03f, 0, 0, 0}, {1.0f, 0, 0, 0}};
    static const mtwnVec4 squash[3] = {{0.0f, 0, 0, 0}, {-0.12f, 0, 0, 0}, {0.0f, 0, 0, 0}};
    // A quarter turn about z, from no turn, by an eighth at the middle.
    float eighthSin = (float)sin(3.14159265358979323846 / 8.0);
    float eighthCos = (float)cos(3.14159265358979323846 / 8.0);
    float quarterSin = (float)sin(3.14159265358979323846 / 4.0);
    float quarterCos = (float)cos(3.14159265358979323846 / 4.0);
    const mtwnVec4 turn[3] = {
        {0, 0, 0, 1.0f}, {0, 0, eighthSin, eighthCos}, {0, 0, quarterSin, quarterCos}};
    mtwnClipId idle = Clip(context, mtwn_valueScalar, breath, 2.0, HUGE_VAL, false);
    mtwnClipId hit = Clip(context, mtwn_valueScalar, squash, 0.3, 1.0, false);
    mtwnClipId spin = Clip(context, mtwn_valueRotation, turn, 0.5, HUGE_VAL, true);

    mtwnTargetId scale = {0, 0};
    mtwnTargetId reference = {0, 0};
    mtwnTargetId wheel = {0, 0};
    Expect(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){1.0f, 0, 0, 0}, &scale) ==
                   mtwn_success &&
               mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){1.0f, 0, 0, 0}, &reference) ==
                   mtwn_success &&
               mtwnCreateTarget(context, mtwn_valueRotation, (mtwnVec4){0, 0, 0, 1.0f}, &wheel) ==
                   mtwn_success,
           "the targets");
    // Players compose in the order they were made: the idle one first.
    Play(context, idle, scale, mtwn_compositeReplace, true);
    Play(context, idle, reference, mtwn_compositeReplace, true);
    Play(context, spin, wheel, mtwn_compositeReplace, true);

    float deepest = 0.0f;
    bool hitStarted = false;
    for (uint32_t ms = 0; ms <= 3000; ms += 10)
    {
        if (ms == 1000)
        {
            // Not kept: it goes when it finishes.
            Play(context, hit, scale, mtwn_compositeAdd, false);
            hitStarted = true;
        }
        Expect(mtwnUpdate(context, (uint64_t)ms * 1000000u, NULL, 0, NULL) == mtwn_success,
               "an update");
        float layered = Value(context, scale).x;
        float alone = Value(context, reference).x;
        float added = layered - alone;
        deepest = added < deepest ? added : deepest;
        if (ms % 250 == 0 || (ms >= 1100 && ms <= 1200 && ms % 50 == 0))
        {
            printf("%4u ms  idle %.4f  with the hit %.4f\n", ms, (double)alone, (double)layered);
        }
        if (!hitStarted || ms > 1300)
        {
            Expect(layered == alone, "nothing added outside the hit");
        }
    }
    Expect(fabsf(deepest + 0.12f) < 1e-6f, "the hit's squash added at its depth");
    // Six quarter turns in three seconds: one and a half turns, z's half
    // angle 3 pi / 2.
    mtwnVec4 q = Value(context, wheel);
    printf("the wheel: (%.6f, %.6f, %.6f, %.6f)\n", (double)q.x, (double)q.y, (double)q.z,
           (double)q.w);
    // A rotation and its negation are the same turn.
    Expect(fabsf(fabsf(q.z) - 1.0f) < 1e-5f && fabsf(q.w) < 1e-5f, "a turn and a half");
    mtwnDestroyContext(context);
    return s_failed;
}
