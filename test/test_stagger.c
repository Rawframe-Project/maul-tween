// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Staggers (record mtwn-0048): items one cell apart are each apart in a
// row and in a grid, the nearest items begin at the start, every origin
// and axis measures what it names, a random order is a permutation that
// repeats for its seed with the same bits everywhere, and misuse writes
// nothing.

#include "test_harness.h"

#include "maul-tween/timing.h"

#include <math.h>
#include <stdint.h>

static bool Near(double a, double b)
{
    return fabs(a - b) <= 1e-15;
}

// Whether count offsets are near the expected ones.
static bool NearAll(const double* got, const double* expected, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        if (!Near(got[i], expected[i]))
        {
            return false;
        }
    }
    return true;
}

static void TestRow(void)
{
    mtwnStagger stagger = mtwnDefaultStagger();
    double got[5];
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success, "the default");
    CHECK(NearAll(got, (const double[]){0.0, 0.1, 0.2, 0.3, 0.4}, 5), "each between neighbours");
    CHECK(got[0] == 0.0, "the first at the start");
    double last[5];
    stagger.origin = mtwn_staggerLast;
    CHECK(mtwnComputeStagger(&stagger, 5, last) == mtwn_success, "from the last");
    bool reversed = true;
    for (int i = 0; i < 5; ++i)
    {
        reversed = reversed && last[i] == got[4 - i];
    }
    CHECK(reversed, "the last reverses the first");
    stagger.origin = mtwn_staggerCentre;
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success &&
              NearAll(got, (const double[]){0.2, 0.1, 0.0, 0.1, 0.2}, 5),
          "from the centre of an odd count");
    CHECK(mtwnComputeStagger(&stagger, 4, got) == mtwn_success &&
              NearAll(got, (const double[]){0.1, 0.0, 0.0, 0.1}, 4),
          "from the centre of an even count, the nearest at the start");
    stagger.origin = mtwn_staggerEdges;
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success &&
              NearAll(got, (const double[]){0.0, 0.1, 0.2, 0.1, 0.0}, 5),
          "from the edges");
    stagger.origin = mtwn_staggerIndex;
    stagger.index = 1;
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success &&
              NearAll(got, (const double[]){0.1, 0.0, 0.1, 0.2, 0.3}, 5),
          "from an index");
    stagger.origin = mtwn_staggerFirst;
    stagger.columns = 9;
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success &&
              NearAll(got, (const double[]){0.0, 0.1, 0.2, 0.3, 0.4}, 5),
          "more columns than items is one row");
}

static void TestSpacing(void)
{
    mtwnStagger stagger = mtwnDefaultStagger();
    stagger.spacingKind = mtwn_staggerAmount;
    stagger.spacing = 1.0;
    stagger.start = 2.0;
    double got[5];
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success && got[0] == 2.0 && got[1] == 2.25 &&
              got[2] == 2.5 && got[3] == 2.75 && got[4] == 3.0,
          "an amount is the whole spread, exactly");
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &stagger.easing) == mtwn_success &&
              mtwnComputeStagger(&stagger, 5, got) == mtwn_success && got[1] == 2.0625 &&
              got[2] == 2.25 && got[4] == 3.0,
          "eased");
    stagger.easing = mtwnLinearEasing();
    stagger.start = 3.0;
    stagger.spacing = -1.0;
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success && got[0] == 3.0 && got[4] == 2.0,
          "a negative spread reverses the order");
    CHECK(mtwnComputeStagger(&stagger, 1, got) == mtwn_success && got[0] == 3.0,
          "one item at the start");
    stagger.origin = mtwn_staggerEdges;
    CHECK(mtwnComputeStagger(&stagger, 2, got) == mtwn_success && got[0] == 3.0 && got[1] == 3.0,
          "all at the same distance, all at the start");
    CHECK(mtwnComputeStagger(&stagger, 0, NULL) == mtwn_success, "no items");
}

