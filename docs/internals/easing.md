# Easing

An easing maps a timing's progress to eased progress. This page explains the easing value, how curves land exactly on their ends, the cubic-bezier solver, and how sampling and the `linear()` fit stay bounded.

## One value type

`mtwnEasing` is a plain value: a kind, a mode and the kind's parameters, precomputed by its maker where that saves work per sample. Makers validate and return a status, one maker per family of parameters, and `mtwnEase` samples with no status, so a timing or a track can hold an easing and sample it every frame with nothing to check. What an easing references (a host function's context, `linear()` points) stays the host's.

One mode applies to every kind: in is the curve as defined, out its reflection `1 - f(1 - p)`, in-out and out-in the reflected halves. Each kind has one native curve ([`src/easing.c`](../../src/easing.c)): Penner's in curve for most, the out curve for elastic and bounce, which GSAP and Penner define by it, and CSS's curves from [`src/css_easing.c`](../../src/css_easing.c). The modes are reflections of the native curve, taken so each half lands where the other starts.

## Exact ends

A curve `g` not exact at both ends is used as `(g(p) - g(0)) / (g(1) - g(0))`, with `g(0)` and `g(1)` computed when the easing is made by the same code that samples it. IEEE arithmetic then gives exactly 0 and 1 at the ends with no special case. A rival that misses 1 by a little and sets it there (GSAP's and tween.js's elastic miss by up to `a 2^-10`) jumps at the end; this one does not. An integer power that is exact already skips the division, giving the same bits, and runs inline in the update.

## Cubic bezier

CSS's `cubic-bezier()` needs `t` for `x(t) = x`. Newton's method from `t = x` converges fast but can leave `[0, 1]` or stall where the slope is near 0; bisection is safe but slow. The solver keeps a bracket every step narrows and takes the Newton step only when it stays inside and the slope is positive, bisecting otherwise. It stops when the error in `x` is at most 1e-14 or the bracket is under 2^-52, after at most 64 steps. The stop is a comparison, so the steps taken, and the bits, are the same everywhere. 0 and 1 map exactly to 0 and 1, and inputs outside `[0, 1]` extend along the end tangents, as CSS Easing Level 2 specifies.

## Steps and `linear()`

`steps()` follows CSS's algorithm with its before flag: in a timing's before phase, a progress landing exactly on a step takes the step below. The flag is part of `mtwnEase` because the timing model computes it; a curve sampled alone passes `false`.

`linear()` canonicalizes its points as CSS does, in the caller's array: a missing first input is 0 and a missing last one 1 or the largest before it, an input below an earlier one is raised to it, and runs of missing inputs spread evenly. A sample finds its segment by binary search.

A rough easing is `linear()` through seeded points rather than a kind evaluated from the seed at each progress, so it has the modes, the monotonicity rule and the fit of `linear()` already. Its inner points are stratified, the k-th of m at an input drawn in `[k/m, (k+1)/m)` by SplitMix64 of the seed, so they come in order without a sort and never meet in a jump, where GSAP's sorted uniform draws can.

## Monotonic easings

A timeline's easing shapes its children's time, which must never run back inside a forward play: a crossing count assumes each iteration's child time moves one way. `mtwnIsEasingMonotonic` answers from the kind alone: linear, power, sine, expo, circ and steps always; a cubic bezier with both `y` in `[0, 1]`; `linear()` whose outputs never decrease; never back, elastic, bounce, a spring or a host function, which the library cannot inspect.

## Sampling and the fit

`mtwnFitLinearEasing` is Ramer, Douglas and Peucker's simplification on the continuous curve ([`src/sample.c`](../../src/sample.c)): a segment whose chord misses the curve by more than the tolerance at one of its 32 probes splits at its worst probe, depth first, so points come out in order. The split is kept within the segment's middle probes, from 5/33 to 28/33, so every child is at most 28/33 of its parent and the pending segments fit a fixed stack: nothing is allocated. A segment narrower than 2^-24 is kept as it is, so a jump in a host function ends. The fit makes at most `MTWN_MAX_FIT_POINTS` points, so a tolerance the curve cannot meet costs at most about twice that many segments of 32 probes, not 10^8 points.

## Traps

- The before flag changes only curves that jump at their start; a new curve kind that jumps must read it.
- The rival tests hold each curve to a bound derived from the definitions, not fitted to the results: 2^-51 where the curves are defined alike, `a (1 + a) 2^-10` for elastic, 2^-10 for expo against Penner's and 2^-7 against GSAP 3's own blend. A change that needs a looser bound has changed the curve.
