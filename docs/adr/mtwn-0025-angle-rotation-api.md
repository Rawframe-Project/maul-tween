# mtwn-0025. Angles with CSS's hue methods; rotations by Kahan-angle slerp

Status: Accepted

## Context

An angle can mean a direction, where 350 to 10 degrees should take the
short way, or a count of turns, where it should not. A rotation is a
unit quaternion, and q and -q are the same rotation, so a naive
interpolation can take the long way round. The family's animation
library already stores rotations as x, y, z, w, glTF's order.

## Decision

- `mtwnMixAngle` interpolates radians raw, turns kept, or by CSS's hue
  methods with a period of a full turn: shorter, longer (a full turn
  between equal angles, as CSS's longer gives), increasing,
  decreasing. The end at progress 1 moves by whole turns, and stays
  exact when it moves by none.
- `mtwnQuat` is x, y, z, w, the family's and glTF's order.
- `mtwnRotationMix` holds the ends normalized in double, the second
  negated for the shortest way (the default) or for the longest, and
  Kahan's angle `2 atan2(|q0 - q1|, |q0 + q1|)`; `mtwnMixRotation`
  weights the ends by slerp's sines written with `sinc`, so tiny angles
  keep their digits, and normalizes once. The ends are the rotations
  given, the second negated where the path needed it. The long way
  between equal rotations has no axis and is refused.

## Consequences

Rotations turn at constant angular speed, within a float's rounding of
slerp in long double, from a millionth of a radian to nearly half a
turn.