static void TestGrid(void)
{
    mtwnStagger stagger = mtwnDefaultStagger();
    stagger.columns = 3;
    stagger.origin = mtwn_staggerCentre;
    double got[9];
    CHECK(mtwnComputeStagger(&stagger, 9, got) == mtwn_success, "a 3 by 3 grid");
    CHECK(got[4] == 0.0 && Near(got[1], 0.1) && Near(got[3], 0.1) && Near(got[5], 0.1) &&
              Near(got[7], 0.1),
          "a cell from the centre is each from it");
    CHECK(Near(got[0], 0.1 * sqrt(2.0)) && got[0] == got[2] && got[0] == got[6] && got[0] == got[8],
          "the corners at the diagonal");
    stagger.origin = mtwn_staggerEdges;
    CHECK(mtwnComputeStagger(&stagger, 9, got) == mtwn_success && got[4] == 0.1 && got[0] == 0.0 &&
              got[1] == 0.0 && got[3] == 0.0 && got[5] == 0.0 && got[7] == 0.0 && got[8] == 0.0,
          "from the edges of a grid, every border cell first, the centre last");
    stagger.columns = 1;
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success && got[0] == 0.0 && got[4] == 0.0 &&
              Near(got[1], 0.1) && Near(got[2], 0.2) && Near(got[3], 0.1),
          "from the edges of one column, by its rows");
    stagger.columns = 3;
    stagger.origin = mtwn_staggerFirst;
    stagger.axis = mtwn_staggerX;
    CHECK(mtwnComputeStagger(&stagger, 6, got) == mtwn_success && got[3] == got[0] &&
              got[4] == got[1] && got[5] == got[2] && Near(got[2], 0.2),
          "along the columns only");
    stagger.axis = mtwn_staggerY;
    CHECK(mtwnComputeStagger(&stagger, 6, got) == mtwn_success && got[0] == got[2] &&
              got[3] == got[5] && Near(got[3], 0.1),
          "along the rows only");
    // From the centre, so items lie on both sides of the origin.
    stagger.origin = mtwn_staggerCentre;
    stagger.axis = mtwn_staggerX;
    CHECK(mtwnComputeStagger(&stagger, 6, got) == mtwn_success && got[1] == 0.0 &&
              Near(got[0], 0.1) && got[0] == got[2] && got[3] == got[5],
          "along the columns, either side of the centre");
    stagger.axis = mtwn_staggerY;
    CHECK(mtwnComputeStagger(&stagger, 9, got) == mtwn_success && got[4] == 0.0 &&
              Near(got[1], 0.1) && got[1] == got[7],
          "along the rows, either side of the centre");
    stagger.origin = mtwn_staggerFirst;
    stagger.axis = mtwn_staggerBoth;
    stagger.origin = mtwn_staggerPoint;
    stagger.pointX = 1.0;
    stagger.pointY = 0.0;
    CHECK(mtwnComputeStagger(&stagger, 6, got) == mtwn_success && got[2] == 0.0 &&
              Near(got[1], 0.1) && Near(got[5], 0.1),
          "from a point, the top right");
    // Five items in two columns: the last row holds one, the last item.
    stagger.columns = 2;
    stagger.origin = mtwn_staggerLast;
    CHECK(mtwnComputeStagger(&stagger, 5, got) == mtwn_success && got[4] == 0.0 &&
              Near(got[2], 0.1) && Near(got[0], 0.2) && Near(got[3], 0.1 * sqrt(2.0)),
          "from the last item of a partial row");
}

static void TestRandom(void)
{
    mtwnStagger stagger = mtwnDefaultStagger();
    stagger.origin = mtwn_staggerRandom;
    stagger.spacing = 1.0;
    stagger.seed = 42;
    enum
    {
        count = 64
    };
    double a[count];
    double b[count];
    CHECK(mtwnComputeStagger(&stagger, count, a) == mtwn_success &&
              mtwnComputeStagger(&stagger, count, b) == mtwn_success,
          "a random order");
    int seen[count] = {0};
    bool same = true;
    bool steps = true;
    for (int i = 0; i < count; ++i)
    {
        same = same && a[i] == b[i];
        int step = (int)a[i];
        steps = steps && (double)step == a[i] && step >= 0 && step < count;
        if (steps)
        {
            ++seen[step];
        }
    }
    bool permutation = steps;
    for (int i = 0; i < count && permutation; ++i)
    {
        permutation = seen[i] == 1;
    }
    CHECK(same, "the same seed, the same order");
    CHECK(permutation, "each item its own step");
    // The same bits on every platform: an order computed once and kept.
    double kept[8];
    CHECK(mtwnComputeStagger(&stagger, 8, kept) == mtwn_success &&
              NearAll(kept, (const double[]){4.0, 6.0, 1.0, 0.0, 2.0, 5.0, 7.0, 3.0}, 8),
          "the kept order");
    stagger.seed = 43;
    CHECK(mtwnComputeStagger(&stagger, count, b) == mtwn_success, "another seed");
    int moved = 0;
    for (int i = 0; i < count; ++i)
    {
        moved += a[i] != b[i];
    }
    CHECK(moved > count / 2, "another seed another order");
}

static void TestMisuse(void)
{
    mtwnStagger good = mtwnDefaultStagger();
    double got[3] = {-1.0, -1.0, -1.0};
    CHECK(mtwnComputeStagger(NULL, 3, got) == mtwn_errorInvalid, "a null stagger");
    CHECK(mtwnComputeStagger(&good, 3, NULL) == mtwn_errorInvalid, "a null out");
    mtwnStagger bad = good;
    bad.origin = mtwn_staggerRandom + 1;
    CHECK(mtwnComputeStagger(&bad, 3, got) == mtwn_errorInvalid, "an unknown origin");
    bad = good;
    bad.axis = mtwn_staggerY + 1;
    CHECK(mtwnComputeStagger(&bad, 3, got) == mtwn_errorInvalid, "an unknown axis");
    bad = good;
    bad.spacingKind = mtwn_staggerAmount + 1;
    CHECK(mtwnComputeStagger(&bad, 3, got) == mtwn_errorInvalid, "an unknown spacing");
    bad = good;
    bad.origin = mtwn_staggerIndex;
    bad.index = 3;
    CHECK(mtwnComputeStagger(&bad, 3, got) == mtwn_errorInvalid, "an index past the items");
    CHECK(mtwnComputeStagger(&bad, 0, NULL) == mtwn_success, "any index for no items");
    bad = good;
    bad.start = (double)NAN;
    CHECK(mtwnComputeStagger(&bad, 3, got) == mtwn_errorInvalid, "a start not finite");
    bad = good;
    bad.spacing = HUGE_VAL;
    CHECK(mtwnComputeStagger(&bad, 3, got) == mtwn_errorInvalid, "a spacing not finite");
    bad = good;
    bad.pointY = (double)NAN;
    CHECK(mtwnComputeStagger(&bad, 3, got) == mtwn_errorInvalid, "a point not finite");
    CHECK(got[0] == -1.0 && got[1] == -1.0 && got[2] == -1.0, "nothing written");
}

int main(void)
{
    TestRow();
    TestSpacing();
    TestGrid();
    TestRandom();
    TestMisuse();
    return s_failures == 0 ? 0 : 1;
}
