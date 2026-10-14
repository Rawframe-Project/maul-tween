// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The easings against their defining formulas, written again here in
// long double where the platform has one wider than double (record
// mtwn-0020): exact ends in every mode, the modes as reflections, the
// parameters mapped to GSAP's, Penner's and tween.js's, and every maker's
// refusals.

#include "test_harness.h"

#include "maul-tween/easing.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_SIN  sinl
#define WIDE_COS  cosl
#define WIDE_POW  powl
#define WIDE_EXP2 exp2l
#define WIDE_SQRT sqrtl
#define WIDE_ASIN asinl
// The reference's own error is far below the bounds.
static const double s_slack = 0.0;
#else
typedef double Wide;
#define WIDE_SIN  sin
#define WIDE_COS  cos
#define WIDE_POW  pow
#define WIDE_EXP2 exp2
#define WIDE_SQRT sqrt
#define WIDE_ASIN asin
// A double reference adds its own error: its reflections lose digits
// forming 1 - p, which circ's vertical tangent and elastic's fast sine
// magnify to about ten ulps of 1. The tight bounds hold where long
// double is wider.
static const double s_slack = 16.0 * DBL_EPSILON;
#endif

// A literal in the reference's width.
#define W(x) ((Wide)(x##L))

static const Wide s_pi = W(3.14159265358979323846264338327950288);

typedef enum Family
{
    family_power,
    family_sine,
    family_expo,
    family_circ,
    family_back,
    family_elastic,
    family_bounce,
} Family;

typedef struct Reference
{
    Family family;
    // Exponent, overshoot, or amplitude and period.
    Wide a;
    Wide b;
} Reference;

static Wide BounceOutWide(Wide q)
{
    if (q < W(1.0) / W(2.75))
    {
        return W(7.5625) * q * q;
    }
    if (q < W(2.0) / W(2.75))
    {
        q -= W(1.5) / W(2.75);
        return W(7.5625) * q * q + W(0.75);
    }
    if (q < W(2.5) / W(2.75))
    {
        q -= W(2.25) / W(2.75);
        return W(7.5625) * q * q + W(0.9375);
    }
    q -= W(2.625) / W(2.75);
    return W(7.5625) * q * q + W(0.984375);
}

// The family's defining curve: the in curve, or for elastic and bounce
// the out curve, which they are defined by.
static Wide RawWide(const Reference* r, Wide x)
{
    switch (r->family)
    {
    case family_power:
        return WIDE_POW(x, r->a);
    case family_sine:
        return W(1.0) - WIDE_COS(x * s_pi / W(2.0));
    case family_expo:
        return WIDE_EXP2(W(10.0) * (x - W(1.0)));
    case family_circ:
        return W(1.0) - WIDE_SQRT(W(1.0) - x * x);
    case family_back:
        return x * x * ((r->a + W(1.0)) * x - r->a);
    case family_elastic:
    {
        Wide amplitude = r->a;
        Wide period = r->b;
        if (amplitude < W(1.0))
        {
            period /= amplitude;
            amplitude = W(1.0);
        }
        Wide shift = period / (W(2.0) * s_pi) * WIDE_ASIN(W(1.0) / amplitude);
        return amplitude * WIDE_EXP2(-W(10.0) * x) *
                   WIDE_SIN((x - shift) * W(2.0) * s_pi / period) +
               W(1.0);
    }
    default:
        return BounceOutWide(x);
    }
}

static Wide NativeWide(const Reference* r, Wide x)
{
    Wide start = RawWide(r, W(0.0));
    return (RawWide(r, x) - start) / (RawWide(r, W(1.0)) - start);
}

static Wide InWide(const Reference* r, Wide x)
{
    bool nativeOut = r->family == family_elastic || r->family == family_bounce;
    return nativeOut ? W(1.0) - NativeWide(r, W(1.0) - x) : NativeWide(r, x);
}

