# mtwn-0070. Mixes of equal ends

Status: Accepted

## Context

The library mixes two values as `(1 - t) a + t b` (a color's
coordinates, a number transition's value and its speed's earlier sample,
an angle, an integer), which is exact at t = 0 and t = 1; records of its
rotations and transitions hold that exactness ("the end exactly where
from + t (to - from) would miss it", "even between ends thirty orders
apart"). Far past its ends, where an easing's output passes about 1e16
(a host curve, `linear()` points), the two terms cancel: a color from
black to white at t = 1e20 had an alpha of 0, both ends being 1, and a
transition's channel between equal ends left its value.

C++'s `std::lerp` (P0811R3) is exact at both ends, monotone, bounded and
consistent (`lerp(a, a, t) == a`); its reference returns b at t = 1 and
otherwise takes `a + t (b - a)` held against b near 1.

## Decision

A mix is `(1 - t) a + t b`, and a when a = b. The weighted sum's error
far past its ends is an ulp of t a beside a result of at least t |b - a|,
so between distinct floats its relative error stays near 2^-29 at any t;
only equal ends lose their value, wholly past t = 2^53. Testing
equality fixes that case and keeps every exact end. One helper,
`mtwnMix`, serves every mix. A color the values layer mixes is held
within the floats where it becomes one, as a target's is (mtwn-0069).

P0811's reference was measured and set aside: Oklab's sample from 1,470
to 1,521 instructions, past its bound, for a monotonicity and
boundedness the ends' exactness already gives within [0, 1]. Plain
`a + t (b - a)` loses the exact end at t = 1.

## Consequences

Equal ends stay put at any progress; a color mixed far out keeps its
alpha and stays finite. Oklab's sample is 1,503 instructions (bound
1,520) and OkLCh's 1,671. The determinism scenario's hash changes, its
equal channels now mixing to their end exactly where the sum rounded.
`test_color`, `test_signal`, `test_rotation` and `test_transition` hold
each mix far out, and each fails with the weighted sum alone.

## Amended 2026-10-18: a color's mix without the test

Counted as record mtwn-0058 counts (GCC 14 at `-O2`), the test of equal
ends took Oklab's sample from 1,779 to 1,827 instructions and OkLCh's
from 2,012 to 2,059, past their targets of 1,800 and 2,050: four tests
on the four coordinates kept GCC from mixing them as one vector. A
color's mix returns its ends at t = 0 and t = 1 before it mixes, so it
needs no exact end there, and its form is now `a + t (b - a)`
(`mtwnMixInside`), as culori 4.0.2's `lerp` is: a for a = b at any
finite t without a test, and cheaper than the weighted sum. Oklab 1,784
and OkLCh 2,017 at `-O2` (1,476 and 1,644 at `-O3`, bounds 1,520 and
1,695). Every other mix, whose ends do come to it, keeps `mtwnMix`. The
form's error far past the ends is an ulp of t (b - a), the result's own
size; within [0, 1] it may pass b by an ulp near t = 1, which the
color's conversion back to floats and its alpha's clamp absorb.

## Amended 2026-10-18: one form, a + t (b - a), its error tested

The decision above bounded the weighted sum's error far past its ends
by an ulp of t a beside a result of at least t |b - a|, near 2^-29 of
it. Tested against the exact value (`test_mix`: 400,000 float ends,
equal, one ulp apart, thirty orders apart or random, at progress within
[0, 1] and up to 2^70 past it, the exact `a + t (b - a)` kept as an
unevaluated sum by TwoSum and `fma`), the claim fails: between ends one
ulp apart the line crosses 0 near t = -a / (b - a), about 2^23 past the
ends, where the result is far below t |b - a| and the weighted sum's
error, about u |t a|, reached 2^-14 of it (u = 2^-53).

Every mix is now `t == 1 ? b : a + t (b - a)`, the form of P0811's
reference for ends of one sign and, without the test, culori's. Its
error is at most u |r| + 2u |t (b - a)| for the result r (one rounding
of the difference, of the product and of the sum), measured at 1.96 of
u (|r| + |t (b - a)|); where the near ends cross 0, 2^-46 of the result
where the weighted sum gave 2^-14. It is exact at t = 0, at t = 1 by its
test, also between ends thirty orders apart whose difference rounds,
and for equal ends at any finite t, as a + t * 0; it is monotone in t,
each rounding being so. `test_mix` holds the bound, the exact cases
and the monotony.

The test is of t alone, the same for every channel: no count moved
(Oklab 1,784 and OkLCh 2,017 at `-O2`, the frames unchanged), and the
color, whose ends return before its mix, takes the same helper,
GCC dropping the test there; `mtwnMixInside` is gone. An infinite
progress, which only a host easing could give and which stays the
host's (mtwn-0069), now makes equal ends NaN, as it already made
distinct ones. The determinism scenario's hash moves to 0x7b710979e70fff40, its
mixes rounding differently.

## Amended 2026-10-18: within its ends for t in [0, 1]

For doubles in general `a + t (b - a)` may pass b just below t = 1,
which P0811's reference clamps. The library's ends are floats held in
doubles, and there it does not. Ends within 2^29 of each other have an
exact difference; `t (b - a)` for t below 1 rounds to no more than it,
and a plus that to no more than b, rounding being monotone. Further
apart the difference rounds, but `t (b - a)` stays a spacing of its
binade short of it (t is at most 1 - 2^-53, and below a power of two the
spacing halves, so it never rounds up to it), which covers that
rounding. So for t in [0, 1] every mix lies within its ends, as a double
and as the float it becomes, without a clamp. `test_mix` holds it on
400,000 float ends (random, one ulp apart, thirty orders apart, powers
of two) at t within 64 ulps below 1, at 1 - 2^-24 and below, and in
[0, 1]; a search of 40 million such cases found none outside.
