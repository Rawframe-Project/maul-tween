// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Players (records mtwn-0004, mtwn-0012 and mtwn-0035): every player in
// creation order, and a clip's player by Web Animations Level 1's
// procedures, its pending play resolved at the next update, whose host
// time is the ready time. Times are seconds; NaN is unresolved.

#include "maul-tween/player.h"

#include "context_internal.h"
#include "fpenv.h"

#include <math.h>

uint32_t mtwnTakePlayer(mtwnContext* context, uint8_t type)
{
    uint32_t index1 = mtwnPoolAcquire(&context->players);
    if (index1 == 0)
    {
        return 0;
    }
    mtwnPlayerSlot* slot = mtwnPlayerAt(context, index1);
    slot->type = type;
    slot->earlier = context->lastPlayer;
    if (context->lastPlayer != 0)
    {
        mtwnPlayerAt(context, context->lastPlayer)->later = index1;
    }
    else
    {
        context->firstPlayer = index1;
    }
    context->lastPlayer = index1;
    return index1;
}

void mtwnFreePlayer(mtwnContext* context, uint32_t player)
{
    mtwnPlayerSlot* slot = mtwnPlayerAt(context, player);
    if (slot->earlier != 0)
    {
        mtwnPlayerAt(context, slot->earlier)->later = slot->later;
    }
    else
    {
        context->firstPlayer = slot->later;
    }
    if (slot->later != 0)
    {
        mtwnPlayerAt(context, slot->later)->earlier = slot->earlier;
    }
    else
    {
        context->lastPlayer = slot->earlier;
    }
    if (slot->type == MTWN_PLAYER_CLIP)
    {
        if (slot->play.bindingCount > 0)
        {
            mtwnGiveRun(&context->bindings, slot->play.bindingStart, slot->play.bindingCount);
        }
        --mtwnClipAt(context, slot->play.clip.index1)->users;
    }
    mtwnPoolRelease(&context->players, player);
}

static mtwnPlayerSlot* FindPlayer(const mtwnContext* context, mtwnPlayerId player)
{
    return (mtwnPlayerSlot*)mtwnPoolFind(&context->players, player.index1, player.generation);
}

// The timeline time: the last update's host time, in seconds.
static double Now(const mtwnContext* context)
{
    return (double)context->lastTimeNs / 1e9;
}

static double EffectEnd(const mtwnContext* context, const mtwnClipPlay* play)
{
    return mtwnClipAt(context, play->clip.index1)->nodes[0].childrenEnd;
}

static double CurrentTime(const mtwnClipPlay* play, double now)
{
    if (!isnan(play->holdTime))
    {
        return play->holdTime;
    }
    if (isnan(play->startTime))
    {
        return (double)NAN;
    }
    return (now - play->startTime) * play->rate;
}

// The rate the player is going to: its pending one, if any.
static double Effective(const mtwnClipPlay* play)
{
    return isnan(play->pendingRate) ? play->rate : play->pendingRate;
}

// Level 1's applying of a pending playback rate.
static void ApplyPendingRate(mtwnClipPlay* play)
{
    if (!isnan(play->pendingRate))
    {
        play->rate = play->pendingRate;
        play->pendingRate = (double)NAN;
    }
}

static mtwnPlayState StateOf(const mtwnClipPlay* play, double now, double end)
{
    double current = CurrentTime(play, now);
    if (isnan(current) && isnan(play->startTime) && !play->pending)
    {
        return mtwn_playIdle;
    }
    if (isnan(play->startTime) && !play->pending)
    {
        return mtwn_playPaused;
    }
    // Level 1 tests the finished state against the effective rate.
    double rate = Effective(play);
    if (!isnan(current) && ((rate > 0.0 && current >= end) || (rate < 0.0 && current <= 0.0)))
    {
        return mtwn_playFinished;
    }
    return mtwn_playRunning;
}

