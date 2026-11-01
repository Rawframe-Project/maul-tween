// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Players in step share their work: twin contexts run
// one script, one with the sharing and one with it off, and after every
// update hold the same bits: every target's base, value and velocity, the
// changed targets in order, every record and the dropped count. The
// crowd is the bench's, in step, spread a millisecond apart, and mixed:
// weights, additive bindings, a slot left out, other rates and
// directions, controls on runs of players, reduced motion. A clip
// destroyed and another made in its slot must not take the first's
// results.

#include "test_harness.h"
#include "test_hooks.h"

#include "maul-tween/clip.h"
#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"

#include <math.h>
#include <string.h>

enum
{
    s_players = 160,
    s_slots = 6,
    s_frames = 240,
};

// The tracks' kinds by slot: the bench's four numbers, then a nested
// timeline's vector and color.
static const mtwnValueKind s_kinds[s_slots] = {mtwn_valueScalar, mtwn_valueScalar, mtwn_valueScalar,
                                               mtwn_valueScalar, mtwn_valueVec2,   mtwn_valueColor};

// Every target's value before any player writes it.
static const mtwnVec4 s_start = {0.5f, 0.25f, 0.75f, 0.5f};

typedef struct Twin
{
    mtwnContext* context;
    mtwnClipId clip;
    mtwnTargetId targets[s_players][s_slots];
    mtwnPlayerId players[s_players];
} Twin;

// A task system that runs every range at once.
static void* RunNow(mtwnTaskFunction* task, int32_t itemCount, int32_t minRange, void* taskContext,
                    void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext);
    return NULL;
}

static void Unreached(void* userTask, void* userContext)
{
    (void)userTask;
    (void)userContext;
    CHECK(false, "no wait for ranges run at once");
}

// NaN for progress from 0.3 to 0.7, a host function breaking its
// contract: the library takes the progress there.
static double Hole(void* context, double progress)
{
    (void)context;
    return progress > 0.3 && progress < 0.7 ? (double)NAN : progress;
}

// Whether MakeClip gives the first track's timing, the second track's
// keyframes and the nested vector's keyframes the host easing above.
static bool s_hostNan;

// The bench's clip, four numbers in a timeline repeating every second,
// with marks in it and a nested timeline of a vector and a color, so the
// players make records; scale sets the keyframes' values, markShift
// moves the marks.
static mtwnClipId MakeClip(mtwnContext* context, float scale, double markShift)
{
    mtwnClipDef def = mtwnDefaultClipDef();
    def.nodeCapacity = 16;
    def.keyframeCapacity = 32;
    def.slotCapacity = s_slots;
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &def, &clip) == mtwn_success, "a clip");
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = 1.0;
    group.timing.iterations = HUGE_VAL;
    mtwnNode loop;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &loop) == mtwn_success, "a timeline");
    mtwnEasing quadOut;
    CHECK(mtwnMakePowerEasing(mtwn_easeOut, 2.0, &quadOut) == mtwn_success, "quad out");
    for (uint32_t t = 0; t < 4; ++t)
    {
        mtwnKeyframe keys[3];
        memset(keys, 0, sizeof keys);
        keys[1].offset = 0.5;
        keys[1].value.x = scale * (100.0f + (float)t);
        keys[2].offset = 1.0;
        keys[0].easing = keys[1].easing = keys[2].easing = quadOut;
        if (s_hostNan && t == 1)
        {
            CHECK(mtwnMakeHostEasing(mtwn_easeOut, Hole, NULL, &keys[0].easing) == mtwn_success,
                  "a host easing");
            keys[1].easing = keys[0].easing;
        }
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.parent = loop;
        track.slot = t;
        track.keyframes = keys;
        track.keyframeCount = 3;
        track.timing.duration = 1.0;
        if (s_hostNan && t == 0)
        {
            CHECK(mtwnMakeHostEasing(mtwn_easeIn, Hole, NULL, &track.timing.easing) == mtwn_success,
                  "a host easing");
        }
        mtwnNode node;
        CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
    }
    for (uint32_t m = 0; m < 3; ++m)
    {
        mtwnPosition at = {0};
        at.kind = mtwn_positionAbsolute;
        at.offset = 0.2 + 0.3 * (double)m + markShift;
        mtwnNode mark;
        CHECK(mtwnClip_AddMark(context, clip, loop, at, m + 1, &mark) == mtwn_success, "a mark");
    }
    mtwnClipTimelineDef inner = mtwnDefaultClipTimelineDef();
    inner.parent = loop;
    inner.timing.iterations = 3.0;
    inner.timing.direction = mtwn_directionAlternate;
    mtwnNode nested;
    CHECK(mtwnClip_AddTimeline(context, clip, &inner, &nested) == mtwn_success, "a nested one");
    for (uint32_t s = 4; s < s_slots; ++s)
    {
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[0].value = (mtwnVec4){0.25f, 0.5f, 0.75f, 1.0f};
        keys[1].offset = 1.0;
        keys[1].value = (mtwnVec4){scale, 0.125f, 0.5f, 0.5f};
        keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
        if (s_hostNan && s == 4)
        {
            CHECK(mtwnMakeHostEasing(mtwn_easeInOut, Hole, NULL, &keys[0].easing) == mtwn_success,
                  "a host easing");
        }
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.parent = nested;
        track.slot = s;
        track.track.kind = s_kinds[s];
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 0.3;
        track.timing.fill = mtwn_fillBoth;
        mtwnNode node;
        CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
    }
    CHECK(mtwnClip_Seal(context, clip) == mtwn_success, "the clip sealed");
    return clip;
}

