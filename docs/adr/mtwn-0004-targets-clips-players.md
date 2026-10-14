# mtwn-0004. Targets, clips and players, with transitions as the short path

Status: Accepted

## Context

An interrupted animation must continue in value and velocity whatever
starts next; several animations may act on one value; and one
description often plays on many objects. A value the library writes
through a host pointer can dangle.

## Decision

- A target is one animated value of a fixed kind holding the host's base
  value, its current value and its velocity. Animations write targets;
  the host reads them by id, one or many, and an update can report which
  changed.
- A clip is an immutable description built once: keyframe, spring and
  decay tracks, nested timelines, labels and marks. Its tracks address
  slots, so one clip plays on many sets of targets, and a finished clip
  can be read from many threads.
- A player plays a clip with its slots bound to targets, its start time,
  hold time and signed rate, and per binding a composite operation and a
  weight. Players composite onto each target in creation order, from the
  base value.
- A transition animates one target to a value with an easing, a spring
  or a decay in one call: a player holding a one-track clip inline,
  starting from the target's current value and velocity and replacing
  the target's running replace-composite player.
- A finished player not held is freed at its end, committing a forward
  fill into its targets' base values first; a clip a player uses cannot
  be destroyed.

## Consequences

Interruption is continuous for every kind of animation, composition is
defined, memory stays bounded however many animations start, and no id
or pointer the host holds can dangle.
