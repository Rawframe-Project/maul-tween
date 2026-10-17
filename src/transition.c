// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Transitions in the player pool (records mtwn-0004 and mtwn-0029): a
// start from the target's current value, the kind's mix made at once,
// and the transition it replaces ended. The mixes, a spring's settling
// time and a decay's stop compute, so starting runs in the library's
// floating-point environment (records mtwn-0029 and mtwn-0030).

#include "maul-tween/transition.h"

#include "context_internal.h"
#include "easing_internal.h"
#include "elementary.h"
#include "fpenv.h"
#include "spring_internal.h"

#include <math.h>

static mtwnColor ToColor(mtwnVec4 value)
{
    return (mtwnColor){value.x, value.y, value.z, value.w};
}

static mtwnQuat ToQuat(mtwnVec4 value)
{
    return (mtwnQuat){value.x, value.y, value.z, value.w};
}

static bool IsTime(double seconds)
{
    return isfinite(seconds) && seconds >= 0.0;
}

static bool IsTolerance(double value)
{
    return isfinite(value) && value > 0.0;
}

// Whether the def's spec suits the kind and its spring, decay and
// tolerances are valid.
static bool IsSpec(const mtwnTransitionDef* def, mtwnValueKind kind)
{
    switch (def->spec)
    {
    case mtwn_transitionEasing:
        return true;
    case mtwn_transitionSpring:
        return kind != mtwn_valueDiscrete && mtwnIsSpring(&def->spring) &&
               IsTolerance(def->restOffset) && IsTolerance(def->restSpeed);
    case mtwn_transitionDecay:
        // A scale decays in its logarithm; colors and rotations have no
        // line to coast along.
        return kind != mtwn_valueDiscrete && (!mtwnIsPathKind(kind) || kind == mtwn_valueScale) &&
               IsTolerance(def->decay.rate) && IsTolerance(def->restSpeed);
    default:
        return false;
    }
}

// The start velocity per channel, or for a path, in progress: the channel
// velocity projected on the end minus the start. inherited is the
// target's.
static bool StartVelocity(const mtwnTransitionDef* def, const mtwnTargetSlot* target,
                          mtwnVec4 inherited, mtwnPlayerSlot* out)
{
    bool given = def->velocitySource == mtwn_velocityGiven;
    const float* v = given ? &def->velocity.x : &inherited.x;
    uint8_t channels = mtwnKindChannels(target->kind);
    for (uint8_t i = 0; i < channels; ++i)
    {
        if (!isfinite(v[i]))
        {
            return false;
        }
        out->velocity[i] = (double)v[i];
    }
    if (target->kind == mtwn_valueScale)
    {
        // The rate in the logarithm, v / value; a spring's or easing's
        // progress takes it over the logarithm's distance.
        double rate = out->velocity[0] / (double)out->from.x;
        double distance = mtwnLog((double)out->to.x) - mtwnLog((double)out->from.x);
        out->velocity[0] = def->spec == mtwn_transitionDecay ? rate
                           : distance != 0.0                 ? rate / distance
                                                             : 0.0;
        return isfinite(out->velocity[0]);
    }
    if (mtwnIsPathKind(target->kind))
    {
        const float* from = &out->from.x;
        const float* to = &out->to.x;
        double along = 0.0;
        double length = 0.0;
        for (uint8_t i = 0; i < channels; ++i)
        {
            double d = (double)to[i] - (double)from[i];
            along += out->velocity[i] * d;
            length += d * d;
        }
        out->velocity[0] = length > 0.0 ? along / length : 0.0;
        out->velocity[1] = out->velocity[2] = out->velocity[3] = 0.0;
    }
    return true;
}