// Player p of a crowd: mixed, its bindings' weights and composites and
// its rate vary with p, and one in seven leaves a slot out.
static void MakePlayer(Twin* twin, uint32_t p, bool mixed, double seek)
{
    mtwnBinding bindings[s_slots];
    uint32_t bound = 0;
    for (uint32_t s = 0; s < s_slots; ++s)
    {
        if (mixed && p % 7 == 3 && s == 2)
        {
            continue;
        }
        mtwnBinding binding = mtwnMakeBinding(s, twin->targets[p][s]);
        binding.weight = mixed && p % 3 == 1 ? 0.5f : 1.0f;
        binding.composite = mixed && p % 5 == 2 ? mtwn_compositeAdd : mtwn_compositeReplace;
        bindings[bound++] = binding;
    }
    mtwnClipPlayerDef def = mtwnDefaultClipPlayerDef();
    def.clip = twin->clip;
    def.bindings = bindings;
    def.bindingCount = bound;
    def.keep = true;
    def.weight = mixed && p % 4 == 0 ? 0.75f : 1.0f;
    def.rate = !mixed ? 1.0 : (p % 11 == 5 ? -1.0 : (p % 13 == 6 ? 2.0 : 1.0));
    CHECK(mtwnCreateClipPlayer(twin->context, &def, &twin->players[p]) == mtwn_success, "a player");
    // One playing backwards starts within the endless clip, which has no
    // end to play back from.
    seek = def.rate < 0.0 && seek == 0.0 ? 0.5 : seek;
    if (seek != 0.0)
    {
        CHECK(mtwnClipPlayer_Seek(twin->context, twin->players[p], seek) == mtwn_success, "a seek");
    }
}

static void MakeTwin(Twin* twin, bool on, bool tasks, uint32_t eventCapacity)
{
    mtwnContextDef def = mtwnDefaultContextDef();
    def.targetCapacity = s_players * s_slots;
    def.playerCapacity = s_players;
    def.bindingCapacity = s_players * s_slots;
    def.eventCapacity = eventCapacity;
    def.enqueueTask = tasks ? RunNow : NULL;
    def.finishTask = tasks ? Unreached : NULL;
    // Off, every work's sharing holds nothing: nothing kept, so nothing
    // taken.
    def.sharedNodeCapacity = on ? def.sharedNodeCapacity : 0;
    def.sharedRecordCapacity = on ? def.sharedRecordCapacity : 0;
    CHECK(mtwnCreateContext(&def, &twin->context) == mtwn_success, "a context");
    for (uint32_t p = 0; p < s_players; ++p)
    {
        for (uint32_t s = 0; s < s_slots; ++s)
        {
            // Not zero, so adding over it differs from replacing it.
            CHECK(mtwnCreateTarget(twin->context, s_kinds[s], s_start, &twin->targets[p][s]) ==
                      mtwn_success,
                  "a target");
        }
    }
}

