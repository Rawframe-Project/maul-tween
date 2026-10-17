// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Inertia (record mtwn-0063): a decay landing on a chosen end, and a
// decay held between bounds that hands over to a spring at the bound it
// crosses, at that crossing time and with the velocity it has there. Kept
// apart from the decay, whose sampling it calls rather than copies.

#include "fpenv.h"

#include "maul-tween/spring.h"

#include <math.h>
#include <stddef.h>

mtwnResult mtwnMakeDecayToRest(double position, double velocity, double rest, mtwnDecay* decayOut)
{
    if (!isfinite(position) || !isfinite(velocity) || !isfinite(rest))
    {
        return mtwn_errorInvalid;
    }
    // An end behind the start or at it, a velocity of 0 or a distance that
    // overflows gives a rate not finite or not above 0, refused there.
    return mtwnMakeDecay(velocity / (rest - position), decayOut);
}

mtwnResult mtwnMakeBoundedDecay(const mtwnDecay* decay, const mtwnSpring* spring, double lower,
                                double upper, mtwnBoundedDecay* boundedOut)
{
    if (decay == NULL || spring == NULL || boundedOut == NULL || !(lower <= upper))
    {
        return mtwn_errorInvalid;
    }
    *boundedOut = (mtwnBoundedDecay){*decay, *spring, lower, upper};
    return mtwn_success;
}

// Where a bounded decay from a start goes over to its spring: the bound,
// the time, and the offset from the bound and velocity then. False when
// it coasts to rest within the bounds.
typedef struct Handoff
{
    double bound;
    double time;
    double offset;
    double velocity;
} Handoff;

static MTWN_NOINLINE bool HandoffOf(const mtwnBoundedDecay* bounded, double position,
                                    double velocity, Handoff* out)
{
    if (position < bounded->lower || position > bounded->upper)
    {
        double bound = position < bounded->lower ? bounded->lower : bounded->upper;
        *out = (Handoff){bound, 0.0, position - bound, velocity};
        return true;
    }
    double rest = position + velocity / bounded->decay.rate;
    if (!(rest < bounded->lower || rest > bounded->upper))
    {
        return false;
    }
    out->bound = rest < bounded->lower ? bounded->lower : bounded->upper;
    out->time = mtwnDecayCrossingTime(&bounded->decay, position, velocity, out->bound);
    // A rest past the bound by rounding alone never reaches it.
    if (isinf(out->time))
    {
        return false;
    }
    out->offset = 0.0;
    mtwnSampleDecay(&bounded->decay, position, velocity, out->time, NULL, &out->velocity);
    return true;
}

void mtwnSampleBoundedDecay(const mtwnBoundedDecay* bounded, double position, double velocity,
                            double seconds, double* positionOut, double* velocityOut)
{
    Handoff handoff;
    if (!HandoffOf(bounded, position, velocity, &handoff) || seconds < handoff.time)
    {
        mtwnSampleDecay(&bounded->decay, position, velocity, seconds, positionOut, velocityOut);
        return;
    }
    double offset = 0.0;
    mtwnSampleSpring(&bounded->spring, handoff.offset, handoff.velocity, seconds - handoff.time,
                     &offset, velocityOut);
    if (positionOut != NULL)
    {
        *positionOut = handoff.bound + offset;
    }
}

double mtwnBoundedDecayStopTime(const mtwnBoundedDecay* bounded, double position, double velocity,
                                double offsetTolerance, double stopSpeed)
{
    if (!(offsetTolerance > 0.0) || !(stopSpeed > 0.0))
    {
        return HUGE_VAL;
    }
    Handoff handoff;
    if (!HandoffOf(bounded, position, velocity, &handoff))
    {
        return mtwnDecayStopTime(&bounded->decay, velocity, stopSpeed);
    }
    return handoff.time + mtwnSpringSettlingTime(&bounded->spring, handoff.offset, handoff.velocity,
                                                 offsetTolerance, stopSpeed);
}
