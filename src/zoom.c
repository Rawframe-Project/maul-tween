// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Zooms: van Wijk and Nuij's optimal path between
// views (u, w), a centre and a width, with rho trading zoom against pan.
// With d the pan's distance, b0 = (w1^2 - w0^2 + rho^4 d^2) / (2 w0
// rho^2 d) and b1 likewise with w1 and -rho^4 d^2, r = -asinh(b), the
// length is S = (r1 - r0) / rho and at s along it
//   u(s) = w0 / rho^2 (cosh r0 tanh(rho s + r0) - sinh r0)
//   w(s) = w0 cosh r0 / cosh(rho s + r0).
// u's difference cancels near s = 0; it is the same as
//   w0 sinh(rho s) / (rho^2 cosh(rho s + r0)),
// which does not, with sinh by its series below 1. asinh is taken as
// log(|b| + sqrt(b^2 + 1)), stable for either sign, and log(2 |b|) where
// b^2 would overflow (where the two agree to the last bit). The terms
// are computed in units of w0, so views of any scale give the same path.
// Equal centres zoom geometrically: S is |log(w1 / w0)| / rho, the limit
// of the above as d goes to 0.

#include "elementary.h"
#include "fpenv.h"
#include "work.h"

#include "maul-tween/interpolate.h"

#include <math.h>
#include <stdbool.h>

// Where exp(x) is still finite and above the smallest normal.
static const double s_expRange = 700.0;
// From here on b^2 + 1 is b^2 to the last bit.
static const double s_asinhBig = 0x1p27;

static double Asinh(double b)
{
    double a = fabs(b);
    double r = mtwnLog(a < s_asinhBig ? a + sqrt(a * a + 1.0) : 2.0 * a);
    return b < 0.0 ? -r : r;
}

static double Cosh(double x)
{
    double e = mtwnExp(fabs(x));
    return 0.5 * (e + 1.0 / e);
}

static double Sinh(double x)
{
    double a = fabs(x);
    if (a < 1.0)
    {
        // x (1 + x^2/3! + x^4/5! + ...), to x^19/19!, under an ulp at 1.
        double xx = x * x;
        double s = 1.0 + xx / 342.0;
        s = 1.0 + xx / 272.0 * s;
        s = 1.0 + xx / 210.0 * s;
        s = 1.0 + xx / 156.0 * s;
        s = 1.0 + xx / 110.0 * s;
        s = 1.0 + xx / 72.0 * s;
        s = 1.0 + xx / 42.0 * s;
        s = 1.0 + xx / 20.0 * s;
        s = 1.0 + xx / 6.0 * s;
        return x * s;
    }
    double e = mtwnExp(a);
    double r = 0.5 * (e - 1.0 / e);
    return x < 0.0 ? -r : r;
}

static bool IsView(mtwnZoomView v)
{
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.width) && v.width > 0.0;
}

static MTWN_NOINLINE mtwnResult mtwnMakeZoomMixWork(mtwnZoomView from, mtwnZoomView to, double rho,
                                                    mtwnZoomMix* mixOut)
{
    if (mixOut == NULL || !IsView(from) || !IsView(to) || !isfinite(rho) || !(rho > 0.0))
    {
        return mtwn_errorInvalid;
    }
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    // The pan and the far width in units of the near one.
    // sqrt, correctly rounded everywhere, where hypot differs between
    // C libraries; in w0's units no square overflows a path that fits.
    double ux = dx / from.width;
    double uy = dy / from.width;
    double d = sqrt(ux * ux + uy * uy);
    double ratio = to.width / from.width;
    double rr = rho * rho;
    mtwnZoomMix mix = {from, to, rho, 0.0, 0.0, 1.0, 0.0, {0.0, 0.0, 0.0, 0.0}};
    if (d == 0.0)
    {
        double logFrom = mtwnLog(from.width);
        mix.widths = (mtwnScaleMix){from.width, to.width, logFrom, mtwnLog(to.width) - logFrom};
        mix.length = fabs(mix.widths.logRatio) / rho;
    }
    else
    {
        double b0 = (ratio * ratio - 1.0 + rr * rr * d * d) / (2.0 * rr * d);
        double b1 = (ratio * ratio - 1.0 - rr * rr * d * d) / (2.0 * ratio * rr * d);
        double r0 = -Asinh(b0);
        double r1 = -Asinh(b1);
        mix.r0 = r0;
        mix.coshR0 = Cosh(r0);
        mix.length = (r1 - r0) / rho;
        // u(s) as a share of the pan: w0 / (rho^2 d) in w0's units.
        mix.panScale = 1.0 / (rr * d);
        if (!(fabs(r0) < s_expRange && fabs(r1) < s_expRange && isfinite(mix.panScale)))
        {
            return mtwn_errorInvalid;
        }
    }
    if (!isfinite(mix.length))
    {
        return mtwn_errorInvalid;
    }
    *mixOut = mix;
    return mtwn_success;
}

mtwnResult mtwnMakeZoomMix(mtwnZoomView from, mtwnZoomView to, double rho, mtwnZoomMix* mixOut)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnResult result = mtwnMakeZoomMixWork(from, to, rho, mixOut);
    mtwnFloatEnvLeave(env);
    return result;
}

static MTWN_NOINLINE mtwnZoomView mtwnMixZoomWork(const mtwnZoomMix* mix, double progress)
{
    if (progress == 0.0)
    {
        return mix->from;
    }
    if (progress == 1.0)
    {
        return mix->to;
    }
    const mtwnZoomView* a = &mix->from;
    const mtwnZoomView* b = &mix->to;
    if (mix->panScale == 0.0)
    {
        // Equal centres: the width geometric, the centre where it is.
        return (mtwnZoomView){a->x, a->y, mtwnMixScaleWork(&mix->widths, progress)};
    }
    double along = mix->rho * progress * mix->length;
    double c = Cosh(along + mix->r0);
    double share = mix->panScale * Sinh(along) / c;
    return (mtwnZoomView){a->x + share * (b->x - a->x), a->y + share * (b->y - a->y),
                          a->width * (mix->coshR0 / c)};
}

mtwnZoomView mtwnMixZoom(const mtwnZoomMix* mix, double progress)
{
    uint64_t env = mtwnFloatEnvEnter();
    mtwnZoomView result = mtwnMixZoomWork(mix, progress);
    mtwnFloatEnvLeave(env);
    return result;
}

static MTWN_NOINLINE double mtwnZoomLengthWork(const mtwnZoomMix* mix)
{
    return mix->length;
}

double mtwnZoomLength(const mtwnZoomMix* mix)
{
    uint64_t env = mtwnFloatEnvEnter();
    double result = mtwnZoomLengthWork(mix);
    mtwnFloatEnvLeave(env);
    return result;
}