// Level 1's update of the hold time, with the time not held already
// worked out.
static void Hold(mtwnClipPlay* play, double now, double end, bool didSeek, double unconstrained)
{
    double previous = play->previousTime;
    if (play->rate > 0.0 && unconstrained >= end)
    {
        // Not past the previous time: a finished player stays where it was.
        double past = isnan(previous) || previous < end ? end : previous;
        play->holdTime = didSeek ? unconstrained : past;
    }
    else if (play->rate < 0.0 && unconstrained <= 0.0)
    {
        double past = isnan(previous) || previous > 0.0 ? 0.0 : previous;
        play->holdTime = didSeek ? unconstrained : past;
    }
    else if (play->rate != 0.0)
    {
        if (didSeek && !isnan(play->holdTime))
        {
            play->startTime = now - play->holdTime / play->rate;
        }
        play->holdTime = (double)NAN;
    }
}

// Level 1's update of the finished state.
static void UpdateFinished(mtwnClipPlay* play, double now, double end, bool didSeek)
{
    double unconstrained = (double)NAN;
    if (didSeek)
    {
        unconstrained = CurrentTime(play, now);
    }
    else if (!isnan(play->startTime))
    {
        unconstrained = (now - play->startTime) * play->rate;
    }
    if (!isnan(unconstrained) && !isnan(play->startTime) && !play->pending)
    {
        Hold(play, now, end, didSeek, unconstrained);
    }
    play->previousTime = CurrentTime(play, now);
}

// Level 1's silent setting of the current time.
static void SilentlySet(mtwnClipPlay* play, double now, double seek)
{
    if (!isnan(play->holdTime) || isnan(play->startTime) || play->rate == 0.0)
    {
        play->holdTime = seek;
    }
    else
    {
        play->startTime = now - seek / play->rate;
    }
    play->previousTime = (double)NAN;
}

static void SetCurrent(mtwnClipPlay* play, double now, double end, double seek)
{
    SilentlySet(play, now, seek);
    UpdateFinished(play, now, end, true);
}

// Level 1's play with auto-rewind; the pending play is resolved by the next
// update.
static mtwnResult Play(mtwnClipPlay* play, double now, double end)
{
    double seek = (double)NAN;
    double current = CurrentTime(play, now);
    double rate = Effective(play);
    if (rate >= 0.0 && (isnan(current) || current < 0.0 || current >= end))
    {
        seek = 0.0;
    }
    else if (rate < 0.0 && (isnan(current) || current <= 0.0 || current > end))
    {
        if (isinf(end))
        {
            return mtwn_errorState;
        }
        seek = end;
    }
    if (!isnan(seek))
    {
        play->holdTime = seek;
    }
    if (!isnan(play->holdTime))
    {
        play->startTime = (double)NAN;
    }
    if (isnan(play->holdTime) && isnan(seek))
    {
        // Already running: nothing to do but a pending rate, which the next
        // update applies from the time it reaches.
        return mtwn_success;
    }
    play->pending = true;
    UpdateFinished(play, now, end, false);
    return mtwn_success;
}

static mtwnResult Pause(mtwnClipPlay* play, double now, double end)
{
    double current = CurrentTime(play, now);
    if (isnan(current))
    {
        if (Effective(play) < 0.0 && isinf(end))
        {
            return mtwn_errorState;
        }
        play->holdTime = Effective(play) >= 0.0 ? 0.0 : end;
    }
    play->pending = false;
    if (!isnan(play->startTime) && isnan(play->holdTime))
    {
        play->holdTime = (now - play->startTime) * play->rate;
    }
    ApplyPendingRate(play);
    play->startTime = (double)NAN;
    UpdateFinished(play, now, end, false);
    return mtwn_success;
}

static mtwnResult Finish(mtwnClipPlay* play, double now, double end)
{
    double rate = Effective(play);
    if (rate == 0.0 || (rate > 0.0 && isinf(end)))
    {
        return mtwn_errorState;
    }
    // The pending rate is applied now, keeping the time reached.
    if (!isnan(play->pendingRate) && !isnan(play->startTime) && isnan(play->holdTime))
    {
        play->holdTime = (now - play->startTime) * play->rate;
        play->startTime = (double)NAN;
    }
    ApplyPendingRate(play);
    double limit = play->rate > 0.0 ? end : 0.0;
    SilentlySet(play, now, limit);
    if (isnan(play->startTime))
    {
        play->startTime = now - limit / play->rate;
    }
    play->pending = false;
    UpdateFinished(play, now, end, true);
    return mtwn_success;
}

