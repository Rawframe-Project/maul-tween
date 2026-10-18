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
