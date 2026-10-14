// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Timings against the library's targets (record mtwn-0043), on the task
// its rivals were measured with: 64,000 transitions of a number from 0 to
// 100 over a second with a quadratic ease out. Creates them all, then
// updates them at 16 ms steps, the best of 50 updates, without and with
// the report of changed targets; prints the bytes each takes with its
// target. Then the cost of one sample of a cubic bezier, a spring, and
// Penner's easings; then 1,000 clip players of four tracks each, in a
// timeline repeating. Prints each section's counts and bytes, which do
// not depend on the machine, as comment lines, then its named results,
// the best of five runs (the updates' best of 50). Given a baseline
// file, such as bench/baseline.txt, it prints each result's ratio to the
// recorded one as well.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/transition.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum
{
    TWEENS = 64000,
    RUNS = 5,
    UPDATES = 50,
    SAMPLES = 1000000,
    PLAYERS = 1000,
    TRACKS = 4,
    STEP_NS = 16000000
};

// The width of a result's name, as printed and as read back.
#define NAME_WIDTH 32

// The baseline's lines, read whole.
static char s_baseline[16384];

// Keeps the samples' results alive.
static volatile double s_sink;

// The value the baseline records for a name, or 0.
static double Recorded(const char* name)
{
    size_t named = strlen(name);
    for (const char* line = s_baseline; *line != '\0';)
    {
        const char* end = strchr(line, '\n');
        size_t length = end != NULL ? (size_t)(end - line) : strlen(line);
        if (line[0] != '#' && length > named && strncmp(line, name, named) == 0 &&
            line[named] == ' ')
        {
            return strtod(line + named, NULL);
        }
        line += length + (end != NULL ? 1 : 0);
    }
    return 0.0;
}

// Prints a result, and its ratio to the baseline's where it has one.
static void Report(const char* name, double value, const char* unit)
{
    double recorded = Recorded(name);
    if (recorded > 0.0)
    {
        printf("%-*s %12.3f %-6s (%.2fx the baseline)\n", NAME_WIDTH, name, value, unit,
               value / recorded);
    }
    else
    {
        printf("%-*s %12.3f %s\n", NAME_WIDTH, name, value, unit);
    }
}

static double Seconds(void)
{
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

static void Check(mtwnResult status, const char* what)
{
    if (status != mtwn_success)
    {
        fprintf(stderr, "%s failed: %d\n", what, (int)status);
        exit(1);
    }
}

static mtwnContext* MakeContext(uint32_t targets, uint32_t players, uint32_t clips,
                                uint32_t bindings)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.targetCapacity = targets;
    def.playerCapacity = players;
    def.clipCapacity = clips;
    def.bindingCapacity = bindings;
    def.eventCapacity = 1;
    def.clipStorageBytes = clips > 0 ? 1u << 16 : 0;
    mtwnContext* context = NULL;
    Check(mtwnCreateContext(&def, &context), "a context");
    return context;
}

// The transitions' section: their creation, their updates, their bytes.
typedef struct Tweens
{
    double create;
    double update;
    double reported;
    double bytes;
} Tweens;

// Creates the transitions, then updates them, the best of 50.
static void RunTweens(Tweens* best, mtwnTargetId* changed)
{
    mtwnContext* context = MakeContext(TWEENS, TWEENS, 0, 0);
    mtwnEasing quadOut;
    Check(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &quadOut), "quad out");
    mtwnTransitionDef def = mtwnDefaultTransitionDef();
    def.value = (mtwnVec4){100.0f, 0.0f, 0.0f, 0.0f};
    def.duration = 1.0;
    def.easing = quadOut;
    double start = Seconds();
    for (uint32_t i = 0; i < TWEENS; ++i)
    {
        Check(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &def.target), "a target");
        mtwnPlayerId id;
        Check(mtwnStartTransition(context, &def, &id), "a transition");
    }
    double create = Seconds() - start;
    best->create = fmin(best->create, create);
    best->bytes = (double)mtwnContext_GetMemoryBytes(context) / TWEENS;
    uint64_t now = 0;
    Check(mtwnUpdate(context, now, NULL, 0, NULL), "the first update");
    for (int u = 0; u < UPDATES; ++u)
    {
        // Alternately without and with the report, both well inside the
        // second the transitions run.
        bool report = (u & 1) != 0;
        now += STEP_NS / 2;
        uint32_t total = 0;
        start = Seconds();
        Check(mtwnUpdate(context, now, report ? changed : NULL, report ? TWEENS : 0, &total),
              "an update");
        double took = Seconds() - start;
        if (report)
        {
            best->reported = fmin(best->reported, took);
        }
        else
        {
            best->update = fmin(best->update, took);
        }
    }
    mtwnDestroyContext(context);
}

