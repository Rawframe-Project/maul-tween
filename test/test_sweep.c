// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What a sweep leaves out changes nothing:
// twin contexts play one random clip, one counting every record and one
// leaving out what its parent certainly did not cross or where no
// iteration started, and hold the same records, bit for bit. The clips
// nest timelines and tracks with delays, iteration starts, directions and
// rates and repeat delays on times of 1/64 s; frames of 1/64 s start on
// their boundaries, irregular ones do not; players run forwards, and
// backwards from 2.5 s into what they held after their end, some turning
// or slowing on the way. A second pass widens the clips: times also on
// tenths and anywhere, rates of 0.5, 3 and -1, eased timelines, and
// frames of a tenth for odd seeds, where two defects hid off the binary
// grid. And counts add up over any cut of the time:
// each clip in both kinds of frame, to the same time, crosses
// the same points as many times each way.

#include "context_internal.h"
#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/easing.h"
#include "maul-tween/event.h"
#include "maul-tween/player.h"
#include "maul-tween/target.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

// Random numbers from a fixed seed (xorshift64).
static uint64_t s_state = 1;

static double Random(void)
{
    s_state ^= s_state << 13;
    s_state ^= s_state >> 7;
    s_state ^= s_state << 17;
    return (double)(s_state >> 11) * 0x1p-53;
}

static int Pick(int n)
{
    return (int)(Random() * n);
}

// Whether the clips are widened past the binary grid (see above).
static bool s_wide;

// A time on 1/64 s, or widened, also on tenths or anywhere.
static double Sixtyfourths(void)
{
    int kind = s_wide ? Pick(3) : 0;
    return kind == 0   ? (1 + Pick(16)) / 64.0
           : kind == 1 ? (1 + Pick(4)) / 10.0
                       : 0.02 + 0.3 * Random();
}

static mtwnPosition At(double offset)
{
    mtwnPosition position;
    memset(&position, 0, sizeof position);
    position.offset = offset;
    return position;
}

// A random timing on times of 1/64 s, or widened.
static void RandomTiming(mtwnTiming* t, bool timeline)
{
    t->duration = Sixtyfourths() * (1 + Pick(3));
    t->iterations = Pick(4) == 0 && timeline ? HUGE_VAL : 1 + Pick(4) + (Pick(3) == 0 ? 0.5 : 0.0);
    t->delay = Pick(3) == 0 ? Sixtyfourths() : 0.0;
    t->iterationStart = Pick(4) == 0 ? 0.25 * Pick(4) : 0.0;
    t->direction = (mtwnDirection)Pick(4);
    if (s_wide)
    {
        static const double rates[] = {1.0, 1.0, 1.0, 2.0, 0.5, -1.0, 3.0};
        t->playbackRate = rates[Pick(7)];
        // An endless timeline has no end to run back from.
        t->playbackRate = isinf(t->iterations) && t->playbackRate < 0.0 ? 1.0 : t->playbackRate;
    }
    else
    {
        t->playbackRate = Pick(5) == 0 ? 2.0 : 1.0;
    }
    t->repeatDelay = Pick(6) == 0 ? Sixtyfourths() : 0.0;
    if (s_wide && timeline && Pick(3) == 0)
    {
        CHECK(mtwnMakePowerEasing((mtwnEaseMode)Pick(4), 1.5 + Pick(3), &t->easing) == mtwn_success,
              "an easing");
    }
}

static void AddTrack(mtwnContext* context, mtwnClipId clip, mtwnNode parent)
{
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = parent;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.position = At(Pick(2) == 0 ? 0.0 : Sixtyfourths());
    RandomTiming(&track.timing, false);
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
}

// Whether MakeClip makes the clip a 1/64 s frame met off its grid: a
// reversed endless timeline of 0.2 s with a repeat delay of 0.078125 s,
// holding a track that starts 0.03125 s in, whose fifth backward crossing
// falls at 1.28125 s, a frame's end.
static bool s_offGrid;

