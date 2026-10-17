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
