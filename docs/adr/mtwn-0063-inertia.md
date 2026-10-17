# mtwn-0063. Inertia: landing and bounds

Status: Accepted

## Context

A fling coasts by a decay (record mtwn-0010). Hosts snap it and bound
it: GSAP's InertiaPlugin lands its `end` exactly, snapping to the
value nearest the natural landing or to a function of it; Motion's
`inertia` moves its target by `modifyTarget` and, past `min` or `max`,
hands the value to a spring back to the nearest bound; Flutter's
BouncingScrollSimulation and UIKit's scroll views do the same for
content. Motion keeps its time constant and rescales the amplitude, so
the start velocity jumps, and checks the bounds at sampled frames, so
its spring starts a frame's travel past the bound. Flutter finds the
crossing time and starts the spring at the bound.

## Decision

- `mtwnMakeDecayToRest(position, velocity, rest)` makes the decay that
  rests on a chosen end with the start's velocity: its rate is
  velocity / (rest - position). The natural rest is the decay's sample
  at infinity (iOS's projection), so the snapping rule stays the
  host's: the nearest page or grid point, a function, an array's
  nearest. An end the velocity does not move toward is refused.
- `mtwnBoundedDecay` holds a decay, a spring and a lower and an upper
  bound. Sampled from a start, it is the decay while it would come to
  rest within the bounds; where it would rest past one, it is the decay
  until the exact time it reaches that bound (`mtwnDecayCrossingTime`),
  then the spring from the bound with the velocity it has there; a
  start past a bound is the spring's from the start, back to the nearer
  bound. Velocity is continuous at the start and at the bound, and the
  same at any frame rate. `mtwnBoundedDecayStopTime` gives when it is at
  rest: the decay's stop, or the crossing time plus the spring's
  settling time.
- The value functions are stateless, as the decay's and the spring's
  own: a start, a time, a result.

## Consequences

`test_inertia` checks a snapped landing (its rest on the page, its
velocity kept, the refusals), a bounded decay bit for bit the decay
within the bounds, the bound exactly at the crossing with the velocity
continuous, the spring carrying it over and back to rest from its stop
time, the lower bound, and a start past a bound. `fuzz_values` samples
both over ordinary and wild parameters; it found two starts where
`mtwnSpringSettlingTime` returned NaN, now fixed: a velocity bound
whose two products overflowed with opposite signs (now infinite, never
ending a spring early) and a critical spring's bound peaking at a time
rounding to nearly 0, where the search started on a slope of 0. The
functions live in their own object, so a program using the decay alone
links none of them.

## Amended 2026-10-18: transitions' decays bounded and landing

A transition's decay takes both (`mtwnTransitionDef`): `bounded` holds
it between `lower` and `upper` per channel, the def's spring as the
bounce, and it ends on the bound a channel reached; `lands` makes it
rest on the def's value, each channel at its own rate with the start
velocity it has, and it ends on the value (an angle's after the
method's turns). Not both. A scale coasts, bounces and lands in its
logarithm, its bounds above 0. Each channel runs as the value functions
run it, sampled alike: a channel that hands over keeps a bit in the
player's slot and its bound as its end value, so the slot grows by no
field beyond two bytes of masks and a flag, the landing's rates sharing
the curve's union. `test_coast` checks a vector's channels each as
`mtwnSampleBoundedDecay` alone, the bound and the stop as the end, a
start past a bound, landings of a vector, a scale and an angle, a
bounded zoom in the logarithm, and seven refusals; `fuzz_api` flings
transitions bounded and landing.

The part sizes: the coast's code kept out of line (per channel, so
GCC's unrolled loops copy calls, not bodies; at first the start grew
3.8 kB) is 1.4 kB with the bounded decay's 0.8: transitions 53.2 to
56.3 kB, clips 132.5 to 137.0 kB (both link the transitions), every
function 151.5 of 152 kB. The transitions' budget is restated from 54
to 58 kB and the clips' from 135 to 139 kB.