static void OffGridClip(mtwnContext* context, mtwnClipId clip)
{
    mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
    group.autoDuration = false;
    group.timing.duration = 0.2;
    group.timing.iterations = HUGE_VAL;
    group.timing.direction = mtwn_directionReverse;
    group.timing.repeatDelay = 0.078125;
    mtwnNode timeline = 0;
    CHECK(mtwnClip_AddTimeline(context, clip, &group, &timeline) == mtwn_success, "a timeline");
    mtwnKeyframe keys[2];
    memset(keys, 0, sizeof keys);
    keys[1].offset = 1.0;
    keys[1].value.x = 1.0f;
    keys[0].easing = keys[1].easing = mtwnLinearEasing();
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = timeline;
    track.keyframes = keys;
    track.keyframeCount = 2;
    track.timing.duration = 1.2;
    track.timing.iterations = 2.0;
    track.timing.delay = 0.03125;
    track.timing.direction = mtwn_directionAlternateReverse;
    mtwnNode node = 0;
    CHECK(mtwnClip_AddTrack(context, clip, &track, &node) == mtwn_success, "a track");
}

// The clip of a seed in a context of its own, and its player at the rate.
static mtwnContext* MakeClip(uint64_t seed, double rate, bool countEvery, mtwnPlayerId* idOut)
{
    s_state = 88172645463325252u ^ (seed * 0x9E3779B97F4A7C15u);
    mtwnContextDef def = mtwnDefaultContextDef();
    def.eventCapacity = 1024;
    mtwnContext* context = NULL;
    CHECK(mtwnCreateContext(&def, &context) == mtwn_success, "a context");
    context->countEvery = countEvery;
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip;
    CHECK(mtwnCreateClip(context, &clipDef, &clip) == mtwn_success, "a clip");
    mtwnNode nodes[3] = {MTWN_ROOT_NODE, MTWN_ROOT_NODE, MTWN_ROOT_NODE};
    int timelines = s_offGrid ? 0 : 1 + Pick(3);
    if (s_offGrid)
    {
        OffGridClip(context, clip);
    }
    for (int k = 0; k < timelines; ++k)
    {
        mtwnClipTimelineDef group = mtwnDefaultClipTimelineDef();
        group.parent = k > 0 ? nodes[Pick(k)] : MTWN_ROOT_NODE;
        group.autoDuration = false;
        RandomTiming(&group.timing, true);
        CHECK(mtwnClip_AddTimeline(context, clip, &group, &nodes[k]) == mtwn_success, "a timeline");
    }
    for (int k = s_offGrid ? 0 : 1 + Pick(3); k > 0; --k)
    {
        AddTrack(context, clip, nodes[Pick(timelines)]);
    }
    if (!s_offGrid)
    {
        // Drawn in order, not as arguments, whose order C leaves open.
        mtwnNode markParent = nodes[Pick(timelines)];
        mtwnPosition markAt = At(Sixtyfourths());
        mtwnNode mark = 0;
        CHECK(mtwnClip_AddMark(context, clip, markParent, markAt, 7, &mark) == mtwn_success,
              "a mark");
    }
    CHECK(mtwnClip_Finish(context, clip) == mtwn_success, "finished");
    mtwnTargetId target;
    CHECK(mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &target) == mtwn_success,
          "a target");
    mtwnBinding binding = mtwnBind(0, target);
    mtwnPlayerDef player = mtwnDefaultPlayerDef();
    player.clip = clip;
    player.bindings = &binding;
    player.bindingCount = 1;
    player.keep = true;
    player.rate = rate;
    CHECK(mtwnCreatePlayer(context, &player, idOut) == mtwn_success, "a player");
    if (rate < 0.0)
    {
        CHECK(mtwnPlayer_Seek(context, *idOut, 2.5) == mtwn_success, "sought");
    }
    return context;
}

