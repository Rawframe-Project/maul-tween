// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The easings against their defining formulas, written again here in
// long double where the platform has one wider than double:
// exact ends in every mode, the modes as reflections, the
// parameters mapped to GSAP's, Penner's and tween.js's, and every maker's
// refusals.

#include "test_harness.h"
#include "test_values.h"

#include "maul-tween/easing.h"
#include "maul-tween/sample.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

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
// magnify to 10.4 ulps of 1 on macOS, Windows and the web. The tight
// bounds hold where long double is wider.
static const double s_slack = 12.0 * DBL_EPSILON;
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

// How much finer than the fixed cases' the sweeps across progress are:
// ten times in the dense test (`test_easing dense`).
static int s_finer = 1;

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
            wrong += EaseAt(&easing, 0.0, false) != 0.0;
            wrong += EaseAt(&easing, 1.0, false) != 1.0;
            if (s_modes[m] >= mtwn_easeInOut)
            {
                wrong += EaseAt(&easing, 0.5, false) != 0.5;
            }
            // Progress outside 0 to 1 is clamped.
            wrong += EaseAt(&easing, -0.25, false) != 0.0;
            wrong += EaseAt(&easing, 7.0, true) != 1.0;
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
                wrong += EaseAt(&easings[e], 0.0, false) != 0.0;
                wrong += EaseAt(&easings[e], 1.0, false) != 1.0;
            }
        }
    }
    CHECK(wrong == 0, "power, back and elastic land exactly for every parameter");
}

// Penner's bounce changes parabola at these points of its out curve, so
// a piece taken a little early or late shows only near them.
static const double s_bounceBreaks[] = {1.0 / 2.75, 2.0 / 2.75, 2.5 / 2.75};

// An easing's distance from its formula at a progress, clamped to [0, 1].
static double FormulaError(const Case* c, mtwnEaseMode mode, const mtwnEasing* easing, double p)
{
    p = p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
    return fabs(EaseAt(easing, p, false) - (double)EaseWide(&c->reference, mode, (Wide)p));
}

static void TestAgainstTheFormulas(void)
{
    for (size_t c = 0; c < sizeof s_cases / sizeof s_cases[0]; ++c)
    {
        bool bounce = s_cases[c].reference.family == family_bounce;
        double worst = 0.0;
        for (int m = 0; m < 4; ++m)
        {
            mtwnEasing easing;
            if (Make(&s_cases[c], s_modes[m], &easing) != mtwn_success)
            {
                worst = HUGE_VAL;
                continue;
            }
            for (int i = 0; i <= 2000 * s_finer; ++i)
            {
                double error =
                    FormulaError(&s_cases[c], s_modes[m], &easing, (double)i / (2000.0 * s_finer));
                worst = error > worst ? error : worst;
            }
            // Every 1e-5 within 1e-3 of a bounce's breaks, wherever a mode
            // puts them: at q and 1 - q, and at their halves in either half.
            for (size_t b = 0; bounce && b < sizeof s_bounceBreaks / sizeof s_bounceBreaks[0]; ++b)
            {
                for (int j = -100; j <= 100; ++j)
                {
                    double q = s_bounceBreaks[b] + (double)j * 1e-5;
                    const double at[6] = {
                        q, 1.0 - q, 0.5 * q, 0.5 * (1.0 - q), 0.5 + 0.5 * q, 0.5 + 0.5 * (1.0 - q)};
                    for (int k = 0; k < 6; ++k)
                    {
                        double error = FormulaError(&s_cases[c], s_modes[m], &easing, at[k]);
                        worst = error > worst ? error : worst;
                    }
                }
            }
        }
        if (!(worst <= s_cases[c].bound + s_slack))
        {
            printf("  %s: %.3g\n", s_cases[c].name, worst);
        }
        CHECK(worst <= s_cases[c].bound + s_slack, "an easing within its bound of its formula");
    }
}

