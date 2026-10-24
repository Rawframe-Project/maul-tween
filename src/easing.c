// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The easings. Each kind has one
// native curve: Penner's in curve for most, the out curve for elastic
// and bounce, which GSAP and Penner define by it, and CSS's curves from
// src/css_easing.c. A curve not exact at
// both ends is normalized by its values there, computed when it is made
// by the same code that samples it, so (g(p) - g(0)) / (g(1) - g(0)) is
// exactly 0 and 1 at the ends. The modes are reflections of the native
// curve, taken so that each half lands where the other starts.

#include "maul-tween/easing.h"

#include "css_easing.h"
#include "easing_internal.h"
#include "elementary.h"
#include "fpenv.h"
#include "spring_easing.h"
#include "work.h"

#include "generated/math_constants.h"

#include <math.h>

// Where the normalization's numbers sit in parameters[].
enum
{
    s_start = MTWN_EASING_START,
    s_range = MTWN_EASING_RANGE,
};

// The largest exponent a power computes by multiplication.
static const uint32_t s_largestIntegerExponent = 8;

static bool IsMode(mtwnEaseMode mode)
{
    return mode <= mtwn_easeOutIn;
}

static double IntegerPower(double x, uint32_t n)
{
    return mtwnIntegerPower(x, n);
}

// Penner's bounce out, before normalization.
static double BounceOut(double q)
{
    if (q < 1.0 / 2.75)
    {
        return 7.5625 * q * q;
    }
    if (q < 2.0 / 2.75)
    {
        q -= 1.5 / 2.75;
        return 7.5625 * q * q + 0.75;
    }
    if (q < 2.5 / 2.75)
    {
        q -= 2.25 / 2.75;
        return 7.5625 * q * q + 0.9375;
    }
    q -= 2.625 / 2.75;
    return 7.5625 * q * q + 0.984375;
}

// The native curve of a Penner kind at x in [0, 1], before normalization.
static double RawCurve(const mtwnEasing* easing, double x)
{
    const double* parameter = easing->parameters;
    switch (easing->kind)
    {
    case mtwn_easingPower:
        return easing->count > 0 ? IntegerPower(x, easing->count) : mtwnPow(x, parameter[0]);
    case mtwn_easingSine:
    {
        double sine = 0.0;
        double cosine = 0.0;
        mtwnSinCos(x * MTWN_PIO2_HI, &sine, &cosine);
        return 1.0 - cosine;
    }
    case mtwn_easingExpo:
        return mtwnExp2(10.0 * (x - 1.0));
    case mtwn_easingCirc:
        // 1 - x^2 as a product, which keeps its digits near x = 1.
        return 1.0 - sqrt((1.0 - x) * (1.0 + x));
    case mtwn_easingBack:
        // (s + 1) x - s as x + s (x - 1): exactly 1 at x = 1 for any s,
        // where s + 1 rounds to s from 2^53 on, and with no cancellation
        // near the end.
        return x * x * (x + parameter[0] * (x - 1.0));
    case mtwn_easingElastic:
    {
        double sine = 0.0;
        double cosine = 0.0;
        mtwnSinCos((x - parameter[2]) * parameter[1], &sine, &cosine);
        return parameter[0] * mtwnExp2(-10.0 * x) * sine + 1.0;
    }
    default:
        return BounceOut(x);
    }
}

static bool IsNativeOut(mtwnEasingKind kind)
{
    return kind == mtwn_easingElastic || kind == mtwn_easingBounce;
}

static bool IsCss(mtwnEasingKind kind)
{
    return kind == mtwn_easingCubicBezier || kind == mtwn_easingSteps ||
           kind == mtwn_easingLinearPoints;
}

// The native curve, Penner's normalized to land exactly on 0 and 1.
// before reaches the curve only where x is progress unreflected.
static double NativeCurve(const mtwnEasing* easing, double x, bool before)
{
    switch (easing->kind)
    {
    case mtwn_easingHost:
        return easing->function(easing->context, x);
    case mtwn_easingCubicBezier:
        return mtwnSampleCubicBezier(easing->parameters, x);
    case mtwn_easingSteps:
        return mtwnSampleSteps(easing->count, easing->position, x, before);
    case mtwn_easingLinearPoints:
        return mtwnSampleLinearPoints(easing->points, easing->count, x);
    case mtwn_easingSpring:
        return mtwnSampleSpringEasing(easing, x);
    default:
    {
        // A curve whose raw ends are already 0 and 1 (the integer powers)
        // skips the division, which would give the same bits.
        double raw = RawCurve(easing, x);
        double start = easing->parameters[s_start];
        double range = easing->parameters[s_range];
        return start == 0.0 && range == 1.0 ? raw : (raw - start) / range;
    }
    }
}

