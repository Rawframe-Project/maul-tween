# mtwn-0015. Rotations by slerp with Kahan's angle; angles with hue methods

Status: Accepted

## Context

The angle between two quaternions from `acos` of their dot product loses
half its digits near zero; q and -q are one rotation, so a naive
interpolation can take the long way round.

## Decision

Unit quaternions slerp along the shortest arc by default, or the long
way on request, with the angle `2 atan2(|q0 - q1|, |q0 + q1|)` and
weights stable at small angles. Composite add multiplies in a stated
order; weighted blends are normalized sums; springs act on the rotation
vector of the remaining rotation. Scalar angles interpolate raw (with
turns), shorter, longer, increasing or decreasing.

## Consequences

Rotations are accurate at every angle and never turn the long way unless
asked.
