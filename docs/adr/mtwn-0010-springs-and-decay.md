# mtwn-0010. Springs and decay in stable closed form

Status: Accepted

## Context

The textbook underdamped formula divides by a frequency that vanishes at
critical damping; the overdamped one overflows when heavily damped;
springs played as fixed-duration easings lose velocity when interrupted;
springs stepped frame by frame depend on frame times.

## Decision

- A spring is `x(t) = e^(-a t) (x0 C(t) + (v0 + a x0) S(t))` in its
  displacement, with C and S trigonometric, linear or hyperbolic by
  regime, S evaluated as `t sinc(w t)` by its series when `w t` is
  small, and hyperbolic terms folded into two decaying exponentials;
  velocity is its analytic derivative.
- A spring is sampled at the time since its last retarget, never
  stepped; a retarget starts a new segment from the position and
  velocity at that time.
- Parameters reduce to natural frequency and damping ratio from
  stiffness, damping and mass; from a ratio and a frequency in hertz; or
  from a perceptual duration and bounce, with stiffness `(2 pi /
  duration)^2` and damping ratio `1 - bounce` (or `1 / (1 + bounce)`
  below zero).
- The settling duration for a tolerance in the value's units is where
  the regime's envelope bound stays below it; at rest the value is the
  target exactly.
- Decay is `x0 + v0 (1 - e^(-k t)) / k`, its stop time `ln(|v0| / vs) /
  k`; a per-millisecond deceleration rate r is `k = -1000 ln r`. A decay
  crossing a bound hands over to a spring at the crossing.

## Consequences

Springs are continuous across every damping ratio, never overflow, keep
velocity through any number of retargets and give the same values at any
frame rate.
