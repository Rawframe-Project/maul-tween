// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Padding: every public struct that has some, built field by field over
// garbage bytes, gives the same bits as one built over zeroes, through the
// calls without a context and through a context's transitions, clips and
// players. A call reads fields, never a struct's bytes.

#include "test_harness.h"

#include "maul-tween/clip.h"
#include "maul-tween/context.h"
#include "maul-tween/interpolate.h"
#include "maul-tween/path.h"
#include "maul-tween/player.h"
#include "maul-tween/spring.h"
#include "maul-tween/target.h"
#include "maul-tween/timing.h"
#include "maul-tween/transition.h"

#include <stdint.h>
#include <string.h>

// The bytes a struct is built over: 0 for the clean one, garbage for the
// other.
static uint8_t s_fill;

static void Fill(void* bytes, size_t size)
{
    memset(bytes, s_fill, size);
}

static bool SameBits(double a, double b)
{
    return memcmp(&a, &b, sizeof a) == 0;
}

static bool SameVec(mtwnVec4 a, mtwnVec4 b)
{
    return memcmp(&a, &b, sizeof a) == 0;
}

static void TimingOver(const mtwnTiming* t, mtwnTiming* out)
{
    Fill(out, sizeof *out);
    out->delay = t->delay;
    out->endDelay = t->endDelay;
    out->iterationStart = t->iterationStart;
    out->iterations = t->iterations;
    out->duration = t->duration;
    out->repeatDelay = t->repeatDelay;
    out->playbackRate = t->playbackRate;
    out->easing = t->easing;
    out->fill = t->fill;
    out->direction = t->direction;
    out->alternateEasing = t->alternateEasing;
}

static void SpringOver(const mtwnSpring* s, mtwnSpring* out)
{
    Fill(out, sizeof *out);
    memcpy(out->parameters, s->parameters, sizeof out->parameters);
    out->regime = s->regime;
}

static void ColorMixOver(const mtwnColorMix* m, mtwnColorMix* out)
{
    Fill(out, sizeof *out);
    memcpy(out->from, m->from, sizeof out->from);
    memcpy(out->to, m->to, sizeof out->to);
    out->fromColor = m->fromColor;
    out->toColor = m->toColor;
    out->space = m->space;
}

static void PathOver(const mtwnPath* p, mtwnPath* out)
{
    Fill(out, sizeof *out);
    out->keyframes = p->keyframes;
    out->lengths = p->lengths;
    out->alpha = p->alpha;
    out->count = p->count;
    out->channels = p->channels;
    out->logarithmic = p->logarithmic;
}

static void TrackOver(const mtwnTrack* t, mtwnTrack* out)
{
    Fill(out, sizeof *out);
    PathOver(&t->path, &out->path);
    out->mixes = t->mixes;
    out->kind = t->kind;
}

static void TrackDefOver(const mtwnTrackDef* d, mtwnTrackDef* out)
{
    Fill(out, sizeof *out);
    out->alpha = d->alpha;
    out->kind = d->kind;
    out->colorSpace = d->colorSpace;
    out->hueMethod = d->hueMethod;
    out->angleMethod = d->angleMethod;
    out->rotationPath = d->rotationPath;
}

static void PositionOver(const mtwnPosition* p, mtwnPosition* out)
{
    Fill(out, sizeof *out);
    out->offset = p->offset;
    out->label = p->label;
    out->kind = p->kind;
}

static void ClipTrackDefOver(const mtwnClipTrackDef* d, mtwnClipTrackDef* out)
{
    Fill(out, sizeof *out);
    out->cookie = d->cookie;
    out->parent = d->parent;
    out->slot = d->slot;
    out->keyframeCount = d->keyframeCount;
    PositionOver(&d->position, &out->position);
    TrackDefOver(&d->track, &out->track);
    TimingOver(&d->timing, &out->timing);
    out->keyframes = d->keyframes;
    out->constantSpeed = d->constantSpeed;
    out->incremental = d->incremental;
}