// Whether two accepted values are equal, as CSS compares computed
// values: channel by channel, so 0 and -0 are; NaN was refused.
static bool Same(mtwnVec4 a, mtwnVec4 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

// The time after the delay a spring or decay ends, and a decay's end.
static void EndOf(const mtwnTransitionDef* def, mtwnPlayerSlot* out)
{
    out->endTime = out->duration;
    if (def->spec == mtwn_transitionEasing)
    {
        return;
    }
    uint8_t channels = mtwnKindChannels(out->kind);
    if (def->spec == mtwn_transitionSpring && mtwnIsPathKind(out->kind))
    {
        out->endTime = mtwnSpringSettlingTime(&out->spring, -1.0, out->velocity[0], def->restOffset,
                                              def->restSpeed);
        return;
    }
    double end = 0.0;
    float* to = &out->to.x;
    const float* from = &out->from.x;
    for (uint8_t i = 0; i < channels; ++i)
    {
        double time = 0.0;
        if (def->spec == mtwn_transitionSpring)
        {
            time = mtwnSpringSettlingTime(&out->spring, (double)from[i] - (double)to[i],
                                          out->velocity[i], def->restOffset, def->restSpeed);
        }
        else
        {
            time = mtwnDecayStopTime(&out->decay, out->velocity[i], def->restSpeed);
        }
        end = time > end ? time : end;
    }
    out->endTime = end;
    if (def->spec == mtwn_transitionDecay)
    {
        bool scale = out->kind == mtwn_valueScale;
        for (uint8_t i = 0; i < channels; ++i)
        {
            double position = 0.0;
            double start = scale ? mtwnLog((double)from[i]) : (double)from[i];
            mtwnSampleDecay(&out->decay, start, out->velocity[i], end, &position, NULL);
            position = scale ? mtwnExp(position) : position;
            to[i] = (float)(out->kind == mtwn_valueInteger ? round(position) : position);
        }
    }
}

// The transition as the def asks it, before it takes a slot.
static mtwnResult Prepare(const mtwnTransitionDef* def, const mtwnTargetSlot* target,
                          mtwnVec4 inherited, mtwnPlayerSlot* out)
{
    out->kind = target->kind;
    out->spec = def->spec;
    out->essential = def->essential;
    out->from = target->value;
    mtwnVec4 end = def->spec == mtwn_transitionDecay ? target->value : def->value;
    if (!mtwnAcceptValue(target->kind, end, &out->to) || !IsTime(def->delay) ||
        !IsTime(def->duration) || !IsSpec(def, target->kind) ||
        def->velocitySource > mtwn_velocityGiven || !StartVelocity(def, target, inherited, out))
    {
        return mtwn_errorInvalid;
    }
    out->delay = def->delay;
    out->duration = def->duration;
    // The one curve its spec uses.
    switch (def->spec)
    {
    case mtwn_transitionEasing:
        out->easing = def->easing;
        if (mtwnIsNormalPower(&def->easing))
        {
            out->power = (uint8_t)def->easing.count;
            out->powerMode = def->easing.mode;
        }
        break;
    case mtwn_transitionSpring:
        out->spring = def->spring;
        break;
    default:
        out->decay = def->decay;
        break;
    }
    // A decay's end, computed after, replaces this.
    if (target->kind == mtwn_valueAngle)
    {
        // The end moved by the method's turns, once.
        out->to.x =
            (float)mtwnMixAngle((double)out->from.x, (double)out->to.x, def->angleMethod, 1.0);
    }
    EndOf(def, out);
    switch (target->kind)
    {
    case mtwn_valueColor:
        return mtwnMakeColorMix(ToColor(out->from), ToColor(out->to), def->colorSpace,
                                def->hueMethod, &out->mix.color);
    case mtwn_valueRotation:
        return mtwnMakeRotationMix(ToQuat(out->from), ToQuat(out->to), def->rotationPath,
                                   &out->mix.rotation);
    case mtwn_valueScale:
        // A decay's end rounded to float may leave a scale's range.
        return mtwnMakeScaleMix((double)out->from.x, (double)out->to.x, &out->mix.scale);
    default:
        return mtwn_success;
    }
}

// CSS Transitions' reversal: an eased transition starts where the one it
// replaces would return to, unless it reverses that one, ending where it
// began: then its duration shrinks by the share of the way that one
// covered, eased (record mtwn-0045).
static void Reverse(const mtwnContext* context, const mtwnTransitionDef* def,
                    const mtwnTargetSlot* target, mtwnPlayerSlot* out)
{
    if (out->spec != mtwn_transitionEasing)
    {
        return;
    }
    out->reversingStart = out->from;
    out->shortening = 1.0;
    if (!def->reverses || target->header.link == 0)
    {
        return;
    }
    const mtwnPlayerSlot* running = mtwnPlayerAt(context, target->header.link);
    mtwnVec4 end = {0};
    if (running->spec != mtwn_transitionEasing ||
        !mtwnAcceptValue(target->kind, def->value, &end) || !Same(end, running->reversingStart))
    {
        return;
    }
    double share = mtwnEasedProgress(context, target->header.link) * running->shortening +
                   (1.0 - running->shortening);
    double factor = fabs(share) < 1.0 ? fabs(share) : 1.0;
    out->reversingStart = running->to;
    out->shortening = factor;
    out->duration *= factor;
    out->endTime = out->duration;
}

static MTWN_NOINLINE mtwnResult StartWorker(mtwnContext* context, const mtwnTransitionDef* def,
                                            mtwnPlayerId* playerOut)
{
    mtwnTargetSlot* target = (mtwnTargetSlot*)mtwnPoolFind(&context->targets, def->target.index1,
                                                           def->target.generation);
    if (target == NULL)
    {
        ++context->misuse;
        return mtwn_errorStale;
    }
    mtwnPlayerSlot prepared = {0};
    mtwnResult result =
        Prepare(def, target, mtwnTargetVelocity(context, def->target.index1), &prepared);
    if (result != mtwn_success)
    {
        ++context->misuse;
        return result;
    }
    Reverse(context, def, target, &prepared);
    // The transition it replaces frees its slot first.
    if (target->header.link != 0)
    {
        mtwnPlayerSlot* replaced = mtwnPlayerAt(context, target->header.link);
        double now = (double)context->lastTimeNs / 1e9;
        double since =
            replaced->started ? now - (double)replaced->startNs / 1e9 - replaced->delay : 0.0;
        mtwnPlayerEvent(context, target->header.link, mtwn_eventCancel, now, since);
        mtwnEndTransition(context, target->header.link);
    }
    uint32_t index1 = mtwnTakePlayer(context, MTWN_PLAYER_TRANSITION);
    if (index1 == 0)
    {
        return mtwn_errorLimit;
    }
    mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
    prepared.header = slot->header;
    prepared.type = slot->type;

    prepared.earlier = slot->earlier;
    prepared.later = slot->later;
    prepared.header.link = def->target.index1;
    prepared.target = def->target;
    *slot = prepared;
    mtwnLinkTarget(context, target, index1);
    *playerOut = (mtwnPlayerId){index1, slot->header.generation};
    return mtwn_success;
}

mtwnResult mtwnStartTransition(mtwnContext* context, const mtwnTransitionDef* def,
                               mtwnPlayerId* playerOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (playerOut != NULL)
    {
        *playerOut = (mtwnPlayerId){0, 0};
    }
    if (def == NULL || playerOut == NULL || def->cookie != MTWN_TRANSITION_DEF_COOKIE)
    {
        ++context->misuse;
        return mtwn_errorInvalid;
    }
    if (def->version != MTWN_ABI_VERSION)
    {
        ++context->misuse;
        return mtwn_errorVersion;
    }
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    mtwnResult result = StartWorker(context, def, playerOut);
    mtwnFloatEnvLeave(&env);
    return result;
}

// The distance from a value to an end, both accepted, in the kind's own
// measure (record mtwn-0060).
static mtwnResult Distance(const mtwnTransitionDef* def, mtwnValueKind kind, mtwnVec4 from,
                           mtwnVec4 to, double* distanceOut)
{
    double a[4] = {(double)from.x, (double)from.y, (double)from.z, (double)from.w};
    double b[4] = {(double)to.x, (double)to.y, (double)to.z, (double)to.w};
    switch (kind)
    {
    case mtwn_valueDiscrete:
        return mtwn_errorInvalid;
    case mtwn_valueAngle:
        *distanceOut = fabs(mtwnMixAngle(a[0], b[0], def->angleMethod, 1.0) - a[0]);
        return mtwn_success;
    case mtwn_valueScale:
        *distanceOut = fabs(mtwnLog(b[0]) - mtwnLog(a[0]));
        return mtwn_success;
    case mtwn_valueRotation:
    {
        // The mix's angle is between the quaternions, half the turn.
        mtwnRotationMix mix;
        mtwnResult made = mtwnMakeRotationMix(ToQuat(from), ToQuat(to), def->rotationPath, &mix);
        *distanceOut = made == mtwn_success ? 2.0 * mix.angle : 0.0;
        return made;
    }
    case mtwn_valueColor:
        if (mtwnColorToSpace(ToColor(from), mtwn_colorOklab, a) != mtwn_success ||
            mtwnColorToSpace(ToColor(to), mtwn_colorOklab, b) != mtwn_success)
        {
            return mtwn_errorInvalid;
        }
        break;
    default:
        break;
    }
    double sum = 0.0;
    for (uint8_t i = 0; i < mtwnKindChannels(kind); ++i)
    {
        sum += (b[i] - a[i]) * (b[i] - a[i]);
    }
    *distanceOut = sqrt(sum);
    return mtwn_success;
}

mtwnResult mtwnMeasureTransition(mtwnContext* context, const mtwnTransitionDef* def,
                                 double* distanceOut)
{
    if (distanceOut != NULL)
    {
        *distanceOut = 0.0;
    }
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (def == NULL || distanceOut == NULL || def->cookie != MTWN_TRANSITION_DEF_COOKIE)
    {
        ++context->misuse;
        return mtwn_errorInvalid;
    }
    if (def->version != MTWN_ABI_VERSION)
    {
        ++context->misuse;
        return mtwn_errorVersion;
    }
    const mtwnTargetSlot* target = (const mtwnTargetSlot*)mtwnPoolFind(
        &context->targets, def->target.index1, def->target.generation);
    if (target == NULL)
    {
        ++context->misuse;
        return mtwn_errorStale;
    }
    mtwnVec4 end = {0};
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    mtwnResult result = mtwnAcceptValue(target->kind, def->value, &end)
                            ? Distance(def, target->kind, target->value, end, distanceOut)
                            : mtwn_errorInvalid;
    mtwnFloatEnvLeave(&env);
    if (result != mtwn_success)
    {
        *distanceOut = 0.0;
        ++context->misuse;
    }
    return result;
}

void mtwnEndTransition(mtwnContext* context, uint32_t player)
{
    uint32_t target = mtwnPlayerAt(context, player)->header.link;
    mtwnSettleVelocity(context, target);
    mtwnLinkTarget(context, mtwnTargetAt(context, target), 0);
    mtwnFreePlayer(context, player);
}