// Where rounding could step a curve down: the doubles next to 0, 1/4, 1/2
// (where the modes and circ's forms meet), 3/4 and 1, and down to the
// subnormals at both ends of each half: where circ's products of two
// factors rounded apart would step an ulp down, at 2^-53 and next to 1/4.
static int StepsDown(const mtwnEasing* easing)
{
    int down = 0;
    static const double s_centers[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
    for (int c = 0; c < 5; ++c)
    {
        double p = s_centers[c];
        for (int i = 0; i < 2000 * s_finer; ++i)
        {
            p = nextafter(p, -1.0);
        }
        double previous = EaseAt(easing, p < 0.0 ? 0.0 : p, false);
        for (int i = 0; i < 4000 * s_finer; ++i)
        {
            p = nextafter(p, 2.0);
            if (p >= 0.0 && p <= 1.0)
            {
                double value = EaseAt(easing, p, false);
                down += value < previous;
                previous = value;
            }
        }
    }
    static const double s_ends[4][2] = {{0.0, 1.0}, {0.5, -1.0}, {0.5, 1.0}, {1.0, -1.0}};
    for (int k = 0; k < 4; ++k)
    {
        double previous = s_ends[k][1] > 0.0 ? -1.0 : 2.0;
        for (int e = -1074; e <= -2; ++e)
        {
            for (int j = 0; j < 16; ++j)
            {
                // Away from the end as e and j grow.
                double p = s_ends[k][0] + s_ends[k][1] * ldexp(1.0 + j / 16.0, e);
                double value = EaseAt(easing, p, false);
                down += s_ends[k][1] > 0.0 ? value < previous : value > previous;
                previous = value;
            }
        }
    }
    return down;
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
            wrong += IsMonotonicEasing(&easing) != expected;
            if (!expected)
            {
                continue;
            }
            double previous = 0.0;
            for (int i = 0; i <= 10000 * s_finer; ++i)
            {
                double value = EaseAt(&easing, (double)i / (10000.0 * s_finer), false);
                wrong += value < previous;
                previous = value;
            }
            wrong += StepsDown(&easing);
        }
    }
    CHECK(wrong == 0, "the easings called monotonic never decrease");
}

// Whether an easing steps down where in-out and out-in join their halves:
// from the doubles just below 1/2 to 1/2 itself. A cubic bezier through
// mtwnSampleEasing may fall an ulp or so short, as its header says (a
// timeline samples one that never does), and is let fall 2^-40.
static bool DownAtTheSeam(const mtwnEasing* easing)
{
    double slack = easing->kind == mtwn_easingCubicBezier ? 0x1p-40 : 0.0;
    double p = 0.5;
    double at = EaseAt(easing, p, false);
    for (int i = 0; i < 64; ++i)
    {
        p = nextafter(p, 0.0);
        if (EaseAt(easing, p, false) > at + slack)
        {
            return true;
        }
    }
    return false;
}