static void ClipTimelineDefOver(const mtwnClipTimelineDef* d, mtwnClipTimelineDef* out)
{
    Fill(out, sizeof *out);
    out->cookie = d->cookie;
    out->parent = d->parent;
    out->autoDuration = d->autoDuration;
    PositionOver(&d->position, &out->position);
    TimingOver(&d->timing, &out->timing);
}

static void BindingOver(const mtwnBinding* b, mtwnBinding* out)
{
    Fill(out, sizeof *out);
    out->slot = b->slot;
    out->composite = b->composite;
    out->target = b->target;
    out->weight = b->weight;
}

static void PlayerDefOver(const mtwnPlayerDef* d, mtwnPlayerDef* out)
{
    Fill(out, sizeof *out);
    out->cookie = d->cookie;
    out->clip = d->clip;
    out->bindingCount = d->bindingCount;
    out->bindings = d->bindings;
    out->play = d->play;
    out->keep = d->keep;
    out->essential = d->essential;
    out->rate = d->rate;
    out->weight = d->weight;
}

static void TransitionDefOver(const mtwnTransitionDef* d, mtwnTransitionDef* out)
{
    Fill(out, sizeof *out);
    out->cookie = d->cookie;
    out->target = d->target;
    out->colorSpace = d->colorSpace;
    out->hueMethod = d->hueMethod;
    out->angleMethod = d->angleMethod;
    out->rotationPath = d->rotationPath;
    out->value = d->value;
    out->velocity = d->velocity;
    out->delay = d->delay;
    out->duration = d->duration;
    out->easing = d->easing;
    SpringOver(&d->spring, &out->spring);
    out->decay = d->decay;
    out->lower = d->lower;
    out->upper = d->upper;
    out->restOffset = d->restOffset;
    out->restSpeed = d->restSpeed;
    out->kind = d->kind;
    out->velocitySource = d->velocitySource;
    out->reverses = d->reverses;
    out->essential = d->essential;
    out->bounded = d->bounded;
    out->lands = d->lands;
}

static mtwnKeyframe Key(double offset, float x)
{
    mtwnKeyframe k;
    memset(&k, 0, sizeof k);
    k.offset = offset;
    k.value = (mtwnVec4){x, 0, 0, 0};
    k.easing = mtwnLinearEasing();
    return k;
}

// What the calls without a context give, from structs built over s_fill.
typedef struct Values
{
    double iteration;
    double transformed;
    double offset;
    double velocity;
    mtwnColor color;
    double point[4];
    double tangent[4];
    mtwnVec4 sampled;
    mtwnVec4 made;
} Values;

