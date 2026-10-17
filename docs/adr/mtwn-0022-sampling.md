# mtwn-0022. Sampling: even tables, and linear() fitted to a tolerance

Status: Accepted

## Context

Hosts need an easing as data: a table a GPU or a lookup reads, and the
points of a CSS `linear()`, the one way a browser can run a spring or a
Penner curve. Motion writes one point per 10 ms of duration, rounded,
with no bound on the error; Jake Archibald's generator simplifies
10,000 samples by Ramer, Douglas and Peucker, which needs them in
memory.

## Decision

- `mtwnSampleEasing` writes `count` floats at progress `i / (count -
  1)`.
- `mtwnFitLinearEasing` writes `linear()` points over progress 0 to 1
  whose interpolation is within a tolerance of the easing, measured in
  eased progress at 32 probes inside every segment. A segment over the
  tolerance splits at its worst probe, held within its middle 3/4, so
  the work is depth first on a fixed stack and allocates nothing. A
  segment narrower than 2^-24 is kept, so a jump in a host function
  ends. A short buffer reports the count needed.
- Steps are refused by the fitter: CSS writes them as `steps()`.

## Consequences

A fitted `linear()` has a stated error and few points: within 1e-3,
the CSS `ease` keyword takes 28 points and GSAP's elastic out 73, where
100 evenly spaced points miss the elastic by 4.4e-3.

## Amended 2026-10-17: a fit's points are limited

A tolerance the curve cannot meet (below its rounding, or an
oscillation finer than 2^-24) took every segment down to 2^-24: about
10^8 points and minutes of work, only to report a count no host can
use. A fit stops at `MTWN_MAX_FIT_POINTS` (65536) points with
`mtwn_errorLimit`, so it examines at most about twice that many
segments; below the limit a short buffer still reports the count
needed. The values' fuzz found it.

It also found a fit returning points `mtwnMakeLinearEasing` refuses: a
curve infinite at a point the fit keeps (a linear() extrapolated
between points 1e-300 apart overflows; a host function may return
anything). The fit refuses such an easing with `mtwn_errorInvalid`, so
its points always make a linear(). A NaN inside the curve is never kept,
as it never misses the chord most, so only the ends and infinities are
checked.