// The in curve f(x) and the out curve 1 - f(1 - x), one of which is
// native; circ has both.
static double InCurve(const mtwnEasing* easing, double x, bool before)
{
    return IsNativeOut(easing->kind) ? 1.0 - NativeCurve(easing, 1.0 - x, false)
                                     : NativeCurve(easing, x, before);
}

static double OutCurve(const mtwnEasing* easing, double x)
{
    if (easing->kind == mtwn_easingCirc)
    {
        // sqrt(1 - (1 - x)^2) without forming 1 - x, which loses the digits
        // of a small x where the curve is steepest; exact at both ends.
        return sqrt(x * (2.0 - x));
    }
    return IsNativeOut(easing->kind) ? NativeCurve(easing, x, false)
                                     : 1.0 - NativeCurve(easing, 1.0 - x, false);
}

// Fills the normalization from the native curve's raw ends.
static void Normalize(mtwnEasing* easing)
{
    double start = RawCurve(easing, 0.0);
    easing->parameters[s_start] = start;
    easing->parameters[s_range] = RawCurve(easing, 1.0) - start;
}

static mtwnEasing Blank(mtwnEasingKind kind, mtwnEaseMode mode)
{
    mtwnEasing easing = {0};
    easing.kind = kind;
    easing.mode = mode;
    return easing;
}

mtwnEasing mtwnLinearEasing(void)
{
    return Blank(mtwn_easingLinear, mtwn_easeIn);
}

static MTWN_NOINLINE mtwnResult mtwnMakePowerEasingWork(mtwnEaseMode mode, double exponent,
                                                        mtwnEasing* easingOut)
{
    if (easingOut == NULL || !IsMode(mode) || !isfinite(exponent) || exponent <= 0.0)
    {
        return mtwn_errorInvalid;
    }
    mtwnEasing easing = Blank(mtwn_easingPower, mode);
    easing.parameters[0] = exponent;
    if (exponent == floor(exponent) && exponent <= (double)s_largestIntegerExponent)
    {
        easing.count = (uint32_t)exponent;
    }
    Normalize(&easing);
    *easingOut = easing;
    return mtwn_success;
}

mtwnResult mtwnMakePowerEasing(mtwnEaseMode mode, double exponent, mtwnEasing* easingOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakePowerEasingWork(mode, exponent, easingOut);
    mtwnFloatEnvLeave(env);
    return result;
}

static MTWN_NOINLINE mtwnResult mtwnMakePennerEasingWork(mtwnEasingKind kind, mtwnEaseMode mode,
                                                         mtwnEasing* easingOut)
{
    bool known = kind == mtwn_easingSine || kind == mtwn_easingExpo || kind == mtwn_easingCirc ||
                 kind == mtwn_easingBounce;
    if (easingOut == NULL || !known || !IsMode(mode))
    {
        return mtwn_errorInvalid;
    }
    mtwnEasing easing = Blank(kind, mode);
    Normalize(&easing);
    *easingOut = easing;
    return mtwn_success;
}

mtwnResult mtwnMakePennerEasing(mtwnEasingKind kind, mtwnEaseMode mode, mtwnEasing* easingOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakePennerEasingWork(kind, mode, easingOut);
    mtwnFloatEnvLeave(env);
    return result;
}

static MTWN_NOINLINE mtwnResult mtwnMakeBackEasingWork(mtwnEaseMode mode, double overshoot,
                                                       mtwnEasing* easingOut)
{
    if (easingOut == NULL || !IsMode(mode) || !isfinite(overshoot) || overshoot < 0.0)
    {
        return mtwn_errorInvalid;
    }
    mtwnEasing easing = Blank(mtwn_easingBack, mode);
    easing.parameters[0] = overshoot;
    Normalize(&easing);
    *easingOut = easing;
    return mtwn_success;
}

mtwnResult mtwnMakeBackEasing(mtwnEaseMode mode, double overshoot, mtwnEasing* easingOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeBackEasingWork(mode, overshoot, easingOut);
    mtwnFloatEnvLeave(env);
    return result;
}

