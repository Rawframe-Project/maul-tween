# mtwn-0029. Transitions with easings, the update, the floating-point guard

Status: Accepted

## Context

Record mtwn-0004 makes a transition the short path: one call animates a
target to a value, starting from where it is, as Compose's `Animatable`
does. Record mtwn-0007 makes results bit-exact, which a host's
flush-to-zero or rounding mode would silently break.

## Decision

- `mtwnUpdate` advances every player to the host's time in integer
  nanoseconds, which may not go back, and reports the targets whose
  value or base changed since the last report, in the order they
  changed, once each, into the caller's array; the total may exceed
  its capacity.
- `mtwnStartTransition` takes a def (cookie, ABI version) naming the
  target, the end value, a delay, a duration, an easing, and the
  kind's choices: color space and hue method, angle method, rotation
  path. It starts from the target's current value at the next update
  and replaces the target's running transition, whose id goes stale.
  Transitions are players, in the player pool.
- Each kind interpolates as the values layer does; integers round to
  nearest with ties away from zero, discrete values flip at eased
  progress 1/2. The start holds through the delay. Velocity is the
  change of the transition's own curve over its last millisecond, as
  Compose measures a tween, 0 for discrete values. At its end a
  transition commits its end value as the target's base, with velocity
  0, and is freed; `mtwnDestroyPlayer` stops one, and the target shows
  its base again.
- Every public call that computes saves the thread's floating-point
  control register (MXCSR, FPCR), sets round to nearest with
  subnormals kept and exceptions masked, and restores it, through
  volatile access, with the work in out-of-line functions.

## Consequences

An interrupted transition continues from the value it reached, and its
velocity is there for a spring to take (record mtwn-0010). A scenario
of scalars through the subnormals, OkLCh colors, rotations and vectors
gives the same bits under a host's flush-to-zero and rounding toward
zero as under the default, which the tests check.
