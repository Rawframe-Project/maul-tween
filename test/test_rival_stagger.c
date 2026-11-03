// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Staggers against GSAP's distribute, run under Node by
// tools/oracle/rivals.mjs: each case translated as the clips guide's
// porting note says agrees with GSAP's offsets within its rounding, over
// rows and grids, a short last row among them, from every origin GSAP
// has, on each axis, by each and amount, positive and negative, linear
// and eased.

#include "test_harness.h"

#include "maul-tween/easing.h"
#include "maul-tween/timing.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

typedef struct RivalStagger
{
    uint32_t count;
    uint32_t columns;
    int from;
    uint32_t index;
    double ratio[2];
    int axis;
    int amount;
    double spacing;
    double base;
    int eased;
    double offsets[12];
} RivalStagger;

#include "generated/rival_stagger.h"

enum
{
    rivalIndex = 0,
    rivalEnd = 1,
    rivalCenter = 2,
    rivalEdges = 3,
    rivalRatio = 4,
};

// GSAP's case as the porting note translates it. "edges" is GSAP's center
// with the spacing negated; in a grid its "center" and "end" are the
// ratios (0.5, 0.5) and (1, 1), and a ratio r is the column r * columns -
// 0.5 and the row r * count / columns - 0.5, as point ratios of the last
// column and row. `each` is an amount: each times count - 1 in a row, and
// in a grid times the columns along x, count / columns along y, and the
// larger of the two across.
static bool Translate(const RivalStagger* rival, mtwnStagger* stagger)
{
    *stagger = mtwnDefaultStagger();
    stagger->columns = rival->columns;
    stagger->axis = rival->axis;
    stagger->start = rival->base;
    double spacing = rival->from == rivalEdges ? -rival->spacing : rival->spacing;
    bool grid = rival->columns != 0;
    double columns = grid ? (double)rival->columns : (double)rival->count;
    double rows = grid ? ceil((double)rival->count / columns) : 1.0;
    static const mtwnStaggerOrigin s_origins[] = {mtwn_staggerIndex, mtwn_staggerLast,
                                                  mtwn_staggerCenter, mtwn_staggerCenter,
                                                  mtwn_staggerPoint};
    stagger->origin = s_origins[rival->from];
    stagger->index = rival->index;
    bool ratios = rival->from == rivalRatio || (grid && rival->from != rivalIndex);
    double ratioX = rival->from == rivalRatio ? rival->ratio[0]
                    : rival->from == rivalEnd ? 1.0
                                              : 0.5;
    double ratioY = rival->from == rivalRatio ? rival->ratio[1]
                    : rival->from == rivalEnd ? 1.0
                                              : 0.5;
    if (ratios)
    {
        stagger->origin = mtwn_staggerPoint;
        stagger->pointX = columns > 1.0 ? (ratioX * columns - 0.5) / (columns - 1.0) : 0.0;
        stagger->pointY =
            rows > 1.0 ? (ratioY * (double)rival->count / columns - 0.5) / (rows - 1.0) : 0.0;
    }
    stagger->spacingKind = mtwn_staggerAmount;
    double cells = !grid                          ? (double)rival->count - 1.0
                   : rival->axis == mtwn_staggerX ? columns
                   : rival->axis == mtwn_staggerY ? (double)rival->count / columns
                                                  : fmax(columns, (double)rival->count / columns);
    stagger->spacing = rival->amount ? spacing : spacing * cells;
    return !rival->eased || mtwnMakePowerEasing(mtwn_easeIn, 2.0, &stagger->easing) == mtwn_success;
}

// GSAP rounds each offset to 1e-7, so half of that, and a few units in the
// last place of both sides' arithmetic on offsets under 2.
static const double s_bound = 0.5e-7 + 0x1.0p-40;

static void TestRivals(void)
{
    uint32_t count = (uint32_t)(sizeof s_rivalStaggers / sizeof s_rivalStaggers[0]);
    uint32_t within = 0;
    for (uint32_t c = 0; c < count; ++c)
    {
        const RivalStagger* rival = &s_rivalStaggers[c];
        mtwnStagger stagger;
        double offsets[12];
        if (!Translate(rival, &stagger) ||
            mtwnComputeStagger(&stagger, rival->count, offsets) != mtwn_success)
        {
            printf("case %u: refused\n", c);
            continue;
        }
        double worst = 0.0;
        for (uint32_t i = 0; i < rival->count; ++i)
        {
            double error = fabs(offsets[i] - rival->offsets[i]);
            worst = error > worst ? error : worst;
        }
        if (worst <= s_bound)
        {
            ++within;
        }
        else
        {
            printf("case %u: %u items, %u columns, from %d, axis %d, %s %g: off by %.3g\n", c,
                   rival->count, rival->columns, rival->from, rival->axis,
                   rival->amount ? "amount" : "each", rival->spacing, worst);
        }
    }
    CHECK(count == 448, "every case the oracle wrote");
    CHECK(within == count, "every case, translated, within GSAP's rounding of its offsets");
}

int main(void)
{
    TestRivals();
    return s_failures == 0 ? 0 : 1;
}
