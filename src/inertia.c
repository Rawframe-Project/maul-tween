// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Inertia: a decay landing on a chosen end, and a
// decay held between bounds that hands over to a spring at the bound it
// crosses, at that crossing time and with the velocity it has there. Kept
// apart from the decay, whose sampling it calls rather than copies.

#include "fpenv.h"
#include "work.h"

#include "maul-tween/spring.h"

#include <math.h>
#include <stddef.h>

static MTWN_NOINLINE mtwnResult mtwnMakeDecayToRestWork(double position, double velocity,
                                                        double rest, mtwnDecay* decayOut)
{
    if (!isfinite(position) || !isfinite(velocity) || !isfinite(rest))
    {
        return mtwn_errorInvalid;
    }
    // An end behind the start or at it, a velocity of 0 or a distance that
    // overflows gives a rate not finite or not above 0, refused there.
    return mtwnMakeDecayWork(velocity / (rest - position), decayOut);
}

mtwnResult mtwnMakeDecayToRest(double position, double velocity, double rest, mtwnDecay* decayOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeDecayToRestWork(position, velocity, rest, decayOut);
    mtwnFloatEnvLeave(env);
    return result;
}

static MTWN_NOINLINE mtwnResult mtwnMakeBoundedDecayWork(const mtwnDecay* decay,
                                                         const mtwnSpring* spring, double lower,
                                                         double upper, mtwnBoundedDecay* boundedOut)
{
    // A lower bound of infinity or an upper of -infinity leaves nowhere
    // finite to rest: a start short of it would bounce toward it forever.
    if (decay == NULL || spring == NULL || boundedOut == NULL || !(lower <= upper) ||
        lower == HUGE_VAL || upper == -HUGE_VAL)
    {
        return mtwn_errorInvalid;
    }
    *boundedOut = (mtwnBoundedDecay){*decay, *spring, lower, upper};
    return mtwn_success;
}

mtwnResult mtwnMakeBoundedDecay(const mtwnDecay* decay, const mtwnSpring* spring, double lower,
                                double upper, mtwnBoundedDecay* boundedOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeBoundedDecayWork(decay, spring, lower, upper, boundedOut);
    mtwnFloatEnvLeave(env);
    return result;
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
    out->time = mtwnDecayCrossingTimeWork(&bounded->decay, position, velocity, out->bound);
    // A rest past the bound by rounding alone never reaches it.
    if (isinf(out->time))
    {
        return false;
    }
    out->offset = 0.0;
    mtwnSampleDecayWork(&bounded->decay, position, velocity, out->time, NULL, &out->velocity);
    return true;
}

MTWN_NOINLINE void mtwnSampleBoundedDecayWork(const mtwnBoundedDecay* bounded, double position,
                                              double velocity, double seconds, double* positionOut,
                                              double* velocityOut)
{
    Handoff handoff;
    if (!HandoffOf(bounded, position, velocity, &handoff) || seconds < handoff.time)
    {
        mtwnSampleDecayWork(&bounded->decay, position, velocity, seconds, positionOut, velocityOut);
        return;
    }
    double offset = 0.0;
    mtwnSampleSpringWork(&bounded->spring, handoff.offset, handoff.velocity, seconds - handoff.time,
                         &offset, velocityOut);
    if (positionOut != NULL)
    {
        *positionOut = handoff.bound + offset;
    }
}

void mtwnSampleBoundedDecay(const mtwnBoundedDecay* bounded, double position, double velocity,
                            double seconds, double* positionOut, double* velocityOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnSampleBoundedDecayWork(bounded, position, velocity, seconds, positionOut, velocityOut);
    mtwnFloatEnvLeave(env);
}

MTWN_NOINLINE double mtwnBoundedDecayStopTimeWork(const mtwnBoundedDecay* bounded, double position,
                                                  double velocity, double offsetTolerance,
                                                  double stopSpeed)
{
    if (!(offsetTolerance > 0.0) || !(stopSpeed > 0.0))
    {
        return HUGE_VAL;
    }
    Handoff handoff;
    if (!HandoffOf(bounded, position, velocity, &handoff))
    {
        return mtwnDecayStopTimeWork(&bounded->decay, velocity, stopSpeed);
    }
    return handoff.time + mtwnSpringSettlingTimeWork(&bounded->spring, handoff.offset,
                                                     handoff.velocity, offsetTolerance, stopSpeed);
}

double mtwnBoundedDecayStopTime(const mtwnBoundedDecay* bounded, double position, double velocity,
                                double offsetTolerance, double stopSpeed)
{
    uint64_t env = mtwnFloatEnvEnter();
    double result =
        mtwnBoundedDecayStopTimeWork(bounded, position, velocity, offsetTolerance, stopSpeed);
    mtwnFloatEnvLeave(env);
    return result;
}
