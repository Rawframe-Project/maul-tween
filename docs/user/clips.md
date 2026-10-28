# Clips and timelines

A clip is an animation written ahead of time: tracks of keyframes on nested timelines, with labels to seek to and marks to be told of. It is built once, finished, and then played by any number of [players](players.md), each on its own targets.

## Contents

- [Building a clip](#building-a-clip)
- [Positions](#positions)
- [Timing and timelines](#timing-and-timelines)
- [Staggers](#staggers)
- [Not obvious](#not-obvious)

## Building a clip

A clip's storage is one block sized by its def, counted against the context's clip storage; `mtwnContext_GetClipBytes` says how much a def takes, so a host can grow the context's `clipStorageBytes` with `mtwnGrowContext` for the clips it makes. It takes the context, like every call that reads a def, so that only a library whose ABI the context def matched reads it.

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

A track writes a slot, which a player binds to a target; a slot keeps one value kind. The track copies its keyframes into the clip. `mtwnClip_Finish` closes the clip: from then on it is read only, can be played, and its tracks can be read (`mtwnClip_GetTrack`) and sampled through the values layer from any thread while the clip lives.

## Positions

Each child is placed when it is added, as GSAP's position parameter places it, and the clip keeps only the resulting start times:

| Position | Starts |
|---|---|
| `mtwn_positionAbsolute` | At the offset. |
| `mtwn_positionAfterEnd` | At the parent's end so far, plus the offset. |
| `mtwn_positionWithPrevious` | With the previous child's start, plus the offset. |
| `mtwn_positionAfterPrevious` | At the previous child's end, plus the offset. |
| `mtwn_positionAtLabel` | At a label of the same parent, plus the offset. |

The previous child is the last track or timeline added to the parent; labels and marks are not children for this, and do not lengthen their parent. A position that resolves to no finite start, such as after the end of an endless timeline, is refused.

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
mtwnBinding bindings[3] = {mtwnBind(0, x), mtwnBind(1, y), mtwnBind(2, y)};
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

A stagger spreads offsets over items by their distance from an origin: the first or last item, the centre, the nearest edge, an item, a point of the grid, or an order from a seed.

```c
mtwnStagger stagger = mtwnDefaultStagger(); // 0.1 s apart, from the first
stagger.columns = 4;                        // a grid of four columns
stagger.origin = mtwn_staggerCentre;        // spreading from its centre
double delays[12];
mtwnResult spread = mtwnComputeStagger(&stagger, 12, delays);
```

The offsets go wherever a time does, such as a track's position offset or a transition's delay, or where a value does, such as a scale from item to item. Items one cell apart are the spacing apart, or with `mtwn_staggerAmount` the spacing is the whole spread; an axis measures along the columns or the rows only, and an easing redistributes the offsets.

Porting from GSAP: a grid with a short last row stays its columns wide, its centre and edges the full rectangle's. GSAP's `distribute` puts the centre row at `count * 0.5 / columns - 0.5`, so for seven items in three columns its centre is 0.67 rows down where here it is 1, and its "edges" is the centre's distance reversed where here it is the nearest edge of the rectangle as drawn.

## Not obvious

- A timeline's easing must never decrease (`mtwnIsEasingMonotonic`), so time never runs back inside a forward play: back, elastic, bounce, springs and host functions are refused there.
- A clip a player plays cannot be destroyed (`mtwn_errorBusy`) until the player is freed.
- A negative playback rate needs a finite active duration, whose end a backward play starts from.
- Clip storage is allocated when the clip is made and freed when it is destroyed; the context's other capacities are not touched.
- A track's and a timeline's `linear()` easings, in their timing and their keyframes, have their points copied into the clip's storage as they are added, within the clip def's `pointCapacity` (64 by default, 16 bytes each), so the host's arrays may change or go afterwards. Points past it are refused with `mtwn_errorCapacity`.