// In-out and out-in join a curve's halves at 1/2: from below f(1) / 2 to
// 1 - f(1) / 2, and from below (1 - f(0)) / 2 to (1 + f(0)) / 2. Every
// kind the check can approve, and the ones it never does, in every mode:
// the check's verdict is true only where the samples on both sides of the
// seam do not step down, and for linear() and a rising rough easing, but
// for a jump at the end that decides, it is false exactly where they do:
// ending above 1 in in-out or starting below 0 in out-in.
static void TestHalvesMeetGoingUp(void)
{
    enum
    {
        s_kinds = 24
    };
    mtwnEasing easings[s_kinds];
    bool exact[s_kinds];
    int made = 0;
    int n = 0;
    made += mtwnMakePowerEasing(mtwn_easeIn, 3.0, &easings[n++]) == mtwn_success;
    made += mtwnMakePowerEasing(mtwn_easeIn, 0.5, &easings[n++]) == mtwn_success;
    made += mtwnMakePennerEasing(mtwn_easingSine, mtwn_easeIn, &easings[n++]) == mtwn_success;
    made += mtwnMakePennerEasing(mtwn_easingExpo, mtwn_easeIn, &easings[n++]) == mtwn_success;
    made += mtwnMakePennerEasing(mtwn_easingCirc, mtwn_easeIn, &easings[n++]) == mtwn_success;
    made += mtwnMakePennerEasing(mtwn_easingBounce, mtwn_easeIn, &easings[n++]) == mtwn_success;
    made += mtwnMakeBackEasing(mtwn_easeIn, 1.70158, &easings[n++]) == mtwn_success;
    made += mtwnMakeElasticEasing(mtwn_easeIn, 1.0, 0.3, &easings[n++]) == mtwn_success;
    for (int position = 0; position < 4; ++position)
    {
        made += mtwnMakeStepsEasing(3, (mtwnStepPosition)position, &easings[n++]) == mtwn_success;
    }
    made += mtwnMakeCssEasing(mtwn_cssEase, &easings[n++]) == mtwn_success;
    made += mtwnMakeCubicBezierEasing(0.5, -0.5, 0.5, 1.5, &easings[n++]) == mtwn_success;
    easings[n++] = mtwnMakeLinearEasing();
    int fixed = n;
    static mtwnLinearPoint s_points[7][3] = {
        {{0.0, 0.0}, {0.5, 0.5}, {1.0, 2.0}},   {{0.0, -1.0}, {0.5, 0.5}, {1.0, 1.0}},
        {{0.0, 0.25}, {0.5, 0.5}, {1.0, 0.75}}, {{0.0, 0.0}, {0.5, 0.25}, {1.0, 0.5}},
        {{0.0, -0.5}, {0.5, 0.0}, {1.0, 1.5}},  {{0.0, 0.0}, {1.0, 0.5}, {1.0, 1.5}},
        {{0.0, -0.5}, {0.0, 0.5}, {1.0, 1.0}},
    };
    for (int k = 0; k < 7; ++k)
    {
        made += mtwnMakeLinearPointsEasing(s_points[k], 3, &easings[n++]) == mtwn_success;
    }
    mtwnRoughDef def = {0};
    def.strength = 0.3;
    def.seed = 17;
    def.flags = mtwn_roughRising;
    static mtwnLinearPoint s_rough[9];
    made += mtwnMakeRoughEasing(&def, s_rough, 9, &easings[n++]) == mtwn_success;
    CHECK(made == n - 1 && n <= s_kinds, "every kind made");
    // The check is strict for a jump at the end that decides: linear(0,
    // 0.5 100%, 1.5 100%) in in-out meets at 1/4 from 1/4, but ends at 1.5.
    for (int i = 0; i < n; ++i)
    {
        exact[i] = i >= fixed && i != fixed + 5;
    }
    static const mtwnEaseMode s_seamModes[4] = {mtwn_easeIn, mtwn_easeOut, mtwn_easeInOut,
                                                mtwn_easeOutIn};
    int wrong = 0;
    for (int i = 0; i < n; ++i)
    {
        for (int m = 0; m < 4; ++m)
        {
            mtwnEasing easing = easings[i];
            if (mtwnSetEaseMode(&easing, s_seamModes[m]) != mtwn_success)
            {
                ++wrong;
                continue;
            }
            bool down = DownAtTheSeam(&easing);
            bool monotonic = IsMonotonicEasing(&easing);
            wrong += monotonic && down;
            wrong += exact[i] && monotonic == down;
        }
    }
    CHECK(wrong == 0, "no easing called monotonic steps down where its halves meet");
    mtwnEasing up = easings[fixed];
    CHECK(mtwnSetEaseMode(&up, mtwn_easeInOut) == mtwn_success && !IsMonotonicEasing(&up) &&
              mtwnSetEaseMode(&up, mtwn_easeOutIn) == mtwn_success && IsMonotonicEasing(&up),
          "linear(0, 0.5, 2): in-out not monotonic, out-in monotonic");
    mtwnEasing below = easings[fixed + 1];
    CHECK(mtwnSetEaseMode(&below, mtwn_easeOutIn) == mtwn_success && !IsMonotonicEasing(&below) &&
              mtwnSetEaseMode(&below, mtwn_easeInOut) == mtwn_success && IsMonotonicEasing(&below),
          "linear(-1, 0.5, 1): out-in not monotonic, in-out monotonic");
}