static Wide EaseWide(const Reference* r, mtwnEaseMode mode, Wide p)
{
    switch (mode)
    {
    case mtwn_easeIn:
        return InWide(r, p);
    case mtwn_easeOut:
        return W(1.0) - InWide(r, W(1.0) - p);
    case mtwn_easeInOut:
        return p < W(0.5) ? InWide(r, W(2.0) * p) / W(2.0)
                          : W(1.0) - InWide(r, W(2.0) - W(2.0) * p) / W(2.0);
    default:
        return p < W(0.5) ? (W(1.0) - InWide(r, W(1.0) - W(2.0) * p)) / W(2.0)
                          : W(0.5) + InWide(r, W(2.0) * p - W(1.0)) / W(2.0);
    }
}

typedef struct Case
{
    const char* name;
    Reference reference;
    // The largest error allowed against the reference, in units of 1.
    double bound;
} Case;

static mtwnResult Make(const Case* c, mtwnEaseMode mode, mtwnEasing* easingOut)
{
    const Reference* r = &c->reference;
    switch (r->family)
    {
    case family_power:
        return mtwnMakePowerEasing(mode, (double)r->a, easingOut);
    case family_sine:
        return mtwnMakePennerEasing(mtwn_easingSine, mode, easingOut);
    case family_expo:
        return mtwnMakePennerEasing(mtwn_easingExpo, mode, easingOut);
    case family_circ:
        return mtwnMakePennerEasing(mtwn_easingCirc, mode, easingOut);
    case family_back:
        return mtwnMakeBackEasing(mode, (double)r->a, easingOut);
    case family_elastic:
        return mtwnMakeElasticEasing(mode, (double)r->a, (double)r->b, easingOut);
    default:
        return mtwnMakePennerEasing(mtwn_easingBounce, mode, easingOut);
    }
}

static const Case s_cases[] = {
    {"quad", {family_power, W(2.0), W(0.0)}, 2.5e-16},
    {"cubic", {family_power, W(3.0), W(0.0)}, 3.5e-16},
    {"quint", {family_power, W(5.0), W(0.0)}, 5e-16},
    {"power 2.5", {family_power, W(2.5), W(0.0)}, 3e-16},
    {"power 0.3", {family_power, W(0.3), W(0.0)}, 2e-16},
    {"power 12", {family_power, W(12.0), W(0.0)}, 8e-16},
    {"sine", {family_sine, W(0.0), W(0.0)}, 4.5e-16},
    {"expo", {family_expo, W(0.0), W(0.0)}, 5.5e-16},
    {"circ", {family_circ, W(0.0), W(0.0)}, 2e-16},
    {"back", {family_back, W(1.70158), W(0.0)}, 6.5e-16},
    {"back 1.525 s", {family_back, W(1.70158) * W(1.525), W(0.0)}, 6.5e-16},
    {"elastic", {family_elastic, W(1.0), W(0.3)}, 4e-16},
    {"elastic 0.45", {family_elastic, W(1.0), W(0.45)}, 4e-16},
    {"elastic 1.5", {family_elastic, W(1.5), W(0.4)}, 5.5e-16},
    {"elastic 0.5", {family_elastic, W(0.5), W(0.3)}, 3.5e-16},
    {"bounce", {family_bounce, W(0.0), W(0.0)}, 4.5e-16},
};

static const mtwnEaseMode s_modes[] = {mtwn_easeIn, mtwn_easeOut, mtwn_easeInOut, mtwn_easeOutIn};

static void TestEndsAreExact(void)
{
    int wrong = 0;
    for (size_t c = 0; c < sizeof s_cases / sizeof s_cases[0]; ++c)
    {
        for (int m = 0; m < 4; ++m)
        {
            mtwnEasing easing;
            if (Make(&s_cases[c], s_modes[m], &easing) != mtwn_success)
            {
                ++wrong;
                continue;
            }
            wrong += mtwnEase(&easing, 0.0, false) != 0.0;
            wrong += mtwnEase(&easing, 1.0, false) != 1.0;
            if (s_modes[m] >= mtwn_easeInOut)
            {
                wrong += mtwnEase(&easing, 0.5, false) != 0.5;
            }
            // Progress outside 0 to 1 is clamped.
            wrong += mtwnEase(&easing, -0.25, false) != 0.0;
            wrong += mtwnEase(&easing, 7.0, true) != 1.0;
        }
    }
    CHECK(wrong == 0, "every Penner easing is exactly 0, 1/2 and 1 at its ends and middle");
}