// A rate change keeps the current time: a moving player changes at the next
// update, as Level 1's seamless update does at its ready time; a held or
// idle one at once.
static void SetRate(mtwnClipPlay* play, double now, double end, double rate)
{
    bool moving = !isnan(play->startTime) && isnan(play->holdTime);
    if (moving)
    {
        play->pendingRate = rate;
        return;
    }
    double previous = CurrentTime(play, now);
    play->rate = rate;
    play->pendingRate = (double)NAN;
    if (!isnan(previous))
    {
        SetCurrent(play, now, end, previous);
    }
}

bool mtwnAdvancePlayer(mtwnContext* context, uint32_t player, double now)
{
    mtwnClipPlay* play = &mtwnPlayerAt(context, player)->play;
    double end = EffectEnd(context, play);
    if (!isnan(play->pendingRate) && !play->pending)
    {
        // The old rate ran until now; the new one runs from the time reached.
        double current = CurrentTime(play, now);
        ApplyPendingRate(play);
        if (play->rate != 0.0)
        {
            play->startTime = now - current / play->rate;
        }
        else
        {
            play->holdTime = current;
            play->startTime = now;
        }
    }
    if (play->pending)
    {
        // The pending play's ready time is this update's.
        play->pending = false;
        ApplyPendingRate(play);
        if (!isnan(play->holdTime))
        {
            play->startTime = play->rate != 0.0 ? now - play->holdTime / play->rate : now;
            if (play->rate != 0.0)
            {
                play->holdTime = (double)NAN;
            }
        }
    }
    UpdateFinished(play, now, end, false);
    mtwnPlayState state = StateOf(play, now, end);
    if (state == mtwn_playIdle)
    {
        return false;
    }
    // Only a running player moves; one not kept commits its end and goes.
    bool done = state == mtwn_playFinished && !play->keep;
    double rate = state == mtwn_playRunning ? play->rate : 0.0;
    mtwnSampleClip(context, play, CurrentTime(play, now), rate, play->rate < 0.0, done);
    return done;
}

static bool IsRate(double rate)
{
    return isfinite(rate);
}

// Whether the bindings name slots the clip's tracks use, each once, and
// live targets of their kinds.
static bool AreBindings(const mtwnContext* context, const mtwnPlayerDef* def,
                        const mtwnClipSlot* clip)
{
    if (def->bindingCount > 0 && def->bindings == NULL)
    {
        return false;
    }
    for (uint32_t i = 0; i < def->bindingCount; ++i)
    {
        const mtwnBinding* binding = &def->bindings[i];
        const mtwnTargetSlot* target = (const mtwnTargetSlot*)mtwnPoolFind(
            &context->targets, binding->target.index1, binding->target.generation);
        if (binding->slot >= clip->slotCount || target == NULL ||
            clip->slotKinds[binding->slot] != target->kind + 1)
        {
            return false;
        }
        for (uint32_t j = 0; j < i; ++j)
        {
            if (def->bindings[j].slot == def->bindings[i].slot)
            {
                return false;
            }
        }
    }
    return true;
}

static mtwnResult Misuse(mtwnContext* context, mtwnResult result)
{
    ++context->misuse;
    return result;
}

static mtwnResult CreateWorker(mtwnContext* context, const mtwnPlayerDef* def,
                               mtwnPlayerId* playerOut)
{
    mtwnClipSlot* clip =
        (mtwnClipSlot*)mtwnPoolFind(&context->clips, def->clip.index1, def->clip.generation);
    if (clip == NULL)
    {
        return Misuse(context, mtwn_errorStale);
    }
    if (!clip->finished)
    {
        return Misuse(context, mtwn_errorState);
    }
    if (!IsRate(def->rate) || !AreBindings(context, def, clip))
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    uint32_t start = 0;
    if (clip->slotCount > 0 && !mtwnTakeRun(&context->bindings, clip->slotCount, &start))
    {
        return mtwn_errorLimit;
    }
    uint32_t index1 = mtwnTakePlayer(context, MTWN_PLAYER_CLIP);
    if (index1 == 0)
    {
        if (clip->slotCount > 0)
        {
            mtwnGiveRun(&context->bindings, start, clip->slotCount);
        }
        return mtwn_errorLimit;
    }
    mtwnTargetId* targets = &context->bindings.entries[start];
    for (uint32_t s = 0; s < clip->slotCount; ++s)
    {
        targets[s] = (mtwnTargetId){0, 0};
    }
    for (uint32_t i = 0; i < def->bindingCount; ++i)
    {
        targets[def->bindings[i].slot] = def->bindings[i].target;
    }
    mtwnClipPlay* play = &mtwnPlayerAt(context, index1)->play;
    *play = (mtwnClipPlay){.clip = def->clip,
                           .startTime = (double)NAN,
                           .holdTime = (double)NAN,
                           .previousTime = (double)NAN,
                           .rate = def->rate,
                           .pendingRate = (double)NAN,
                           .bindingStart = start,
                           .bindingCount = clip->slotCount,
                           .keep = def->keep};
    ++clip->users;
    if (def->play)
    {
        (void)Play(play, Now(context), EffectEnd(context, play));
    }
    *playerOut = (mtwnPlayerId){index1, mtwnPlayerAt(context, index1)->header.generation};
    return mtwn_success;
}

