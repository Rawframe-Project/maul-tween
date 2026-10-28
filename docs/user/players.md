# Players, events and layers

A player plays a finished [clip](clips.md) with its slots bound to targets, by Web Animations' procedures on the host's clock. Players report what they cross as event records, and layer over each other by composite operations and weights.

## Contents

- [Playing](#playing)
- [A rate that eases](#a-rate-that-eases)
- [Scroll-driven playback](#scroll-driven-playback)
- [Events](#events)
- [Layers](#layers)
- [Not obvious](#not-obvious)

## Playing

```c
mtwnTargetId slide;
mtwnResult playing = mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){0}, &slide);
mtwnBinding bindings[2] = {mtwnBind(0, opacity), mtwnBind(1, slide)};
mtwnPlayerDef play = mtwnDefaultPlayerDef();
play.clip = clip;
play.bindings = bindings;
play.bindingCount = 2;
play.keep = true; // kept when finished, to play again
mtwnPlayerId intro;
playing = mtwnCreatePlayer(context, &play, &intro); // plays from the next update
playing = mtwnPlayer_Pause(context, intro);
playing = mtwnPlayer_Seek(context, intro, 0.2);
playing = mtwnPlayer_SetRate(context, intro, -1.0); // backwards
playing = mtwnPlayer_Play(context, intro);
```

A player plays, pauses, reverses, finishes, cancels and seeks by time, by progress (`mtwnPlayer_SeekProgress`) or to a label of the clip's root (`mtwnPlayer_SeekLabel`). A play that needs a start time takes the next update's, as Web Animations' pending play waits for the ready time; pause, seek, finish and cancel take effect at once. `mtwnPlayer_GetState` reads its time, rate, duration and play state (idle, running, paused, finished).

A player not kept commits what it fills forward as its targets' bases when it finishes, and frees itself, its id stale. A kept player stays, finished, to be played again. A slot the bindings leave out plays on nothing.

## A rate that eases

A rate set each frame eases a player, as GSAP tweens a timeline's `timeScale` to a stop: a transition on a scalar target runs the rate, and after each update the host gives the player what it reached. The player keeps its time at every change, so it slows without a jump and holds at 0. A moving player takes a new rate at the next update, as Web Animations' seamless change does, so it runs a frame behind the transition:

```c
// After each update: the rate the speed transition has reached.
mtwnVec4 rate;
made = mtwnTarget_GetValue(context, speed, &rate);
made = mtwnPlayer_SetRate(context, player, (double)rate.x);
```

Where it stops then depends on the frame rate: a falling rate overshoots by about a frame and a half at its start speed (2.5 of 133.3 at 60 frames a second, 1.0 at 144). A program that replays or keeps hosts in step uses the exact form instead: the player paused where the ramp starts and swept each frame to the time the eased speed has travelled, `t0 + r0 t + (r1 - r0) d E(t / d)` for a rate eased from `r0` to `r1` over `d` by an easing whose integral is `E`. For 1 to 0 over a second by a quadratic out, `E(u)` is `(1 - (1 - u)^3) / 3`:

```c
// Exact at any frame rate: paused where the ramp starts, swept
// each frame to the time the eased speed has travelled.
double u = fmin(seconds - rampStart, 1.0);
made = mtwnPlayer_Sweep(context, player, rampTime + (1.0 - pow(1.0 - u, 3.0)) / 3.0);
```

## Scroll-driven playback

A paused player swept to the scroll's progress before each update plays as CSS's scroll timelines do. `mtwnPlayer_Sweep` crosses everything between, so marks and starts give their records as they are scrolled past, either way. To smooth it, as GSAP's `scrub: 0.5` does, the playhead chases the scroll on a spring the host retargets at each scroll event, from where it is with the velocity it has, so a new event never jolts it:

```c
// At a scroll event: the chase from where it is, toward the new progress.
double offset = 0.0;
double speed = 0.0;
mtwnResult chased = mtwnSampleSpring(&chase, away, pace, now - since, &offset, &speed);
away = goal + offset - scrolled; // the same place, against the new goal
pace = speed;
goal = scrolled;
since = now;
```

```c
// Each frame, before the update: the paused player swept to the chase.
chased = mtwnSampleSpring(&chase, away, pace, now - since, &offset, NULL);
mtwnResult swept = mtwnPlayer_Sweep(context, reader, (goal + offset) * length);
```

## Events

What a player's time crosses between updates is recorded exactly, however far it moved: a track's or timeline's start and end, each iteration, each mark, and the player's own seeks and cancels. A transition records its start when its delay ends and its end when it commits. Each record says when it happened in host time and in clip time, how many times it was crossed, which way, and in which iterations.

```c
mtwnEvent events[32];
uint32_t count = 0;
uint64_t dropped = 0;
mtwnResult read = mtwnReadEvents(context, events, 32, &count, &dropped);
for (uint32_t i = 0; i < count; ++i)
{
    if (events[i].kind == mtwn_eventMark && events[i].key == 7)
    {
        // Play the sound: crossed events[i].count times since the last read.
    }
}
```

Records are never callbacks: they wait in the context's queue until the host reads them, sorted by host time, then the players' creation order, then the clip's order. An update makes at most one record per player, node or mark, kind and direction, with the count of crossings, so a clip that loops a thousand times in one frame costs one record.

## Layers

Players write their targets in the order they were made. A binding replaces what lies under it or adds to it, at a weight, so one clip can play over another: an idle loop with a hit added, or two clips cross-faded by their weights.

```c
mtwnBinding layer = mtwnBind(0, opacity);
layer.composite = mtwn_compositeAdd; // added to what lies under it
layer.weight = 0.5f;
play.bindings = &layer;
play.bindingCount = 1;
mtwnPlayerId pulse;
mtwnResult layered = mtwnCreatePlayer(context, &play, &pulse);
layered = mtwnPlayer_SetWeight(context, pulse, 0.25f); // the whole player's weight
```

What lies under a player is what earlier players wrote in this update, or the target's base, so an added layer never compounds on its own last frame. A target bound to a clip's player that no player writes in an update shows its base.

## Not obvious

- A seek jumps: what lies between is not crossed, and a seek record says so. `mtwnPlayer_Sweep` and `mtwnPlayer_Finish` cross it, their records at once.
- An iteration record is made where an iteration's start is reached and names the iteration current there: the one entered going forwards, the one left going backwards (the one before it follows just after).
- The queue holds the context def's `eventCapacity` records. A record arriving at a full queue is dropped and counted, never silently: `dropped` gives those since the last read, `mtwnContext_GetDroppedEvents` all of them. A host that reads no events sets `eventCapacity` to 0: no records are made, and clips' players skip the work of finding their crossings, which is most of a frame where clips repeat.
- A rate is 0 or from `MTWN_MIN_RATE` to `MTWN_RATE_LIMIT` (2^-20 to 2^20) in size; another is refused with `mtwn_errorLimit`, NaN with `mtwn_errorInvalid`. A rate change keeps the current time as Web Animations does, through the start time, so at that limit and 10^7 seconds of host time the time is kept to 2 ms.
- A seek's time, a share of the duration included, is at most `MTWN_SEEK_LIMIT` (2^1000) in size, and so is a clip's finite end; past it, a seek or a node is refused with `mtwn_errorLimit`. With the rates' bounds this keeps every start time finite.
- A finished player held at its end that is given a lower rate plays again from its end, as Web Animations' steps and Firefox do (Chrome keeps it finished). Set the rate before the end to stay finished.
- Colors add premultiplied by alpha; an added rotation turns the one under it; an integer composes unrounded and rounds once; a discrete value is not additive and steps at half its weight.
