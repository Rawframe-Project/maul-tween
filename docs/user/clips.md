# Clips and timelines

A clip is an animation written ahead of time: tracks of keyframes on nested timelines, with labels to seek to and marks to be told of. It is built once, sealed, and then played by any number of [players](players.md), each on its own targets.

## Contents

- [Building a clip](#building-a-clip)
- [Positions](#positions)
- [Timing and timelines](#timing-and-timelines)
- [Staggers](#staggers)
- [Not obvious](#not-obvious)

## Building a clip

A clip's storage is one block sized by its def, counted against the context's clip storage; `mtwnContext_GetClipBytes` says how much a def takes, so a host can grow the context's `clipStorageBytes` with `mtwnContext_Grow` for the clips it makes. It takes the context, like every call that reads a def, so that only a library whose ABI the context def matched reads it.

```c
mtwnClipDef clipDef = mtwnDefaultClipDef();
uint64_t bytes = 0;
// Its share of the clip storage.
mtwnResult built = mtwnContext_GetClipBytes(context, &clipDef, &bytes);
mtwnClipId clip;
built = mtwnCreateClip(context, &clipDef, &clip);

mtwnKeyframe keys[2] = {0};
keys[1].value = (mtwnVec4){1.0f, 0, 0, 0};
keys[1].offset = 1.0;
keys[0].easing = keys[1].easing = mtwnMakeLinearEasing();
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
built = mtwnClip_Seal(context, clip);
```

A track writes a slot, which a player binds to a target; a slot keeps one value kind. The track copies its keyframes into the clip. `mtwnClip_Seal` closes the clip: from then on it is read only, can be played, and its tracks can be read (`mtwnClip_GetTrack`) and sampled through the values layer from any thread while the clip lives.

## Positions

Each child is placed when it is added, as GSAP's position parameter places it, with the differences below, and the clip keeps only the resulting start times:

| Position | Starts |
|---|---|
| `mtwn_positionAbsolute` | At the offset. |
| `mtwn_positionAfterEnd` | At the parent's end so far, plus the offset. |
| `mtwn_positionWithPrevious` | With the previous child's start, plus the offset. |
| `mtwn_positionAfterPrevious` | At the previous child's end, plus the offset. |
| `mtwn_positionAtLabel` | At a label of the same parent, plus the offset. |

The previous child is the last track or timeline added to the parent; labels and marks are not children for this, and do not lengthen their parent. A position that resolves to no finite start, such as after the end of an endless timeline, is refused.

Porting from GSAP: a clip places each child where GSAP 3.15.0's timeline does once a port translates these (`test/test_rival_position.c` checks scripts of children, labels and callbacks against GSAP's own start times):

- A child's `delay` moves its start in GSAP, so a `"<"` after it starts after the delay. Port the delay as part of the position's offset, not as the timing's `delay`, which stays inside the node as in Web Animations; then with the previous child's start is GSAP's `"<"`. `repeat: n` is `n + 1` iterations (`repeat: -1` infinite), `repeatDelay` the repeat delay and a time scale the playback rate.
- GSAP's `"+=50%"`, `"-=50%"`, `"<+=50%"`, `">+=50%"` and `"label+=50%"` are that share of the inserted child's total duration, all its iterations and repeat delays before its time scale, and `"<50%"` and `">50%"`, without `=`, of the previous child's. Port them as that many seconds.
- A callback (`call()`) is a child of no duration in GSAP: it is the previous child for `"<"` and `">"`, and it lengthens the timeline to its time. A mark is neither, so port a callback as a mark and a label at its position, and place at that label a child GSAP puts at `"<"` or `">"` right after it, or at the end while the callback lies past every child's end.
- Once a child is endless, GSAP's end, for the default position, `"+="`, `"-="` and a label it adds, is the first iteration end of the child added last, whichever that is: its start plus its duration over its time scale, or a callback's time. Here the end of an endless parent is no finite start, and the add is refused, so port it as with the previous child's start plus that.
- GSAP's `">"` is the previous child's end, its repeats included, as after the previous child's end here; only where that child is itself endless is it its first iteration end. Here that one is refused, so port it as with the previous child's start plus its first iteration.
- A label GSAP does not know yet is added at its end, and the child goes there. `mtwn_positionAtLabel` refuses it, so add the label after the end first.
- GSAP moves every child, but not its labels, so that none starts before 0, at its next string position or render: with 1 s children, `"-=2"` after the first puts the second at 0 and the first at 1. Here a child keeps a start before 0, and at the clip's time 0 it is already that far in: a 1 s track at -0.5 shows its value at 0.5 s first, its first half never plays, and its clip lasts 0.5 s. A port that wants GSAP's layout starts its children that much later itself.

Everything else agrees: absolute starts, the end, the previous child's start and end, labels with offsets, rates and repeats, and a nested timeline that grows after a later sibling is placed, which keeps its start.

## Timing and timelines

Every track and timeline has a Web Animations timing (`mtwnTiming`): delay and end delay, iterations (fractional or infinite) from an iteration start, duration, direction, fill and an easing, plus a playback rate of its own, a repeat delay between iterations and a choice of how reverse iterations ease (time running back, Web Animations' way, or the shape repeated). `mtwnComputeTiming` gives the phase, iteration and progress at a local time for a host that samples on its own. It returns a status: it refuses a timing `mtwnValidateTiming` refuses and a local time that is NaN or negative infinity, leaving its result as it was.

`mtwnClip_AddTimeline` nests a group with timing of its own. By default its duration is its latest child's end, kept as children arrive (endless after an endless child); with `autoDuration` off its timing's duration clips them. Timelines nest at most `MTWN_MAX_CLIP_DEPTH` (32) below the root. `mtwnClip_AddLabel` names a time to seek to, and `mtwnClip_AddMark` an instant that gives an event each time playback crosses it.

Tracks compose where one helper would elsewhere. DOTween's DOJump is a clip of three tracks: x straight across, y along the line to its end height, and the hops, added over that line, repeating up and down:

```c
// Three hops of 50 over 1.2 s: up and down each 0.4 s, eased out
// going up and back the same way down (DOTween's DOJump).
mtwnKeyframe hop[2] = {0};
hop[1].offset = 1.0;
hop[1].value = (mtwnVec4){50.0f, 0, 0, 0};
made = mtwnMakePowerEasing(mtwn_easeOut, 2.0, &hop[0].easing);
mtwnClipTrackDef jump = mtwnDefaultClipTrackDef();
jump.slot = 2; // bound to the height too, added over its line
jump.keyframes = hop;
jump.keyframeCount = 2;
jump.timing.duration = 0.2;
jump.timing.iterations = 6.0;
jump.timing.direction = mtwn_directionAlternate; // each hop down the way it went up
jump.timing.fill = mtwn_fillForwards;            // landed, it stays
mtwnNode hops;
made = mtwnClip_AddTrack(context, clip, &jump, &hops);
```

Its slots are bound to x, to y, and to y again, adding:

```c
mtwnBinding bindings[3] = {mtwnMakeBinding(0, x), mtwnMakeBinding(1, y), mtwnMakeBinding(2, y)};
bindings[2].composite = mtwn_compositeAdd;
```

A timeline's easing shapes its children's time. Eased by `steps()`, it holds them at as many poses, the stepped look of animation on twos or After Effects' Posterize Time:

```c
// Held 12 times a second (on twos at 24 frames): 2 s in 24 steps.
mtwnClipTimelineDef twos = mtwnDefaultClipTimelineDef();
twos.timing.duration = 2.0;
made = mtwnMakeStepsEasing(24, mtwn_jumpEnd, &twos.timing.easing);
mtwnNode held;
made = mtwnClip_AddTimeline(context, clip, &twos, &held);
```

Tracks added with `held` as their parent move in steps; the clip's others stay smooth. The holds fall on a grid from the timeline's own start, a whole number of them: to hold on the clock's twelfths, start the timeline at a whole number of twelfths and make its duration a whole number of holds (2 s at 12 a second is 24 steps; 1.3 s would be 15.6, which `steps()` cannot take). To step everything a context animates, the host rounds its clock down to the rate before each update instead.

A track with `incremental` set moves on each iteration by its last keyframe minus its first, so a repeated step climbs and a repeated spin turns on.

## Staggers

A stagger spreads offsets over items by their distance from an origin: the first or last item, the center, the nearest edge, an item, a point of the grid, or an order from a seed.

```c
mtwnStagger stagger = mtwnDefaultStagger(); // 0.1 s apart, from the first
stagger.columns = 4;                        // a grid of four columns
stagger.origin = mtwn_staggerCenter;        // spreading from its center
double delays[12];
mtwnResult spread = mtwnComputeStagger(&stagger, 12, delays);
```

The offsets go wherever a time does, such as a track's position offset or a transition's delay, or where a value does, such as a scale from item to item. Items one cell apart are the spacing apart, or with `mtwn_staggerAmount` the spacing is the whole spread; an axis measures along the columns or the rows only, and an easing redistributes the offsets. A negative spacing reverses the order as GSAP's `distribute` does: the farthest items take the start and the nearest the start plus the spread, on the easing turned around, so no offset falls below the start and the offsets stay valid delays. Every offset written is finite: a stagger whose start plus its spread times an eased distance would pass the largest double is refused, with nothing written. The easing is sampled at every item before an offset is written, so a host easing that gives a value not finite only when sampled again is refused too, as a rough easing's shape is, and leaves the offsets unspecified.

Porting from GSAP: a stagger gives the offsets of GSAP 3.15.0's `distribute`, within its rounding, once a port translates these, in all but one case (`test/test_rival_stagger.c` checks every one against GSAP's own outputs):

- `grid: [rows, columns]` is `columns`, or the count where the items are fewer than the columns, since GSAP lays them out in one row; `base` is `start`. A short last row keeps the grid its columns wide here too.
- `each` is between items one index apart in GSAP and one cell apart here. GSAP's spread is `each` times `count - 1` in a row or where the items are fewer than the columns, and otherwise in a grid times the columns along x, `count / columns` along y and the larger of the two across, whatever the origin. Port it as `mtwn_staggerAmount` of that product.
- A ratio point `from: [x, y]` lies in GSAP at column `x * columns - 0.5`, with the columns as ported, and row `y * count / columns - 0.5`, with GSAP's own columns, so 0 and 1 are half a cell outside the first and last items, where a point here takes the last column and row as 1. Port it as `mtwn_staggerPoint` with `pointX = (x * columns - 0.5) / (columns - 1)` and `pointY` that row over `rows - 1`, where `rows` is `count / columns` rounded up and a ratio over one column or row is 0; a row counts its items as its columns.
- In a grid, `"end"` and `"center"` are GSAP's ratio points `[1, 1]` and `[0.5, 0.5]`: port them as those points. GSAP's center row is then `count * 0.5 / columns - 0.5`, so for seven items in three columns it is 0.67 rows down where `mtwn_staggerCenter` is 1. In a row they are `mtwn_staggerLast` and `mtwn_staggerCenter`.
- A grid of one row has no translation, measured across both axes, where GSAP's row above is not 0: GSAP then measures from off the row, and a point here lies on it. The row is 0 only for a `y` of `columns / count * 0.5`, with GSAP's columns (0.5 in a full row), so `"end"` and every other ratio point have none, and where the items are fewer than the columns `"center"` and `"edges"` too. Along x the row drops out, and along y every item is at one distance, so both of those translate.
- `"edges"` is GSAP's center with the spacing negated, the items farthest from the center first. Port it as that, not as `mtwn_staggerEdges`, which measures to the nearest edge of the rectangle as drawn.
- GSAP rounds each offset to 1e-7, and these offsets are not rounded, so a port differs from GSAP by up to 5e-8.

## Not obvious

- A timeline's easing must never decrease (`mtwnIsEasingMonotonic`), so time never runs back inside a forward play: back, elastic, bounce, springs and host functions are refused there. A cubic bezier is sampled there so that its values never decrease to the bit, which can differ from what the same easing gives in a keyframe, a transition or `mtwnSampleEasing` by a few 1e-14 ([easing.md](easing.md)).
- A clip a player plays cannot be destroyed (`mtwn_errorBusy`) until the player is freed. The refusal counts no misuse, since a player not kept frees itself at an end the update decides.
- A negative playback rate needs a finite active duration, whose end a backward play starts from.
- Clip storage is allocated when the clip is made and freed when it is destroyed; the context's other capacities are not touched.
- A track's and a timeline's `linear()` easings, in their timing and their keyframes, have their points copied into the clip's storage as they are added, within the clip def's `pointCapacity` (64 by default, 16 bytes each), so the host's arrays may change or go afterwards. Points past it are refused with `mtwn_errorCapacity`.
