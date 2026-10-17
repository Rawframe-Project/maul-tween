# mtwn-0060. Distances for durations set by speed

Status: Accepted

## Context

A move across a screen and a move of a few pixels should not take the
same time. DOTween's `SetSpeedBased` reads a tween's duration as units
or degrees a second, worked out when it starts, and suggests a linear
ease for a constant speed; it has no effect in a Sequence. VRChat's
VRCTween copies it. Material Design's motion guidance tunes durations to
the distance travelled, about 300 ms on mobile, and warns that past
400 ms motion feels slow: a duration from speed wants bounds.

## Decision

`mtwnMeasureTransition` gives the distance a transition def would move
its target, from the target's current value to the def's value, in the
kind's own measure; the host sets the duration from it, for example
`fmin(fmax(distance / speed, 0.15), 1.0)`. The measure, per kind:

- numbers, vectors and integers: their Euclidean distance;
- an angle: the radians it turns after the method's whole turns;
- a rotation: the angle it turns on its path (twice the quaternions'
  own angle), the longest way included;
- a scale: |ln(to / from)| (record mtwn-0059);
- a color: Oklab's Euclidean distance, CSS Color 4's deltaEOK, with
  alpha as a fourth coordinate, so a fade has a length;
- a discrete value has none and is refused.

A speed field on the def was not taken: it fixes one formula where hosts
want a floor and a ceiling, and grows the def and its version for a
division. Clips are left out, as DOTween leaves its Sequences: a track
at constant speed has its length already (`mtwnMakeTrack`'s lengths).

## Consequences

`test_transition` measures each kind against its exact distance, the
recipe's duration, and the refusals. The size budget of a program that
takes every function (record mtwn-0055) is restated from 146.0 to 152.0
kB: scales and zooms (record mtwn-0059: the zoom 2.4 kB, the scale kind
in transitions, tracks and composition) and this measure took the part
from 142.7 kB to 146.8 kB. Every growth is a feature's.
