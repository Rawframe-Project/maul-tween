// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Where a crossing is: the time a sweep's count of a way first (or last)
// reaches one, by halving the doubles between its ends on the count.

#include "crossing.h"

#include <math.h>
#include <string.h>

static double Of(mtwnCrossings c, mtwnCrossingWay way)
{
    return way == mtwn_crossingForwards    ? c.same
           : way == mtwn_crossingBackwards ? c.opposite
                                           : c.same + c.opposite;
}

// The doubles in their order as integers: the bisection below halves the
// doubles between two times, so it ends exactly, after at most 64 halvings.
static int64_t Ordered(double x)
{
    int64_t bits = 0;
    memcpy(&bits, &x, sizeof bits);
    return bits < 0 ? INT64_MIN - bits : bits;
}

static double Unordered(int64_t ordered)
{
    int64_t bits = ordered < 0 ? INT64_MIN - ordered : ordered;
    double x = 0.0;
    memcpy(&x, &bits, sizeof x);
    return x;
}

// A search: the sweep, the way, which end it is near, and the bracket
// of ordered doubles it halves. The count does not reach one at near and
// does at far.
typedef struct Search
{
    const mtwnLevel* levels;
    uint32_t count;
    const mtwnLeaf* leaf;
    double from;
    double to;
    bool fromClosed;
    bool last;
    bool up;
    mtwnCrossingWay way;
    int64_t near;
    int64_t far;
} Search;

// Whether the sweep part a search tests counts a crossing: from the sweep's
// start to the ordered double t, or from t to its end.
MTWN_NODISCARD static bool Found(const Search* search, int64_t t)
{
    double at = Unordered(t);
    mtwnCrossings c = search->last ? mtwnCountCrossings(search->levels, search->count, search->leaf,
                                                        at, search->to, true)
                                   : mtwnCountCrossings(search->levels, search->count, search->leaf,
                                                        search->from, at, search->fromClosed);
    return Of(c, search->way) >= 1.0;
}

// Moves the bracket's end the ordered double t tests: far when it counts,
// near when not.
static void Test(Search* search, int64_t t)
{
    if (Found(search, t))
    {
        search->far = t;
    }
    else
    {
        search->near = t;
    }
}

// Whether ordered b lies strictly beyond a, going up or down.
MTWN_NODISCARD static bool Beyond(bool up, int64_t a, int64_t b)
{
    return up ? b > a : b < a;
}

// A guess past the near end is tested, and when it counts so is its
// neighbor toward the near end: a right guess leaves the two neighbors
// the halving ends with. A guess past the far end is the far end, which
// counts: an interpolated guess can pass the sweep's end by a double.
static void TestGuess(Search* search, double guess)
{
    int64_t g = isnan(guess) ? search->near : Ordered(guess);
    if (!Beyond(search->up, search->near, g))
    {
        return;
    }
    if (Beyond(search->up, search->far, g))
    {
        g = search->far;
    }
    else
    {
        Test(search, g);
    }
    int64_t before = search->up ? g - 1 : g + 1;
    if (search->far == g && Beyond(search->up, search->near, before))
    {
        Test(search, before);
    }
}

double mtwnFindCrossingFrom(const mtwnLevel* levels, uint32_t count, const mtwnLeaf* leaf,
                            double from, double to, bool fromClosed, bool last, mtwnCrossingWay way,
                            double guess)
{
    // The point at the near end itself.
    double end = last ? to : from;
    if ((last || fromClosed) &&
        Of(mtwnCountCrossings(levels, count, leaf, end, end, true), way) >= 1.0)
    {
        return end;
    }
    // The count is not reached at the near end and is at the far one;
    // halve the doubles between until they are neighbors.
    Search search = {levels,
                     count,
                     leaf,
                     from,
                     to,
                     fromClosed,
                     last,
                     false,
                     way,
                     Ordered(last ? to : from),
                     Ordered(last ? from : to)};
    search.up = search.far > search.near;
    TestGuess(&search, guess);
    for (;;)
    {
        uint64_t distance = search.up ? (uint64_t)search.far - (uint64_t)search.near
                                      : (uint64_t)search.near - (uint64_t)search.far;
        if (distance <= 1)
        {
            return Unordered(search.far);
        }
        int64_t half = (int64_t)(distance / 2);
        Test(&search, search.up ? search.near + half : search.near - half);
    }
}
