// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Switches and counts for the tests and the fuzz targets, which no host
// needs. Nothing here is MTWN_API: the library's symbols are hidden but
// for the public functions, so a shared build exports none of these, and
// no header here is installed. A test reads what it needs through these
// functions, never through the library's structs.

#ifndef MAUL_TWEEN_SRC_TEST_HOOKS_H
#define MAUL_TWEEN_SRC_TEST_HOOKS_H

#include "maul-tween/context.h"
#include "maul-tween/player.h"

#include <stdbool.h>
#include <stdint.h>

// Has every crossing record of the context's clip players counted, the
// sweep's shortcuts for records certainly missed or settled off, so a
// test can check that leaving them out changes nothing. Off as made; the
// update only branches on it.
void mtwnTestSetCountEvery(mtwnContext* context, bool on);

// What the sharing of players in step held after the last update, over
// the serial work and every group's.
typedef struct mtwnTestSharing
{
    // A kept sample, a kept sweep, and any key noted at all.
    bool sampleKept;
    bool sweepKept;
    bool noted;
} mtwnTestSharing;

mtwnTestSharing mtwnTestGetSharing(const mtwnContext* context);

// A clip player's inputs to one sweep of its records: its swept time (NaN
// for none) and way, its own rate, and the sweep's end, rate and host
// times.
typedef struct mtwnTestSweepInputs
{
    double swept;
    int8_t way;
    double playRate;
    double to;
    double rate;
    double hostFrom;
    double hostTo;
} mtwnTestSweepInputs;

// Sets a clip player's sweep inputs and sweeps its records on the serial
// work: alone, or near as one in step with the last swept, which takes the
// records last kept if its key is theirs.
void mtwnTestSweep(mtwnContext* context, mtwnPlayerId player, const mtwnTestSweepInputs* in,
                   bool near);

// Whether the context is inside a call, so host code called back from it
// is refused.
bool mtwnTestIsBusy(const mtwnContext* context);

#endif // MAUL_TWEEN_SRC_TEST_HOOKS_H
