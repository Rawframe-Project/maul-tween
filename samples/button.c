// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A button's states as transitions: its scale on a spring, its lift and
// fill eased. The pointer enters, leaves before the hover settles,
// presses and releases; every change starts from where the last one
// left the value and how fast it was moving, so nothing jumps. The host
// redraws only what the update reports changed, and once the button
// rests nothing is reported. Returns 0 when the button ends where it
// began and never jumped.

#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

enum
{
    FRAMES = 120,
    TARGETS = 3
};

// A frame at 60 Hz, in nanoseconds.
static const uint64_t s_frame = 16666667u;

typedef struct Button
{
    mtwnContext* context;
    mtwnTargetId scale;
    mtwnTargetId lift;
    mtwnTargetId fill;
} Button;

static int s_failed;

static void Expect(int ok, const char* what)
{
    if (!ok)
    {
        printf("FAIL: %s\n", what);
        s_failed = 1;
    }
}

// The scale springs to a size: from the value and velocity it has now.
static void Spring(Button* b, float size, double bounce)
{
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.target = b->scale;
    def.value = (mtwnVec4){size, 0.0f, 0.0f, 0.0f};
    def.kind = mtwn_transitionSpring;
    Expect(mtwnMakeSpringFromDuration(0.35, bounce, &def.spring) == mtwn_success, "a spring");
    mtwnPlayerId player;
    Expect(mtwnStartTransition(b->context, &def, &player) == mtwn_success, "the scale moves");
}

// The lift and fill ease to a state over a time.
static void Ease(Button* b, float lift, mtwnVec4 fill, double seconds)
{
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.duration = seconds;
    Expect(mtwnMakeCssEasing(mtwn_cssEaseOut, &def.easing) == mtwn_success, "ease-out");
    def.target = b->lift;
    def.value = (mtwnVec4){0.0f, lift, 0.0f, 0.0f};
    mtwnPlayerId player;
    Expect(mtwnStartTransition(b->context, &def, &player) == mtwn_success, "the lift moves");
    def.target = b->fill;
    def.value = fill;
    def.colorSpace = mtwn_colorOklab;
    Expect(mtwnStartTransition(b->context, &def, &player) == mtwn_success, "the fill moves");
}

static float Scale(const Button* b)
{
    mtwnTargetState state;
    Expect(mtwnTarget_GetState(b->context, b->scale, &state) == mtwn_success, "the scale read");
    return state.value.x;
}

int main(void)
{
    static const mtwnVec4 rest = {0.20f, 0.40f, 0.90f, 1.0f};
    static const mtwnVec4 hover = {0.35f, 0.55f, 1.00f, 1.0f};
    static const mtwnVec4 pressed = {0.15f, 0.30f, 0.75f, 1.0f};
    Button b = {0};
    mtwnContextDef def = mtwnDefaultContextDef();
    def.targetCapacity = TARGETS;
    def.playerCapacity = TARGETS;
    def.clipCapacity = 0;
    def.bindingCapacity = 0;
    def.clipStorageBytes = 0;
    Expect(mtwnCreateContext(&def, &b.context) == mtwn_success, "a context");
    Expect(mtwnCreateTarget(b.context, mtwn_valueScalar, (mtwnVec4){1.0f, 0, 0, 0}, &b.scale) ==
                   mtwn_success &&
               mtwnCreateTarget(b.context, mtwn_valueVec2, (mtwnVec4){0}, &b.lift) ==
                   mtwn_success &&
               mtwnCreateTarget(b.context, mtwn_valueColor, rest, &b.fill) == mtwn_success,
           "the button's targets");
    if (s_failed)
    {
        return 1;
    }
    float last = 1.0f;
    float largestStep = 0.0f;
    uint32_t redraws = 0;
    uint32_t quietFrames = 0;
    for (uint32_t frame = 0; frame < FRAMES; ++frame)
    {
        switch (frame)
        {
        case 6: // The pointer enters.
            Spring(&b, 1.08f, 0.3);
            Ease(&b, -4.0f, hover, 0.2);
            break;
        case 11: // It leaves before the hover settles.
            Spring(&b, 1.0f, 0.3);
            Ease(&b, 0.0f, rest, 0.25);
            break;
        case 30: // A press.
            Spring(&b, 0.96f, 0.0);
            Ease(&b, 1.0f, pressed, 0.08);
            break;
        case 38: // The release.
            Spring(&b, 1.0f, 0.4);
            Ease(&b, 0.0f, rest, 0.2);
            break;
        default:
            break;
        }
        mtwnTargetId changed[TARGETS];
        uint32_t total = 0;
        Expect(mtwnUpdate(b.context, frame * s_frame, changed, TARGETS, &total) == mtwn_success,
               "an update");
        // The host redraws what changed, and nothing once it rests.
        redraws += total;
        quietFrames += total == 0 ? 1u : 0u;
        float scale = Scale(&b);
        float step = fabsf(scale - last);
        largestStep = step > largestStep ? step : largestStep;
        last = scale;
        if (frame % 10 == 0 || (frame >= 5 && frame <= 14))
        {
            mtwnTargetState state;
            Expect(mtwnTarget_GetState(b.context, b.scale, &state) == mtwn_success, "read");
            printf("frame %3u  scale %.4f  speed %+.3f/s  changed %u\n", frame,
                   (double)state.value.x, (double)state.velocity.x, total);
        }
    }
    mtwnTargetState scale;
    mtwnTargetState lift;
    mtwnTargetState fill;
    Expect(mtwnTarget_GetState(b.context, b.scale, &scale) == mtwn_success &&
               mtwnTarget_GetState(b.context, b.lift, &lift) == mtwn_success &&
               mtwnTarget_GetState(b.context, b.fill, &fill) == mtwn_success,
           "the final state");
    Expect(scale.value.x == 1.0f && scale.velocity.x == 0.0f, "the scale back at rest");
    Expect(lift.value.y == 0.0f && fill.value.x == rest.x && fill.value.y == rest.y &&
               fill.value.z == rest.z,
           "the lift and fill back where they began");
    // A retarget starts from the moving value: no frame moves the scale
    // by more than a spring would in one frame.
    Expect(largestStep < 0.02f, "no jump at any retarget");
    Expect(quietFrames > 20, "a resting button reports nothing");
    printf("largest step %.4f, %u redraws, %u quiet frames\n", (double)largestStep, redraws,
           quietFrames);
    mtwnDestroyContext(b.context);
    return s_failed;
}
