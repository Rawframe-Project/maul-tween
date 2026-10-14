# mtwn-0033. The clip: one block of storage, tracks at positions, then finished

Status: Accepted

## Context

Records mtwn-0004 and mtwn-0013 make a clip an immutable description
of tracks on timelines, built at positions resolved when added, played
on many sets of targets through slots. Record mtwn-0006 names every
capacity up front.

## Decision

- `mtwnClipDef` (cookie, ABI version) names the clip's nodes,
  keyframes, slots, marks and labels. `mtwnCreateClip` allocates the
  clip's storage in one block through the context's allocator, when
  the clip is created, counted against the context's clip storage;
  destroying the clip, or the context, frees it.
- Node 1 is the root, a parallel timeline lasting to its latest
  child's end. `mtwnClip_AddTrack` takes a def naming the parent, a
  position, the slot, the track (record mtwn-0032), the keyframes,
  constant speed and the node's timing; it copies the keyframes and
  makes the track in the clip's storage, under the floating-point
  guard, so a refusal comes from the call that caused it. A slot keeps
  one value kind.
- Positions resolve to start times as children are added: absolute,
  after the parent's end so far, with the previous child's start, or
  after the previous child's end, each with an offset.
- `mtwnClip_Finish` makes the clip immutable. Only then is it read:
  `mtwnClip_GetTrack` gives a node's track with timing, start, parent
  and slot, referencing the clip's storage, which any thread may sample
  through the values layer while the clip lives; `mtwnClip_GetDuration`
  gives the root's end. Calls the clip's state does not allow are
  `mtwn_errorState`; a clip a player uses is `mtwn_errorBusy` to
  destroy.

## Consequences

A clip costs one allocation of a size known from its def; a bad
keyframe is reported where it was given; a finished clip is shared
across threads and players without copies.
