# mtwn-0023. Springs as a motion matrix; decay and rubber band

Status: Accepted

## Context

A value of several channels (a position, a color) springs every
channel with the same spring from its own start. Hosts describe springs
three ways: physically, by frequency and damping ratio, and by Apple's
perceptual duration and bounce. A settling time must never end a spring
early, and must say what "settled" means in the value's own units.

## Decision

- `mtwnSpring` is a value with natural frequency and damping ratio,
  made from stiffness, damping and mass; from hertz and ratio; or from
  duration and bounce as SwiftUI, Motion and anime.js map them. A ratio
  of 0 is an oscillator that never settles.
- A spring's motion over a time is a 2 by 2 matrix taking a start's
  offset from the target and velocity to those at the time.
  `mtwnSpringMotionAt` computes it once per time; each channel then
  costs four multiplications. A retarget changes the offset and keeps
  the velocity.
- The overdamped matrix is written in its two rates, the slow one as
  `w0^2 / (a + w)`; below `w t = 1/2`, `sinh(w t) / w` comes from its
  series, and `sin(w t) / w` needs none, the sine keeping its relative
  digits, so the regimes meet with no vanishing divisor, cancellation
  or overflow.
- `mtwnSpringSettlingTime` takes an offset and a velocity tolerance and
  returns when bounds on both stay below them: in closed form where the
  bound is a pure exponential, else by Newton from the right on its
  concave logarithm, whose iterates never pass the true time.
- Decay in closed form at a rate per second, also from iOS's rate per
  millisecond, with its stop time, rest position and bound crossing;
  iOS's rubber band and its inverse; a spring turned into an easing
  over a duration, landing exactly on 1.

## Consequences

Springs are sampled, never stepped, so every frame rate gives the same
values. The motion matches an integration of the oscillator in long
double to within an ulp or two of 1 in every regime.
