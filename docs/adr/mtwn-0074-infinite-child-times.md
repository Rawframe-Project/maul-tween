# mtwn-0074. Infinite child times

Status: Accepted

## Context

An endless timeline (an infinite duration) gives its children its active
time times its rate going forwards and infinity reversed (record
mtwn-0034). Two consequences were left open. Reversed, the children sit
at an infinite local time, which `mtwnComputeTiming` was documented to
refuse: an endless child there divided infinity by infinity, so its
time, its tracks' values and its end's iteration were NaN, and a record
of its end carried no iteration. With no iterations, the crossing count
took the children's time as progress times duration, 0 times infinity,
where the values take the active time, 0: a mark at such a timeline's
start was never recorded, and a random probe found 126 of 20,000 small
clips with a NaN iteration in a record.

Web Animations Level 2 says the same for the second case: "If the
transformed progress is zero and the iteration duration is infinity,
then the transformed time is zero." Neither level reaches an infinite
local time, since a timeline's times are finite there.

## Decision

- `mtwnComputeTiming` takes positive infinity as a local time, the limit
  of the finite ones: a finite active interval lies behind it, filled as
  after its end; an endless one is still active there, never left; an
  infinite duration's overall progress is its iteration start there, as
  at every finite active time. Results at finite times are the same
  bits.
- An endless node's end lies at no time: it is never crossed, not even
  by the children of a reversed endless timeline at infinity.
- The crossing count gives an endless timeline of no iterations' children
  the values' time: 0 going forwards, infinity reversed.

## Consequences

A track in an endless child of a reversed endless timeline fills as past
its end, where it showed nothing, and no record carries a NaN
iteration. Nothing at infinity is crossed, as a player started past a
track crosses nothing behind it.
