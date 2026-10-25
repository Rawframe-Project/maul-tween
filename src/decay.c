// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Decay and the rubber band. A decay
// moves velocity (1 - e^(-y)) / k from its start at y = k t, computed as
// velocity t times (1 - e^(-y)) / y, by its series below y = 1/2, where
// 1 - e^(-y) would lose the digits of a small y. Its crossing time
// -ln(1 - f) / k is 2 atanh(f / (2 - f)) / k by the atanh series for
// f <= 1/2, for the same reason.

#include "elementary.h"
#include "fpenv.h"
#include "valid.h"
#include "work.h"

#include "maul-tween/spring.h"

#include <math.h>

static const double s_seriesBelow = 0.5;

MTWN_NOINLINE mtwnResult mtwnMakeDecayWork(double rate, mtwnDecay* decayOut)
{
    mtwnDecay decay = {rate};
    if (decayOut == NULL || !mtwnValidDecay(&decay))
    {
        return mtwn_errorInvalid;
    }
    *decayOut = decay;
    return mtwn_success;
}

bool mtwnValidDecay(const mtwnDecay* decay)
{
    return decay != NULL && isfinite(decay->rate) && decay->rate > 0.0;
}

mtwnResult mtwnMakeDecay(double rate, mtwnDecay* decayOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeDecayWork(rate, decayOut);
    mtwnFloatEnvLeave(env);
    return result;
}

static MTWN_NOINLINE mtwnResult mtwnMakeDecayFromDecelerationWork(double perMillisecond,
                                                                  mtwnDecay* decayOut)
{
    // A share outside (0, 1) gives a rate that is NaN, infinite or not above
    // 0, which mtwnMakeDecay refuses.
    return mtwnMakeDecay(-1000.0 * mtwnLog(perMillisecond), decayOut);
}

mtwnResult mtwnMakeDecayFromDeceleration(double perMillisecond, mtwnDecay* decayOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeDecayFromDecelerationWork(perMillisecond, decayOut);
    mtwnFloatEnvLeave(env);
    return result;
}

// (1 - e^(-y)) / y for y >= 0.
static double TravelShare(double y)
{
    if (y >= s_seriesBelow)
    {
        return (1.0 - mtwnExp(-y)) / y;
    }
    // The sum of (-y)^n / (n + 1)! for n from 0 to 17, nested as 1 + (-y)
    // / 2 (1 + (-y) / 3 (1 + ...)); the next term is under 1e-20 at
    // y = 1/2.
    double sum = 1.0;
    for (int k = 18; k >= 2; --k)
    {
        sum = 1.0 + -y / (double)k * sum;
    }
    return sum;
}

MTWN_NOINLINE void mtwnSampleDecayWork(const mtwnDecay* decay, double position, double velocity,
                                       double seconds, double* positionOut, double* velocityOut)
{
    double rate = decay->rate;
    double travel = 0.0;
    double kept = 0.0;
    if (isinf(seconds))
    {
        travel = velocity / rate;
    }
    else
    {
        double y = rate * seconds;
        travel = velocity * seconds * TravelShare(y);
        kept = velocity * mtwnExp(-y);
    }
    if (positionOut != NULL)
    {
        *positionOut = position + travel;
    }
    if (velocityOut != NULL)
    {
        *velocityOut = kept;
    }
}

static MTWN_NOINLINE mtwnResult mtwnSampleDecayChecked(const mtwnDecay* decay, double position,
                                                       double velocity, double seconds,
                                                       double* positionOut, double* velocityOut)
{
    if (!mtwnValidDecay(decay) || !isfinite(position) || !isfinite(velocity) || !(seconds >= 0.0))
    {
        return mtwn_errorInvalid;
    }
    mtwnSampleDecayWork(decay, position, velocity, seconds, positionOut, velocityOut);
    return mtwn_success;
}

mtwnResult mtwnSampleDecay(const mtwnDecay* decay, double position, double velocity, double seconds,
                           double* positionOut, double* velocityOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result =
        mtwnSampleDecayChecked(decay, position, velocity, seconds, positionOut, velocityOut);
    mtwnFloatEnvLeave(env);
    return result;
}

MTWN_NOINLINE double mtwnDecayStopTimeWork(const mtwnDecay* decay, double velocity,
                                           double stopSpeed)
{
    if (!(stopSpeed > 0.0))
    {
        return HUGE_VAL;
    }
    double speed = fabs(velocity);
    return speed <= stopSpeed ? 0.0 : mtwnLog(speed / stopSpeed) / decay->rate;
}

static MTWN_NOINLINE mtwnResult mtwnDecayStopTimeChecked(const mtwnDecay* decay, double velocity,
                                                         double stopSpeed, double* secondsOut)
{
    if (!mtwnValidDecay(decay) || !isfinite(velocity) || !(stopSpeed > 0.0) || secondsOut == NULL)
    {
        return mtwn_errorInvalid;
    }
    *secondsOut = mtwnDecayStopTimeWork(decay, velocity, stopSpeed);
    return mtwn_success;
}

