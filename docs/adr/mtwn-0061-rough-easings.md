# mtwn-0061. Rough easings, seeded

Status: Accepted

## Context

GSAP's RoughEase draws points around a template ease, sorted by input,
offset within a strength shaped by a taper (in, out, both), optionally
clamped to 0 to 1, or set in an even zig-zag; anime.js 4's `irregular`
draws rising values between even steps and hands them to its
`linear()`. Both draw from `Math.random()`, so a replay, a test or two
peers see different curves. GSAP's CustomWiggle oscillates and ends
where it began; its SlowMo is linear in the middle with power curves at
the ends.

## Decision

`mtwnMakeRoughEasing` fills a host's array of `linear()` points from a
`mtwnRoughDef` and makes the `linear()` easing through them (record
mtwn-0011). The def holds the shape (any easing; zeroed, linear), the
strength (the largest offset either side, in eased progress), a seed,
a taper and flags:

- the points' draws are SplitMix64 of the seed, as shake's gradients
  (record mtwn-0026), so a seed gives the same bits on every platform;
- the first point is (0, 0) and the last (1, 1); the k-th of the m
  inner points falls at a drawn input in [k/m, (k+1)/m), in order
  without a sort and never two at one input, where GSAP's sorted
  uniform draws can meet in a jump;
- each output is the shape there plus a drawn offset of at most the
  strength times the taper's share: 1, x^2, (1 - x)^2, or
  (1 - |2x - 1|)^2, GSAP's quadratics, the last peaking at 1 so the
  strength means the same for each;
- `mtwn_roughClamp` keeps outputs in [0, 1]; `mtwn_roughRising` keeps
  each at least the one before and at most 1, so the easing never
  decreases (anime.js's irregular); `mtwn_roughZigZag` sets the inputs
  at their ranges' middles and the offsets at the full strength,
  alternately up and down, with no seed (GSAP's unrandomized points).

A kind evaluated from the seed at each progress was not taken: the
curve is `linear()` already, with its modes and its monotonicity rule.
A wiggle is a displacement that ends where it began, as punch and shake
are (record mtwn-0026), not a progress; SlowMo is GSAP's alone among
the libraries surveyed, and a host easing gives it exactly. Neither is
built.

## Consequences

`test_rough` checks a seed's points bit for bit against SplitMix64
worked in Python, a seed repeating and another differing, every
offset within its strength by each taper about a back shape, each
input inside its range, the flags, and the refusals. The curves part's
size budget is restated from 24 to 25 kB: the maker took it from 22.9
to 24.0 kB, its loop kept to one copy.