static void TestEndsAreExactForEveryParameter(void)
{
    // The normalization divides by the curve's range: a multiplication by
    // its reciprocal would miss 1 for about one range in six.
    int wrong = 0;
    for (int i = 1; i <= 400; ++i)
    {
        double parameter = (double)i / 37.0;
        mtwnEasing easings[3];
        mtwnResult made = mtwnMakePowerEasing(mtwn_easeIn, parameter, &easings[0]);
        made |= mtwnMakeBackEasing(mtwn_easeIn, parameter, &easings[1]);
        made |= mtwnMakeElasticEasing(mtwn_easeIn, 1.0 + parameter, 0.05 + parameter / 20.0,
                                      &easings[2]);
        wrong += made != mtwn_success;
        for (int e = 0; e < 3; ++e)
        {
            for (int m = 0; m < 4; ++m)
            {
                wrong += mtwnSetEaseMode(&easings[e], s_modes[m]) != mtwn_success;
                wrong += mtwnEase(&easings[e], 0.0, false) != 0.0;
                wrong += mtwnEase(&easings[e], 1.0, false) != 1.0;
            }
        }
    }
    CHECK(wrong == 0, "power, back and elastic land exactly for every parameter");
}

static void TestAgainstTheFormulas(void)
{
    for (size_t c = 0; c < sizeof s_cases / sizeof s_cases[0]; ++c)
    {
        double worst = 0.0;
        for (int m = 0; m < 4; ++m)
        {
            mtwnEasing easing;
            if (Make(&s_cases[c], s_modes[m], &easing) != mtwn_success)
            {
                worst = HUGE_VAL;
                continue;
            }
            for (int i = 0; i <= 20000; ++i)
            {
                double p = (double)i / 20000.0;
                double error = fabs(mtwnEase(&easing, p, false) -
                                    (double)EaseWide(&s_cases[c].reference, s_modes[m], (Wide)p));
                worst = error > worst ? error : worst;
            }
        }
        if (!(worst <= s_cases[c].bound + s_slack))
        {
            printf("  %s: %.3g\n", s_cases[c].name, worst);
        }
        CHECK(worst <= s_cases[c].bound + s_slack, "an easing within its bound of its formula");
    }
}

static void TestMonotonicEasingsNeverDecrease(void)
{
    int wrong = 0;
    for (size_t c = 0; c < sizeof s_cases / sizeof s_cases[0]; ++c)
    {
        for (int m = 0; m < 4; ++m)
        {
            mtwnEasing easing;
            if (Make(&s_cases[c], s_modes[m], &easing) != mtwn_success)
            {
                continue;
            }
            Family family = s_cases[c].reference.family;
            bool expected =
                family != family_back && family != family_elastic && family != family_bounce;
            wrong += mtwnIsEasingMonotonic(&easing) != expected;
            if (!expected)
            {
                continue;
            }
            double previous = 0.0;
            for (int i = 0; i <= 100000; ++i)
            {
                double value = mtwnEase(&easing, (double)i / 100000.0, false);
                wrong += value < previous;
                previous = value;
            }
        }
    }
    CHECK(wrong == 0, "the easings called monotonic never decrease");
}

