# mtwn-0034. Timelines: nested groups, labels and marks as nodes

Status: Accepted

## Context

Records mtwn-0012 and mtwn-0013 give a clip nested timelines with Web
Animations Level 2's group timing, GSAP's positions resolved when
built, labels for positions and seeking, and marks that give events.

## Decision

- `mtwnClip_AddTimeline` adds a timeline at a position with a timing
  whose iteration duration is its latest child's end (by default) or
  the one the host sets, which clips. Its easing must be monotonic,
  else it is refused, so time never runs back inside a forward play.
  Sequences and groups are timelines whose children are added after
  the end or with the previous child.
- `mtwnClip_AddLabel` and `mtwnClip_AddMark` add instants with a host
  key at a position. They are nodes, counted in the clip's node
  capacity; a label's key is unique in its parent; neither is the
  previous child of later positions, nor lengthens its parent.
- `mtwn_positionAtLabel` places a child at a label of the same parent
  plus an offset.
- Ends are carried up as children arrive: a child's end raises its
  timeline's, and a timeline lasting as long as its children moves its
  own end in its parent, up to the root; siblings already placed keep
  their starts. A child that would make a timeline played backwards
  infinitely long is refused, as it would leave no end to start from.
- An endless child (infinite iterations or duration) makes a timeline
  sized by its children endless, and so on up while the ancestors are
  sized by theirs, as a Web Animations Level 2 group's intrinsic
  duration and a GSAP timeline holding a `repeat: -1` tween are. A
  timeline of set duration clips it. A position that resolves to no
  finite start, after the end of an endless timeline or after an
  endless previous child, is refused.
- `mtwnClip_GetNode` reads any node (kind, parent, start, end, the
  timing with a timeline's duration resolved, slot, key) and
  `mtwnClip_FindLabel` a label's time.

## Consequences

A built clip holds only numbers: every node's start and end, and the
order of each timeline's children, which players walk.
