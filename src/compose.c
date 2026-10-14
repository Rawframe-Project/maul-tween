// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Composition (record mtwn-0039), Web Animations Level 1's replace and
// add (accumulate is addition for every kind here, as CSS Values 4 says)
// with a weight: the result mixes from the underlying value to the
// composited one. Numbers add and mix per channel; colors premultiplied
// by alpha; rotations multiply (the effect turning in the underlying's
// frame) and mix by slerp; a discrete value is not additive and steps at
// half.

#include "compose.h"

#include "maul-tween/interpolate.h"

#include <math.h>

static void Unpack(mtwnVec4 v, double out[4])
{
    out[0] = (double)v.x;
    out[1] = (double)v.y;
    out[2] = (double)v.z;
    out[3] = (double)v.w;
}

static mtwnVec4 Pack(const double v[4])
{
    return (mtwnVec4){(float)v[0], (float)v[1], (float)v[2], (float)v[3]};
}

// Per channel: the sum for add, then the mix by the weight.
static mtwnVec4 Linear(mtwnComposite composite, double weight, mtwnVec4 under, mtwnVec4 effect)
{
    double a[4];
    double b[4];
    Unpack(under, a);
    Unpack(effect, b);
    double out[4];
    for (int i = 0; i < 4; ++i)
    {
        double composed = composite == mtwn_compositeAdd ? a[i] + b[i] : b[i];
        out[i] = weight == 1.0 ? composed : a[i] + weight * (composed - a[i]);
    }
    return Pack(out);
}

static void Premultiply(mtwnVec4 v, double out[4])
{
    Unpack(v, out);
    for (int i = 0; i < 3; ++i)
    {
        out[i] *= out[3];
    }
}

static mtwnVec4 Color(mtwnComposite composite, double weight, mtwnVec4 under, mtwnVec4 effect)
{
    double a[4];
    double b[4];
    Premultiply(under, a);
    Premultiply(effect, b);
    double out[4];
    for (int i = 0; i < 4; ++i)
    {
        double composed = composite == mtwn_compositeAdd ? a[i] + b[i] : b[i];
        out[i] = a[i] + weight * (composed - a[i]);
    }
    out[3] = out[3] > 1.0 ? 1.0 : out[3];
    for (int i = 0; i < 3; ++i)
    {
        out[i] = out[3] > 0.0 ? out[i] / out[3] : 0.0;
    }
    return Pack(out);
}

// The Hamilton product a b: b's turn in a's frame.
static mtwnQuat Multiply(mtwnQuat a, mtwnQuat b)
{
    double ax = (double)a.x;
    double ay = (double)a.y;
    double az = (double)a.z;
    double aw = (double)a.w;
    double bx = (double)b.x;
    double by = (double)b.y;
    double bz = (double)b.z;
    double bw = (double)b.w;
    double x = aw * bx + ax * bw + ay * bz - az * by;
    double y = aw * by - ax * bz + ay * bw + az * bx;
    double z = aw * bz + ax * by - ay * bx + az * bw;
    double w = aw * bw - ax * bx - ay * by - az * bz;
    double norm = sqrt(x * x + y * y + z * z + w * w);
    if (!(norm > 0.0))
    {
        return a;
    }
    return (mtwnQuat){(float)(x / norm), (float)(y / norm), (float)(z / norm), (float)(w / norm)};
}

static mtwnVec4 Rotation(mtwnComposite composite, double weight, mtwnVec4 under, mtwnVec4 effect)
{
    mtwnQuat a = {under.x, under.y, under.z, under.w};
    mtwnQuat b = {effect.x, effect.y, effect.z, effect.w};
    mtwnQuat composed = composite == mtwn_compositeAdd ? Multiply(a, b) : b;
    mtwnRotationMix mix;
    if (weight < 1.0 &&
        mtwnMakeRotationMix(a, composed, mtwn_rotationShortest, &mix) == mtwn_success)
    {
        composed = mtwnMixRotation(&mix, weight);
    }
    return (mtwnVec4){composed.x, composed.y, composed.z, composed.w};
}

mtwnMoving mtwnCompose(mtwnValueKind kind, mtwnComposite composite, double weight,
                       mtwnMoving underlying, mtwnMoving effect)
{
    if (weight == 0.0)
    {
        return underlying;
    }
    mtwnMoving out = {effect.value, {0.0f, 0.0f, 0.0f, 0.0f}};
    switch (kind)
    {
    case mtwn_valueDiscrete:
        // Not additive; a weight steps at half, as discrete values mix.
        return weight >= 0.5 ? (mtwnMoving){effect.value, {0.0f, 0.0f, 0.0f, 0.0f}} : underlying;
    case mtwn_valueColor:
        out.value = Color(composite, weight, underlying.value, effect.value);
        break;
    case mtwn_valueRotation:
        out.value = Rotation(composite, weight, underlying.value, effect.value);
        break;
    default:
        out.value = Linear(composite, weight, underlying.value, effect.value);
        break;
    }
    out.velocity = Linear(composite, weight, underlying.velocity, effect.velocity);
    if (kind == mtwn_valueInteger)
    {
        out.value.x = (float)round((double)out.value.x);
    }
    return out;
}