static void TestRivalMappings(void)
{
    // GSAP's back.in(s) is p^2 ((s + 1) p - s) unnormalized; ours lands on
    // 1 and differs from it by rounding only.
    mtwnEasing back;
    CHECK(mtwnMakeBackEasing(mtwn_easeIn, 1.70158, &back) == mtwn_success, "back");
    double worst = 0.0;
    for (int i = 0; i <= 1000; ++i)
    {
        double p = (double)i / 1000.0;
        double gsap = p * p * (2.70158 * p - 1.70158);
        double error = fabs(mtwnEase(&back, p, false) - gsap);
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 4e-16, "back.in matches GSAP's");
    // GSAP's elastic.out(1, 0.3) is 2^(-10 p) sin((p - c) 2 pi / 0.3) + 1,
    // which misses 1 at p = 1 by under 2^-10; ours is normalized.
    mtwnEasing elastic;
    CHECK(mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &elastic) == mtwn_success, "elastic");
    worst = 0.0;
    for (int i = 0; i <= 1000; ++i)
    {
        double p = (double)i / 1000.0;
        double c = 0.3 / (2.0 * 3.141592653589793) * asin(1.0);
        double gsap = pow(2.0, -10.0 * p) * sin((p - c) * 2.0 * 3.141592653589793 / 0.3) + 1.0;
        double error = fabs(mtwnEase(&elastic, p, false) - gsap);
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0x1.0p-10, "elastic.out within 2^-10 of GSAP's");
    // Penner's expo in is 2^(10 (p - 1)), 2^-10 at 0.
    mtwnEasing expo;
    CHECK(mtwnMakePennerEasing(mtwn_easingExpo, mtwn_easeIn, &expo) == mtwn_success, "expo");
    worst = 0.0;
    for (int i = 1; i <= 1000; ++i)
    {
        double p = (double)i / 1000.0;
        double error = fabs(mtwnEase(&expo, p, false) - pow(2.0, 10.0 * (p - 1.0)));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0x1.0p-10, "expo within 2^-10 of Penner's");
    // Penner's bounce out at 1/2 is 7.5625 (1/2 - 1.5/2.75)^2 + 0.75.
    mtwnEasing bounce;
    CHECK(mtwnMakePennerEasing(mtwn_easingBounce, mtwn_easeOut, &bounce) == mtwn_success, "bounce");
    double penner = 7.5625 * (0.5 - 1.5 / 2.75) * (0.5 - 1.5 / 2.75) + 0.75;
    CHECK(fabs(mtwnEase(&bounce, 0.5, false) - penner) <= 2e-16, "bounce.out(1/2) is Penner's");
}

static void TestModesAreReflections(void)
{
    mtwnEasing in;
    mtwnEasing out;
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 3.0, &in) == mtwn_success, "cubic in");
    out = in;
    CHECK(mtwnSetEaseMode(&out, mtwn_easeOut) == mtwn_success, "set the mode");
    int wrong = 0;
    for (int i = 0; i <= 1024; ++i)
    {
        // Binary fractions, so 1 - p is exact and the identity holds to the
        // bit.
        double p = (double)i / 1024.0;
        wrong += mtwnEase(&out, p, false) != 1.0 - mtwnEase(&in, 1.0 - p, false);
    }
    CHECK(wrong == 0, "out is 1 - in(1 - p)");
    CHECK(mtwnSetEaseMode(&out, 4) == mtwn_errorInvalid && out.mode == mtwn_easeOut,
          "an unknown mode is refused and changes nothing");
    mtwnEasing linear = mtwnLinearEasing();
    CHECK(mtwnEase(&linear, 0.3, false) == 0.3 && mtwnEase(&linear, -2.0, true) == -2.0,
          "linear is the identity, unclamped");
    CHECK(mtwnSetEaseMode(&linear, mtwn_easeInOut) == mtwn_success &&
              mtwnEase(&linear, 0.3, false) == 0.3,
          "linear in every mode");
    CHECK(mtwnIsEasingMonotonic(&linear), "linear is monotonic");
}

typedef struct HostCalls
{
    int count;
    double lowest;
    double highest;
} HostCalls;

static double Smoothstep(void* context, double progress)
{
    HostCalls* calls = (HostCalls*)context;
    calls->count += 1;
    calls->lowest = progress < calls->lowest ? progress : calls->lowest;
    calls->highest = progress > calls->highest ? progress : calls->highest;
    return progress * progress * (3.0 - 2.0 * progress);
}

