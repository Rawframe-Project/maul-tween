// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Timings against the library's targets, on the task
// its rivals were measured with: 64,000 transitions of a number from 0 to
// 100 over a second with a quadratic ease out. Creates them all, then
// updates them at 16 ms steps, the best of 50 updates, without and with
// the report of changed targets; prints the bytes each takes with its
// target. Then the cost of one sample of a cubic bezier, a spring, and
// Penner's easings; then 1,000 clip players of four tracks each, in a
// timeline repeating; then one sample each of the values layer's cases.
// Prints each section's counts and bytes, which do
// not depend on the machine, as comment lines, then its named results,
// the best of five runs (the updates' best of 50). Given a file of an
// earlier run's output, it prints each result's ratio to the one there
// as well. With --count and a layout of the clip players
// (step, spread, clips, repeat, churn; -tasks after one runs it in groups
// on the calling thread, -quiet in a context of no records) and a number
// of updates, and the clip's period if not 1 s, or a values case (oklab,
// oklch, slerp, path, track, monotone) and a number of samples, it runs
// only those, for an instruction counter; --compiler prints the compiler
// it was built with.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/easing.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
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
// The clip's period in seconds: 1, or as --count is given, so that a
// repeat can fall on every other frame.
static double s_period = 1.0;

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

// The values layer's cases: a color, red to blue, mixed in
// Oklab and in OkLCh; a rotation, identity to 120 degrees about
// (1, 2, 3), by slerp; a path through eight points at constant speed
// (centripetal Catmull-Rom); a track of a number, 0, 100, 0 at 0, 1/2
// and 1, a quadratic out on each segment; a monotone track of a number,
// 0, 30, 40, 100, 100 at 0, 0.1, 0.4, 0.5 and 1.
enum
{
    VALUE_OKLAB,
    VALUE_OKLCH,
    VALUE_SLERP,
    VALUE_PATH,
    VALUE_TRACK,
    VALUE_MONOTONE,
    VALUE_CASES
};

static const char* const s_valueCases[VALUE_CASES] = {"oklab", "oklch", "slerp",
                                                      "path",  "track", "monotone"};

typedef struct Values
{
    mtwnColorMix colors[2];
    mtwnRotationMix rotation;
    mtwnKeyframe pathKeys[8];
    double pathLengths[8];
    mtwnPath path;
    mtwnKeyframe trackKeys[3];
    mtwnTrack track;
    mtwnKeyframe monotoneKeys[5];
    mtwnTrack monotone;
} Values;

static void MakeValues(Values* v)
{
    memset(v, 0, sizeof *v);
    mtwnColor red = {1.0f, 0.0f, 0.0f, 1.0f};
    mtwnColor blue = {0.0f, 0.0f, 1.0f, 1.0f};
    Check(mtwnMakeColorMix(red, blue, mtwn_colorOklab, mtwn_hueShorter, &v->colors[0]), "Oklab");
    Check(mtwnMakeColorMix(red, blue, mtwn_colorOklch, mtwn_hueShorter, &v->colors[1]), "OkLCh");
    double axis = sqrt(14.0);
    double half = sin(acos(-1.0) / 3.0);
    mtwnQuat to = {(float)(1.0 / axis * half), (float)(2.0 / axis * half),
                   (float)(3.0 / axis * half), (float)cos(acos(-1.0) / 3.0)};
    Check(mtwnMakeRotationMix((mtwnQuat){0.0f, 0.0f, 0.0f, 1.0f}, to, mtwn_rotationShortest,
                              &v->rotation),
          "a rotation");
    static const float points[8][2] = {{0, 0}, {1, 2}, {3, 3}, {4, 1},
                                       {6, 0}, {7, 2}, {9, 3}, {10, 0}};
    for (int k = 0; k < 8; ++k)
    {
        v->pathKeys[k].value = (mtwnVec4){points[k][0], points[k][1], 0.0f, 0.0f};
        v->pathKeys[k].offset = k == 7 ? 1.0 : k / 7.0;
        v->pathKeys[k].easing = mtwnMakeLinearEasing();
        v->pathKeys[k].kind = mtwn_segmentCatmullRom;
    }
    Check(mtwnMakePath(v->pathKeys, 8, 2, 0.5, v->pathLengths, &v->path), "a path");
    mtwnEasing quadOut;
    Check(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &quadOut), "quad out");
    v->trackKeys[1].value.x = 100.0f;
    v->trackKeys[1].offset = 0.5;
    v->trackKeys[2].offset = 1.0;
    for (int k = 0; k < 3; ++k)
    {
        v->trackKeys[k].easing = quadOut;
    }
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueScalar);
    Check(mtwnMakeTrack(&def, v->trackKeys, 3, NULL, NULL, &v->track), "a track");
    static const float rising[5] = {0.0f, 30.0f, 40.0f, 100.0f, 100.0f};
    static const double at[5] = {0.0, 0.1, 0.4, 0.5, 1.0};
    for (int k = 0; k < 5; ++k)
    {
        v->monotoneKeys[k].value.x = rising[k];
        v->monotoneKeys[k].offset = at[k];
        v->monotoneKeys[k].easing = mtwnMakeLinearEasing();
        v->monotoneKeys[k].kind = mtwn_segmentMonotone;
    }
    Check(mtwnMakeTrack(&def, v->monotoneKeys, 5, NULL, NULL, &v->monotone), "a monotone track");
}