static void TestRivalMappings(void)
{
    // GSAP's back.in(s) is p^2 ((s + 1) p - s) unnormalized; ours lands on
    // 1 and differs from it by rounding only, mostly GSAP's: its form
    // misses the exact curve by up to 4.6e-16 here, ours, p^2 (p + s (p -
    // 1)), by 1.1e-16.
    mtwnEasing back;
    CHECK(mtwnMakeBackEasing(mtwn_easeIn, 1.70158, &back) == mtwn_success, "back");
    double worst = 0.0;
    for (int i = 0; i <= 1000; ++i)
    {
        double p = (double)i / 1000.0;
        double gsap = p * p * (2.70158 * p - 1.70158);
        double error = fabs(EaseAt(&back, p, false) - gsap);
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0x1p-51, "back.in matches GSAP's");
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
        double error = fabs(EaseAt(&elastic, p, false) - gsap);
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
        double error = fabs(EaseAt(&expo, p, false) - pow(2.0, 10.0 * (p - 1.0)));
        worst = error > worst ? error : worst;
    }
    CHECK(worst <= 0x1.0p-10, "expo within 2^-10 of Penner's");
    // Penner's bounce out at 1/2 is 7.5625 (1/2 - 1.5/2.75)^2 + 0.75.
    mtwnEasing bounce;
    CHECK(mtwnMakePennerEasing(mtwn_easingBounce, mtwn_easeOut, &bounce) == mtwn_success, "bounce");
    double penner = 7.5625 * (0.5 - 1.5 / 2.75) * (0.5 - 1.5 / 2.75) + 0.75;
    CHECK(fabs(EaseAt(&bounce, 0.5, false) - penner) <= 2e-16, "bounce.out(1/2) is Penner's");
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
        wrong += EaseAt(&out, p, false) != 1.0 - EaseAt(&in, 1.0 - p, false);
    }
    CHECK(wrong == 0, "out is 1 - in(1 - p)");
    CHECK(mtwnSetEaseMode(&out, 4) == mtwn_errorInvalid && out.mode == mtwn_easeOut,
          "an unknown mode is refused and changes nothing");
    mtwnEasing linear = mtwnMakeLinearEasing();
    CHECK(EaseAt(&linear, 0.3, false) == 0.3 && EaseAt(&linear, -2.0, true) == -2.0,
          "linear is the identity, unclamped");
    CHECK(mtwnSetEaseMode(&linear, mtwn_easeInOut) == mtwn_success &&
              EaseAt(&linear, 0.3, false) == 0.3,
          "linear in every mode");
    CHECK(IsMonotonicEasing(&linear), "linear is monotonic");
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
    CHECK(EaseAt(&easing, 0.5, false) == 0.5, "the host's value");
    CHECK(EaseAt(&easing, -3.0, false) == 0.0 && EaseAt(&easing, 3.0, false) == 1.0,
          "clamped before the host sees it");
    CHECK(calls.count == 3 && calls.lowest == 0.0 && calls.highest == 1.0,
          "called once per sample with progress in 0 to 1");
    CHECK(mtwnSetEaseMode(&easing, mtwn_easeOut) == mtwn_success &&
              EaseAt(&easing, 0.25, false) == 1.0 - Smoothstep(&calls, 0.75),
          "the out mode reflects the host's curve");
    CHECK(!IsMonotonicEasing(&easing), "a host function is never called monotonic");
}

