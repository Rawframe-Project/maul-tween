// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A grid of tiles appearing from its center: each tile fades in and
// slides up, its delay from a stagger, so the center leads and the
// corners follow. Played twice: with motion, and under reduced motion,
// where the slides land at once while the fades, marked essential (a
// change of opacity is not motion), still play in their order. Returns 0
// when the center leads, every tile ends at rest, and reduced motion
// removes the movement but keeps the fades.

#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <stdint.h>
#include <stdio.h>

enum
{
    COLUMNS = 4,
    TILES = 12
};

// A frame at 60 Hz, in nanoseconds.
static const uint64_t s_frame = 16666667u;

typedef struct Grid
{
    mtwnContext* context;
    mtwnTargetId opacity[TILES];
    mtwnTargetId offset[TILES];
} Grid;

static int s_failed;

static void Expect(int ok, const char* what)
{
    if (!ok)
    {
        printf("FAIL: %s\n", what);
        s_failed = 1;
    }
}

static float Read(const Grid* g, mtwnTargetId target)
{
    mtwnVec4 value = {0};
    Expect(mtwnTarget_GetValue(g->context, target, &value) == mtwn_success, "a value");
    return value.x;
}

// Every tile transparent and 24 units low, then the entrance: each
// tile's fade and slide delayed by its stagger offset.
static void Enter(Grid* g, bool reduced)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    Expect(mtwnCreateContext(&def, &g->context) == mtwn_success, "a context");
    Expect(mtwnContext_SetReducedMotion(g->context, reduced) == mtwn_success, "the preference");
    for (int i = 0; i < TILES; ++i)
    {
        Expect(mtwnCreateTarget(g->context, mtwn_valueScalar, (mtwnVec4){0}, &g->opacity[i]) ==
                       mtwn_success &&
                   mtwnCreateTarget(g->context, mtwn_valueScalar, (mtwnVec4){24.0f, 0, 0, 0},
                                    &g->offset[i]) == mtwn_success,
               "a tile");
    }
    // 60 ms between neighbors, from the center, eased so the outer
    // rings close up.
    mtwnStagger stagger = mtwnDefaultStagger();
    stagger.columns = COLUMNS;
    stagger.origin = mtwn_staggerCenter;
    stagger.spacing = 0.06;
    Expect(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &stagger.easing) == mtwn_success, "an easing");
    double delays[TILES];
    Expect(mtwnComputeStagger(&stagger, TILES, delays) == mtwn_success, "the delays");
    for (int i = 0; i < TILES; ++i)
    {
        mtwnTransitionDef fade = mtwnDefaultTransitionDef();
        fade.target = g->opacity[i];
        fade.value = (mtwnVec4){1.0f, 0, 0, 0};
        fade.delay = delays[i];
        fade.duration = 0.3;
        fade.essential = true;
        mtwnTransitionDef slide = fade;
        slide.target = g->offset[i];
        slide.value = (mtwnVec4){0};
        slide.duration = 0.4;
        slide.essential = false;
        Expect(mtwnMakeCssEasing(mtwn_cssEaseOut, &slide.easing) == mtwn_success, "an easing");
        mtwnPlayerId fading;
        mtwnPlayerId sliding;
        Expect(mtwnStartTransition(g->context, &fade, &fading) == mtwn_success &&
                   mtwnStartTransition(g->context, &slide, &sliding) == mtwn_success,
               "the entrance");
    }
}

static void Update(Grid* g, int frame)
{
    Expect(mtwnUpdate(g->context, (uint64_t)frame * s_frame, NULL, 0, NULL) == mtwn_success,
           "an update");
}

static void WithMotion(void)
{
    Grid g;
    Enter(&g, false);
    for (int frame = 0; frame <= 8; ++frame)
    {
        Update(&g, frame);
    }
    // Tile 5 is beside the center, tile 0 a corner.
    Expect(Read(&g, g.opacity[5]) > Read(&g, g.opacity[0]), "the center fades in first");
    Expect(Read(&g, g.offset[5]) < Read(&g, g.offset[0]), "and rises first");
    for (int frame = 9; frame <= 60; ++frame)
    {
        Update(&g, frame);
    }
    int resting = 1;
    for (int i = 0; i < TILES; ++i)
    {
        resting = resting && Read(&g, g.opacity[i]) == 1.0f && Read(&g, g.offset[i]) == 0.0f;
    }
    Expect(resting, "every tile in place after a second");
    mtwnDestroyContext(g.context);
}

static void Reduced(void)
{
    Grid g;
    Enter(&g, true);
    Update(&g, 0);
    int landed = 1;
    for (int i = 0; i < TILES; ++i)
    {
        landed = landed && Read(&g, g.offset[i]) == 0.0f;
    }
    Expect(landed, "reduced motion: every slide lands at the first update");
    for (int frame = 1; frame <= 8; ++frame)
    {
        Update(&g, frame);
    }
    float center = Read(&g, g.opacity[5]);
    Expect(center > 0.0f && center < 1.0f, "the fades still play");
    Expect(center > Read(&g, g.opacity[0]), "in their order, from the center");
    for (int frame = 9; frame <= 60; ++frame)
    {
        Update(&g, frame);
    }
    Expect(Read(&g, g.opacity[0]) == 1.0f, "and end");
    mtwnDestroyContext(g.context);
}

int main(void)
{
    WithMotion();
    Reduced();
    if (s_failed == 0)
    {
        printf("grid: the center leads; reduced motion lands the slides, keeps the fades\n");
    }
    return s_failed;
}