// Samples a case at n progresses, (i + 0.5) / n: the sum of the results.
static double SampleValues(const Values* v, int which, uint32_t n)
{
    double sum = 0.0;
    for (uint32_t i = 0; i < n; ++i)
    {
        double p = ((double)i + 0.5) / (double)n;
        if (which <= VALUE_OKLCH)
        {
            mtwnColor c = {0};
            if (mtwnMixColor(&v->colors[which], p, &c) == mtwn_success)
            {
                sum += (double)c.red + (double)c.green + (double)c.blue;
            }
        }
        else if (which == VALUE_SLERP)
        {
            mtwnQuat q = {0};
            if (mtwnMixRotation(&v->rotation, p, &q) == mtwn_success)
            {
                sum += (double)q.x + (double)q.w;
            }
        }
        else if (which == VALUE_PATH)
        {
            double at[4] = {0};
            sum += mtwnSamplePath(&v->path, p, at, NULL) == mtwn_success ? at[0] + at[1] : 0.0;
        }
        else
        {
            mtwnVec4 at = {0};
            sum += mtwnSampleTrack(which == VALUE_TRACK ? &v->track : &v->monotone, p, &at) ==
                           mtwn_success
                       ? (double)at.x
                       : 0.0;
        }
    }
    return sum;
}

// Nanoseconds a sample of each case, over a million progresses.
static void RunValues(double best[VALUE_CASES])
{
    static Values values;
    MakeValues(&values);
    for (int which = 0; which < VALUE_CASES; ++which)
    {
        double start = Seconds();
        s_sink = SampleValues(&values, which, SAMPLES);
        best[which] = fmin(best[which], (Seconds() - start) * 1e9 / SAMPLES);
    }
}

// The def of the clip MakeClip makes.
static mtwnClipDef ClipDef(void)
{
    mtwnClipDef def = mtwnDefaultClipDef();
    def.nodeCapacity = TRACKS + 1;
    def.keyframeCapacity = TRACKS * 3;
    def.slotCapacity = TRACKS;
    return def;
}

// The clip storage one of them takes in a context.
static uint64_t ClipBytes(mtwnContext* context)
{
    mtwnClipDef def = ClipDef();
    uint64_t bytes = 0;
    Check(mtwnContext_GetClipBytes(context, &def, &bytes), "a clip's bytes");
    return bytes;
}

// Whether a context runs the clips' players in groups on a task system,
// here the calling thread, as --count's layouts ending in -tasks ask: what
// the hooks cost on one worker.
static bool s_tasks = false;

// Whether a context makes no records (an event capacity of 0), as
// --count's layouts ending in -quiet ask: what a host that
// reads no events pays.
static bool s_quiet = false;

static void* RunAtOnce(mtwnTaskFunction* task, int32_t itemCount, int32_t minRange,
                       void* taskContext, void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext);
    return NULL;
}

static void WaitForNothing(void* userTask, void* userContext)
{
    (void)userTask;
    (void)userContext;
}