mtwnResult mtwnCreatePlayer(mtwnContext* context, const mtwnPlayerDef* def, mtwnPlayerId* playerOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    if (playerOut != NULL)
    {
        *playerOut = (mtwnPlayerId){0, 0};
    }
    if (def == NULL || playerOut == NULL || def->cookie != MTWN_PLAYER_DEF_COOKIE)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    if (def->version != MTWN_ABI_VERSION)
    {
        return Misuse(context, mtwn_errorVersion);
    }
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    mtwnResult result = CreateWorker(context, def, playerOut);
    mtwnFloatEnvLeave(&env);
    return result;
}

mtwnResult mtwnDestroyPlayer(mtwnContext* context, mtwnPlayerId player)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnPlayerSlot* slot = FindPlayer(context, player);
    if (slot == NULL)
    {
        return Misuse(context, mtwn_errorStale);
    }
    if (slot->type == MTWN_PLAYER_CLIP)
    {
        // Its targets show their bases again, until another player writes
        // them.
        for (uint32_t s = 0; s < slot->play.bindingCount; ++s)
        {
            uint32_t target = mtwnBoundTarget(context, &slot->play, s);
            if (target != 0)
            {
                mtwnWriteTarget(context, target, mtwnTargetAt(context, target)->base,
                                (mtwnVec4){0});
            }
        }
        mtwnFreePlayer(context, player.index1);
        return mtwn_success;
    }
    uint32_t index1 = slot->header.link;
    mtwnEndTransition(context, player.index1);
    // The target shows its base again.
    mtwnTargetSlot* target = mtwnTargetAt(context, index1);
    target->value = target->base;
    target->velocity = (mtwnVec4){0};
    mtwnMarkChanged(context, index1);
    return mtwn_success;
}

bool mtwnPlayer_IsValid(const mtwnContext* context, mtwnPlayerId player)
{
    return context != NULL && FindPlayer(context, player) != NULL;
}

// What a control asks of its player.
typedef enum Control
{
    s_play,
    s_pause,
    s_reverse,
    s_finish,
    s_cancel,
    s_seek,
    s_seekProgress,
    s_seekLabel,
    s_setRate,
} Control;

static MTWN_NOINLINE mtwnResult ControlWorker(mtwnContext* context, mtwnClipPlay* play,
                                              Control control, double argument, uint32_t key)
{
    double now = Now(context);
    double end = EffectEnd(context, play);
    switch (control)
    {
    case s_play:
        return Play(play, now, end);
    case s_pause:
        return Pause(play, now, end);
    case s_reverse:
    {
        // Level 1's reverse: the negated rate pending, then a play with
        // auto-rewind; both take effect at the next update.
        mtwnClipPlay before = *play;
        play->pendingRate = -Effective(play);
        mtwnResult result = Play(play, now, end);
        if (result != mtwn_success)
        {
            *play = before;
        }
        return result;
    }
    case s_finish:
        return Finish(play, now, end);
    case s_cancel:
        ApplyPendingRate(play);
        play->pending = false;
        play->holdTime = (double)NAN;
        play->startTime = (double)NAN;
        play->previousTime = (double)NAN;
        return mtwn_success;
    case s_seekProgress:
        if (!isfinite(end))
        {
            return mtwn_errorState;
        }
        SetCurrent(play, now, end, argument * end);
        return mtwn_success;
    case s_seekLabel:
    {
        const mtwnClipSlot* clip = mtwnClipAt(context, play->clip.index1);
        for (uint32_t child = clip->nodes[0].firstChild; child != 0;
             child = clip->nodes[child - 1].nextSibling)
        {
            const mtwnClipNode* node = &clip->nodes[child - 1];
            if (node->kind == mtwn_nodeLabel && node->key == key)
            {
                SetCurrent(play, now, end, node->start);
                return mtwn_success;
            }
        }
        return mtwn_errorInvalid;
    }
    case s_setRate:
        SetRate(play, now, end, argument);
        return mtwn_success;
    default:
        SetCurrent(play, now, end, argument);
        return mtwn_success;
    }
}

