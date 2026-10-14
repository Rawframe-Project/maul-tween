# mtwn-0030. Spring and decay transitions; velocity handed over

Status: Accepted

## Context

Record mtwn-0029 gave transitions easings. A spring or a decay is what
makes an interruption continuous in velocity too, as Compose's
`animateTo` and `animateDecay` do, from the motion matrix and the
settling bounds of record mtwn-0023.

## Decision

- A transition's spec is an easing, a spring or a decay; its start
  velocity is the target's own (the default) or one given. Rest
  tolerances `restOffset` and `restSpeed` end springs and decays.
- Numbers, vectors, angles and integers spring per channel from the
  start's offset and velocity, by one motion matrix a sample, with the
  spring's own velocity; the transition ends at the latest channel's
  settling time and snaps to the end. An undamped spring runs until
  replaced or stopped.
- Colors and rotations spring their path's progress from 0 to 1, so
  the path stays the color space's or the slerp's; the start velocity
  is the channel velocity projected on the end minus the start, and
  the tolerances are in progress.
- A decay coasts each channel of numbers, vectors, angles and integers
  from the start's position and velocity, with no end value, until
  every channel's speed falls to `restSpeed`, and commits where it
  stopped (an integer's rounded).
- Discrete values take only easings; colors and rotations no decay.
- An angle's end moves by its method's turns once, when started, and
  that end is committed, so no transition jumps by a turn at its end.
- A delay holds the start at rest; when it ends, a spring or decay of
  numbers begins with its start velocity, reported from that instant,
  so a replacement made before the next update inherits it.

## Consequences

A fling handed to a decay and the decay's motion handed to a spring
are continuous in value and velocity; the tests check the handover
against the values layer's springs and decays.