static void TestRefusals(void)
{
    mtwnEasing easing = mtwnMakeLinearEasing();
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
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, -0.5, 0.3, &easing) == mtwn_errorInvalid,
          "a negative amplitude");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1.0, 0.0, &easing) == mtwn_errorInvalid, "period");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1.0, -0.3, &easing) == mtwn_errorInvalid,
          "a negative period");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1.0, HUGE_VAL, &easing) == mtwn_errorInvalid,
          "infinite period");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1e-300, 1e10, &easing) == mtwn_errorInvalid,
          "a period whose stretch overflows");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1.0, 1e-310, &easing) == mtwn_errorInvalid,
          "a period whose frequency overflows");
    // At amplitude 1024 the sine at the out curve's end rounds to -1 for
    // this period, putting both ends at 0: past the largest amplitude.
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 1024.0, 0x1.55433a91bf526p+0, &easing) ==
              mtwn_errorInvalid,
          "an elastic whose ends coincide");
    mtwnEasing edge;
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, 512.0, 0x1.0p-10, &edge) == mtwn_success &&
              mtwnMakeElasticEasing(mtwn_easeIn, 0.5, 0x1.0p-11, &edge) == mtwn_success,
          "the largest amplitude and the shortest period, stretched or not, taken");
    CHECK(mtwnMakeElasticEasing(mtwn_easeIn, nextafter(512.0, HUGE_VAL), 0.3, &edge) ==
                  mtwn_errorInvalid &&
              mtwnMakeElasticEasing(mtwn_easeIn, 1.0, nextafter(0x1.0p-10, 0.0), &edge) ==
                  mtwn_errorInvalid &&
              mtwnMakeElasticEasing(mtwn_easeIn, 0.5, nextafter(0x1.0p-11, 0.0), &edge) ==
                  mtwn_errorInvalid,
          "past them refused");
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, NULL, NULL, &easing) == mtwn_errorInvalid,
          "null function");
    CHECK(easing.kind == mtwn_easingLinear, "a refusal leaves the out-parameter as it was");
    CHECK(mtwnSetEaseMode(NULL, mtwn_easeIn) == mtwn_errorInvalid, "null easing");
    CHECK(mtwnMakeBackEasing(mtwn_easeIn, 0.0, &easing) == mtwn_success &&
              EaseAt(&easing, 0.5, false) == 0.125,
          "back with no overshoot is the cubic");
    // An overshoot from 2^53, where s + 1 rounds to s: still on 0 and 1,
    // and finite between.
    bool lands = true;
    for (int mode = mtwn_easeIn; mode <= mtwn_easeOutIn; ++mode)
    {
        CHECK(mtwnMakeBackEasing((mtwnEaseMode)mode, 1e300, &easing) == mtwn_success, "huge");
        lands = lands && EaseAt(&easing, 0.0, false) == 0.0 && EaseAt(&easing, 1.0, false) == 1.0 &&
                isfinite(EaseAt(&easing, 0.3, false)) && isfinite(EaseAt(&easing, 2.5, false)) &&
                isfinite(EaseAt(&easing, -1.5, false));
    }
    CHECK(lands, "back with an overshoot of 1e300 lands");
}

// An elastic easing's amplitude and period at 0 are refused.
static void TestElasticZero(void)
{
    mtwnEasing easing;
    CHECK(mtwnMakeElasticEasing(mtwn_easeOut, 0.0, 0.3, &easing) == mtwn_errorInvalid &&
              mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.0, &easing) == mtwn_errorInvalid &&
              mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &easing) == mtwn_success,
          "amplitude or period 0 refused");
}

// Spreading a run of missing inputs leaves the run's end as given.
static void TestLinearRunEnd(void)
{
    mtwnEasing easing;
    mtwnLinearPoint points[4] = {{0.0, 0.0}, {0.03, 0.2}, {(double)NAN, 0.5}, {0.3, 0.8}};
    CHECK(mtwnMakeLinearPointsEasing(points, 4, &easing) == mtwn_success && points[3].input == 0.3,
          "the run's end input kept");
}

static bool SameBits(double a, double b)
{
    uint64_t x = 0;
    uint64_t y = 0;
    memcpy(&x, &a, sizeof x);
    memcpy(&y, &b, sizeof y);
    return x == y;
}