static mtwnContext* MakeContext(uint32_t targets, uint32_t players, uint32_t clips,
                                uint32_t bindings)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.enqueueTask = s_tasks ? RunAtOnce : NULL;
    def.finishTask = s_tasks ? WaitForNothing : NULL;
    def.targetCapacity = targets;
    def.playerCapacity = players;
    def.clipCapacity = clips;
    def.bindingCapacity = bindings;
    def.eventCapacity = s_quiet ? 0u : 1u;
    def.clipStorageBytes = 0;
    mtwnContext* context = NULL;
    Check(mtwnCreateContext(&def, &context), "a context");
    if (clips > 0)
    {
        // Grown to hold its clips, as a host sizes its clip storage.
        def.clipStorageBytes = (uint64_t)clips * ClipBytes(context);
        Check(mtwnContext_Grow(&context, &def), "a context grown for its clips");
    }
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
    Check(mtwnContext_Update(context, now, NULL, 0, NULL), "the first update");
    for (int u = 0; u < UPDATES; ++u)
    {
        // Alternately without and with the report, both well inside the
        // second the transitions run.
        bool report = (u & 1) != 0;
        now += STEP_NS / 2;
        uint32_t total = 0;
        start = Seconds();
        Check(
            mtwnContext_Update(context, now, report ? changed : NULL, report ? TWEENS : 0, &total),
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
        double eased = 0.0;
        mtwnResult sampled = mtwnSampleEasing(easing, (double)i * (1.0 / SAMPLES), false, &eased);
        sum += sampled == mtwn_success ? eased : (double)NAN;
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
        mtwnResult sampled =
            mtwnSampleSpring(spring, 1.0, 0.0, (double)i * (2.0 / SAMPLES), &offset, NULL);
        sum += sampled == mtwn_success ? offset : (double)NAN;
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
    mtwnClipDef clipDef = ClipDef();
    mtwnClipId clip;
    Check(mtwnCreateClip(context, &clipDef, &clip), "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = s_period;
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
        track.timing.duration = s_period;
        mtwnNode node;
        Check(mtwnClip_AddTrack(context, clip, &track, &node), "a track");
    }
    Check(mtwnClip_Seal(context, clip), "the clip sealed");
    return clip;
}

// How the clip players are laid out: in step; spread, player p sought to
// p ms; each on a clip of its own, in step, so none takes another's work;
// and so, sought to 0.99 s, so that the first update
// after the start crosses every player's repeat.
typedef enum Layout
{
    LAYOUT_IN_STEP,
    LAYOUT_SPREAD,
    LAYOUT_OWN_CLIPS,
    LAYOUT_REPEAT,
    LAYOUT_CHURN
} Layout;

static const char* const s_layouts[] = {"step", "spread", "clips", "repeat", "churn"};

// The players' ids, clips and bindings, for the churn: before each
// update a tenth of the players, oldest first, are destroyed and made
// again on their clips and targets, as a host whose players come and go
// each frame.
enum
{
    CHURN = PLAYERS / 10
};
static mtwnPlayerId s_players[PLAYERS];
static mtwnClipId s_clips[PLAYERS];
static mtwnBinding s_bindings[PLAYERS][TRACKS];

// A thousand players of the clip, each bound to four targets of its own,
// laid out as asked, and the update that starts them.
static mtwnContext* MakePlayers(Layout layout)
{
    uint32_t clips = layout >= LAYOUT_OWN_CLIPS ? PLAYERS : 1;
    // The churn's made players come before their old ones are freed.
    uint32_t players = layout == LAYOUT_CHURN ? PLAYERS + CHURN : PLAYERS;
    mtwnContext* context = MakeContext(PLAYERS * TRACKS, players, clips, players * TRACKS);
    mtwnBinding bindings[TRACKS];
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = MakeClip(context);
    def.bindings = bindings;
    def.bindingCount = TRACKS;
    for (uint32_t p = 0; p < PLAYERS; ++p)
    {
        if (p > 0 && clips > 1)
        {
            def.clip = MakeClip(context);
        }
        for (uint32_t t = 0; t < TRACKS; ++t)
        {
            mtwnTargetId target;
            Check(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target), "a target");
            bindings[t] = mtwnMakeBinding(t, target);
        }
        mtwnPlayerId id;
        Check(mtwnCreatePlayer(context, &def, &id), "a player");
        s_players[p] = id;
        s_clips[p] = def.clip;
        memcpy(s_bindings[p], bindings, sizeof bindings);
        if (layout == LAYOUT_SPREAD || layout == LAYOUT_REPEAT)
        {
            double at = layout == LAYOUT_SPREAD ? (double)p * 0.001 : 0.99;
            Check(mtwnPlayer_Seek(context, id, at), "a seek");
        }
    }
    Check(mtwnContext_Update(context, 0, NULL, 0, NULL), "the first update");
    return context;
}

// The players in step, updated at 16 ms steps: the best of 50.
static void RunClips(double* best)
{
    mtwnContext* context = MakePlayers(LAYOUT_IN_STEP);
    uint64_t now = 0;
    for (int u = 0; u < UPDATES; ++u)
    {
        now += STEP_NS;
        double start = Seconds();
        Check(mtwnContext_Update(context, now, NULL, 0, NULL), "an update");
        *best = fmin(*best, Seconds() - start);
    }
    mtwnDestroyContext(context);
}