static bool Same(mtwnVec4 a, mtwnVec4 b)
{
    return memcmp(&a, &b, sizeof a) == 0;
}

static bool SameRecord(const mtwnEvent* a, const mtwnEvent* b)
{
    return a->hostTime == b->hostTime && a->clipTime == b->clipTime &&
           (a->firstIteration == b->firstIteration ||
            (isnan(a->firstIteration) && isnan(b->firstIteration))) &&
           (a->lastIteration == b->lastIteration ||
            (isnan(a->lastIteration) && isnan(b->lastIteration))) &&
           a->count == b->count && a->player.index1 == b->player.index1 &&
           a->player.generation == b->player.generation && a->node == b->node && a->key == b->key &&
           a->kind == b->kind && a->backwards == b->backwards;
}

// Updates the twins to now and returns their differences; records counts
// the records read.
static uint32_t Step(Twin twins[2], uint64_t now, uint32_t* records)
{
    static mtwnTargetId changed[2][s_players * s_slots];
    uint32_t totals[2] = {0, 0};
    for (int k = 0; k < 2; ++k)
    {
        CHECK(mtwnContext_Update(twins[k].context, now, changed[k], s_players * s_slots,
                                 &totals[k]) == mtwn_success,
              "an update");
    }
    uint32_t differences = totals[0] == totals[1] ? 0u : 1u;
    for (uint32_t i = 0; i < totals[0] && i < totals[1] && i < s_players * s_slots; ++i)
    {
        differences += changed[0][i].index1 == changed[1][i].index1 ? 0u : 1u;
    }
    for (uint32_t p = 0; p < s_players; ++p)
    {
        for (uint32_t s = 0; s < s_slots; ++s)
        {
            mtwnTargetState x;
            mtwnTargetState y;
            CHECK(mtwnTarget_GetState(twins[0].context, twins[0].targets[p][s], &x) == mtwn_success,
                  "a state");
            CHECK(mtwnTarget_GetState(twins[1].context, twins[1].targets[p][s], &y) == mtwn_success,
                  "a state");
            differences +=
                Same(x.base, y.base) && Same(x.value, y.value) && Same(x.velocity, y.velocity) ? 0u
                                                                                               : 1u;
        }
    }
    static mtwnEvent read[2][4096];
    uint32_t n[2] = {0, 0};
    uint64_t dropped[2] = {0, 0};
    for (int k = 0; k < 2; ++k)
    {
        CHECK(mtwnContext_ReadEvents(twins[k].context, read[k], 4096, &n[k], &dropped[k]) ==
                  mtwn_success,
              "the records");
    }
    differences += n[0] == n[1] && dropped[0] == dropped[1] ? 0u : 1u;
    for (uint32_t i = 0; i < n[0] && i < n[1]; ++i)
    {
        differences += SameRecord(&read[0][i], &read[1][i]) ? 0u : 1u;
    }
    *records += n[0];
    return differences;
}

// Controls on a run of ten players at once, the same on both twins, so
// they leave step and come back to it together. One a state refuses (a
// reverse to the endless clip's end) is refused on both: those applied
// are counted.
static uint32_t s_applied;

static void Control(Twin* twin, uint32_t frame)
{
    uint32_t first = (frame / 30u * 37u) % (s_players - 10u);
    for (uint32_t p = first; p < first + 10u; ++p)
    {
        mtwnContext* context = twin->context;
        mtwnPlayerId player = twin->players[p];
        mtwnResult result = mtwn_success;
        switch (frame / 30u % 5u)
        {
        case 0:
            result = mtwnClipPlayer_Seek(context, player, 0.375);
            break;
        case 1:
            result = mtwnClipPlayer_Pause(context, player);
            break;
        case 2:
            result = mtwnClipPlayer_Reverse(context, player);
            break;
        case 3:
            result = mtwnClipPlayer_SetRate(context, player, 0.5);
            break;
        default:
            result = mtwnClipPlayer_Play(context, player);
            break;
        }
        s_applied += result == mtwn_success ? 1u : 0u;
    }
}

