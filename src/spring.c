// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Springs in one closed form (records mtwn-0010 and mtwn-0023). With
// decay rate a = zeta w0 and damped frequency w = w0 sqrt(|1 - zeta^2|),
// a start (x0, v0) moves as x = e^(-a t) (x0 C + (v0 + a x0) S) and
// v = e^(-a t) (v0 C - (a v0 + w0^2 x0) S), where C and S are cos(w t)
// and sin(w t) / w below critical damping, 1 and t at it, cosh(w t) and
// sinh(w t) / w above it. Below w t = 1/2, S is t times a series, so the
// regimes meet without dividing by a vanishing w; above it an
// overdamped spring is written in its two rates, the slow one as
// w0^2 / (a + w), so nothing cancels or overflows however heavy the
// damping.

#include "maul-tween/spring.h"

#include "elementary.h"

#include "generated/math_constants.h"

#include <math.h>

enum
{
    s_frequency = 0,
    s_ratio = 1,
    s_decay = 2,
    s_damped = 3,
    s_squared = 4,
};

enum
{
    s_underdamped = 0,
    s_critical = 1,
    s_overdamped = 2,
};

// Where the series take over from the closed forms of S.
static const double s_seriesBelow = 0.5;
// Newton's steps for a settling time, and when one is small enough.
static const int s_newtonSteps = 64;
static const double s_newtonStop = 0x1.0p-50;

// sin(y) / y for |y| < 1/2, and sinh(y) / y with sign 1 instead of -1:
// the series to y^12, whose next term is under 5e-17.
static double SincSeries(double y, double sign)
{
    double z = sign * y * y;
    double sum = 1.0 / 6227020800.0;
    sum = sum * z + 1.0 / 39916800.0;
    sum = sum * z + 1.0 / 362880.0;
    sum = sum * z + 1.0 / 5040.0;
    sum = sum * z + 1.0 / 120.0;
    sum = sum * z + 1.0 / 6.0;
    return 1.0 + z * sum;
}

static mtwnResult Finish(double frequency, double ratio, mtwnSpring* springOut)
{
    if (springOut == NULL || !isfinite(frequency) || frequency <= 0.0 || !isfinite(ratio) ||
        ratio < 0.0)
    {
        return mtwn_errorInvalid;
    }
    double decay = ratio * frequency;
    double damped = frequency * sqrt(fabs((1.0 - ratio) * (1.0 + ratio)));
    double squared = frequency * frequency;
    if (!isfinite(decay + damped) || !isfinite(squared) || squared == 0.0)
    {
        return mtwn_errorInvalid;
    }
    mtwnSpring spring = {{0.0}, 0, 0};
    spring.parameters[s_frequency] = frequency;
    spring.parameters[s_ratio] = ratio;
    spring.parameters[s_decay] = decay;
    spring.parameters[s_damped] = damped;
    spring.parameters[s_squared] = squared;
    spring.regime = ratio < 1.0 ? s_underdamped : (ratio == 1.0 ? s_critical : s_overdamped);
    *springOut = spring;
    return mtwn_success;
}

mtwnResult mtwnMakeSpring(double stiffness, double damping, double mass, mtwnSpring* springOut)
{
    if (!isfinite(stiffness) || stiffness <= 0.0 || !isfinite(damping) || damping < 0.0 ||
        !isfinite(mass) || mass <= 0.0)
    {
        return mtwn_errorInvalid;
    }
    // sqrt(k m) as a product of roots, which cannot overflow where k m does.
    double frequency = sqrt(stiffness / mass);
    double ratio = damping / (2.0 * sqrt(stiffness) * sqrt(mass));
    return Finish(frequency, ratio, springOut);
}

mtwnResult mtwnMakeSpringFromFrequency(double hertz, double dampingRatio, mtwnSpring* springOut)
{
    if (!isfinite(hertz) || hertz <= 0.0)
    {
        return mtwn_errorInvalid;
    }
    return Finish(4.0 * MTWN_PIO2_HI * hertz, dampingRatio, springOut);
}

mtwnResult mtwnMakeSpringFromDuration(double duration, double bounce, mtwnSpring* springOut)
{
    if (!isfinite(duration) || duration <= 0.0 || !(bounce > -1.0 && bounce <= 1.0))
    {
        return mtwn_errorInvalid;
    }
    double ratio = bounce >= 0.0 ? 1.0 - bounce : 1.0 / (1.0 + bounce);
    return Finish(4.0 * MTWN_PIO2_HI / duration, ratio, springOut);
}

// The motion from e^(-a t), C and S.
static mtwnSpringMotion FromTerms(const mtwnSpring* spring, double envelope, double c, double s)
{
    double decay = spring->parameters[s_decay];
    double scaledSine = envelope * s;
    mtwnSpringMotion motion;
    motion.offsetFromOffset = envelope * c + decay * scaledSine;
    motion.offsetFromVelocity = scaledSine;
    motion.velocityFromOffset = -spring->parameters[s_squared] * scaledSine;
    motion.velocityFromVelocity = envelope * c - decay * scaledSine;
    return motion;
}

// The overdamped motion in its two rates, for w t >= 1/2.
static mtwnSpringMotion Overdamped(const mtwnSpring* spring, double seconds)
{
    double decay = spring->parameters[s_decay];
    double damped = spring->parameters[s_damped];
    double fast = decay + damped;
    double slow = spring->parameters[s_squared] / fast;
    double p = mtwnExp(-slow * seconds);
    double q = mtwnExp(-fast * seconds);
    double twice = 2.0 * damped;
    mtwnSpringMotion motion;
    motion.offsetFromOffset = (fast * p - slow * q) / twice;
    motion.offsetFromVelocity = (p - q) / twice;
    motion.velocityFromOffset = -spring->parameters[s_squared] * ((p - q) / twice);
    motion.velocityFromVelocity = (fast * q - slow * p) / twice;
    return motion;
}

