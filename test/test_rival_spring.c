// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Springs against Motion's own, run under Node by tools/oracle/rivals.mjs:
// by stiffness, damping and mass, underdamped, near
// and at critical damping and overdamped, with and without a starting
// velocity, the value and the velocity agree with Motion's closed forms
// to rounding, scaled by the spring's amplitude.

#include "test_harness.h"

#include "maul-tween/spring.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

typedef struct RivalSpring
{
    // Stiffness, damping, mass, origin, target, velocity per second.
    double params[6];
    double values[101];
    double velocities[101];
} RivalSpring;

#include "generated/rival_spring.h"

static void TestSprings(void)
{
    uint32_t count = (uint32_t)(sizeof s_rivalSprings / sizeof s_rivalSprings[0]);
    int within = 0;
    for (uint32_t c = 0; c < count; ++c)
    {
        const RivalSpring* r = &s_rivalSprings[c];
        mtwnSpring spring;
        CHECK(mtwnMakeSpring(r->params[0], r->params[1], r->params[2], &spring) == mtwn_success,
              "a spring");
        // The motion's scale: its offset and how far its velocity alone
        // carries it in a radian of the undamped frequency.
        double omega = sqrt(r->params[0] / r->params[2]);
        double offset0 = r->params[3] - r->params[4];
        double scale = fabs(offset0) + fabs(r->params[5]) / omega;
        bool ok = true;
        for (int i = 0; i <= RIVAL_SPRING_STEPS; ++i)
        {
            double offset = 0.0;
            double speed = 0.0;
            mtwnSampleSpring(&spring, offset0, r->params[5], i * 0.02, &offset, &speed);
            ok = ok && fabs(r->params[4] + offset - r->values[i]) <= 64.0 * DBL_EPSILON * scale &&
                 fabs(speed - r->velocities[i]) <= 64.0 * DBL_EPSILON * scale * omega;
        }
        within += ok;
    }
    CHECK(count == 8, "every spring the oracle wrote");
    CHECK(within == (int)count, "values and velocities within 64 ulps of the scale of Motion's");
}

int main(void)
{
    TestSprings();
    return s_failures == 0 ? 0 : 1;
}