// mtwnSampleCubicBezierEasing against mtwnSampleEasing, bit for bit: CSS's keywords and
// curves with control points at the corners, overshooting and flat, in
// every mode, at progress through 0 to 1 and past both ends; the linear
// easing as its progress, and every other kind refused.
static void TestCubicBezierSampler(void)
{
    static const double s_curves[][4] = {
        {0.25, 0.1, 0.25, 1.0}, {0.42, 0.0, 1.0, 1.0},   {0.0, 0.0, 0.58, 1.0},
        {0.0, 0.0, 1.0, 1.0},   {0.68, -0.6, 0.32, 1.6}, {0.0, 1.0, 1.0, 0.0},
        {1.0, 0.0, 0.0, 1.0},   {0.5, 0.5, 0.5, 0.5},    {0.0, -2.0, 1.0, 3.0},
        {1.0, 1.0, 1.0, 1.0},   {0.0, 0.0, 0.0, 0.0},    {0.3, 1e-9, 0.7, 1.0 - 1e-9},
    };
    static const double s_edges[] = {
        -1e300,        -2.0,          -0.5, -0x1p-1074,    -0.0, 0.0, 0x1p-1074, 0.5 - 0x1p-54, 0.5,
        0.5 + 0x1p-53, 1.0 - 0x1p-53, 1.0,  1.0 + 0x1p-52, 1.5,  3.0, 1e300};
    uint32_t compared = 0;
    uint32_t same = 0;
    for (size_t c = 0; c < sizeof s_curves / sizeof s_curves[0]; ++c)
    {
        mtwnEasing easing;
        CHECK(mtwnMakeCubicBezierEasing(s_curves[c][0], s_curves[c][1], s_curves[c][2],
                                        s_curves[c][3], &easing) == mtwn_success,
              "a cubic bezier");
        for (int mode = mtwn_easeIn; mode <= mtwn_easeOutIn; ++mode)
        {
            CHECK(mtwnSetEaseMode(&easing, (mtwnEaseMode)mode) == mtwn_success, "its mode");
            for (int i = -200; i <= 1200; ++i)
            {
                double p = (double)i / 1000.0;
                same += SameBits(EaseBezierAt(&easing, p), EaseAt(&easing, p, false));
                same += SameBits(EaseBezierAt(&easing, p), EaseAt(&easing, p, true));
                compared += 2;
            }
            for (size_t e = 0; e < sizeof s_edges / sizeof s_edges[0]; ++e)
            {
                double p = s_edges[e];
                same += SameBits(EaseBezierAt(&easing, p), EaseAt(&easing, p, false));
                compared += 1;
            }
        }
    }
    CHECK(compared == 12u * 4u * (2u * 1401u + 16u) && same == compared,
          "every cubic bezier in every mode, as mtwnSampleEasing gives it");
    for (int k = mtwn_cssLinear; k <= mtwn_cssEaseInOut; ++k)
    {
        mtwnEasing keyword;
        CHECK(mtwnMakeCssEasing((mtwnCssKeyword)k, &keyword) == mtwn_success, "a keyword");
        CHECK(SameBits(EaseBezierAt(&keyword, 0.3), EaseAt(&keyword, 0.3, false)) &&
                  SameBits(EaseBezierAt(&keyword, -0.25), EaseAt(&keyword, -0.25, false)),
              "CSS's keywords as mtwnSampleEasing gives them");
    }
    mtwnEasing linear = mtwnMakeLinearEasing();
    CHECK(SameBits(EaseBezierAt(&linear, 0.3), 0.3) &&
              SameBits(EaseBezierAt(&linear, -2.5), EaseAt(&linear, -2.5, false)),
          "the linear easing, its progress");
    mtwnEasing others[4];
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &others[0]) == mtwn_success &&
              mtwnMakePennerEasing(mtwn_easingSine, mtwn_easeOut, &others[1]) == mtwn_success &&
              mtwnMakeStepsEasing(4, mtwn_jumpEnd, &others[2]) == mtwn_success &&
              mtwnMakeCssEasing(mtwn_cssStepStart, &others[3]) == mtwn_success,
          "easings of other kinds");
    for (int k = 0; k < 4; ++k)
    {
        double eased = 7.0;
        CHECK(mtwnSampleCubicBezierEasing(&others[k], 0.5, &eased) == mtwn_errorInvalid &&
                  eased == 7.0,
              "another kind refused");
    }
    mtwnEasing bezier;
    CHECK(mtwnMakeCubicBezierEasing(0.3, 0.1, 0.7, 0.9, &bezier) == mtwn_success, "a bezier");
    mtwnEasing mode = bezier;
    mode.mode = 4;
    mtwnEasing outside = bezier;
    outside.parameters[2] = 1.5;
    double eased = 7.0;
    CHECK(mtwnSampleCubicBezierEasing(&mode, 0.5, &eased) == mtwn_errorInvalid &&
              mtwnSampleCubicBezierEasing(&outside, 0.5, &eased) == mtwn_errorInvalid &&
              mtwnSampleCubicBezierEasing(NULL, 0.5, &eased) == mtwn_errorInvalid && eased == 7.0,
          "an unknown mode, a control point no maker takes, or none refused");
}

