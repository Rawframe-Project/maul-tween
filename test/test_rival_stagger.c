// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Staggers against GSAP's distribute, run under Node by
// tools/oracle/rivals.mjs: each case translated as the clips guide's
// porting note says agrees with GSAP's offsets within its rounding, over
// rows and grids, a short last row, one row and fewer items than columns
// among them, from every origin GSAP has, on each axis, by each and
// amount, positive and negative, linear and eased.

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
// ratios (0.5, 0.5) and (1, 1). The items fill the smaller of GSAP's
// columns and the count, and a ratio r is the column r times those less
// 0.5, and the row r * count / columns - 0.5 with GSAP's columns, as point
// ratios of the last column and row. `each` is an amount: each times count
// - 1 in a row or where the columns outnumber the items, and otherwise in
// a grid times the columns along x, count / columns along y, and the
// larger of the two across. False where the note gives no translation: a
// grid of one row whose origin GSAP puts off that row, measured across
// both axes, where the point is put on the row.
static bool Translate(const RivalStagger* rival, mtwnStagger* stagger)
{
    *stagger = mtwnDefaultStagger();
    stagger->axis = rival->axis;
    stagger->start = rival->base;
    double spacing = rival->from == rivalEdges ? -rival->spacing : rival->spacing;
    bool grid = rival->columns != 0;
    double count = (double)rival->count;
    double wrap = grid ? (double)rival->columns : count;
    double columns = fmin(wrap, count);
    double rows = ceil(count / columns);
    stagger->columns = grid ? (uint32_t)columns : 0;
    static const mtwnStaggerOrigin s_origins[] = {mtwn_staggerIndex, mtwn_staggerLast,
                                                  mtwn_staggerCenter, mtwn_staggerCenter,
                                                  mtwn_staggerPoint};
    stagger->origin = s_origins[rival->from];
    stagger->index = rival->index;
    bool ratios = rival->from == rivalRatio || (grid && rival->from != rivalIndex);
    bool exact = true;
    double ratioX = rival->from == rivalRatio ? rival->ratio[0]
                    : rival->from == rivalEnd ? 1.0
                                              : 0.5;
    double ratioY = rival->from == rivalRatio ? rival->ratio[1]
                    : rival->from == rivalEnd ? 1.0
                                              : 0.5;
    if (ratios)
    {
        double row = grid ? ratioY * count / wrap - 0.5 : 0.0;
        exact = rows > 1.0 || row == 0.0 || rival->axis != mtwn_staggerBoth;
        stagger->origin = mtwn_staggerPoint;
        stagger->pointX = columns > 1.0 ? (ratioX * columns - 0.5) / (columns - 1.0) : 0.0;
        stagger->pointY = rows > 1.0 ? row / (rows - 1.0) : 0.0;
    }
    stagger->spacingKind = mtwn_staggerAmount;
    double cells = !grid || wrap > count          ? count - 1.0
                   : rival->axis == mtwn_staggerX ? wrap
                   : rival->axis == mtwn_staggerY ? count / wrap
                                                  : fmax(wrap, count / wrap);
    stagger->spacing = rival->amount ? spacing : spacing * cells;
    return exact;
}

// GSAP rounds each offset to 1e-7, so half of that, and a few units in the
// last place of both sides' arithmetic on offsets under 2.
static const double s_bound = 0.5e-7 + 0x1.0p-40;

static void TestRivals(void)
{
    uint32_t count = (uint32_t)(sizeof s_rivalStaggers / sizeof s_rivalStaggers[0]);
    uint32_t within = 0;
    uint32_t untranslated = 0;
    uint32_t apart = 0;
    for (uint32_t c = 0; c < count; ++c)
    {
        const RivalStagger* rival = &s_rivalStaggers[c];
        mtwnStagger stagger;
        double offsets[12];
        bool exact = Translate(rival, &stagger);
        if ((rival->eased &&
             mtwnMakePowerEasing(mtwn_easeIn, 2.0, &stagger.easing) != mtwn_success) ||
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
        if (!exact)
        {
            // Put on the row, the origin GSAP puts off it gives other
            // offsets, as the note says.
            ++untranslated;
            apart += worst > s_bound ? 1u : 0u;
        }
        else if (worst <= s_bound)
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
    // Across both axes: in the one full row of four, "end" and the three
    // ratio points; in five items of eight columns, "center" and "edges"
    // too. Each by four spacings, linear and eased.
    CHECK(count == 832 && untranslated == 80,
          "every case the oracle wrote, as the note sorts them");
    CHECK(within == count - untranslated,
          "every case translated, within GSAP's rounding of its offsets");
    CHECK(apart == untranslated, "every case without a translation, apart from GSAP's on the row");
}

int main(void)
{
    TestRivals();
    return s_failures == 0 ? 0 : 1;
}
