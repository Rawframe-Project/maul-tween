# mtwn-0002. The whole scope, and what stays outside

Status: Accepted

## Context

The strongest tweening libraries each cover part of what a host needs:
GSAP's timelines, the Web Animations timing model, CSS easing, Motion's
and Apple's springs, PrimeTween's and LitMotion's allocation free
storage. A host that takes one ends up writing the rest, and Maul UI
already carries its own transitions, springs, flings and rubber banding.

## Decision

Maul Tween covers, in its first release:

- Easing: Penner's families landing exactly on their ends, CSS Easing
  Level 2 entire, host functions, reverse and mirror.
- Physics: damped springs in closed form that keep velocity when
  retargeted, decay, rubber banding, punch and seeded shake.
- Values: scalars, vectors of 2 to 4, colors in a named space,
  rotations, angles, integers, discrete values, keyframe tracks and
  paths at constant speed.
- Timing and composition: Web Animations' timing on every node,
  timelines with positions, labels and marks nested to any depth,
  players, composite replace, add, accumulate and weighted blend.
- Events as records, host time only, fixed capacities, bit-exact
  results.

Outside it: skeletal animation and particles, finding or setting
properties of host objects, parsing CSS or any string syntax, a clock or
frame loop, physics beyond one-dimensional springs and decay per
channel, scroll and gesture policy, and any file format.

## Consequences

Neither a game nor a UI needs a second tween library, and a host that
moves one button carries none of what it does not call. A file format,
if a tool ever needs one, is a later addition with a record of its own.