// How many of the five calls taking an easing refuse this one, or -1 if
// one changed an output while refusing.
static int Refusals(const mtwnEasing* easing)
{
    double eased = 7.0;
    bool monotonic = true;
    float samples[4] = {7.0f, 7.0f, 7.0f, 7.0f};
    mtwnLinearPoint points[4] = {{7.0, 7.0}, {7.0, 7.0}, {7.0, 7.0}, {7.0, 7.0}};
    mtwnEasing rough;
    memset(&rough, 0xAB, sizeof rough);
    // Copied byte for byte, padding included, so the comparisons below see
    // only what a call wrote.
    mtwnEasing before;
    memcpy(&before, &rough, sizeof before);
    mtwnRoughDef def = {.shape = *easing, .strength = 0.1, .seed = 3};
    mtwnEasing moded;
    memcpy(&moded, easing, sizeof moded);
    int refused = 0;
    refused += mtwnSampleEasing(easing, 0.5, false, &eased) == mtwn_errorInvalid;
    refused += mtwnIsEasingMonotonic(easing, &monotonic) == mtwn_errorInvalid;
    refused += mtwnSetEaseMode(&moded, mtwn_easeOut) == mtwn_errorInvalid;
    refused += mtwnSampleEasingTable(easing, 4, samples) == mtwn_errorInvalid;
    refused += mtwnMakeRoughEasing(&def, points, 4, &rough) == mtwn_errorInvalid;
    bool kept = eased == 7.0 && monotonic && samples[0] == 7.0f && samples[3] == 7.0f &&
                memcmp(&moded, easing, sizeof moded) == 0 && points[1].output == 7.0 &&
                memcmp(&rough, &before, sizeof rough) == 0;
    return refused == 5 && !kept ? -1 : refused;
}

static double ReturnHalf(void* context, double progress)
{
    (void)context;
    (void)progress;
    return 0.5;
}

// One made easing with one field written as no maker writes it.
#define FORGED(base, change, what)                                                                 \
    do                                                                                             \
    {                                                                                              \
        mtwnEasing e = (base);                                                                     \
        change;                                                                                    \
        CHECK(Refusals(&e) == 5, what);                                                            \
    } while (0)

static void TestForgedPenner(void)
{
    mtwnEasing power = mtwnMakeLinearEasing();
    mtwnEasing sine = power;
    mtwnEasing expo = power;
    mtwnEasing circ = power;
    mtwnEasing bounce = power;
    mtwnEasing back = power;
    mtwnEasing elastic = power;
    CHECK(mtwnMakePowerEasing(mtwn_easeIn, 2.0, &power) == mtwn_success &&
              mtwnMakePennerEasing(mtwn_easingSine, mtwn_easeIn, &sine) == mtwn_success &&
              mtwnMakePennerEasing(mtwn_easingExpo, mtwn_easeIn, &expo) == mtwn_success &&
              mtwnMakePennerEasing(mtwn_easingCirc, mtwn_easeIn, &circ) == mtwn_success &&
              mtwnMakePennerEasing(mtwn_easingBounce, mtwn_easeIn, &bounce) == mtwn_success &&
              mtwnMakeBackEasing(mtwn_easeIn, 1.7, &back) == mtwn_success &&
              mtwnMakeElasticEasing(mtwn_easeOut, 1.2, 0.3, &elastic) == mtwn_success,
          "Penner's easings");
    const mtwnEasing* made[] = {&power, &sine, &expo, &circ, &bounce, &back, &elastic};
    int taken = 0;
    for (size_t i = 0; i < sizeof made / sizeof made[0]; ++i)
    {
        taken += Refusals(made[i]) == 0;
    }
    CHECK(taken == 7, "as made, taken");
    FORGED(power, e.mode = 4, "an unknown mode");
    FORGED(power, e.kind = 13, "an unknown kind");
    FORGED(power, e.parameters[0] = 0.0, "an exponent of 0");
    FORGED(power, e.parameters[0] = HUGE_VAL, "an infinite exponent");
    FORGED(power, (e.count = 0, e.parameters[0] = HUGE_VAL), "an infinite exponent, not counted");
    FORGED(power, e.count = 3, "a count not its exponent");
    FORGED(power, (e.parameters[0] = 9.0, e.count = 9), "a count past 8");
    FORGED(power, e.parameters[3] = 0.5, "a power's start not 0");
    FORGED(power, e.parameters[4] = 2.0, "a power's range not 1");
    FORGED(sine, e.parameters[4] = 1.0, "sine's range not its own");
    FORGED(expo, e.parameters[3] = 0.0, "expo's start not its own");
    FORGED(expo, e.parameters[4] = 1.0, "expo's range not its own");
    FORGED(circ, e.parameters[4] = 0x1.0p-1074, "circ's range not 1");
    FORGED(bounce, e.parameters[3] = -1.0, "bounce's start not 0");
    FORGED(back, e.parameters[0] = -1.0, "a negative overshoot");
    FORGED(back, e.parameters[0] = HUGE_VAL, "an infinite overshoot");
    FORGED(back, e.parameters[4] = 0.0, "back's range 0");
    FORGED(elastic, e.parameters[0] = 0.5, "an amplitude below 1");
    FORGED(elastic, e.parameters[0] = 513.0, "an amplitude past 512");
    FORGED(elastic, e.parameters[1] = 0.0, "no frequency");
    FORGED(elastic, e.parameters[1] = 0x1.0p13, "a period below 2^-10");
    FORGED(elastic, e.parameters[2] = (double)NAN, "a NaN phase");
    FORGED(elastic, e.parameters[2] = -1e308, "a phase whose angle overflows");
    FORGED(elastic, e.parameters[2] = 0.5, "a phase past a quarter turn");
    FORGED(elastic, e.parameters[3] = 1.5, "a start past 1");
    FORGED(elastic, e.parameters[4] = 0.2, "a range below 1/4");
    FORGED(elastic, e.parameters[4] = 2.5, "a range past 2");
}