typedef struct Seen
{
    uint32_t differences;
    uint32_t records;
    // Updates after which the sharing held a kept sample and a kept sweep,
    // and after which it had noted any key at all.
    uint32_t keptSamples;
    uint32_t keptSweeps;
    uint32_t noted;
} Seen;

// The crowd on twins: spread is the seconds between players' starts.
static Seen Crowd(double spread, bool mixed, bool tasks, uint32_t eventCapacity)
{
    Twin twins[2];
    memset(twins, 0, sizeof twins);
    for (int k = 0; k < 2; ++k)
    {
        MakeTwin(&twins[k], k == 1, tasks, eventCapacity);
        twins[k].clip = MakeClip(twins[k].context, 1.0f, 0.0);
        for (uint32_t p = 0; p < s_players; ++p)
        {
            MakePlayer(&twins[k], p, mixed, (double)p * spread);
        }
    }
    Seen seen = {0, 0, 0, 0, 0};
    uint64_t now = 0;
    for (uint32_t f = 0; f < s_frames; ++f)
    {
        for (int k = 0; k < 2 && mixed; ++k)
        {
            if (f % 30u == 29u)
            {
                Control(&twins[k], f);
            }
            if (f == 120u || f == 128u)
            {
                mtwnContext_EnableReducedMotion(twins[k].context, f == 120u);
                CHECK(mtwnContext_IsReducedMotionEnabled(twins[k].context) == (f == 120u),
                      "reduced motion");
            }
        }
        seen.differences += Step(twins, now, &seen.records);
        mtwnTestSharing sharing = mtwnTestGetSharing(twins[1].context);
        seen.keptSamples += sharing.sampleKept ? 1u : 0u;
        seen.keptSweeps += sharing.sweepKept ? 1u : 0u;
        seen.noted += sharing.noted ? 1u : 0u;
        now += 16000000u;
    }
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
    return seen;
}

static void TestInStep(void)
{
    for (int tasks = 0; tasks < 2; ++tasks)
    {
        Seen seen = Crowd(0.0, false, tasks == 1, 4096u);
        CHECK(seen.differences == 0, "the crowd in step: the same bits with the sharing off");
        CHECK(seen.records > 1000, "the crowd in step makes records");
        CHECK(seen.keptSamples > 200 && seen.keptSweeps > 100,
              "the crowd's results and records kept for the rest");
    }
}

static void TestSpread(void)
{
    for (int tasks = 0; tasks < 2; ++tasks)
    {
        Seen seen = Crowd(0.001, false, tasks == 1, 4096u);
        CHECK(seen.differences == 0, "the crowd a millisecond apart: the same bits");
        CHECK(seen.records > 1000, "the spread crowd makes records");
        CHECK(seen.noted == 0, "players out of step note no key");
    }
}

static void TestMixed(void)
{
    s_applied = 0;
    // A queue that holds every record, and one that drops most.
    for (uint32_t capacity = 4096u; capacity >= 8u; capacity /= 512u)
    {
        for (int tasks = 0; tasks < 2; ++tasks)
        {
            Seen seen = Crowd(0.0, true, tasks == 1, capacity);
            CHECK(seen.differences == 0, "the mixed crowd: the same bits with the sharing off");
            CHECK(seen.keptSamples > 100 && seen.keptSweeps > 50, "the mixed crowd shares");
        }
    }
    CHECK(s_applied > 600, "the controls applied");
}