static bool FreeCalls(Values* out)
{
    memset(out, 0, sizeof *out);
    mtwnTiming base = mtwnDefaultTiming();
    base.duration = 2.0;
    base.iterations = 3.0;
    base.repeatDelay = 0.5;
    base.direction = mtwn_directionAlternate;
    mtwnTiming timing;
    TimingOver(&base, &timing);
    mtwnTimingResult result;
    bool ok = mtwnComputeTiming(&timing, 3.25, false, &result) == mtwn_success;
    out->iteration = result.iteration;
    out->transformed = result.transformedProgress;
    mtwnSpring made;
    ok = ok && mtwnMakeSpring(170.0, 26.0, 1.0, &made) == mtwn_success;
    mtwnSpring spring;
    SpringOver(&made, &spring);
    ok = ok &&
         mtwnSampleSpring(&spring, 1.0, 0.0, 0.3, &out->offset, &out->velocity) == mtwn_success;
    mtwnColorMix madeMix;
    mtwnColor from = {0.9f, 0.2f, 0.1f, 1.0f};
    mtwnColor to = {0.1f, 0.3f, 0.8f, 0.5f};
    ok = ok &&
         mtwnMakeColorMix(from, to, mtwn_colorOklch, mtwn_hueShorter, &madeMix) == mtwn_success;
    mtwnColorMix mix;
    ColorMixOver(&madeMix, &mix);
    ok = ok && mtwnMixColor(&mix, 0.4, &out->color) == mtwn_success;
    static mtwnKeyframe keys[3];
    keys[0] = Key(0.0, 0.0f);
    keys[1] = Key(0.5, 2.0f);
    keys[2] = Key(1.0, 1.0f);
    double lengths[3];
    mtwnPath madePath;
    ok = ok && mtwnMakePath(keys, 3, 1, 0.5, lengths, &madePath) == mtwn_success;
    mtwnPath path;
    PathOver(&madePath, &path);
    ok = ok && mtwnSamplePath(&path, 0.7, out->point, out->tangent) == mtwn_success;
    static mtwnKeyframe trackKeys[3];
    memcpy(trackKeys, keys, sizeof trackKeys);
    mtwnTrackDef def = mtwnDefaultTrackDef(mtwn_valueScalar);
    mtwnTrackDef over;
    TrackDefOver(&def, &over);
    double trackLengths[3];
    mtwnTrack madeTrack;
    ok = ok && mtwnMakeTrack(&over, trackKeys, 3, trackLengths, NULL, &madeTrack) == mtwn_success;
    ok = ok && mtwnSampleTrack(&madeTrack, 0.3, &out->made) == mtwn_success;
    mtwnTrack track;
    TrackOver(&madeTrack, &track);
    ok = ok && mtwnSampleTrack(&track, 0.6, &out->sampled) == mtwn_success;
    return ok;
}

// The garbage reaches a struct's padding: a timing built over it differs
// from one built over zeroes only there, and it has padding on every
// target.
static void TestGarbageThere(void)
{
    mtwnTiming base = mtwnDefaultTiming();
    mtwnTiming clean;
    mtwnTiming dirty;
    s_fill = 0x00;
    TimingOver(&base, &clean);
    s_fill = 0xA5;
    TimingOver(&base, &dirty);
    CHECK(memcmp(&clean, &dirty, sizeof clean) != 0, "a timing's padding holds the garbage");
}

static void TestFreeCalls(void)
{
    Values clean;
    Values dirty;
    s_fill = 0x00;
    bool cleanOk = FreeCalls(&clean);
    s_fill = 0xA5;
    bool dirtyOk = FreeCalls(&dirty);
    CHECK(cleanOk && dirtyOk, "every call taken over zeroes and over garbage");
    CHECK(SameBits(clean.iteration, dirty.iteration) &&
              SameBits(clean.transformed, dirty.transformed),
          "a timing's iteration and progress the same");
    CHECK(SameBits(clean.offset, dirty.offset) && SameBits(clean.velocity, dirty.velocity),
          "a spring's offset and velocity");
    CHECK(memcmp(&clean.color, &dirty.color, sizeof clean.color) == 0, "a color mix's color");
    bool path = true;
    for (int c = 0; c < 4; ++c)
    {
        path = path && SameBits(clean.point[c], dirty.point[c]) &&
               SameBits(clean.tangent[c], dirty.tangent[c]);
    }
    CHECK(path, "a path's point and tangent");
    CHECK(SameVec(clean.made, dirty.made) && SameVec(clean.sampled, dirty.sampled),
          "a track made from its def, and a track sampled");
}

