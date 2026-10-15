// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Staggers (record mtwn-0048). Each item's distance from the origin is
// written into the caller's offsets, then normalized, eased and scaled
// in place, so nothing is allocated. The distances use only exactly
// rounded operations and the easing is the library's own, so the
// offsets are the same bits on every platform. A random order is an
// inside-out shuffle driven by SplitMix64 of the seed.

#include "maul-tween/timing.h"

#include <math.h>
#include <stdbool.h>

// The grid the items fill, and the origin's place in it.
typedef struct Grid
{
    uint32_t columns;
    double lastX;
    double lastY;
    double originX;
    double originY;
} Grid;

mtwnStagger mtwnDefaultStagger(void)
{
    mtwnStagger stagger = {0};
    stagger.spacing = 0.1;
    stagger.easing = mtwnLinearEasing();
    return stagger;
}

static bool IsValid(const mtwnStagger* stagger, uint32_t count, const double* offsetsOut)
{
    if (stagger == NULL || (count != 0 && offsetsOut == NULL))
    {
        return false;
    }
    bool known = stagger->origin <= mtwn_staggerRandom && stagger->axis <= mtwn_staggerY &&
                 stagger->spacingKind <= mtwn_staggerAmount;
    bool finite = isfinite(stagger->start) && isfinite(stagger->spacing) &&
                  isfinite(stagger->pointX) && isfinite(stagger->pointY);
    bool index = stagger->origin != mtwn_staggerIndex || count == 0 || stagger->index < count;
    return known && finite && index;
}

static Grid MakeGrid(const mtwnStagger* stagger, uint32_t count)
{
    Grid grid = {0};
    grid.columns = stagger->columns == 0 || stagger->columns > count ? count : stagger->columns;
    grid.lastX = (double)(grid.columns - 1u);
    uint32_t lastRow = (count - 1u) / grid.columns;
    grid.lastY = (double)lastRow;
    uint32_t at = stagger->origin == mtwn_staggerLast ? count - 1u : stagger->index;
    switch (stagger->origin)
    {
    case mtwn_staggerLast:
    case mtwn_staggerIndex:
    {
        uint32_t row = at / grid.columns;
        grid.originX = (double)(at % grid.columns);
        grid.originY = (double)row;
        break;
    }
    case mtwn_staggerCentre:
        grid.originX = grid.lastX / 2.0;
        grid.originY = grid.lastY / 2.0;
        break;
    case mtwn_staggerPoint:
        grid.originX = stagger->pointX * grid.lastX;
        grid.originY = stagger->pointY * grid.lastY;
        break;
    default:
        break;
    }
    return grid;
}

static double Least(double a, double b)
{
    return a < b ? a : b;
}

// An item's distance to the nearest edge.
static double EdgeDistance(const Grid* grid, mtwnStaggerAxis axis, double x, double y)
{
    double toX = Least(x, grid->lastX - x);
    double toY = Least(y, grid->lastY - y);
    if (axis == mtwn_staggerX || (axis == mtwn_staggerBoth && grid->lastY == 0.0))
    {
        return toX;
    }
    if (axis == mtwn_staggerY || grid->lastX == 0.0)
    {
        return toY;
    }
    return Least(toX, toY);
}

static double Distance(const Grid* grid, const mtwnStagger* stagger, uint32_t item)
{
    uint32_t row = item / grid->columns;
    double x = (double)(item % grid->columns);
    double y = (double)row;
    if (stagger->origin == mtwn_staggerEdges)
    {
        return EdgeDistance(grid, stagger->axis, x, y);
    }
    double dx = x - grid->originX;
    double dy = y - grid->originY;
    if (stagger->axis == mtwn_staggerX)
    {
        return fabs(dx);
    }
    if (stagger->axis == mtwn_staggerY)
    {
        return fabs(dy);
    }
    return sqrt(dx * dx + dy * dy);
}

// A permutation of 0 ... count - 1 from the seed, each item its own step.
static void Shuffle(uint64_t seed, uint32_t count, double* out)
{
    uint64_t state = seed;
    for (uint32_t j = 0; j < count; ++j)
    {
        state += 0x9E3779B97F4A7C15ull;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        // A place in [0, j] from the top 32 bits, without a division.
        uint32_t k = (uint32_t)(((z >> 32) * ((uint64_t)j + 1u)) >> 32);
        if (k != j)
        {
            out[j] = out[k];
        }
        out[k] = (double)j;
    }
}

mtwnResult mtwnComputeStagger(const mtwnStagger* stagger, uint32_t count, double* offsetsOut)
{
    if (!IsValid(stagger, count, offsetsOut))
    {
        return mtwn_errorInvalid;
    }
    if (count == 0)
    {
        return mtwn_success;
    }
    if (stagger->origin == mtwn_staggerRandom)
    {
        Shuffle(stagger->seed, count, offsetsOut);
    }
    else
    {
        Grid grid = MakeGrid(stagger, count);
        for (uint32_t i = 0; i < count; ++i)
        {
            offsetsOut[i] = Distance(&grid, stagger, i);
        }
    }
    double least = offsetsOut[0];
    double most = offsetsOut[0];
    for (uint32_t i = 1; i < count; ++i)
    {
        least = Least(least, offsetsOut[i]);
        most = offsetsOut[i] > most ? offsetsOut[i] : most;
    }
    double range = most - least;
    double span =
        stagger->spacingKind == mtwn_staggerAmount ? stagger->spacing : stagger->spacing * range;
    for (uint32_t i = 0; i < count; ++i)
    {
        double t = range > 0.0 ? (offsetsOut[i] - least) / range : 0.0;
        offsetsOut[i] = stagger->start + span * mtwnEase(&stagger->easing, t, false);
    }
    return mtwn_success;
}
