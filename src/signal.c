// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Integers, discrete values, punch and shake (records mtwn-0016,
// mtwn-0017 and mtwn-0026). Shake is Perlin's improved noise in one
// dimension: at a lattice point it is 0, between two it blends their
// gradients' lines by the quintic fade 6u^5 - 15u^4 + 10u^3, whose first
// two derivatives vanish at the lattice; the gradients come from
// SplitMix64 of the seed, the channel and the lattice index, so a seed
// repeats exactly on every platform.

#include "elementary.h"

#include "generated/math_constants.h"
#include "maul-tween/interpolate.h"

#include <math.h>
#include <stdbool.h>

// The largest 1D gradient noise from gradients in [-1, 1) is under 1/2.
static const double s_noiseScale = 2.0;

int64_t mtwnMixInteger(int64_t from, int64_t to, double progress)
{
    if (progress == 0.0)
    {
        return from;
    }
    if (progress == 1.0)
    {
        return to;
    }
    double value = round((1.0 - progress) * (double)from + progress * (double)to);
    // 2^63 is the first double past the type's range on either side.
    if (!(value < 0x1.0p63))
    {
        return isnan(value) ? from : INT64_MAX;
    }
    if (value < -0x1.0p63)
    {
        return INT64_MIN;
    }
    return (int64_t)value;
}

uint32_t mtwnMixDiscrete(uint32_t from, uint32_t to, double progress)
{
    return progress < 0.5 ? from : to;
}

static bool IsPositive(double value)
{
    return isfinite(value) && value > 0.0;
}

// Fewer lattice points than this over a shake's duration: its position
// keeps a fraction between them, and its index stays far from 2^64.
static const double s_shakePoints = 0x1.0p53;

// The envelope (1 - t / duration)^exponent inside the duration, and 0
// outside it.
static double Envelope(double seconds, double duration, double exponent)
{
    if (!(seconds > 0.0 && seconds < duration))
    {
        return 0.0;
    }
    return mtwnPow(1.0 - seconds / duration, exponent);
}

mtwnResult mtwnMakePunch(double frequency, double duration, double exponent, mtwnPunch* punchOut)
{
    if (punchOut == NULL || !IsPositive(frequency) || !IsPositive(duration) ||
        !IsPositive(exponent))
    {
        return mtwn_errorInvalid;
    }
    *punchOut = (mtwnPunch){frequency, duration, exponent};
    return mtwn_success;
}

double mtwnSamplePunch(const mtwnPunch* punch, double seconds)
{
    double envelope = Envelope(seconds, punch->duration, punch->exponent);
    if (envelope == 0.0)
    {
        return 0.0;
    }
    double sine = 0.0;
    double cosine = 0.0;
    // The sine of the cycles' fraction, exact for any time: the angle stays
    // within a turn, where the sine is accurate however long the punch.
    // From 2^52 cycles every double is whole, its fraction 0, and so is an
    // overflowed count's.
    double cycles = punch->frequency * seconds;
    double fraction = isinf(cycles) ? 0.0 : cycles - floor(cycles);
    mtwnSinCos(4.0 * MTWN_PIO2_HI * fraction, &sine, &cosine);
    return sine * envelope;
}

mtwnResult mtwnMakeShake(double frequency, double duration, double exponent, uint64_t seed,
                         mtwnShake* shakeOut)
{
    if (shakeOut == NULL || !IsPositive(frequency) || !IsPositive(duration) ||
        !IsPositive(exponent) || !(frequency * duration < s_shakePoints))
    {
        return mtwn_errorInvalid;
    }
    *shakeOut = (mtwnShake){frequency, duration, exponent, seed};
    return mtwn_success;
}

// The gradient in [-1, 1) at a lattice index of a channel: SplitMix64's
// finalizer of the three mixed together.
static double Gradient(uint64_t seed, uint32_t channel, uint64_t index)
{
    uint64_t z =
        seed + 0x9E3779B97F4A7C15ull * ((uint64_t)channel + 1u) + 0xD1B54A32D192ED03ull * index;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (double)(z >> 11) * 0x1.0p-52 - 1.0;
}

double mtwnSampleShake(const mtwnShake* shake, uint32_t channel, double seconds)
{
    double envelope = Envelope(seconds, shake->duration, shake->exponent);
    if (envelope == 0.0)
    {
        return 0.0;
    }
    double x = shake->frequency * seconds;
    double lattice = floor(x);
    double u = x - lattice;
    uint64_t index = (uint64_t)lattice;
    double left = Gradient(shake->seed, channel, index) * u;
    double right = Gradient(shake->seed, channel, index + 1u) * (u - 1.0);
    double fade = u * u * u * (u * (u * 6.0 - 15.0) + 10.0);
    return s_noiseScale * (left + fade * (right - left)) * envelope;
}