// A host easing's NaN on a track's timing and keyframes, in the crowd on
// four contexts: with the sharing on and off, serially and on tasks. Each
// player's sample that met the NaN counts one misuse, whether it worked
// the sample out or took it from the player in step before it, so all
// four count the same and hold the same bits, all finite.
static void TestHostNan(void)
{
    s_hostNan = true;
    Twin twins[4];
    memset(twins, 0, sizeof twins);
    for (int k = 0; k < 4; ++k)
    {
        MakeTwin(&twins[k], k % 2 == 1, k >= 2, 4096u);
        twins[k].clip = MakeClip(twins[k].context, 1.0f, 0.0);
        for (uint32_t p = 0; p < s_players; ++p)
        {
            MakePlayer(&twins[k], p, true, 0.0);
        }
    }
    s_hostNan = false;
    uint32_t differences = 0;
    uint32_t infinite = 0;
    uint32_t kept = 0;
    uint64_t now = 0;
    for (uint32_t f = 0; f < 120; ++f, now += 16000000u)
    {
        for (int k = 0; k < 4; ++k)
        {
            uint32_t total = 0;
            CHECK(mtwnContext_Update(twins[k].context, now, NULL, 0, &total) == mtwn_success,
                  "an update");
        }
        kept += mtwnTestGetSharing(twins[1].context).sampleKept ? 1u : 0u;
        for (int k = 1; k < 4; ++k)
        {
            differences +=
                mtwnContext_GetMisuse(twins[k].context) == mtwnContext_GetMisuse(twins[0].context)
                    ? 0u
                    : 1u;
        }
        for (uint32_t p = 0; p < s_players; ++p)
        {
            for (uint32_t s = 0; s < s_slots; ++s)
            {
                mtwnTargetState x;
                CHECK(mtwnTarget_GetState(twins[0].context, twins[0].targets[p][s], &x) ==
                          mtwn_success,
                      "a state");
                const float* v = &x.value.x;
                const float* w = &x.velocity.x;
                for (int c = 0; c < 4; ++c)
                {
                    infinite += isfinite(v[c]) && isfinite(w[c]) ? 0u : 1u;
                }
                for (int k = 1; k < 4; ++k)
                {
                    mtwnTargetState y;
                    CHECK(mtwnTarget_GetState(twins[k].context, twins[k].targets[p][s], &y) ==
                              mtwn_success,
                          "a state");
                    differences += Same(x.value, y.value) && Same(x.velocity, y.velocity) ? 0u : 1u;
                }
            }
        }
    }
    uint64_t misuse = mtwnContext_GetMisuse(twins[0].context);
    CHECK(differences == 0, "a host's NaN: the same bits and misuse count with the sharing on "
                            "and off, serially and on tasks");
    CHECK(infinite == 0, "every value and velocity finite");
    CHECK(kept > 50, "the players in step share their samples");
    CHECK(misuse > 1000, "each sample that met the NaN counted");
    for (int k = 0; k < 4; ++k)
    {
        mtwnDestroyContext(twins[k].context);
    }
}

// Held players of a clip at a time, then the clip destroyed and another
// made in its slot, its players held at the same time: the first of
// them is near the last of the old clip's and must not take its results.
static void TestReusedSlot(void)
{
    Twin twins[2];
    memset(twins, 0, sizeof twins);
    uint32_t differences = 0;
    uint32_t records = 0;
    uint32_t oldIndex = 0;
    for (int k = 0; k < 2; ++k)
    {
        MakeTwin(&twins[k], k == 1, false, 4096u);
        twins[k].clip = MakeClip(twins[k].context, 1.0f, 0.0);
        for (uint32_t p = 0; p < 20; ++p)
        {
            MakePlayer(&twins[k], p, false, 0.375);
            CHECK(mtwnClipPlayer_Pause(twins[k].context, twins[k].players[p]) == mtwn_success,
                  "a pause");
        }
        oldIndex = twins[k].clip.index1;
    }
    uint64_t now = 0;
    for (uint32_t f = 0; f < 4; ++f, now += 16000000u)
    {
        differences += Step(twins, now, &records);
    }
    for (int k = 0; k < 2; ++k)
    {
        for (uint32_t p = 0; p < 20; ++p)
        {
            CHECK(mtwnDestroyPlayer(twins[k].context, twins[k].players[p]) == mtwn_success,
                  "a player destroyed");
        }
        CHECK(mtwnDestroyClip(twins[k].context, twins[k].clip) == mtwn_success,
              "the clip destroyed");
        twins[k].clip = MakeClip(twins[k].context, -3.0f, 0.0);
        CHECK(twins[k].clip.index1 == oldIndex, "the new clip in the old one's slot");
        for (uint32_t p = 0; p < 20; ++p)
        {
            MakePlayer(&twins[k], p, false, 0.375);
            CHECK(mtwnClipPlayer_Pause(twins[k].context, twins[k].players[p]) == mtwn_success,
                  "a pause");
        }
    }
    for (uint32_t f = 0; f < 4; ++f, now += 16000000u)
    {
        differences += Step(twins, now, &records);
    }
    CHECK(differences == 0, "a clip in a reused slot takes nothing of the old one's");
    CHECK(mtwnTestGetSharing(twins[1].context).sampleKept, "the new clip's players share");
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
}