static void TestForgedOthers(void)
{
    mtwnEasing host = mtwnMakeLinearEasing();
    mtwnEasing bezier = host;
    mtwnEasing steps = host;
    mtwnEasing linear = host;
    mtwnLinearPoint points[3] = {{0.0, 0.0}, {0.5, 0.8}, {1.0, 1.0}};
    CHECK(mtwnMakeHostEasing(mtwn_easeIn, ReturnHalf, NULL, &host) == mtwn_success &&
              mtwnMakeCubicBezierEasing(0.25, 0.1, 0.25, 1.0, &bezier) == mtwn_success &&
              mtwnMakeStepsEasing(4, mtwn_jumpEnd, &steps) == mtwn_success &&
              mtwnMakeLinearPointsEasing(points, 3, &linear) == mtwn_success,
          "the others");
    mtwnEasing zeroed;
    memset(&zeroed, 0, sizeof zeroed);
    CHECK(Refusals(&host) == 0 && Refusals(&bezier) == 0 && Refusals(&steps) == 0 &&
              Refusals(&linear) == 0 && Refusals(&zeroed) == 0,
          "as made, taken, and a zeroed one is linear");
    FORGED(host, e.function = NULL, "a host easing without its function");
    FORGED(bezier, e.parameters[0] = -0.1, "a control x below 0");
    FORGED(bezier, e.parameters[2] = 1.5, "a control x past 1");
    FORGED(bezier, e.parameters[1] = 0x1.0p1001, "a control y past 2^1000");
    FORGED(bezier, e.parameters[3] = (double)NAN, "a NaN control y");
    FORGED(steps, e.count = 0, "no steps");
    FORGED(steps, (e.count = 1, e.position = mtwn_jumpNone), "steps(1) jumping at neither end");
    FORGED(steps, e.position = 4, "an unknown position");
    FORGED(linear, e.points = NULL, "linear() without its points");
    FORGED(linear, e.count = 1, "linear() of one point");
    CHECK(mtwnMakeCubicBezierEasing(0.5, 0x1.0p1000, 0.5, -0x1.0p1000, &bezier) == mtwn_success &&
              mtwnMakeCubicBezierEasing(0.5, nextafter(0x1.0p1000, HUGE_VAL), 0.5, 0.0, &bezier) ==
                  mtwn_errorInvalid,
          "a control y of 2^1000 taken, past it refused");
}

int main(int argc, char** argv)
{
    bool dense = argc == 2 && strcmp(argv[1], "dense") == 0;
    if (argc > 2 || (argc == 2 && !dense))
    {
        printf("usage: test_easing [dense]\n");
        return 2;
    }
    s_finer = dense ? 10 : 1;
    TestElasticZero();
    TestForgedPenner();
    TestForgedOthers();
    TestLinearRunEnd();
    TestEndsAreExact();
    TestEndsAreExactForEveryParameter();
    TestAgainstTheFormulas();
    TestMonotonicEasingsNeverDecrease();
    TestHalvesMeetGoingUp();
    TestRivalMappings();
    TestModesAreReflections();
    TestHostEasing();
    TestRefusals();
    TestCubicBezierSampler();
    return s_failures == 0 ? 0 : 1;
}
