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
