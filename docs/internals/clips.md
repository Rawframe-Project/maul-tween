# Clips and composition

A clip is built once and read many times. This page explains why it is built node by node and then sealed, how its timelines carry their ends, how a player samples it into targets, and how players compose.

## Built, then sealed

A clip's storage is one block sized by its def (nodes, keyframes, slots), allocated when the clip is made and counted against the context's clip storage ([`src/clip.c`](../../src/clip.c)). Nodes are added one at a time and `mtwnClip_Seal` makes the clip immutable. Only then is it played or read ([`src/clip_read.c`](../../src/clip_read.c)), so:

- a sealed clip's tracks can be sampled from any thread while it lives, with no lock, since nothing writes them;
- every node's numbers (start, end, active duration) are worked out once, when it is added or when the clip is sealed, never per frame;
- a refusal comes from the call that caused it: a track's keyframes are copied and the track made, under the library's floating-point environment, inside `mtwnClip_AddTrack`.

Positions resolve to start times as children are added (absolute, after the parent's end so far, with or after the previous child, at a label), so a built clip holds numbers only, as GSAP's position parameter would be resolved once. Labels and marks are nodes, counted in the node capacity, named by a host key; neither is a previous child nor lengthens its parent.

## Ends carried up

A timeline sized by its children lasts to its latest child's end. As children arrive, a child's end raises its timeline's, and a timeline sized by its children moves its own end in its parent, up to the root; siblings already placed keep their starts. An endless child (infinite iterations or duration) makes such a timeline endless, and so on up while its ancestors are sized by theirs, as a Web Animations Level 2 group's intrinsic duration and a GSAP timeline holding a `repeat: -1` tween are. A timeline of set duration clips its children. A position that resolves to no finite start, and a child that would make a timeline played backwards infinitely long, are refused, as either would leave no time to start from.

Timelines nest at most 32 deep below the root, so the chain of timelines above any node fits a fixed scratch in the crossing count and the sampler.

## Sampling

The update walks a player's nodes in the order they were added, each after its parent ([`src/clip_sample.c`](../../src/clip_sample.c)). The root passes the player's current time, held at its ends; a timeline gives its children its transformed progress times its iteration duration, as Web Animations Level 2 does, or its active time when that duration is infinite. Time runs backwards below reversed iterations, negative rates and players played backwards. A track with an effect writes its slot's target, later tracks over earlier ones.

A moving player's velocity is its values' change over the last millisecond, the same span a transition measures over. A track of numbers replacing its target at full weight writes its value and leaves its velocity to be worked out when read: by `mtwnTarget_GetState`, by a transition inheriting it, or by a player composing over it. That is the same arithmetic run later, from what the target's spare velocity field keeps (the track's parent's time a moment ago and which track of which clip), so no player state is read and a rate change, a seek or the player's going cannot change the answer. Every other track computes its velocity at once. A clip's destruction stores the velocities left pending on it.

An incremental track adds `k` times its last keyframe minus its first in iteration `k`, counted from the iteration start, as Level 2's iteration composite counts. The path stays continuous for any first keyframe, where Level 2's final value would jump unless a track starts at zero. A rotation turns by the rotation from its first keyframe to its last raised to the power `k` (its axis with its angle times `k`), not `k` products, so iteration 10^6 does not drift.

## Composition

A binding composes by replace or add, at a weight in [0, 1], and a player's weight scales its bindings' ([`src/compose.c`](../../src/compose.c)). The result mixes from the underlying value toward the composited one by the weight. The underlying value is what earlier players wrote in this update, or the target's base, so an add layer never compounds on its own last frame.

Accumulate is not offered: for every value kind here it is addition, as CSS Values 4 defines it.

- Numbers, vectors and angles add and mix per channel, in double; an integer composes unrounded and rounds once.
- Colors add and mix premultiplied by alpha, alpha clamped to 1, and a result of alpha 0 has color channels 0, as CSS gives.
- A rotation replaced slerps from the underlying toward the effect by the weight; one added slerps from no rotation toward the effect by the weight, then turns the underlying by it, the effect in the underlying's frame.
- Scales compose in their logarithm: adding multiplies.
- A discrete value is not additive and steps at half its weight.

A finished player not kept commits the composited value, what lies under it included, as the base. Transitions always replace.

## Traps

- The sampling loop's track result stays inline: left out of line, as GCC 14 leaves it, the loop costs every player 2% more.
- A track's keyframes are copied before its track is made, since making a track of angles or rotations rewrites them.
- A clip a player uses is busy; destroying it is refused, so no player reads freed storage.
