// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Decay and the rubber band (records mtwn-0010 and mtwn-0023). A decay
// moves velocity (1 - e^(-y)) / k from its start at y = k t, computed as
// velocity t times (1 - e^(-y)) / y, by its series below y = 1/2, where
// 1 - e^(-y) would lose the digits of a small y. Its crossing time
// -ln(1 - f) / k is 2 atanh(f / (2 - f)) / k by the atanh series for
// f <= 1/2, for the same reason.

#include "elementary.h"

#include "maul-tween/spring.h"

#include <math.h>

static const double s_seriesBelow = 0.5;

mtwnResult mtwnMakeDecay(double rate, mtwnDecay* decayOut)
{
    if (decayOut == NULL || !isfinite(rate) || rate <= 0.0)
    {
        return mtwn_errorInvalid;
    }
    decayOut->rate = rate;
    return mtwn_success;
}

mtwnResult mtwnMakeDecayFromDeceleration(double perMillisecond, mtwnDecay* decayOut)
{
    // A share outside (0, 1) gives a rate that is NaN, infinite or not above
    // 0, which mtwnMakeDecay refuses.
    return mtwnMakeDecay(-1000.0 * mtwnLog(perMillisecond), decayOut);
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

void mtwnSampleDecay(const mtwnDecay* decay, double position, double velocity, double seconds,
                     double* positionOut, double* velocityOut)
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

double mtwnDecayStopTime(const mtwnDecay* decay, double velocity, double stopSpeed)
{
    if (!(stopSpeed > 0.0))
    {
        return HUGE_VAL;
    }
    double speed = fabs(velocity);
    return speed <= stopSpeed ? 0.0 : mtwnLog(speed / stopSpeed) / decay->rate;
}

double mtwnDecayCrossingTime(const mtwnDecay* decay, double position, double velocity,
                             double target)
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

double mtwnRubberBand(double distance, double dimension, double coefficient)
{
    if (!(dimension > 0.0) || !(coefficient > 0.0) || isnan(distance))
    {
        return 0.0;
    }
    double past = fabs(distance);
    // s d / (s + d) with s = c x, as m / (1 + m / M) for the smaller m and
    // larger M of the two, which neither overflows nor underflows, and is d
    // for an infinite drag; m when both are infinite, as the form tends to.
    double scaled = coefficient * past;
    double smaller = scaled < dimension ? scaled : dimension;
    double larger = scaled < dimension ? dimension : scaled;
    return copysign(isinf(smaller) ? smaller : smaller / (1.0 + smaller / larger), distance);
}

double mtwnRubberBandInverse(double offset, double dimension, double coefficient)
{
    if (!(dimension > 0.0) || !(coefficient > 0.0) || isnan(offset))
    {
        return 0.0;
    }
    double moved = fabs(offset);
    if (moved >= dimension)
    {
        return copysign(HUGE_VAL, offset);
    }
    // An endless view resists nothing: d / (d - o) tends to 1.
    double resisted = isinf(dimension) ? 1.0 : dimension / (dimension - moved);
    return copysign(resisted * moved / coefficient, offset);
}
