// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Uses the installed library as a consumer would, in C17: it checks that
// the linked library has the ABI it was compiled against, eases a
// value to its end over half a second of host time and checks it lands
// there exactly.

#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <stdint.h>
#include <stdio.h>

int main(void)
{
    mtwnVersion version = mtwnGetVersion();
    // Before 1.0 any release may change the ABI, so the whole version must
    // match; from 1.0 on, the major and minor version.
    if (version.major != MTWN_VERSION_MAJOR || version.minor != MTWN_VERSION_MINOR ||
        (MTWN_VERSION_MAJOR == 0 && version.patch != MTWN_VERSION_PATCH))
    {
        printf("FAIL: linked %u.%u.%u, compiled against %d.%d.%d\n", version.major, version.minor,
               version.patch, MTWN_VERSION_MAJOR, MTWN_VERSION_MINOR, MTWN_VERSION_PATCH);
        return 1;
    }
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    mtwnTargetId x = {0, 0};
    mtwnPlayerId player = {0, 0};
    mtwnTransitionDef move = mtwnDefaultTransitionDef();
    move.value.x = 100.0f;
    move.duration = 0.5;
    int ok = mtwnCreateContext(&def, &context) == mtwn_success &&
             mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &x) == mtwn_success &&
             mtwnMakeCssEasing(mtwn_cssEaseInOut, &move.easing) == mtwn_success;
    move.target = x;
    ok = ok && mtwnStartTransition(context, &move, &player) == mtwn_success;
    // Host time in nanoseconds, a frame of 60 Hz at a time.
    for (uint64_t frame = 0; ok && frame <= 40; ++frame)
    {
        ok = mtwnContext_Update(context, frame * 16666667u, NULL, 0, NULL) == mtwn_success;
    }
    mtwnTargetState state = {0};
    ok = ok && mtwnTarget_GetState(context, x, &state) == mtwn_success && state.value.x == 100.0f;
    mtwnDestroyContext(context);
    printf("%s: maul-tween %u.%u.%u, the value at %.1f\n", ok ? "ok" : "FAIL", version.major,
           version.minor, version.patch, (double)state.value.x);
    return ok ? 0 : 1;
}