static void TestHostEasing(void)
{
    HostCalls calls = {0, 1.0, 0.0};
    mtwnEasing easing;
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, Smoothstep, &calls, &easing) == mtwn_success, "host");
    CHECK(mtwnEase(&easing, 0.5, false) == 0.5, "the host's value");
    CHECK(mtwnEase(&easing, -3.0, false) == 0.0 && mtwnEase(&easing, 3.0, false) == 1.0,
          "clamped before the host sees it");
    CHECK(calls.count == 3 && calls.lowest == 0.0 && calls.highest == 1.0,
          "called once per sample with progress in 0 to 1");
    CHECK(mtwnSetEaseMode(&easing, mtwn_easeOut) == mtwn_success &&
              mtwnEase(&easing, 0.25, false) == 1.0 - Smoothstep(&calls, 0.75),
          "the out mode reflects the host's curve");
    CHECK(!mtwnIsEasingMonotonic(&easing), "a host function is never called monotonic");
}

static void TestRefusals(void)
{
    mtwnEasing easing = mtwnLinearEasing();
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 0.0, &easing) == mtwn_errorInvalid, "exponent 0");
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, -1.0, &easing) == mtwn_errorInvalid, "negative");
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, HUGE_VAL, &easing) == mtwn_errorInvalid, "infinite");
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, (double)NAN, &easing) == mtwn_errorInvalid,
          "NaN exponent");
    CHECK(mtwnMakePowerEasing(4, 2.0, &easing) == mtwn_errorInvalid, "unknown mode");
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, NULL) == mtwn_errorInvalid, "null out");
    CHECK(mtwnMakePennerEasing(mtwn_easingBack, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "back has parameters");
    CHECK(mtwnMakePennerEasing(mtwn_easingPower, mtwn_easeIn, &easing) == mtwn_errorInvalid,
          "power has parameters");
    CHECK(mtwnMakePennerEasing(200, mtwn_easeIn, &easing) == mtwn_errorInvalid, "unknown kind");
    CHECK(mtwnMakeBackEasing(mtwn_easeIn, -0.1, &easing) == mtwn_errorInvalid, "overshoot");
    CHECK(mtwnMakeBackEasing(mtwn_easeIn, (double)NAN, &easing) == mtwn_errorInvalid,
          "NaN overshoot");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 0.0, 0.3, &easing) == mtwn_errorInvalid, "amplitude");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1.0, 0.0, &easing) == mtwn_errorInvalid, "period");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1.0, HUGE_VAL, &easing) == mtwn_errorInvalid,
          "infinite period");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1e-300, 1e10, &easing) == mtwn_errorInvalid,
          "a period whose stretch overflows");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1.0, 1e-310, &easing) == mtwn_errorInvalid,
          "a period whose frequency overflows");
    // At amplitude 1024 the sine at the out curve's end rounds to -1 for
    // this period, putting both ends at 0.
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1024.0, 0x1.55433a91bf526p+0, &easing) ==
              mtwn_errorInvalid,
          "an elastic whose ends coincide");
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, NULL, NULL, &easing) == mtwn_errorInvalid,
          "null function");
    CHECK(easing.kind == mtwn_easingLinear, "a refusal leaves the out-parameter as it was");
    CHECK(mtwnSetEaseMode(NULL, mtwn_easeIn) == mtwn_errorInvalid, "null easing");
    CHECK(mtwnMakeBackEasing(mtwn_easeIn, 0.0, &easing) == mtwn_success &&
              mtwnEase(&easing, 0.5, false) == 0.125,
          "back with no overshoot is the cubic");
}

// An elastic easing's amplitude and period at 0 are refused (found by
// mutants).
static void TestElasticZero(void)
{
    mtwnEasing easing;
    CHECK(mtwnMakeElasticEasing(mtwn_easeOut, 0.0, 0.3, &easing) == mtwn_errorInvalid &&
              mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.0, &easing) == mtwn_errorInvalid &&
              mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &easing) == mtwn_success,
          "amplitude or period 0 refused");
}

int main(void)
{
    TestElasticZero();
    TestEndsAreExact();
    TestEndsAreExactForEveryParameter();
    TestAgainstTheFormulas();
    TestMonotonicEasingsNeverDecrease();
    TestRivalMappings();
    TestModesAreReflections();
    TestHostEasing();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
