# Transitions

A context holds the values a program animates as targets; a transition moves one target to a new value in one call; each frame the host updates the context to its clock and redraws what changed. This is the short path: [clips](clips.md) are for animations written ahead of time.

## Contents

- [A context and its targets](#a-context-and-its-targets)
- [Starting a transition](#starting-a-transition)
- [Duration by distance](#duration-by-distance)
- [The update](#the-update)
- [Reduced motion](#reduced-motion)
- [Not obvious](#not-obvious)

## A context and its targets

```c
mtwnContextDef def = mtwnDefaultContextDef(); // capacities, allocator, version
def.targetCapacity = 4096;
mtwnContext* context = NULL;
mtwnResult made = mtwnCreateContext(&def, &context);

mtwnTargetId opacity;
made = mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){1.0f, 0, 0, 0}, &opacity);
```

The default def is for a medium host: 1024 targets, 256 players (transitions included) with 64 points of `linear()` for each, 16 clips, 256 event records, 1024 bindings and the C library's allocator. Every capacity is allocated when the context is made; a full one refuses with `mtwn_errorCapacity`. `mtwnContext_GetMemoryBytes` says how large the block is, and `mtwnDestroyContext` gives everything back at once.

A target has a base value, which `mtwnTarget_SetBase` sets and animations compose over, and a current value with its velocity, which `mtwnTarget_GetValue`, `mtwnTarget_GetState` and `mtwnContext_ReadTargets` read. A target no animation writes shows its base.

## Starting a transition

A transition is eased over a duration, on a spring, or coasting as a decay. It starts at the next update from the value the target has then, so starting one on a moving target replaces the transition moving it, from where it got to.

```c
mtwnTransitionDef fade = mtwnDefaultTransitionDef();
fade.target = opacity;
fade.value = (mtwnVec4){0.0f, 0, 0, 0};
fade.duration = 0.3;
mtwnResult started = mtwnMakeCssEasing(mtwn_cssEaseOut, &fade.easing);
mtwnPlayerId fading;
started = mtwnStartTransition(context, &fade, &fading);
```

A spring or a decay starts with the target's velocity, so an interrupted motion carries on without a jolt (or with a velocity the def gives):

```c
mtwnTransitionDef settle = mtwnDefaultTransitionDef();
settle.target = opacity;
settle.value = (mtwnVec4){1.0f, 0, 0, 0};
settle.kind = mtwn_transitionSpring; // from the value and velocity it has now
mtwnResult sprung = mtwnMakeSpringFromDuration(0.5, 0.0, &settle.spring);
mtwnPlayerId settling;
sprung = mtwnStartTransition(context, &settle, &settling);
```

A fling coasts as a decay from the gesture's velocity, with no end value. Bounded, it bounces back from the content's ends on the def's spring, handing over at the exact time it reaches one; with `lands` it comes to rest on the def's value instead, an end the host snapped from the natural one (see [physics.md](physics.md#decay-and-inertia)):

```c
mtwnTransitionDef fling = mtwnDefaultTransitionDef();
fling.target = scroll;
fling.kind = mtwn_transitionDecay;
fling.velocitySource = mtwn_velocityGiven;
fling.velocity = (mtwnVec4){1500.0f, 0, 0, 0}; // from the gesture, a second
fling.bounded = true;                          // the content's ends
fling.lower = (mtwnVec4){0.0f, 0, 0, 0};
fling.upper = (mtwnVec4){600.0f, 0, 0, 0};
mtwnPlayerId flinging;
mtwnResult flung = mtwnStartTransition(context, &fling, &flinging);
```

At its end (the duration, a spring's settling time, a decay's stop) a transition commits its end value as the target's base with velocity 0 and frees itself; its id goes stale. `mtwnDestroyPlayer` stops one without committing, and the target shows its base again. A transition that has ended is gone like any destroyed object, and a stale id given to `mtwnDestroyPlayer` is refused and counted as misuse. So a host that may stop a transition after its end asks `mtwnPlayer_IsValid` first. Starting a new transition on the target replaces the running one, ended or not, without that question.

## Duration by distance

A long move and a short one need not take the same time. `mtwnContext_MeasureTransition` gives how far a def would move its target, in its kind's own measure (a color's perceptual difference, deltaEOK; a rotation's angle; a scale's ratio as a logarithm), and the host sets the duration from its speed, within bounds of its choosing:

```c
mtwnTransitionDef move = mtwnDefaultTransitionDef();
move.target = opacity;
move.value = (mtwnVec4){0.25f, 0, 0, 0};
double distance = 0.0;
mtwnResult measured = mtwnContext_MeasureTransition(context, &move, &distance);
move.duration = fmin(fmax(distance / 2.0, 0.15), 0.4); // 2 a second, 150 to 400 ms
```

## The update

Each frame the host updates the context to its clock and redraws the targets whose value or base changed, which the update reports once each, in the order they changed:

```c
mtwnTargetId changed[64];
uint32_t total = 0;
mtwnResult updated = mtwnContext_Update(context, nowNs, changed, 64, &total);
for (uint32_t i = 0; i < total && i < 64; ++i)
{
    mtwnVec4 value;
    if (mtwnTarget_GetValue(context, changed[i], &value) == mtwn_success)
    {
        // Show value on whatever changed[i] stands for.
    }
}
```

`total` may exceed the array; the targets past it are left out of the report only. A context at rest reports nothing and costs next to nothing. The host's time may never go back: an earlier time is refused and nothing advances.

## Reduced motion

Under reduced motion every transition, and every playing clip with an end, finishes at its next update, its delay skipped and its records at that update's time, as if it had played. One marked essential plays on:

```c
bool reduce = true; // the platform's preference, as the host reads it
mtwnContext_EnableReducedMotion(context, reduce);
fade.essential = true; // an opacity change is not motion: it may play on
```

The library cannot tell motion from a fade, so marking is the host's. A looping clip has no end to finish at: `mtwnContext_GetPlayingOnCount` counts the players that play on, and the host keeps, seeks or stops each.

## Not obvious

- Colors move through their color space and rotations along their arc; a spring on them springs the path's progress, so it overshoots along the path. A scale springs its ratio and decays in its logarithm. Discrete values take only easings; colors and rotations take no decay.
- An angle's end moves by its method's whole turns once, when the transition starts, and that end is committed, so nothing jumps by a turn at the end.
- With `reverses`, an eased transition that sends a target back to where the one it replaces began is shortened as CSS Transitions shortens it: a hover left a quarter of the way in returns in a quarter of the time.
- A value shown on a grid (whole pixels, a snap step) is rounded where the host shows it, skipping the write when the shown value did not change. The target keeps the exact value, which composition, velocities and springs need. A count that steps by whole numbers is an `mtwn_valueInteger` target.
- A value driven past the floats' range is held at the largest finite float, still while held; a target's value and velocity are always finite.
- A transition copies its `linear()` easing's points into the context when it starts, so the host's array may change or go afterwards. Each transition holds up to the context def's `pointsPerTransition` points, 64 by default: 16 bytes each, for every player, 256 KiB for the default 256 players. One with more is refused with `mtwn_errorCapacity`. A longer table, such as Motion's spring sampled every 10 ms past 0.64 s, belongs in a clip, whose `pointCapacity` the host sizes; a host using no `linear()` transitions can set 0.