mtwnResult mtwnDecayStopTime(const mtwnDecay* decay, double velocity, double stopSpeed,
                             double* secondsOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnDecayStopTimeChecked(decay, velocity, stopSpeed, secondsOut);
    mtwnFloatEnvLeave(env);
    return result;
}

MTWN_NOINLINE double mtwnDecayCrossingTimeWork(const mtwnDecay* decay, double position,
                                               double velocity, double target)
{
    double distance = target - position;
    if (distance == 0.0)
    {
        return 0.0;
    }
    // The share of the whole travel, velocity / k, the target lies at.
    double share = distance * decay->rate / velocity;
    if (!(share > 0.0 && share < 1.0))
    {
        return HUGE_VAL;
    }
    if (share > s_seriesBelow)
    {
        // 1 - share is exact here (Sterbenz).
        return -mtwnLog(1.0 - share) / decay->rate;
    }
    // 2 atanh(s) with s = share / (2 - share) <= 1/3: s^2 <= 1/9, so 17
    // terms leave under 1e-17.
    double s = share / (2.0 - share);
    double z = s * s;
    double sum = 0.0;
    for (int n = 16; n >= 0; --n)
    {
        sum = sum * z + 1.0 / (double)(2 * n + 1);
    }
    return 2.0 * s * sum / decay->rate;
}

static MTWN_NOINLINE mtwnResult mtwnDecayCrossingTimeChecked(const mtwnDecay* decay,
                                                             double position, double velocity,
                                                             double target, double* secondsOut)
{
    if (!mtwnValidDecay(decay) || !isfinite(position) || !isfinite(velocity) || !isfinite(target) ||
        secondsOut == NULL)
    {
        return mtwn_errorInvalid;
    }
    *secondsOut = mtwnDecayCrossingTimeWork(decay, position, velocity, target);
    return mtwn_success;
}

mtwnResult mtwnDecayCrossingTime(const mtwnDecay* decay, double position, double velocity,
                                 double target, double* secondsOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnDecayCrossingTimeChecked(decay, position, velocity, target, secondsOut);
    mtwnFloatEnvLeave(env);
    return result;
}

// Whether a rubber band takes a view's dimension and a coefficient: above
// 0, an endless one too.
static bool Bands(double dimension, double coefficient)
{
    return dimension > 0.0 && coefficient > 0.0;
}

static double RubberBand(double distance, double dimension, double coefficient)
{
    double past = fabs(distance);
    // Nothing past the limit moves nothing, for any coefficient: an endless
    // one's c x would be NaN.
    if (past == 0.0)
    {
        return distance;
    }
    // s d / (s + d) with s = c x, as m / (1 + m / M) for the smaller m and
    // larger M of the two, which neither overflows nor underflows, and is d
    // for an infinite drag; m when both are infinite, as the form tends to.
    double scaled = coefficient * past;
    double smaller = scaled < dimension ? scaled : dimension;
    double larger = scaled < dimension ? dimension : scaled;
    return copysign(isinf(smaller) ? smaller : smaller / (1.0 + smaller / larger), distance);
}

static MTWN_NOINLINE mtwnResult mtwnRubberBandChecked(double distance, double dimension,
                                                      double coefficient, double* offsetOut)
{
    if (offsetOut == NULL || !Bands(dimension, coefficient) || isnan(distance))
    {
        return mtwn_errorInvalid;
    }
    *offsetOut = RubberBand(distance, dimension, coefficient);
    return mtwn_success;
}

mtwnResult mtwnRubberBand(double distance, double dimension, double coefficient, double* offsetOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnRubberBandChecked(distance, dimension, coefficient, offsetOut);
    mtwnFloatEnvLeave(env);
    return result;
}

static double RubberBandInverse(double offset, double dimension, double coefficient)
{
    double moved = fabs(offset);
    if (moved >= dimension)
    {
        return copysign(HUGE_VAL, offset);
    }
    // An endless view resists nothing: d / (d - o) tends to 1.
    double resisted = isinf(dimension) ? 1.0 : dimension / (dimension - moved);
    return copysign(resisted * moved / coefficient, offset);
}

static MTWN_NOINLINE mtwnResult mtwnRubberBandInverseChecked(double offset, double dimension,
                                                             double coefficient, double* dragOut)
{
    if (dragOut == NULL || !Bands(dimension, coefficient) || isnan(offset))
    {
        return mtwn_errorInvalid;
    }
    *dragOut = RubberBandInverse(offset, dimension, coefficient);
    return mtwn_success;
}

mtwnResult mtwnRubberBandInverse(double offset, double dimension, double coefficient,
                                 double* dragOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnRubberBandInverseChecked(offset, dimension, coefficient, dragOut);
    mtwnFloatEnvLeave(env);
    return result;
}