static mtwnResult Apply(mtwnContext* context, mtwnPlayerId player, Control control, double argument,
                        uint32_t key)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnPlayerSlot* slot = FindPlayer(context, player);
    if (slot == NULL)
    {
        return Misuse(context, mtwn_errorStale);
    }
    if (slot->type != MTWN_PLAYER_CLIP)
    {
        return Misuse(context, mtwn_errorState);
    }
    if ((control == s_seek || control == s_seekProgress || control == s_setRate) &&
        !isfinite(argument))
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    mtwnFloatEnv env;
    mtwnFloatEnvEnter(&env);
    mtwnResult result = ControlWorker(context, &slot->play, control, argument, key);
    mtwnFloatEnvLeave(&env);
    return result == mtwn_success ? result : Misuse(context, result);
}

mtwnResult mtwnPlayer_Play(mtwnContext* context, mtwnPlayerId player)
{
    return Apply(context, player, s_play, 0.0, 0);
}

mtwnResult mtwnPlayer_Pause(mtwnContext* context, mtwnPlayerId player)
{
    return Apply(context, player, s_pause, 0.0, 0);
}

mtwnResult mtwnPlayer_Reverse(mtwnContext* context, mtwnPlayerId player)
{
    return Apply(context, player, s_reverse, 0.0, 0);
}

mtwnResult mtwnPlayer_Finish(mtwnContext* context, mtwnPlayerId player)
{
    return Apply(context, player, s_finish, 0.0, 0);
}

mtwnResult mtwnPlayer_Cancel(mtwnContext* context, mtwnPlayerId player)
{
    return Apply(context, player, s_cancel, 0.0, 0);
}

mtwnResult mtwnPlayer_Seek(mtwnContext* context, mtwnPlayerId player, double seconds)
{
    return Apply(context, player, s_seek, seconds, 0);
}

mtwnResult mtwnPlayer_SeekProgress(mtwnContext* context, mtwnPlayerId player, double progress)
{
    return Apply(context, player, s_seekProgress, progress, 0);
}

mtwnResult mtwnPlayer_SeekLabel(mtwnContext* context, mtwnPlayerId player, uint32_t key)
{
    return Apply(context, player, s_seekLabel, 0.0, key);
}

mtwnResult mtwnPlayer_SetRate(mtwnContext* context, mtwnPlayerId player, double rate)
{
    return Apply(context, player, s_setRate, rate, 0);
}

mtwnResult mtwnPlayer_GetState(mtwnContext* context, mtwnPlayerId player, mtwnPlayerState* stateOut)
{
    if (context == NULL)
    {
        return mtwn_errorInvalid;
    }
    mtwnPlayerSlot* slot = FindPlayer(context, player);
    if (slot == NULL)
    {
        return Misuse(context, mtwn_errorStale);
    }
    if (stateOut == NULL)
    {
        return Misuse(context, mtwn_errorInvalid);
    }
    if (slot->type != MTWN_PLAYER_CLIP)
    {
        return Misuse(context, mtwn_errorState);
    }
    const mtwnClipPlay* play = &slot->play;
    double now = Now(context);
    double end = EffectEnd(context, play);
    *stateOut = (mtwnPlayerState){.currentTime = CurrentTime(play, now),
                                  .rate = Effective(play),
                                  .duration = end,
                                  .state = StateOf(play, now, end),
                                  .pending = play->pending || !isnan(play->pendingRate)};
    return mtwn_success;
}
