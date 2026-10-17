# Maul Tween guide

This guide walks through the library in the order a program meets it:
curves and values on their own, then a context of targets that
transitions move, then clips of tracks on timelines, players that play
them, the events they report and the ways they layer. Every function
named here is described in full in [the API reference](api.md); the
design records behind each part are in `adr/`. The programs in
`samples/` are built from the steps below; [the samples page](samples.md)
says what each shows.

## The ground rules

- **Host time.** Nothing runs by itself. The host calls `mtwnUpdate`
  with its own clock in nanoseconds, never going back, and every
  transition, clip and event follows that time: a paused game, a
  slowed one or a recorded replay is the host's choice of clock.
- **Two layers.** Easings, springs, decays and the interpolation of
  colors, angles and rotations are plain functions on values, with no
  context and no allocation (`easing.h`, `spring.h`, `interpolate.h`,
  `path.h`, `sample.h`, `timing.h`). The context (`context.h` and the
  rest) holds targets, transitions, clips and players.
- **Results.** Every fallible function returns `mtwnResult`: zero is
  success, negative values are errors (`mtwn_errorInvalid`,
  `mtwn_errorStale`, `mtwn_errorLimit`, `mtwn_errorCapacity`,
  `mtwn_errorState`, `mtwn_errorVersion`), and `mtwnResultName` names
  any of them. They are marked `[[nodiscard]]` where the compiler
  supports it. A refused call changes nothing, and a context counts
  the calls refused as misuse (`mtwnContext_GetMisuseCount`).
- **Defs and limits.** A context, a clip, a track, a timeline, a
  transition and a player are made from a def you get from its
  `mtwnDefault...Def` function and adjust. The defs carry named
  capacities: the context allocates them all when it is made, and a
  full pool refuses with `mtwn_errorLimit`, never by growing behind
  your back. `mtwnGrowContext` grows it when you ask.
- **Versions.** The defaults are built in your program from the headers
  it includes and stamp their version into the def; a library of
  another minor version refuses it with `mtwn_errorVersion`, so always
  start a def from its default.
- **Ids.** Targets, players and clips are named by ids of an index and
  a generation. An id of something destroyed is stale: functions given
  it return `mtwn_errorStale` and never touch another object that took
  its slot.
- **Memory.** A context def has an allocator; zeroed, the C library's
  is used. The context is one block; each clip is one more, counted
  against the context's clip storage. Nothing else is allocated after.
- **Threads.** The library starts none. A context and everything in it
  is used by one thread at a time; the values layer may be used from
  any thread.
- **Determinism.** The library computes in its own floating-point
  environment (round to nearest, denormals kept) whatever the host's
  is, with its own exponentials and trigonometry, so the same calls
  give the same bits on every platform and compiler. The tests check
  one hash of a scenario of every part on all of them.

## Easing

An easing maps progress from 0 to 1 onto eased progress. Every kind is
made by a function that checks its parameters: CSS's keywords and
`cubic-bezier()`, `steps()` and `linear()`, Penner's family (power,
sine, expo, circ, back, elastic, bounce) in every mode, GSAP's
parameters, or a host function.

```c
mtwnEasing ease;
mtwnResult made = mtwnMakeCssEasing(mtwn_cssEaseInOut, &ease); // CSS's ease-in-out
double eased = mtwnEase(&ease, 0.25, false);                   // a quarter of the way

mtwnEasing back;
made = mtwnMakeBackEasing(mtwn_easeOut, 1.70158, &back); // GSAP's back.out
```

CSS's easings take progress outside 0 to 1 as CSS defines them there;
the others clamp it. The last argument of `mtwnEase` says, for
`steps()`, whether the progress is reached from before. `mtwnSampleEasing` fills a table of
evenly spaced samples, and `mtwnFitLinearEasing` turns any easing into
CSS `linear()` points within a tolerance, to hand to a browser or
another engine. A fit makes at most `MTWN_MAX_FIT_POINTS` points: a
tolerance the curve cannot meet (below its rounding) ends with
`mtwn_errorLimit` rather than with millions of them.

## Springs and decays

A spring is made from stiffness, damping and mass, from a frequency
and damping ratio, or from SwiftUI's perceptual duration and bounce.
It is sampled from a start's offset and velocity, analytically: no
steps, the same answer at any frame rate.