// FNV-1a over a frame's records.
static void HashRecords(mtwnContext* context, uint64_t* hash, uint32_t* records,
                        uint64_t (*sums)[8][2])
{
    mtwnEvent e[256];
    uint32_t n = 0;
    CHECK(mtwnReadEvents(context, e, 256, &n, NULL) == mtwn_success, "read");
    for (uint32_t i = 0; i < n; ++i)
    {
        double times[4] = {e[i].hostTime, e[i].clipTime, e[i].firstIteration, e[i].lastIteration};
        uint64_t fields[5] = {e[i].count, e[i].node, e[i].key, e[i].kind, e[i].backwards ? 1u : 0u};
        if (sums != NULL && e[i].node < 16 && e[i].kind < 8)
        {
            sums[e[i].node][e[i].kind][e[i].backwards ? 1 : 0] += e[i].count;
        }
        const unsigned char* bytes[2] = {(const unsigned char*)times, (const unsigned char*)fields};
        size_t sizes[2] = {sizeof times, sizeof fields};
        for (int part = 0; part < 2; ++part)
        {
            for (size_t b = 0; b < sizes[part]; ++b)
            {
                *hash = (*hash ^ bytes[part][b]) * 1099511628211u;
            }
        }
    }
    *records += n;
}

// Counts summed by node, kind and way.
typedef uint64_t Sums[16][8][2];

// A clip's records to 3.001 s from an update at 0, in frames of 1/64 s or
// irregular ones, the rate changed on the way unless counts are summed:
// hashed, and their counts summed when sums is not NULL.
static uint64_t Play(uint64_t seed, double rate, bool aligned, bool countEvery, uint32_t* records,
                     Sums* sums)
{
    mtwnPlayerId id;
    mtwnContext* context = MakeClip(seed, rate, countEvery, &id);
    s_state = seed * 2654435761u + 1u;
    uint64_t hash = 14695981039346656037u;
    uint64_t now = 0;
    CHECK(mtwnUpdate(context, 0, NULL, 0, NULL) == mtwn_success, "started");
    HashRecords(context, &hash, records, sums != NULL ? *sums : NULL);
    // A turn at about 1 s for some, half speed from about 2 s for others.
    double turn = seed % 4 == 1 && sums == NULL ? -rate : 0.0;
    double slow = seed % 5 == 2 && sums == NULL ? 0.5 * rate : 0.0;
    while (now < 3001000000u)
    {
        if (now >= 1000000000u && turn != 0.0)
        {
            CHECK(mtwnPlayer_SetRate(context, id, turn) == mtwn_success, "turned");
            turn = 0.0;
        }
        if (now >= 2000000000u && slow != 0.0)
        {
            CHECK(mtwnPlayer_SetRate(context, id, slow) == mtwn_success, "slowed");
            slow = 0.0;
        }
        uint64_t frame = s_wide && seed % 2 != 0 ? 100000000u : 15625000u;
        now += aligned ? frame : 5000000u + (uint64_t)Pick(20000000);
        // Both cuts end at 3.001 s, off every boundary.
        now = now > 3001000000u ? 3001000000u : now;
        CHECK(mtwnUpdate(context, now, NULL, 0, NULL) == mtwn_success, "updated");
        HashRecords(context, &hash, records, sums != NULL ? *sums : NULL);
    }
    mtwnDestroyContext(context);
    return hash;
}