// Nanoseconds per call of an easing, over a million progresses.
static double EaseNs(const mtwnEasing* easing)
{
    double sum = 0.0;
    double start = Seconds();
    for (uint32_t i = 0; i < SAMPLES; ++i)
    {
        sum += mtwnEase(easing, (double)i * (1.0 / SAMPLES), false);
    }
    double took = Seconds() - start;
    s_sink = sum;
    return took * 1e9 / SAMPLES;
}

static double SpringNs(const mtwnSpring* spring)
{
    double sum = 0.0;
    double start = Seconds();
    for (uint32_t i = 0; i < SAMPLES; ++i)
    {
        double offset = 0.0;
        mtwnSampleSpring(spring, 1.0, 0.0, (double)i * (2.0 / SAMPLES), &offset, NULL);
        sum += offset;
    }
    double took = Seconds() - start;
    s_sink = sum;
    return took * 1e9 / SAMPLES;
}

// The curves' samples: the cubic bezier of CSS's ease, an underdamped
// spring, and the slowest of Penner's easings in-out, those of
// arithmetic alone (quad, circ, back, bounce) and those of exponentials
// and sines (sine, expo, elastic).
typedef struct Curves
{
    double bezier;
    double spring;
    double arithmetic;
    double transcendental;
} Curves;

static void RunCurves(Curves* best)
{
    mtwnEasing ease;
    Check(mtwnMakeCssEasing(mtwn_cssEase, &ease), "ease");
    best->bezier = fmin(best->bezier, EaseNs(&ease));
    mtwnSpring spring;
    Check(mtwnMakeSpringFromDuration(0.5, 0.3, &spring), "a spring");
    best->spring = fmin(best->spring, SpringNs(&spring));
    mtwnEasing penner;
    double arithmetic = 0.0;
    Check(mtwnMakePowerEasing(mtwn_easeInOut, 2.0, &penner), "quad");
    arithmetic = fmax(arithmetic, EaseNs(&penner));
    Check(mtwnMakePennerEasing(mtwn_easingCirc, mtwn_easeInOut, &penner), "circ");
    arithmetic = fmax(arithmetic, EaseNs(&penner));
    Check(mtwnMakeBackEasing(mtwn_easeInOut, 1.70158 * 1.525, &penner), "back");
    arithmetic = fmax(arithmetic, EaseNs(&penner));
    Check(mtwnMakePennerEasing(mtwn_easingBounce, mtwn_easeInOut, &penner), "bounce");
    arithmetic = fmax(arithmetic, EaseNs(&penner));
    double transcendental = 0.0;
    Check(mtwnMakePennerEasing(mtwn_easingSine, mtwn_easeInOut, &penner), "sine");
    transcendental = fmax(transcendental, EaseNs(&penner));
    Check(mtwnMakePennerEasing(mtwn_easingExpo, mtwn_easeInOut, &penner), "expo");
    transcendental = fmax(transcendental, EaseNs(&penner));
    Check(mtwnMakeElasticEasing(mtwn_easeInOut, 1.0, 0.45, &penner), "elastic");
    transcendental = fmax(transcendental, EaseNs(&penner));
    best->arithmetic = fmin(best->arithmetic, arithmetic);
    best->transcendental = fmin(best->transcendental, transcendental);
}