// Records of both twins since the last read: the differences; count
// adds the records read.
static uint32_t CompareRecords(Twin twins[2], uint32_t* count)
{
    static mtwnEvent read[2][4096];
    uint32_t n[2] = {0, 0};
    uint64_t dropped[2] = {0, 0};
    for (int k = 0; k < 2; ++k)
    {
        CHECK(mtwnContext_ReadEvents(twins[k].context, read[k], 4096, &n[k], &dropped[k]) ==
                  mtwn_success,
              "the records");
    }
    uint32_t differences = n[0] == n[1] && dropped[0] == dropped[1] ? 0u : 1u;
    for (uint32_t i = 0; i < n[0] && i < n[1]; ++i)
    {
        differences += SameRecord(&read[0][i], &read[1][i]) ? 0u : 1u;
    }
    *count += n[0];
    return differences;
}

// A sweep's inputs, set on a player before it sweeps: its swept time (NaN
// for none) and way, its own rate, and the call's.
typedef mtwnTestSweepInputs SweepCase;

// Player p of each twin sweeps: the first twin's alone, the second's as
// one near the last, which takes the records last kept if its key is
// theirs.
static void SweepBoth(Twin twins[2], uint32_t p, const SweepCase* c)
{
    for (int k = 0; k < 2; ++k)
    {
        mtwnTestSweep(twins[k].context, twins[k].players[p], c, k == 1);
    }
}