// A player made again on its clip and targets, and the old one freed.
static void Churn(mtwnContext* context, uint32_t p)
{
    mtwnPlayerDef def = mtwnDefaultPlayerDef();
    def.clip = s_clips[p];
    def.bindings = s_bindings[p];
    def.bindingCount = TRACKS;
    mtwnPlayerId id;
    Check(mtwnCreatePlayer(context, &def, &id), "a player made again");
    Check(mtwnDestroyPlayer(context, s_players[p]), "a player freed");
    s_players[p] = id;
}

// For an instruction counter: a layout's players and then count updates
// at 16 ms steps, or a values case's count samples, printing nothing, so
// that two counts' difference is their updates' or samples'
// (`perf stat -e instructions:u`).
static int Count(const char* name, int count)
{
    size_t length = strlen(name);
    size_t suffix = strlen("-tasks");
    s_tasks = length > suffix && strcmp(name + length - suffix, "-tasks") == 0;
    s_quiet = length > suffix && strcmp(name + length - suffix, "-quiet") == 0;
    size_t layoutLength = s_tasks || s_quiet ? length - suffix : length;
    for (int layout = LAYOUT_IN_STEP; layout <= LAYOUT_CHURN; ++layout)
    {
        if (strlen(s_layouts[layout]) == layoutLength &&
            strncmp(name, s_layouts[layout], layoutLength) == 0)
        {
            mtwnContext* context = MakePlayers((Layout)layout);
            uint64_t now = 0;
            for (int u = 0; u < count; ++u)
            {
                for (uint32_t k = 0; layout == LAYOUT_CHURN && k < CHURN; ++k)
                {
                    Churn(context, ((uint32_t)u * CHURN + k) % PLAYERS);
                }
                now += STEP_NS;
                Check(mtwnContext_Update(context, now, NULL, 0, NULL), "an update");
            }
            mtwnDestroyContext(context);
            return 0;
        }
    }
    for (int which = 0; which < VALUE_CASES; ++which)
    {
        if (strcmp(name, s_valueCases[which]) == 0)
        {
            static Values values;
            MakeValues(&values);
            s_sink = SampleValues(&values, which, (uint32_t)count);
            return 0;
        }
    }
    fprintf(
        stderr,
        "--count takes step, spread, clips, repeat or churn, each with -tasks, -quiet or neither, "
        "oklab, "
        "oklch, slerp, path, track or monotone\n");
    return 1;
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
    if (argc == 5 && strcmp(argv[1], "--count") == 0)
    {
        s_period = atof(argv[4]);
        return Count(argv[2], atoi(argv[3]));
    }
    if (argc == 4 && strcmp(argv[1], "--count") == 0)
    {
        return Count(argv[2], atoi(argv[3]));
    }
    if (argc == 2 && strcmp(argv[1], "--compiler") == 0)
    {
        // Named beside instruction counts, which differ between compilers.
#if defined(__clang__)
        printf("Clang %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
        printf("GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
        printf("unknown\n");
#endif
        return 0;
    }
    if (argc > 1)
    {
        ReadBaseline(argv[1]);
    }
    static mtwnTargetId changed[TWEENS];
    Tweens tweens = {HUGE_VAL, HUGE_VAL, HUGE_VAL, 0.0};
    Curves curves = {HUGE_VAL, HUGE_VAL, HUGE_VAL, HUGE_VAL};
    double clips = HUGE_VAL;
    double values[VALUE_CASES] = {HUGE_VAL, HUGE_VAL, HUGE_VAL, HUGE_VAL, HUGE_VAL, HUGE_VAL};
    for (int run = 0; run < RUNS; ++run)
    {
        RunTweens(&tweens, changed);
        RunCurves(&curves);
        RunClips(&clips);
        RunValues(values);
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
    printf("# one sample each of the values layer, over %d progresses\n", SAMPLES);
    Report("color mix sample, Oklab", values[VALUE_OKLAB], "ns");
    Report("color mix sample, OkLCh", values[VALUE_OKLCH], "ns");
    Report("rotation slerp sample", values[VALUE_SLERP], "ns");
    Report("path sample, constant speed", values[VALUE_PATH], "ns");
    Report("track sample, three keyframes", values[VALUE_TRACK], "ns");
    Report("track sample, monotone, five keyframes", values[VALUE_MONOTONE], "ns");
    return 0;
}
