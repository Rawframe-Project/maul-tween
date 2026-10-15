// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's thread rules, under the thread sanitizer in CI: the
// values layer is used from four threads at once (easings, springs, a
// color mix, staggers, timing, and a finished clip's track read through
// mtwnClip_GetTrack and sampled with mtwnSampleTrack) while the context's
// own thread updates players of that clip and makes and destroys more.
// Every thread's results are the same bits as one thread's. The web
// build has no threads, so there the workers run one after another.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
typedef HANDLE Thread;
#elif !defined(__EMSCRIPTEN__)
#include <pthread.h>
typedef pthread_t Thread;
#else
typedef int Thread;
#endif

enum
{
    s_workers = 4,
    s_rounds = 200,
    s_items = 64,
};

// What the workers share, read only once they start.
typedef struct Shared
{
    mtwnEasing ease;
    mtwnEasing bezier;
    mtwnSpring spring;
    mtwnColorMix mix;
    mtwnStagger stagger;
    mtwnTiming timing;
    mtwnClipTrack track;
} Shared;

static Shared s_shared;

// A hash of every value a worker computes, its bits mixed in order.
static uint64_t Mix(uint64_t hash, double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    hash ^= bits + 0x9E3779B97F4A7C15ull + (hash << 6) + (hash >> 2);
    return hash;
}

static uint64_t Work(void)
{
    uint64_t hash = 0;
    double offsets[s_items];
    for (int round = 0; round < s_rounds; ++round)
    {
        for (int i = 0; i <= 64; ++i)
        {
            double p = (double)i / 64.0;
            hash = Mix(hash, mtwnEase(&s_shared.ease, p, false));
            hash = Mix(hash, mtwnEase(&s_shared.bezier, p, false));
            double offset = 0.0;
            double speed = 0.0;
            mtwnSampleSpring(&s_shared.spring, -1.0, 0.0, p, &offset, &speed);
            hash = Mix(Mix(hash, offset), speed);
            mtwnColor color = mtwnMixColor(&s_shared.mix, p);
            hash = Mix(Mix(Mix(hash, (double)color.red), (double)color.green), (double)color.blue);
            mtwnTimingResult timed;
            mtwnComputeTiming(&s_shared.track.timing, p * 2.0, false, &timed);
            mtwnVec4 value = mtwnSampleTrack(&s_shared.track.track, timed.transformedProgress);
            hash = Mix(Mix(hash, (double)value.x), (double)value.y);
            mtwnComputeTiming(&s_shared.timing, p * 3.0, false, &timed);
            hash = Mix(hash, timed.transformedProgress);
        }
        if (mtwnComputeStagger(&s_shared.stagger, s_items, offsets) != mtwn_success)
        {
            return 0;
        }
        for (int i = 0; i < s_items; ++i)
        {
            hash = Mix(hash, offsets[i]);
        }
    }
    return hash;
}

static uint64_t s_results[s_workers];

#if defined(_WIN32)
static DWORD WINAPI Worker(LPVOID slot)
{
    s_results[(uintptr_t)slot] = Work();
    return 0;
}
#elif !defined(__EMSCRIPTEN__)
static void* Worker(void* slot)
{
    s_results[(uintptr_t)slot] = Work();
    return NULL;
}
#endif

static bool Start(uintptr_t slot, void* thread)
{
#if defined(_WIN32)
    *(Thread*)thread = CreateThread(NULL, 0, Worker, (LPVOID)slot, 0, NULL);
    return *(Thread*)thread != NULL;
#elif !defined(__EMSCRIPTEN__)
    return pthread_create((Thread*)thread, NULL, Worker, (void*)slot) == 0;
#else
    (void)thread;
    s_results[slot] = Work();
    return true;
#endif
}

static void Join(void* thread)
{
#if defined(_WIN32)
    WaitForSingleObject(*(Thread*)thread, INFINITE);
    CloseHandle(*(Thread*)thread);
#elif !defined(__EMSCRIPTEN__)
    pthread_join(*(Thread*)thread, NULL);
#else
    (void)thread;
#endif
}

