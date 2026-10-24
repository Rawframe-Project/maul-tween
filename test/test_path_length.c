// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A path's arc lengths on random paths against Gauss-Legendre in long
// double, far inside the accuracy the header promises.

#include "test_harness.h"

#include "maul-tween/easing.h"
#include "maul-tween/path.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#if LDBL_MANT_DIG >= 64 && !defined(__EMSCRIPTEN__)
typedef long double Wide;
#define WIDE_SQRT sqrtl
#else
typedef double Wide;
#define WIDE_SQRT sqrt
#endif

// Random numbers from a fixed seed (xorshift64).
static uint64_t s_state = 88172645463325252u;

static double Random(void)
{
    s_state ^= s_state << 13;
    s_state ^= s_state >> 7;
    s_state ^= s_state << 17;
    return (double)(s_state >> 11) * 0x1p-53;
}

static float Coordinate(void)
{
    return (float)(Random() * 20.0 - 10.0);
}

static mtwnVec4 Point(uint8_t channels)
{
    float x = Coordinate();
    float y = Coordinate();
    float z = channels > 2 ? Coordinate() : 0.0f;
    float w = channels > 3 ? Coordinate() : 0.0f;
    return (mtwnVec4){x, y, z, w};
}

typedef struct RandomPath
{
    mtwnKeyframe keys[8];
    uint32_t count;
    uint8_t channels;
    double alpha;
} RandomPath;

// Three to eight keyframes of two to four channels, Bezier, Catmull-Rom
// and linear segments, uneven offsets, any alpha.
static RandomPath MakeRandom(void)
{
    RandomPath p;
    memset(&p, 0, sizeof p);
    p.count = 3u + (uint32_t)(Random() * 6.0);
    p.channels = (uint8_t)(2u + (uint32_t)(Random() * 3.0));
    double offsets[8] = {0.0};
    for (uint32_t k = 1; k < p.count; ++k)
    {
        offsets[k] = offsets[k - 1] + 0.1 + Random();
    }
    for (uint32_t k = 0; k < p.count; ++k)
    {
        mtwnKeyframe* key = &p.keys[k];
        key->offset = k + 1 == p.count ? 1.0 : offsets[k] / offsets[p.count - 1];
        key->value = Point(p.channels);
        key->in = Point(p.channels);
        key->out = Point(p.channels);
        key->easing = mtwnLinearEasing();
        double kind = Random();
        key->kind = kind < 0.4   ? mtwn_segmentBezier
                    : kind < 0.8 ? mtwn_segmentCatmullRom
                                 : mtwn_segmentLinear;
    }
    p.alpha = Random();
    return p;
}

// The 8-point Gauss-Legendre rule on [-1, 1].
static const Wide s_nodes[4] = {
    0.1834346424956498049394761423601840L, 0.5255324099163289858177390491891843L,
    0.7966664774136267395915539364758304L, 0.9602898564975362316835608685694730L};
static const Wide s_weights[4] = {
    0.3626837833783619829651504492771595L, 0.3137066458778872873379622019866013L,
    0.2223810344533744705443559944262409L, 0.1012285362903762591525313543099622L};

// A segment's length in Wide: the path's own speed (its tangent by the
// segment's parameter, sampled without constant speed and linear easing)
// on 4,096 panels.
static Wide SegmentLength(const mtwnPath* timed, double a, double b, uint8_t channels)
{
    enum
    {
        panels = 4096
    };
    Wide length = 0;
    for (int q = 0; q < panels; ++q)
    {
        for (int g = 0; g < 8; ++g)
        {
            Wide x = g < 4 ? -s_nodes[g] : s_nodes[g - 4];
            Wide u = ((Wide)q + (x + 1) / 2) / panels;
            double v[4];
            double t[4];
            mtwnSamplePath(timed, a + (double)u * (b - a), v, t);
            Wide speed = 0;
            for (uint8_t c = 0; c < channels; ++c)
            {
                speed += (Wide)t[c] * (Wide)t[c];
            }
            length += s_weights[g < 4 ? g : g - 4] * WIDE_SQRT(speed) / (2 * panels);
        }
    }
    return length;
}

// Each segment's measured length within 1e-10 relative of its length
// found again, on 4 random paths, and 20 in the nightly run
// (`test_path_length dense`): measured on 1,000 the worst was 1.8e-12
// (the reference here, on fewer panels, adds up to 1.7e-11 where a
// segment's speed has a cusp).
static void TestLengthError(int paths)
{
    s_state = 0x9E3779B97F4A7C15u;
    double worst = 0.0;
    for (int n = 0; n < paths; ++n)
    {
        RandomPath p = MakeRandom();
        double cumulative[8];
        mtwnPath even;
        mtwnPath timed;
        CHECK(mtwnMakePath(p.keys, p.count, p.channels, p.alpha, cumulative, &even) ==
                      mtwn_success &&
                  mtwnMakePath(p.keys, p.count, p.channels, p.alpha, NULL, &timed) == mtwn_success,
              "a path");
        for (uint32_t i = 0; i + 1 < p.count; ++i)
        {
            Wide reference =
                SegmentLength(&timed, p.keys[i].offset, p.keys[i + 1].offset, p.channels);
            double measured = cumulative[i + 1] - cumulative[i];
            double error = reference > 0 ? fabs((double)(((Wide)measured - reference) / reference))
                                         : fabs(measured);
            worst = error > worst ? error : worst;
        }
    }
    if (!(worst <= 1e-10))
    {
        printf("  worst segment length %.3g relative\n", worst);
    }
    CHECK(worst <= 1e-10, "every segment's length far inside the tolerance");
}

int main(int argc, char** argv)
{
    bool dense = argc == 2 && strcmp(argv[1], "dense") == 0;
    if (argc > 2 || (argc == 2 && !dense))
    {
        printf("usage: test_path_length [dense]\n");
        return 2;
    }
    TestLengthError(dense ? 20 : 4);
    return s_failures == 0 ? 0 : 1;
}