// Each input of the sweep's key apart from the last kept: the player
// works its records out, as one alone does.
static void TestSweepKeys(void)
{
    Twin twins[2];
    memset(twins, 0, sizeof twins);
    for (int k = 0; k < 2; ++k)
    {
        MakeTwin(&twins[k], true, false, 4096u);
        twins[k].clip = MakeClip(twins[k].context, 1.0f, 0.0);
        for (uint32_t p = 0; p < 4; ++p)
        {
            MakePlayer(&twins[k], p, false, 0.0);
        }
    }
    const SweepCase base = {0.1, 1, 1.0, 0.6, 1.0, 1.0, 1.5};
    // Each case: the sweep two players in step make, and the one apart.
    enum
    {
        s_cases = 12
    };
    SweepCase first[s_cases];
    SweepCase apart[s_cases];
    for (uint32_t v = 0; v < s_cases; ++v)
    {
        first[v] = base;
    }
    // From no time.
    first[8].swept = (double)NAN;
    // Held, the records at the host's end time; and from a mark, where
    // a sweep turning takes the mark itself.
    first[9].rate = 0.0;
    first[10].swept = 0.2;
    // From no time at rate 0, which runs forwards, as a negative rate does
    // not.
    first[11].swept = (double)NAN;
    first[11].playRate = 0.0;
    for (uint32_t v = 0; v < s_cases; ++v)
    {
        apart[v] = first[v];
    }
    apart[0].swept = 0.3;
    apart[1].to = 0.55;
    apart[2].rate = 0.5;
    apart[3].hostFrom = 1.1;
    apart[4].hostTo = 1.6;
    apart[5].swept = (double)NAN;
    apart[6].playRate = 2.0;
    apart[7].way = -1;
    // From no time, the player's direction decides its way.
    apart[8].playRate = -1.0;
    apart[9].hostTo = 1.6;
    apart[10].way = -1;
    apart[11].playRate = -1.0;
    uint32_t differences = 0;
    uint32_t records = 0;
    uint32_t kept = 0;
    for (uint32_t v = 0; v < s_cases; ++v)
    {
        SweepBoth(twins, 0, &first[v]);
        SweepBoth(twins, 1, &first[v]);
        kept += mtwnTestGetSharing(twins[1].context).sweepKept ? 1u : 0u;
        SweepBoth(twins, 2, &apart[v]);
        // And one in step with it, which takes them.
        SweepBoth(twins, 3, &apart[v]);
        differences += CompareRecords(twins, &records);
    }
    CHECK(kept == s_cases, "the second in step keeps its records");
    CHECK(records > 40, "the sweeps make records");
    CHECK(differences == 0, "a sweep one input apart works its records out");

    // A clip made in a destroyed one's slot, its marks elsewhere: its
    // player, its key the old clip's but for the generation, does not take
    // the old clip's records.
    SweepBoth(twins, 0, &base);
    SweepBoth(twins, 1, &base);
    CHECK(mtwnTestGetSharing(twins[1].context).sweepKept, "the old clip's records kept");
    for (int k = 0; k < 2; ++k)
    {
        for (uint32_t p = 0; p < 4; ++p)
        {
            CHECK(mtwnDestroyPlayer(twins[k].context, twins[k].players[p]) == mtwn_success,
                  "a player destroyed");
        }
    }
    (void)CompareRecords(twins, &records);
    for (int k = 0; k < 2; ++k)
    {
        uint32_t old = twins[k].clip.index1;
        CHECK(mtwnDestroyClip(twins[k].context, twins[k].clip) == mtwn_success, "destroyed");
        twins[k].clip = MakeClip(twins[k].context, 1.0f, 0.05);
        CHECK(twins[k].clip.index1 == old, "the new clip in the old one's slot");
        MakePlayer(&twins[k], 0, false, 0.0);
    }
    SweepBoth(twins, 0, &base);
    uint32_t reused = 0;
    CHECK(CompareRecords(twins, &reused) == 0 && reused > 0,
          "a clip in a reused slot takes no records of the old one's");
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
}

// A clip of more marks than a work keeps records of: the sweep that runs
// past them keeps none, and the next in step works its own out.
static void TestManyRecords(void)
{
    Twin twins[2];
    memset(twins, 0, sizeof twins);
    for (int k = 0; k < 2; ++k)
    {
        MakeTwin(&twins[k], true, false, 4096u);
        mtwnClipDef def = mtwnDefaultClipDef();
        def.nodeCapacity = 100;
        def.slotCapacity = s_slots;
        CHECK(mtwnCreateClip(twins[k].context, &def, &twins[k].clip) == mtwn_success, "a clip");
        mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
        group.autoDuration = false;
        group.timing.duration = 1.0;
        group.timing.iterations = HUGE_VAL;
        mtwnNode loop;
        CHECK(mtwnClip_AddTimeline(twins[k].context, twins[k].clip, &group, &loop) == mtwn_success,
              "a timeline");
        for (uint32_t m = 0; m < 80; ++m)
        {
            mtwnPosition at = {0};
            at.offset = 0.01 * (double)(m + 1);
            mtwnNode mark;
            CHECK(mtwnClip_AddMark(twins[k].context, twins[k].clip, loop, at, m, &mark) ==
                      mtwn_success,
                  "a mark");
        }
        CHECK(mtwnClip_Seal(twins[k].context, twins[k].clip) == mtwn_success, "sealed");
        for (uint32_t p = 0; p < 3; ++p)
        {
            mtwnClipPlayerDef player = mtwnDefaultClipPlayerDef();
            player.clip = twins[k].clip;
            CHECK(mtwnCreateClipPlayer(twins[k].context, &player, &twins[k].players[p]) ==
                      mtwn_success,
                  "a player");
        }
    }
    const SweepCase across = {0.0, 1, 1.0, 0.95, 1.0, 1.0, 1.95};
    uint32_t records = 0;
    SweepBoth(twins, 0, &across);
    SweepBoth(twins, 1, &across);
    CHECK(!mtwnTestGetSharing(twins[1].context).sweepKept, "past the records kept, none kept");
    SweepBoth(twins, 2, &across);
    CHECK(CompareRecords(twins, &records) == 0 && records == 240,
          "past the records kept, each works its own out");
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
}