```c
mtwnSpring spring;
mtwnResult made = mtwnMakeSpringFromDuration(0.4, 0.2, &spring); // 0.4 s, a little bounce
double offset = 0.0;
double speed = 0.0;
mtwnSampleSpring(&spring, -1.0, 0.0, 0.1, &offset, &speed); // 0.1 s after starting 1 short
double settled = mtwnSpringSettlingTime(&spring, -1.0, 0.0, 1e-3, 1e-2);
```

`mtwnSpringSettlingTime` gives when the motion stays within both
tolerances for good, by bounds that never end it early. A decay
(`mtwnMakeDecay`, or `mtwnMakeDecayFromDeceleration` with iOS's rates)
coasts from a velocity; `mtwnDecayStopTime` and `mtwnDecayCrossingTime`
say when it stops and when it passes a point, and `mtwnRubberBand`
gives iOS's resistance past an edge.

## Values

A target holds one kind of value in up to four channels (`mtwnVec4`):
numbers and vectors, colors with straight alpha, angles in radians,
rotations as quaternions (x, y, z, w), integers and discrete values.
The interpolations are functions too:

```c
mtwnColor red = {1.0f, 0.0f, 0.0f, 1.0f};
mtwnColor blue = {0.0f, 0.0f, 1.0f, 1.0f};
mtwnColorMix mix;
mtwnResult made = mtwnMakeColorMix(red, blue, mtwn_colorOklch, mtwn_hueShorter, &mix);
mtwnColor between = mtwnMixColor(&mix, 0.5); // half way, in OkLCh
```

Colors mix in Oklab, OkLCh, linear sRGB or sRGB, premultiplied by
alpha, with CSS's hue methods; angles by CSS's methods too
(`mtwnMixAngle`); rotations along the shortest or longest arc
(`mtwnMakeRotationMix`, `mtwnMixRotation`). Keyframes of any kind make
paths (`mtwnMakePath`, `mtwnMakeTrack`): linear, stepped, Bezier or
Catmull-Rom segments, at constant speed if asked.

## A context and its targets

The context holds everything the library animates. Make one from its
def, then a target for each value the host shows: an opacity, a
position, a color.

```c
mtwnContextDef def = mtwnDefaultContextDef(); // capacities, allocator, version
def.targetCapacity = 4096;
mtwnContext* context = NULL;
mtwnResult made = mtwnCreateContext(&def, &context);

mtwnTargetId opacity;
made = mtwnCreateTarget(context, mtwn_valueScalar, (mtwnVec4){1.0f, 0, 0, 0}, &opacity);
```

A target has a base value, which `mtwnTarget_SetBase` sets and an
animation composes over, and a current value with its velocity, which
`mtwnTarget_GetValue`, `mtwnTarget_GetState` and `mtwnReadTargets`
read. `mtwnDestroyContext` gives everything back at once.

## Transitions

A transition moves one target to a value: eased over a duration, on a
spring, or coasting as a decay. It starts from the value the target has
now, so starting one on a moving target replaces the one moving it.

```c
mtwnTransitionDef fade = mtwnDefaultTransitionDef();
fade.target = opacity;
fade.value = (mtwnVec4){0.0f, 0, 0, 0};
fade.duration = 0.3;
mtwnResult started = mtwnMakeCssEasing(mtwn_cssEaseOut, &fade.easing);
mtwnPlayerId fading;
started = mtwnStartTransition(context, &fade, &fading);
```

A spring or decay starts with the target's velocity, so an interrupted
motion carries on without a jolt (or with one you give):

```c
mtwnTransitionDef settle = mtwnDefaultTransitionDef();
settle.target = opacity;
settle.value = (mtwnVec4){1.0f, 0, 0, 0};
settle.spec = mtwn_transitionSpring; // from the value and velocity it has now
mtwnResult sprung = mtwnMakeSpringFromDuration(0.5, 0.0, &settle.spring);
mtwnPlayerId settling;
sprung = mtwnStartTransition(context, &settle, &settling);
```

A delay holds the start; colors move through their color space,
rotations along their arc, angles by their method. At its end a
transition commits its value as the target's base and frees itself.
An eased transition with `reverses` set is shortened when it reverses
one still running, as CSS Transitions shortens it: a hover left a
quarter of the way in returns in a quarter of the time.

Under reduced motion every transition, and every playing clip with an
end, finishes at its next update, its records at that update's time,
as if it had played; one marked essential plays on:

```c
bool reduce = true; // the platform's preference, as the host reads it
mtwnResult set = mtwnContext_SetReducedMotion(context, reduce);
fade.essential = true; // an opacity change is not motion: it may play on
```

The library cannot tell motion from a fade, so marking is the host's.
A looping clip has no end to finish at; the host keeps it, seeks it to
a still frame or stops it.

## The update

Each frame the host updates the context to its clock and redraws the
targets whose value changed, which the update reports:

```c
mtwnTargetId changed[64];
uint32_t total = 0;
mtwnResult updated = mtwnUpdate(context, nowNs, changed, 64, &total);
for (uint32_t i = 0; i < total && i < 64; ++i)
{
    mtwnVec4 value;
    if (mtwnTarget_GetValue(context, changed[i], &value) == mtwn_success)
    {
        // Show value on whatever changed[i] stands for.
    }
}
```

`total` may exceed the array; those past it are left out of the report
only. A context at rest reports nothing and costs next to nothing.

## Clips

A clip is a definition: tracks of keyframes on timelines, made once
and played by any number of players. Its storage is one block, sized
by its def; `mtwnGetClipBytes` says how much of the context's clip
storage a def takes.

```c
mtwnClipDef clipDef = mtwnDefaultClipDef();
uint64_t bytes = 0;
mtwnResult built = mtwnGetClipBytes(&clipDef, &bytes); // its share of the clip storage
mtwnClipId clip;
built = mtwnCreateClip(context, &clipDef, &clip);

mtwnKeyframe keys[2] = {0};
keys[1].value = (mtwnVec4){1.0f, 0, 0, 0};
keys[1].offset = 1.0;
keys[0].easing = keys[1].easing = mtwnLinearEasing();
mtwnClipTrackDef track = mtwnDefaultClipTrackDef(); // on the root at 0, slot 0, a number
track.keyframes = keys;
track.keyframeCount = 2;
track.timing.duration = 0.4;
mtwnNode fadeIn;
built = mtwnClip_AddTrack(context, clip, &track, &fadeIn);

track.slot = 1;
track.position.kind = mtwn_positionAfterPrevious; // when the fade ends
mtwnNode slideIn;
built = mtwnClip_AddTrack(context, clip, &track, &slideIn);
built = mtwnClip_Finish(context, clip);
```

A track's slot is the binding a player gives it a target through.
Positions place a child at a time, after the parent's end so far, with
or after the previous child, or at a label, each with an offset, as
GSAP's position parameter does. `mtwnClip_AddTimeline` nests a group
with timing of its own (delay, iterations, direction, fill, rate and an
easing of its child time, Web Animations' model); `mtwnClip_AddLabel`
names a time to seek to, and `mtwnClip_AddMark` a time to be told of.
`mtwnClip_Finish` closes the clip; a finished clip is read only.

## Staggers

A stagger spreads offsets over items by their distance from an
origin: the first or last item, the centre, the edges, an index, a
point of the grid, or an order from a seed.

```c
mtwnStagger stagger = mtwnDefaultStagger(); // 0.1 s apart, from the first
stagger.columns = 4;                        // a grid of four columns
stagger.origin = mtwn_staggerCentre;        // spreading from its centre
double delays[12];
mtwnResult spread = mtwnComputeStagger(&stagger, 12, delays);
```

The offsets go where a time does: a track's position offset, a
transition's delay. Items one cell apart are the spacing apart, or the
spacing is the whole spread (`mtwn_staggerAmount`); an axis measures
along the columns or the rows only, and an easing redistributes the
offsets. They may be values as well as times, such as a scale from
item to item.

Porting from GSAP: a grid with a short last row stays its columns wide,
its centre and edges the full rectangle's. GSAP's `distribute` puts the
centre row at `count * 0.5 / columns - 0.5`, so for seven items in
three columns its centre is 0.67 rows down where here it is 1, and its
"edges" is the centre's distance reversed where here it is the nearest
edge of the rectangle as drawn.

## Players

A player plays a clip with its slots bound to targets. It follows Web
Animations' procedures: it plays from the next update, pauses, plays
backwards, seeks, finishes and cancels, and a rate changes how fast
and which way its time runs.

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

`mtwnPlayer_GetState` reads its time, rate, duration and play state. A
player not kept commits its last values as the targets' bases when it
finishes and frees itself; a target no player writes shows its base.

## Events

What a player's time crosses between updates is recorded, exactly,
however far it moved: a track's or timeline's start and end, each
iteration, each mark, and the player's own seeks and cancels. Each
record says when it happened in host time and in clip time, how many
times it was crossed, which way, and in which iterations. An iteration
record is made when an iteration's start is reached and names the
iteration current there: the one entered going forwards, the one left
going backwards (the one before it follows just after).

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

The queue holds the context def's `eventCapacity` records, sorted by
host time. A record arriving at a full queue is dropped and counted,
never silently: `dropped` gives those since the last read.

## Layers

Players write their targets in the order they were made. A binding
replaces what lies under it or adds to it, at a weight, so one clip can
play over another: an idle loop with a hit added, or two clips cross
faded by their weights.

```c
mtwnBinding layer = mtwnBind(0, opacity);
layer.composite = mtwn_compositeAdd; // added to what lies under it
layer.weight = 0.5f;
play.bindings = &layer;
play.bindingCount = 1;
mtwnPlayerId pulse;
mtwnResult layered = mtwnCreatePlayer(context, &play, &pulse);
layered = mtwnPlayer_SetWeight(context, pulse, 0.25); // the whole player's weight
```

A track whose def asks for `incremental` moves on each iteration by
its last keyframe minus its first, so a repeated step climbs and a
repeated spin turns on.

## Threads

Clips' players can run on the host's task system, the same hook pair
Maul VFX takes. The host hands the library a function that runs a
range of work items, on any thread and in any order:

```c
static void* RunNow(mtwnTaskFn* task, int32_t itemCount, int32_t minRange, void* taskContext,
                    void* userContext)
{
    (void)minRange;
    (void)userContext;
    task(0, itemCount, taskContext); // a real host splits it over its workers
    return NULL;                     // ran it all: nothing to wait for
}

static void WaitFor(void* userTask, void* userContext)
{
    (void)userTask;
    (void)userContext;
}
```

and sets both in the def before making the context:

```c
mtwnContextDef threaded = mtwnDefaultContextDef();
threaded.enqueueTask = RunNow;
threaded.finishTask = WaitFor;
threaded.userTaskContext = NULL; // passed to both, the host's own
mtwnContext* parallel = NULL;
mtwnResult madeThreaded = mtwnCreateContext(&threaded, &parallel);
```

Each update then hands the host up to 16 groups of the clips' players,
in the order they were made. A group advances its players with its own
scratch and holds what they make; afterwards the update composes it
into the targets in creation order, between the transitions. Records,
values, velocities and the changed list are the same bits as without
the hooks, whatever the split and the order. The groups cost memory
(about 900 KB more for the default def, see `mtwnContextDef`), so a
host with a few players leaves the hooks unset. Transitions, and
targets several players write, are composed on the calling thread; a
target one player alone writes, with no transition on it, is composed
in that player's group (record mtwn-0056). Before that, on a 12-core
desktop at load 5 to 7, 1,000 players of four tracks updated about 2.4
times faster on six workers and the frame where they all repeat about
3.7 times on eight, the composing on the calling thread bounding both;
that serial rest is about four times shorter since.

Some players stay on the calling thread. One whose clip has more than
128 nodes and other players plays there wholly: a group keeps times for
128, and only a clip's one player uses the clip's own. A group
stages up to 1,024 records, or the queue's capacity when that is
smaller: with a queue of 1,024 or less, records past it are dropped
and counted, as the queue would drop them; with a larger queue, a
player whose records do not fit is put back, with the players after it
in its group. The bench's 1,000 players stage at most 567 records a
group in the frame where they all repeat. A player with targets of its
own whose results might not fit its group's staging plays on the
calling thread before it starts, as what a group writes in place
cannot be undone. `mtwnContext_GetSerialAdvances` counts these
advances since the context was made; when it grows, larger binding or
event capacities, or smaller clips, keep the players in their groups.

Players of one clip at the same time and rate share their sample and
records (record mtwn-0057). The sharing holds `sharedNodeCapacity`
nodes and `sharedRecordCapacity` records on the calling thread and in
each group, 14,848 bytes each at the defaults; a host whose players
never run in step sets both to 0 and holds none of it.

## Growing

Capacities are set when the context is made. When the host outgrows
them, it grows the context, once, at a moment it chooses:

```c
mtwnContextDef bigger = def;
bigger.targetCapacity *= 2;
bigger.playerCapacity *= 2;
mtwnResult grown = mtwnGrowContext(&context, &bigger); // it may move; ids stay valid
```

Every id stays valid and everything playing plays on; the context's
address may change, so the call writes it back. On a refusal or a
failed allocation nothing changes.