static MTWN_NOINLINE mtwnResult mtwnMakeElasticEasingWork(mtwnEaseMode mode, double amplitude,
                                                          double period, mtwnEasing* easingOut)
{
    if (easingOut == NULL || !IsMode(mode) || !isfinite(amplitude) || amplitude <= 0.0 ||
        !isfinite(period) || period <= 0.0)
    {
        return mtwn_errorInvalid;
    }
    // GSAP's _configElastic: an amplitude below 1 stretches the period.
    if (amplitude < 1.0)
    {
        period /= amplitude;
        amplitude = 1.0;
    }
    double angular = 4.0 * MTWN_PIO2_HI / period;
    mtwnEasing easing = Blank(mtwn_easingElastic, mode);
    easing.parameters[0] = amplitude;
    easing.parameters[1] = angular;
    // asin(1 / a) as atan2(1, sqrt(a^2 - 1)), exactly pi / 2 at a = 1.
    easing.parameters[2] = mtwnAtan2(1.0, sqrt((amplitude - 1.0) * (amplitude + 1.0))) / angular;
    // A period whose frequency overflows or vanishes leaves no finite,
    // nonzero range between the curve's ends.
    Normalize(&easing);
    if (!isfinite(easing.parameters[s_range]) || easing.parameters[s_range] == 0.0)
    {
        return mtwn_errorInvalid;
    }
    *easingOut = easing;
    return mtwn_success;
}

mtwnResult mtwnMakeElasticEasing(mtwnEaseMode mode, double amplitude, double period,
                                 mtwnEasing* easingOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeElasticEasingWork(mode, amplitude, period, easingOut);
    mtwnFloatEnvLeave(env);
    return result;
}

mtwnResult mtwnMakeHostEasing(mtwnEaseMode mode, mtwnEaseFunction function, void* context,
                              mtwnEasing* easingOut)
{
    if (easingOut == NULL || function == NULL || !IsMode(mode))
    {
        return mtwn_errorInvalid;
    }
    mtwnEasing easing = Blank(mtwn_easingHost, mode);
    easing.function = function;
    easing.context = context;
    *easingOut = easing;
    return mtwn_success;
}

static MTWN_NOINLINE mtwnResult mtwnMakeCubicBezierWork(double x1, double y1, double x2, double y2,
                                                        mtwnEasing* easingOut)
{
    bool inside = x1 >= 0.0 && x1 <= 1.0 && x2 >= 0.0 && x2 <= 1.0;
    if (easingOut == NULL || !inside || !isfinite(y1) || !isfinite(y2))
    {
        return mtwn_errorInvalid;
    }
    mtwnEasing easing = Blank(mtwn_easingCubicBezier, mtwn_easeIn);
    easing.parameters[0] = x1;
    easing.parameters[1] = y1;
    easing.parameters[2] = x2;
    easing.parameters[3] = y2;
    *easingOut = easing;
    return mtwn_success;
}

mtwnResult mtwnMakeCubicBezier(double x1, double y1, double x2, double y2, mtwnEasing* easingOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeCubicBezierWork(x1, y1, x2, y2, easingOut);
    mtwnFloatEnvLeave(env);
    return result;
}

mtwnResult mtwnMakeSteps(uint32_t count, mtwnStepPosition position, mtwnEasing* easingOut)
{
    uint32_t fewest = position == mtwn_jumpNone ? 2u : 1u;
    if (easingOut == NULL || position > mtwn_jumpBoth || count < fewest)
    {
        return mtwn_errorInvalid;
    }
    mtwnEasing easing = Blank(mtwn_easingSteps, mtwn_easeIn);
    easing.count = count;
    easing.position = position;
    *easingOut = easing;
    return mtwn_success;
}