// Held players at a track's start, in pairs one way and the other: going
// back, a track is before its start there and, filling nothing, has no
// effect; going forward it has. A held player's direction is in the key.
static void TestDirectionAtStart(void)
{
    Twin twins[2];
    memset(twins, 0, sizeof twins);
    for (int k = 0; k < 2; ++k)
    {
        MakeTwin(&twins[k], k == 1, false, 4096u);
        mtwnClipDef def = mtwnDefaultClipDef();
        def.slotCapacity = 1;
        CHECK(mtwnCreateClip(twins[k].context, &def, &twins[k].clip) == mtwn_success, "a clip");
        mtwnKeyframe keys[2];
        memset(keys, 0, sizeof keys);
        keys[0].value.x = 7.0f;
        keys[1].offset = 1.0;
        keys[1].value.x = 9.0f;
        keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
        mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
        track.position.offset = 0.25;
        track.keyframes = keys;
        track.keyframeCount = 2;
        track.timing.duration = 0.25;
        track.timing.fill = mtwn_fillNone;
        mtwnNode node;
        CHECK(mtwnClip_AddTrack(twins[k].context, twins[k].clip, &track, &node) == mtwn_success,
              "a track");
        CHECK(mtwnClip_Seal(twins[k].context, twins[k].clip) == mtwn_success, "sealed");
        for (uint32_t p = 0; p < 8; ++p)
        {
            mtwnBinding binding = mtwnMakeBinding(0, twins[k].targets[p][0]);
            mtwnClipPlayerDef player = mtwnDefaultClipPlayerDef();
            player.clip = twins[k].clip;
            player.bindings = &binding;
            player.bindingCount = 1;
            player.keep = true;
            player.rate = p / 2u % 2u == 0u ? 1.0 : -1.0;
            CHECK(mtwnCreateClipPlayer(twins[k].context, &player, &twins[k].players[p]) ==
                      mtwn_success,
                  "a player");
            CHECK(mtwnClipPlayer_Seek(twins[k].context, twins[k].players[p], 0.25) == mtwn_success,
                  "a seek");
            CHECK(mtwnClipPlayer_Pause(twins[k].context, twins[k].players[p]) == mtwn_success,
                  "a pause");
        }
    }
    uint32_t differences = 0;
    uint32_t records = 0;
    uint64_t now = 0;
    for (uint32_t f = 0; f < 4; ++f, now += 16000000u)
    {
        differences += Step(twins, now, &records);
    }
    mtwnTargetState forward;
    mtwnTargetState back;
    CHECK(mtwnTarget_GetState(twins[0].context, twins[0].targets[0][0], &forward) == mtwn_success,
          "a state");
    CHECK(mtwnTarget_GetState(twins[0].context, twins[0].targets[2][0], &back) == mtwn_success,
          "a state");
    CHECK(forward.value.x == 7.0f && back.value.x == s_start.x,
          "at the start, the track has an effect going forward only");
    CHECK(differences == 0, "held players of either direction: the same bits");
    mtwnDestroyContext(twins[0].context);
    mtwnDestroyContext(twins[1].context);
}

// With no argument, the fixed cases; with "crowd", the bench's crowd of
// players on twin contexts, the nightly run's.
int main(int argc, char** argv)
{
    bool crowd = argc == 2 && strcmp(argv[1], "crowd") == 0;
    if (argc > 2 || (argc == 2 && !crowd))
    {
        printf("usage: test_clip_step [crowd]\n");
        return 2;
    }
    if (crowd)
    {
        TestInStep();
        TestSpread();
        TestMixed();
        return s_failures == 0 ? 0 : 1;
    }
    TestReusedSlot();
    TestHostNan();
    TestSweepKeys();
    TestManyRecords();
    TestDirectionAtStart();
    return s_failures == 0 ? 0 : 1;
}
