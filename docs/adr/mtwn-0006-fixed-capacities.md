# mtwn-0006. Every capacity in the def, allocated at creation

Status: Accepted

## Context

Libraries that allocate per tween or grow their pools pay for it in
garbage and frame hitches: measured by the TweenPerformance benchmark,
DOTween allocates 732 bytes per tween started, while PrimeTween and
LitMotion allocate none with no loss of features.

## Decision

The context def names the capacities of targets, players, clips, the
event queue and the changed-target report. Creation allocates them
through the host's allocator; a clip's storage is allocated when the
clip is created, from the counts in its def. Nothing allocates during an
update; a full pool refuses with `mtwn_errorLimit`; only the host grows
a context. The curves never allocate.

## Consequences

The host knows the library's memory before the first frame, and the hot
path never touches an allocator.