// A finished clip of one 2D track from (0, 0) through (1, 2) to (3, 1)
// over a second, repeated twice, alternating.
static mtwnClipId MakeClip(mtwnContext* context, mtwnNode* nodeOut)
{
    mtwnClipDef def = mtwnDefaultClipDef();
    mtwnClipId clip = {0, 0};
    CHECK(mtwnCreateClip(context, &def, &clip) == mtwn_success, "a clip");
    mtwnKeyframe keys[3];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 0.5;
    keys[1].value = (mtwnVec4){1.0f, 2.0f, 0, 0};
    keys[2].offset = 1.0;
    keys[2].value = (mtwnVec4){3.0f, 1.0f, 0, 0};
    keys[0].easing = keys[1].easing = keys[2].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.track.kind = mtwn_valueVec2;
    track.keyframes = keys;
    track.keyframeCount = 3;
    track.timing.duration = 1.0;
    track.timing.iterations = 2.0;
    track.timing.direction = mtwn_directionAlternate;
    CHECK(mtwnClip_AddTrack(context, clip, &track, nodeOut) == mtwn_success, "a track");
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    return clip;
}

static void Prepare(mtwnContext* context, mtwnClipId clip, mtwnNode node)
{
    CHECK(mtwnMakeElasticEasing(mtwn_easeOut, 1.0, 0.3, &s_shared.ease) == mtwn_success &&
              mtwnMakeCubicBezier(0.25, 0.1, 0.25, 1.0, &s_shared.bezier) == mtwn_success &&
              mtwnMakeSpringFromDuration(0.5, 0.3, &s_shared.spring) == mtwn_success,
          "curves");
    mtwnColor red = {1.0f, 0.0f, 0.0f, 1.0f};
    mtwnColor blue = {0.0f, 0.0f, 1.0f, 0.5f};
    CHECK(mtwnMakeColorMix(red, blue, mtwn_colorOklch, mtwn_hueLonger, &s_shared.mix) ==
              mtwn_success,
          "a color mix");
    s_shared.stagger = mtwnDefaultStagger();
    s_shared.stagger.columns = 8;
    s_shared.stagger.origin = mtwn_staggerCentre;
    s_shared.timing = mtwnDefaultTiming();
    s_shared.timing.duration = 0.75;
    s_shared.timing.iterations = 2.5;
    s_shared.timing.repeatDelay = 0.25;
    CHECK(mtwnMakePowerEasing(mtwn_easeInOut, 3.0, &s_shared.timing.easing) == mtwn_success,
          "a timing easing");
    CHECK(mtwnClip_GetTrack(context, clip, node, &s_shared.track) == mtwn_success, "the track");
}

static void TestThreads(void)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    mtwnNode node = 0;
    mtwnClipId clip = MakeClip(context, &node);
    Prepare(context, clip, node);
    uint64_t reference = Work();
    CHECK(reference != 0, "one thread's results");

    mtwnTargetId targets[8];
    for (int i = 0; i < 8; ++i)
    {
        CHECK(mtwnCreateTarget(context, mtwn_valueVec2, (mtwnVec4){0}, &targets[i]) == mtwn_success,
              "a target");
    }
    Thread threads[s_workers];
    bool started = true;
    for (uintptr_t i = 0; i < s_workers; ++i)
    {
        started = Start(i, &threads[i]) && started;
    }
    CHECK(started, "the workers started");
    // The context's thread: players of the clip made, updated, destroyed.
    uint64_t now = 0;
    for (int round = 0; round < 400; ++round)
    {
        mtwnBinding binding = mtwnBind(0, targets[round % 8]);
        mtwnPlayerDef player = mtwnDefaultPlayerDef();
        player.clip = clip;
        player.bindings = &binding;
        player.bindingCount = 1;
        mtwnPlayerId id = {0, 0};
        CHECK(mtwnCreatePlayer(context, &player, &id) == mtwn_success, "a player");
        now += 16000000u;
        CHECK(mtwnUpdate(context, now, NULL, 0, NULL) == mtwn_success, "an update");
        if (round % 3 == 0)
        {
            CHECK(mtwnDestroyPlayer(context, id) == mtwn_success, "a player destroyed");
        }
    }
    for (int i = 0; i < s_workers; ++i)
    {
        Join(&threads[i]);
    }
    bool same = true;
    for (int i = 0; i < s_workers; ++i)
    {
        same = same && s_results[i] == reference;
    }
    CHECK(same, "every thread's results the same bits as one thread's");
    mtwnDestroyContext(context);
}

int main(void)
{
    TestThreads();
    return s_failures == 0 ? 0 : 1;
}