static MTWN_NOINLINE mtwnResult mtwnMakeCssEasingWork(mtwnCssKeyword keyword, mtwnEasing* easingOut)
{
    // The keywords' curves, CSS Easing Level 2 section 2.
    static const double s_beziers[4][4] = {
        {0.25, 0.1, 0.25, 1.0},
        {0.42, 0.0, 1.0, 1.0},
        {0.0, 0.0, 0.58, 1.0},
        {0.42, 0.0, 0.58, 1.0},
    };
    if (easingOut == NULL || keyword > mtwn_cssStepEnd)
    {
        return mtwn_errorInvalid;
    }
    if (keyword == mtwn_cssLinear)
    {
        *easingOut = mtwnLinearEasing();
        return mtwn_success;
    }
    if (keyword == mtwn_cssStepStart || keyword == mtwn_cssStepEnd)
    {
        return mtwnMakeSteps(1, keyword == mtwn_cssStepStart ? mtwn_jumpStart : mtwn_jumpEnd,
                             easingOut);
    }
    const double* c = s_beziers[keyword - mtwn_cssEase];
    return mtwnMakeCubicBezier(c[0], c[1], c[2], c[3], easingOut);
}

mtwnResult mtwnMakeCssEasing(mtwnCssKeyword keyword, mtwnEasing* easingOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeCssEasingWork(keyword, easingOut);
    mtwnFloatEnvLeave(env);
    return result;
}

MTWN_NOINLINE mtwnResult mtwnMakeLinearEasingWork(mtwnLinearPoint* points, uint32_t count,
                                                  mtwnEasing* easingOut)
{
    if (easingOut == NULL || !mtwnCanonicalizeLinearPoints(points, count))
    {
        return mtwn_errorInvalid;
    }
    mtwnEasing easing = Blank(mtwn_easingLinearPoints, mtwn_easeIn);
    easing.points = points;
    easing.count = count;
    *easingOut = easing;
    return mtwn_success;
}

mtwnResult mtwnMakeLinearEasing(mtwnLinearPoint* points, uint32_t count, mtwnEasing* easingOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeLinearEasingWork(points, count, easingOut);
    mtwnFloatEnvLeave(env);
    return result;
}

mtwnResult mtwnSetEaseMode(mtwnEasing* easing, mtwnEaseMode mode)
{
    if (easing == NULL || !IsMode(mode))
    {
        return mtwn_errorInvalid;
    }
    easing->mode = mode;
    return mtwn_success;
}

MTWN_NOINLINE bool mtwnIsEasingMonotonicWork(const mtwnEasing* easing)
{
    switch (easing->kind)
    {
    case mtwn_easingLinear:
    case mtwn_easingPower:
    case mtwn_easingSine:
    case mtwn_easingExpo:
    case mtwn_easingCirc:
    case mtwn_easingSteps:
        return true;
    case mtwn_easingCubicBezier:
    {
        // With both y in [0, 1] the y polynomial never decreases in t, as x
        // does, nor do the end tangents.
        double y1 = easing->parameters[1];
        double y2 = easing->parameters[3];
        return y1 >= 0.0 && y1 <= 1.0 && y2 >= 0.0 && y2 <= 1.0;
    }
    case mtwn_easingLinearPoints:
        for (uint32_t i = 1; i < easing->count; ++i)
        {
            if (easing->points[i].output < easing->points[i - 1].output)
            {
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}

bool mtwnIsEasingMonotonic(const mtwnEasing* easing)
{
    uint64_t env = mtwnFloatEnvEnter();
    bool result = mtwnIsEasingMonotonicWork(easing);
    mtwnFloatEnvLeave(env);
    return result;
}

MTWN_NOINLINE double mtwnEaseWork(const mtwnEasing* easing, double progress, bool before)
{
    if (easing->kind == mtwn_easingLinear)
    {
        return progress;
    }
    if (mtwnIsNormalPower(easing))
    {
        return mtwnEaseNormalPower(easing, progress);
    }
    double p = progress;
    if (!IsCss(easing->kind))
    {
        p = p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
    }
    switch (easing->mode)
    {
    case mtwn_easeIn:
        return InCurve(easing, p, before);
    case mtwn_easeOut:
        return OutCurve(easing, p);
    case mtwn_easeInOut:
        return p < 0.5 ? 0.5 * InCurve(easing, 2.0 * p, before)
                       : 0.5 + 0.5 * OutCurve(easing, 2.0 * p - 1.0);
    default:
        return p < 0.5 ? 0.5 * OutCurve(easing, 2.0 * p)
                       : 0.5 + 0.5 * InCurve(easing, 2.0 * p - 1.0, before);
    }
}

double mtwnEase(const mtwnEasing* easing, double progress, bool before)
{
    uint64_t env = mtwnFloatEnvEnter();
    double result = mtwnEaseWork(easing, progress, before);
    mtwnFloatEnvLeave(env);
    return result;
}