// A clip of four numbers' tracks in a timeline repeating every second.
static mtwnClipId MakeClip(mtwnContext* context)
{
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    clipDef.nodeCapacity = TRACKS + 1;
    clipDef.keyframeCapacity = TRACKS * 3;
    clipDef.slotCapacity = TRACKS;
    mtwnClipId clip;
    Check(mtwnCreateClip(context, &clipDef, &clip), "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = 1.0;
    group.timing.iterations = HUGE_VAL;
    mtwnNode loop;
    Check(mtwnClip_AddTimeline(context, clip, &group, &loop), "a timeline");
    mtwnEasing quadOut;
    Check(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &quadOut), "quad out");
    for (uint32_t t = 0; t < TRACKS; ++t)
    {
        mtwnKeyframe keys[3];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 0.5;
        keys[1].value.x = 100.0f + (float)t;
        keys[2].offset = 1.0;
        keys[0].easing = keys[1].easing = keys[2].easing = quadOut;
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.parent = loop;
        track.slot = t;
        track.keyframes = keys;
        track.keyframeCount = 3;
        track.timing.duration = 1.0;
        mtwnNode node;
        Check(mtwnClip_AddTrack(context, clip, &track, &node), "a track");
    }
    Check(mtwnClip_Finish(context, clip), "the clip finished");
    return clip;
}

// A thousand players of the clip, each bound to four targets of its own,
// updated at 16 ms steps: the best of 50.
static void RunClips(double* best)
{
    mtwnContext* context = MakeContext(PLAYERS * TRACKS, PLAYERS, 1, PLAYERS * TRACKS);
    mtwnClipId clip = MakeClip(context);
    mtwnBinding bindings[TRACKS];
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = clip;
    def.bindings = bindings;
    def.bindingCount = TRACKS;
    for (uint32_t p = 0; p < PLAYERS; ++p)
    {
        for (uint32_t t = 0; t < TRACKS; ++t)
        {
            mtwnTargetId target;
            Check(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target), "a target");
            bindings[t] = mtwnBind(t, target);
        }
        mtwnPlayerId id;
        Check(mtwnCreatePlayer(context, &def, &id), "a player");
    }
    uint64_t now = 0;
    Check(mtwnUpdate(context, now, NULL, 0, NULL), "the first update");
    for (int u = 0; u < UPDATES; ++u)
    {
        now += STEP_NS;
        double start = Seconds();
        Check(mtwnUpdate(context, now, NULL, 0, NULL), "an update");
        *best = fmin(*best, Seconds() - start);
    }
    mtwnDestroyContext(context);
}

static void ReadBaseline(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    size_t read = fread(s_baseline, 1, sizeof s_baseline - 1, file);
    s_baseline[read] = '\0';
    fclose(file);
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        ReadBaseline(argv[1]);
    }
    static mtwnTargetId changed[TWEENS];
    Tweens tweens = {HUGE_VAL, HUGE_VAL, HUGE_VAL, 0.0};
    Curves curves = {HUGE_VAL, HUGE_VAL, HUGE_VAL, HUGE_VAL};
    double clips = HUGE_VAL;
    for (int run = 0; run < RUNS; ++run)
    {
        RunTweens(&tweens, changed);
        RunCurves(&curves);
        RunClips(&clips);
    }
    printf("# %d transitions of a number, quad out, 16 ms steps\n", TWEENS);
    printf("# %.1f bytes a transition with its target\n", tweens.bytes);
    Report("create 64,000 transitions", tweens.create * 1e3, "ms");
    Report("update 64,000 transitions", tweens.update * 1e3, "ms");
    Report("update 64,000, changed reported", tweens.reported * 1e3, "ms");
    printf("# one sample each, over %d progresses\n", SAMPLES);
    Report("cubic bezier sample", curves.bezier, "ns");
    Report("spring sample", curves.spring, "ns");
    Report("Penner sample, arithmetic", curves.arithmetic, "ns");
    Report("Penner sample, exp and sin", curves.transcendental, "ns");
    printf("# %d clip players of %d tracks in a repeating timeline\n", PLAYERS, TRACKS);
    Report("update 1,000 clip players", clips * 1e3, "ms");
    return 0;
}