mtwnSpringMotion mtwnSpringMotionAt(const mtwnSpring* spring, double seconds)
{
    double decay = spring->parameters[s_decay];
    double damped = spring->parameters[s_damped];
    if (isinf(seconds))
    {
        double rest = decay > 0.0 ? 0.0 : (double)NAN;
        return (mtwnSpringMotion){rest, rest, rest, rest};
    }
    double envelope = mtwnExp(-decay * seconds);
    double y = damped * seconds;
    switch (spring->regime)
    {
    case s_critical:
        return FromTerms(spring, envelope, 1.0, seconds);
    case s_underdamped:
    {
        double sine = 0.0;
        double cosine = 0.0;
        mtwnSinCos(y, &sine, &cosine);
        double s = y < s_seriesBelow ? seconds * SincSeries(y, -1.0) : sine / damped;
        return FromTerms(spring, envelope, cosine, s);
    }
    default:
        if (y >= s_seriesBelow)
        {
            return Overdamped(spring, seconds);
        }
        return FromTerms(spring, envelope, 0.5 * (mtwnExp(y) + mtwnExp(-y)),
                         seconds * SincSeries(y, 1.0));
    }
}

void mtwnSampleSpring(const mtwnSpring* spring, double offset, double velocity, double seconds,
                      double* offsetOut, double* velocityOut)
{
    mtwnSpringMotion motion = mtwnSpringMotionAt(spring, seconds);
    if (offsetOut != NULL)
    {
        *offsetOut = motion.offsetFromOffset * offset + motion.offsetFromVelocity * velocity;
    }
    if (velocityOut != NULL)
    {
        *velocityOut = motion.velocityFromOffset * offset + motion.velocityFromVelocity * velocity;
    }
}

// The bound e^(-r t) (a + b min(t, cap)) on |x| for x = e^(-alpha t) (x0 C
// + B S), with a = |x0| and b = |B|, and its logarithm's distance above
// ln tolerance.
typedef struct Bound
{
    double a;
    double b;
    double rate;
    double cap;
    double logTolerance;
} Bound;

static double Excess(const Bound* bound, double t)
{
    double g = t < bound->cap ? t : bound->cap;
    return mtwnLog(bound->a + bound->b * g) - bound->rate * t - bound->logTolerance;
}

// The time after which the bound stays at most the tolerance. Its
// logarithm is concave, so it crosses the tolerance last where it is
// falling, and Newton's iterates from the right never pass that time.
static double BoundTime(const Bound* bound)
{
    if (bound->a == 0.0 && bound->b == 0.0)
    {
        return 0.0;
    }
    // The bound's largest value, where its logarithm's slope is 0.
    double peak = 0.0;
    if (bound->b > 0.0 && bound->rate > 0.0)
    {
        peak = 1.0 / bound->rate - bound->a / bound->b;
        peak = peak < 0.0 ? 0.0 : (peak > bound->cap ? bound->cap : peak);
    }
    else if (bound->b > 0.0)
    {
        peak = bound->cap;
    }
    if (Excess(bound, peak) <= 0.0)
    {
        return 0.0;
    }
    if (bound->rate == 0.0)
    {
        return HUGE_VAL;
    }
    if (isfinite(bound->cap))
    {
        // Past the cap the bound is a pure exponential.
        double flat =
            (mtwnLog(bound->a + bound->b * bound->cap) - bound->logTolerance) / bound->rate;
        if (flat >= bound->cap)
        {
            return flat;
        }
    }
    double t = isfinite(bound->cap) ? bound->cap : (peak > 0.0 ? 2.0 * peak : 1.0 / bound->rate);
    while (Excess(bound, t) > 0.0)
    {
        t *= 2.0;
    }
    for (int i = 0; i < s_newtonSteps; ++i)
    {
        double slope = bound->b / (bound->a + bound->b * t) - bound->rate;
        double step = Excess(bound, t) / slope;
        t -= step;
        if (fabs(step) <= s_newtonStop * t)
        {
            break;
        }
    }
    return t;
}

double mtwnSpringSettlingTime(const mtwnSpring* spring, double offset, double velocity,
                              double offsetTolerance, double velocityTolerance)
{
    if (!(offsetTolerance > 0.0) || !(velocityTolerance > 0.0))
    {
        return HUGE_VAL;
    }
    double decay = spring->parameters[s_decay];
    double damped = spring->parameters[s_damped];
    Bound bound = {0.0, 0.0, decay, HUGE_VAL, 0.0};
    if (spring->regime == s_underdamped)
    {
        bound.cap = 1.0 / damped;
    }
    else if (spring->regime == s_overdamped)
    {
        // e^(-a t) cosh(w t) <= e^(-(a - w) t), and e^(-a t) sinh(w t) / w
        // <= e^(-(a - w) t) min(t, 1 / (2 w)).
        bound.rate = spring->parameters[s_squared] / (decay + damped);
        bound.cap = 0.5 / damped;
    }
    Bound offsetBound = bound;
    offsetBound.a = fabs(offset);
    offsetBound.b = fabs(velocity + decay * offset);
    offsetBound.logTolerance = mtwnLog(offsetTolerance);
    Bound velocityBound = bound;
    velocityBound.a = fabs(velocity);
    velocityBound.b = fabs(decay * velocity + spring->parameters[s_squared] * offset);
    velocityBound.logTolerance = mtwnLog(velocityTolerance);
    double offsetTime = BoundTime(&offsetBound);
    double velocityTime = BoundTime(&velocityBound);
    return offsetTime > velocityTime ? offsetTime : velocityTime;
}
