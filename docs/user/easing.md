# Easing

`easing.h` maps a timing's progress, from 0 to 1, onto eased progress. An `mtwnEasing` is a plain value: a maker checks its parameters and returns a status, and `mtwnEase` samples it, giving the eased progress through a pointer and its own status. It may be copied freely; what it references stays the host's: a host function's context must outlive every copy, and the points of `linear()` every copy the host keeps, since a transition or a clip copies the points in when it takes the easing.

## Making and sampling

```c
mtwnEasing ease;
mtwnResult made = mtwnMakeCssEasing(mtwn_cssEaseInOut, &ease); // CSS's ease-in-out
double eased = 0.0;
mtwnResult sampled = mtwnEase(&ease, 0.25, false, &eased); // a quarter of the way

mtwnEasing back;
made = mtwnMakeBackEasing(mtwn_easeOut, 1.70158, &back); // GSAP's back.out
```

The makers:

| Maker | Curves |
|---|---|
| `mtwnLinearEasing` | The identity; it cannot fail. |
| `mtwnMakePowerEasing` | `p^k`: quad, cubic, quart, quint and any exponent above 0. GSAP's `power(n)` is `k = n + 1`. |
| `mtwnMakePennerEasing` | Sine, expo, circ and bounce. |
| `mtwnMakeBackEasing` | Back, with GSAP's and Penner's overshoot (1.70158 by default). |
| `mtwnMakeElasticEasing` | Elastic, with GSAP's amplitude and period. |
| `mtwnMakeCssEasing` | CSS's keywords: `linear`, `ease`, `ease-in`, `ease-out`, `ease-in-out`, `step-start`, `step-end`. |
| `mtwnMakeCubicBezierEasing` | CSS's `cubic-bezier(x1, y1, x2, y2)`. |
| `mtwnMakeStepsEasing` | CSS's `steps(n, position)`, every jump position. |
| `mtwnMakeLinearPointsEasing` | CSS's `linear()` with points. |
| `mtwnMakeRoughEasing` | A rough curve from a seed, below. |
| `mtwnMakeSpringEasing` | A spring over a duration (see [physics.md](physics.md)). |
| `mtwnMakeHostEasing` | A function of the host's. |

Every kind takes the same four modes: in (the curve as defined), out (`1 - f(1 - p)`), in-out and out-in. `mtwnSetEaseMode` changes the mode of an easing already made.

## Not obvious

- Penner's curves, springs, linear and cubic bezier land exactly on 0 at 0 and 1 at 1, in every mode. Expo is normalized to do so and differs from Penner's by at most 2^-10. GSAP 3's expo is its own blend, up to 2^-7 from this one.
- Penner's and tween.js's in-out back use 1.525 times the overshoot; pass that value to match them. GSAP's and tween.js's elastic miss 1 at the end by up to `a 2^-10` and set it there, so they differ from this one by at most `a (1 + a) 2^-10`.
- CSS's curves follow CSS Easing Level 2 outside 0 to 1 (a cubic bezier extends along its end tangents); Penner's curves, springs and host functions see progress clamped to 0 to 1.
- The last argument of `mtwnEase` is the timing's before flag: in the before phase, `steps()` landing exactly on a step takes the step below, as CSS says. Pass `false` when sampling a curve on its own.
- `linear()` canonicalizes its points in place as CSS does (missing inputs spread, decreasing inputs raised), so the array is rewritten; an input of NaN is a stop given without a percentage. The easing reads the points where they are: changed afterwards, even out of order, while finite they give an unspecified but finite value. A transition or a clip copies them when it takes the easing, and refuses points the host put out of their canonical order.
- Every curve but a host function's is finite at every finite progress. Where CSS's extend past the doubles' range (a steep end tangent far outside 0 to 1), they give the largest finite double of its sign.
- Every call taking an easing refuses one whose fields are not as its maker sets them (`mtwn_errorInvalid`), and `mtwnEase` refuses a progress that is not finite. A host never writes an easing's fields; a struct it zeroes is the linear easing.
- An elastic's amplitude is at most 512 and its period, once an amplitude below 1 has stretched it, at least 2^-10; a cubic bezier's y is at most 2^1000 in magnitude; a spring easing may oscillate at most once in 2^-10 of its duration, and its spring's natural frequency, in radians a second, times the duration is below 2^512 (about 1.34e154), so its square stays a double, as a spring's own does. Past these a curve's normalization could overflow or a fit of `MTWN_MAX_FIT_POINTS` points could not follow it, and the makers refuse them.
- A host function is called from every thread that samples the easing, so it must be safe there, and it never calls the context sampling it.
- `mtwnEaseCubicBezier` samples a cubic bezier, or the linear easing, as `mtwnEase` does, to the bit, without its dispatch over every kind, which keeps every curve linked. A program that samples only cubic beziers through it is smaller: with GCC 14, `-Os`, link-time optimization and unused sections dropped, making one `cubic-bezier()` and sampling it links 1.4 kB of code on x86-64 against 7.9 kB through `mtwnEase`. Given an easing of another kind it refuses it.
- `mtwnIsEasingMonotonic` tells whether an easing never decreases, which a timeline's easing must. Back, elastic, bounce, springs and host functions are never monotonic for it.

## Rough easings

A rough easing wanders from a shape at points drawn from a seed, as GSAP's RoughEase and anime.js's irregular, the same curve for a seed on every platform. It fills points the host keeps, and is then `linear()` through them, so its ends stay exactly 0 and 1. A taper grows or calms the offsets toward the ends; the flags clamp the curve to 0 to 1, keep it rising, or set an even zig-zag with no seed.

```c
mtwnLinearPoint points[22]; // kept while the easing is used
mtwnRoughDef def = {.shape = mtwnLinearEasing(), .strength = 0.1, .seed = 42};
def.taper = mtwn_roughTaperOut; // calm by the end
mtwnEasing rough;
mtwnResult madeRough = mtwnMakeRoughEasing(&def, points, 22, &rough);
```

## Tables and `linear()`

`mtwnSampleEasing` fills a table of evenly spaced samples, for a shader or a lookup. `mtwnFitLinearEasing` turns any easing but steps into CSS `linear()` points within a tolerance, to hand to a browser or another engine: the points lie on the curve and the line between them is within the tolerance at 32 probes in each segment. A buffer that is too small gets `mtwn_errorCapacity` and the count needed, so call again with that capacity. A fit makes at most `MTWN_MAX_FIT_POINTS` points; a tolerance the curve cannot meet (below its rounding) ends with `mtwn_errorLimit` rather than with millions of them. Steps are refused: CSS writes them as `steps()`.