static void TestLeftOutChangesNothing(bool wide)
{
    // Seeds 1 to 600, then widened clips whose count fell an ulp past an
    // iteration's start beside a repeat delay, so that a search from a
    // guess ended two ulps from one without (found by a fuzz of 4,000).
    static const uint64_t hard[] = {2799, 3372};
    s_wide = wide;
    uint32_t differ = 0;
    uint32_t records = 0;
    uint64_t plays = 600 + (wide ? sizeof hard / sizeof hard[0] : 0);
    for (uint64_t n = 1; n <= plays; ++n)
    {
        uint64_t seed = n <= 600 ? n : hard[n - 601];
        double rate = seed % 3 == 0 ? -1.0 : 1.0;
        for (int aligned = 0; aligned < 2; ++aligned)
        {
            uint32_t counted = 0;
            uint32_t left = 0;
            uint64_t every = Play(seed, rate, aligned != 0, true, &counted, NULL);
            uint64_t some = Play(seed, rate, aligned != 0, false, &left, NULL);
            differ += every != some || counted != left ? 1u : 0u;
            records += counted;
        }
    }
    s_wide = false;
    if (differ != 0)
    {
        printf("  %u of %u %s plays recorded otherwise\n", differ, (unsigned)(2 * plays),
               wide ? "widened" : "");
    }
    CHECK(records > 10000 && differ == 0, "the same records, bit for bit");
}

// Counts add up over any cut of the time into frames:
// each clip played in frames of 1/64 s, which land on its boundaries and
// on its holds' edges, and in irregular ones, to the same time, crosses
// the same points as many times each way. A frame landing exactly where
// a held time starts to move lost the crossings at the held time (found
// this way).
static void TestCountsAcrossCuts(bool wide)
{
    // Clips 1 to 300, then clips found to need the margin of the edges.
    static const uint64_t hard[] = {454, 619, 1682, 2884};
    s_wide = wide;
    uint32_t differ = 0;
    uint64_t clips = 300 + (wide ? 0 : sizeof hard / sizeof hard[0]);
    for (uint64_t n = 1; n <= clips; ++n)
    {
        uint64_t seed = n <= 300 ? n : hard[n - 301];
        double rate = seed % 3 == 0 ? -1.0 : 1.0;
        Sums sums[2];
        memset(sums, 0, sizeof sums);
        uint32_t records = 0;
        (void)Play(seed, rate, true, false, &records, &sums[0]);
        (void)Play(seed, rate, false, false, &records, &sums[1]);
        differ += memcmp(sums[0], sums[1], sizeof sums[0]) != 0 ? 1u : 0u;
    }
    s_wide = false;
    if (differ != 0)
    {
        printf("  %u of %u %s clips crossed otherwise in other frames\n", differ, (unsigned)clips,
               wide ? "widened" : "");
    }
    CHECK(differ == 0, "the same crossings whatever the frames");
}

// A crossing at a frame's end, off the 1/64 s grid: the exact count puts
// it an ulp into the next frame, so that frame records it, whose start
// the shortcuts must not take for one the frame before crossed.
// The count's iteration progress past a repeat delay is now
// formed as the timing model's, so the two agree on the child time there.
static void TestOffGridFrameEnd(void)
{
    s_offGrid = true;
    uint32_t counted = 0;
    uint32_t left = 0;
    uint64_t every = Play(3, 1.0, true, true, &counted, NULL);
    uint64_t some = Play(3, 1.0, true, false, &left, NULL);
    s_offGrid = false;
    if (every != some || counted != left)
    {
        printf("  %u records counting every one, %u leaving out\n", counted, left);
    }
    CHECK(counted > 10 && every == some && counted == left,
          "a crossing at a frame's end off the grid recorded once either way");
}

// With no argument, the fixed cases; with "twins", the random clips on
// twin contexts, the nightly run's.
int main(int argc, char** argv)
{
    bool twins = argc == 2 && strcmp(argv[1], "twins") == 0;
    if (argc > 2 || (argc == 2 && !twins))
    {
        printf("usage: test_sweep [twins]\n");
        return 2;
    }
    if (twins)
    {
        TestLeftOutChangesNothing(false);
        TestLeftOutChangesNothing(true);
        TestCountsAcrossCuts(false);
        TestCountsAcrossCuts(true);
    }
    else
    {
        TestOffGridFrameEnd();
    }
    return s_failures == 0 ? 0 : 1;
}
