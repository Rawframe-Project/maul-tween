// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The easings against GSAP's and tween.js's own, run under Node by
// tools/oracle/rivals.mjs: with their parameters
// mapped as the easing header documents, each curve agrees with the
// rival's to a few units in the last place, except where Maul Tween lands
// exactly on 0 and 1 and the rival does not (expo, elastic), where the
// difference comes from the rival's miss.

#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/easing.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

typedef struct RivalCurve
{
    int source;
    int kind;
    int mode;
    double a;
    double b;
    double values[101];
} RivalCurve;

#include "generated/rival_easing.h"

enum
{
    rivalPower = 0,
    rivalSine = 1,
    rivalExpo = 2,
    rivalCirc = 3,
    rivalBounce = 4,
    rivalBack = 5,
    rivalElastic = 6,
};

static bool Make(const RivalCurve* curve, mtwnEasing* easing)
{
    static const mtwnEasingKind s_penner[] = {0, mtwn_easingSine, mtwn_easingExpo, mtwn_easingCirc,
                                              mtwn_easingBounce};
    mtwnEaseMode mode = (mtwnEaseMode)curve->mode;
    switch (curve->kind)
    {
    case rivalPower:
        return mtwnMakePowerEasing(mode, curve->a, easing) == mtwn_success;
    case rivalBack:
        return mtwnMakeBackEasing(mode, curve->a, easing) == mtwn_success;
    case rivalElastic:
        return mtwnMakeElasticEasing(mode, curve->a, curve->b, easing) == mtwn_success;
    default:
        return mtwnMakePennerEasing(s_penner[curve->kind], mode, easing) == mtwn_success;
    }
}

// The largest difference allowed from a rival's curve: rounding for the
// curves defined alike; for elastic the normalization by the rivals' end
// e = 1 + q 2^-10 sin(...), q the larger of a and 1 as GSAP takes an
// amplitude below 1: the rival's g differs from g / e by |g| |e - 1| / e,
// with |g| at most 1 + q, |e - 1| at most q 2^-10 and e at least
// 1 - q 2^-10; for expo the normalization, 2^-10 from Penner's (tween.js),
// and GSAP's own blend.
static double Bound(const RivalCurve* curve)
{
    if (curve->kind == rivalElastic)
    {
        double q = curve->a < 1.0 ? 1.0 : curve->a;
        double miss = q * 0x1.0p-10;
        return (1.0 + q) * miss / (1.0 - miss);
    }
    if (curve->kind == rivalExpo)
    {
        return curve->source == 0 ? 0x1.0p-7 : 0x1.0p-10;
    }
    return 0x1.0p-51;
}

static void TestRivals(void)
{
    uint32_t count = (uint32_t)(sizeof s_rivalCurves / sizeof s_rivalCurves[0]);
    int made = 0;
    int within = 0;
    for (uint32_t c = 0; c < count; ++c)
    {
        const RivalCurve* curve = &s_rivalCurves[c];
        mtwnEasing easing;
        if (!Make(curve, &easing))
        {
            continue;
        }
        ++made;
        double worst = 0.0;
        for (int i = 0; i <= RIVAL_STEPS; ++i)
        {
            double p = (double)i / RIVAL_STEPS;
            double error = fabs(EaseAt(&easing, p, false) - curve->values[i]);
            worst = error > worst ? error : worst;
        }
        if (worst <= Bound(curve))
        {
            ++within;
        }
        else
        {
            printf("source %d kind %d mode %d a %g b %g: %.3g over %.3g\n", curve->source,
                   curve->kind, curve->mode, curve->a, curve->b, worst, Bound(curve));
        }
    }
    CHECK(count == 90 && made == (int)count, "every curve the oracle wrote, made");
    CHECK(within == made, "every curve within its bound of the rival's");
}

int main(void)
{
    TestRivals();
    return s_failures == 0 ? 0 : 1;
}