// A scene of a spring transition and a clip of a timeline holding a track,
// played by a player through a binding, every def built over s_fill; the
// two targets' values at three updates.
static bool Scene(mtwnVec4 values[6])
{
    memset(values, 0, 6 * sizeof *values);
    mtwnContextDef contextDef = mtwnDefaultContextDef();
    mtwnContext* context = NULL;
    if (mtwnCreateContext(&contextDef, &context) != mtwn_success)
    {
        return false;
    }
    mtwnTargetId moved = {0};
    mtwnTargetId played = {0};
    bool ok = mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &moved) == mtwn_success &&
              mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &played) == mtwn_success;
    mtwnTransitionDef transition = mtwnDefaultTransitionDef();
    transition.target = moved;
    transition.value = (mtwnVec4){1.0f, 0, 0, 0};
    transition.kind = mtwn_transitionSpring;
    ok = ok && mtwnMakeSpring(170.0, 26.0, 1.0, &transition.spring) == mtwn_success;
    mtwnTransitionDef transitionOver;
    TransitionDefOver(&transition, &transitionOver);
    mtwnPlayerId spring;
    ok = ok && mtwnStartTransition(context, &transitionOver, &spring) == mtwn_success;
    mtwnClipDef clipDef = mtwnDefaultClipDef();
    mtwnClipId clip = {0};
    ok = ok && mtwnCreateClip(context, &clipDef, &clip) == mtwn_success;
    mtwnClipTimelineDef timeline = mtwnDefaultClipTimelineDef();
    timeline.position = (mtwnPosition){0.25, 0, mtwn_positionAbsolute};
    timeline.timing.iterations = 2.0;
    timeline.timing.direction = mtwn_directionAlternate;
    mtwnClipTimelineDef timelineOver;
    ClipTimelineDefOver(&timeline, &timelineOver);
    mtwnNode group = 0;
    ok = ok && mtwnClip_AddTimeline(context, clip, &timelineOver, &group) == mtwn_success;
    mtwnKeyframe keys[3] = {Key(0.0, 0.0f), Key(0.5, 2.0f), Key(1.0, 1.0f)};
    mtwnClipTrackDef track = mtwnDefaultClipTrackDef();
    track.parent = group;
    track.keyframes = keys;
    track.keyframeCount = 3;
    track.timing.duration = 0.5;
    mtwnClipTrackDef trackOver;
    ClipTrackDefOver(&track, &trackOver);
    mtwnNode node;
    ok = ok && mtwnClip_AddTrack(context, clip, &trackOver, &node) == mtwn_success &&
         mtwnClip_Seal(context, clip) == mtwn_success;
    mtwnBinding binding = mtwnMakeBinding(0, played);
    mtwnBinding bindingOver;
    BindingOver(&binding, &bindingOver);
    mtwnPlayerDef player = mtwnDefaultPlayerDef();
    player.clip = clip;
    player.bindings = &bindingOver;
    player.bindingCount = 1;
    mtwnPlayerDef playerOver;
    PlayerDefOver(&player, &playerOver);
    mtwnPlayerId playing;
    ok = ok && mtwnCreatePlayer(context, &playerOver, &playing) == mtwn_success;
    const uint64_t times[3] = {1000000u, 400000000u, 900000000u};
    for (int i = 0; ok && i < 3; ++i)
    {
        ok = mtwnContext_Update(context, times[i], NULL, 0, NULL) == mtwn_success &&
             mtwnTarget_GetValue(context, moved, &values[2 * i]) == mtwn_success &&
             mtwnTarget_GetValue(context, played, &values[2 * i + 1]) == mtwn_success;
    }
    ok = ok && mtwnContext_GetMisuse(context) == 0;
    mtwnDestroyContext(context);
    return ok;
}

static void TestContextCalls(void)
{
    mtwnVec4 clean[6];
    mtwnVec4 dirty[6];
    s_fill = 0x00;
    bool cleanOk = Scene(clean);
    s_fill = 0xA5;
    bool dirtyOk = Scene(dirty);
    CHECK(cleanOk && dirtyOk, "the scene made and played over zeroes and over garbage");
    bool same = true;
    for (int i = 0; i < 6; ++i)
    {
        same = same && SameVec(clean[i], dirty[i]);
    }
    CHECK(same && clean[5].x != 0.0f && clean[4].x != clean[2].x,
          "the transition's and the clip's values the same at every update");
}

int main(void)
{
    TestGarbageThere();
    TestFreeCalls();
    TestContextCalls();
    return s_failures == 0 ? 0 : 1;
}
