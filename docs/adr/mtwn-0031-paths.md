# mtwn-0031. Paths: keyframe curves in the values layer

Status: Accepted

## Context

Record mtwn-0016 gives tracks keyframes with linear, step, cubic Bezier
and Catmull-Rom segments, centripetal by default, and constant speed by
arc length. Record mtwn-0003 keeps every curve in the values layer, so
a host can bake a path with no context.

## Decision

- `mtwnKeyframe` holds a value of up to four channels, its offset in
  [0, 1], the kind and easing of the segment it starts, and Bezier
  controls (`out` starts a segment, `in` ends one). Offsets do not
  decrease, start at 0 and end at 1; equal offsets jump.
- `mtwnMakePath` references the caller's keyframes and, for constant
  speed, a caller array it fills with the cumulative arc length at each
  keyframe. Nothing is allocated.
- A progress picks the last keyframe at or below it (the end segments
  extend outside [0, 1]), eases the segment's local progress by its
  easing and evaluates its kind. A step holds until the next keyframe.
- Catmull-Rom takes a knot exponent alpha from 0 to 1 (0.5
  centripetal, 0 uniform, 1 chordal), reflects the ends' missing
  neighbors, and is evaluated as a cubic Hermite with Yuksel, Schaefer
  and Keyser's end tangents; each knot gap is raised to 1e-12 of the
  largest of the segment's three, so the curve is the same at any
  scale and coincident keyframes hold still.
- Arc length is adaptive Gauss-Legendre (orders 5 and 10 agreeing to
  1e-9 relative, a fixed stack bounding the splitting). At constant
  speed a progress is a share of the whole length; the segment is found
  by binary search and the parameter by safeguarded Newton. Step
  segments are refused there.
- `mtwnSamplePath` also gives the segment's tangent by its own
  parameter, for orienting along the path.

## Consequences

The tests find the Hermite form within 1.6e-15 of Barry and Goldman's
pyramid in long double for alpha 0, 1/2 and 1, arc length within 1e-15
relative of an extrapolated polyline, and constant speed within 6e-12
on lines whose parameter is uneven.

## Amended 2026-10-16: the length and the parameter, faster and exact

- Arc length is adaptive Gauss-Kronrod: QUADPACK's 15-point rule
  against its 7-point Gauss rule, agreeing to 1e-9 relative. About an
  interval's middle the derivative is a quadratic, so each pair of
  abscissas shares its even part.
- A path of one channel has its total variation for a length, summed
  between the turns where its derivative is 0 (the roots by the stable
  quadratic formula): exact, where a quadrature misses the kink each
  turn puts in |x'|. A number turning twice had points at constant
  speed 1.1e-4 of its length off.
- The parameter at a length starts from the length measured from the
  segment's nearer end and takes Halley's steps (the speed and its
  first two derivatives), in the bracket as before. A step under 1e-4
  in the parameter, where the speed is at least 1e-3 of the segment's
  length, adds the length's Taylor series to h^3 instead of measuring.
- Summing each Newton step's measured length had drifted from the
  length measured afresh: on a random curve of two channels a point
  came 1.6e-7 off. Kronrod's pieces, fewer and far more accurate, keep
  the sum within the tolerance; a test checks each share's point
  against the parameter found again by Simpson's rule in long double.

The bench's path through eight points (record mtwn-0058): 5,250 to
2,026 instructions a sample, GCC 14 at `-O2`; the library's code
0.6 kB smaller.

## Amended 2026-10-16: the lengths' error, measured

The rule is asked for 1e-9 relative agreement between its 15 and 7
points, but that difference overstates the error by orders, as
QUADPACK's own estimate does: the 15-point result is far closer than
the 7-point one it is checked against. Measured on 1,000 random paths
of two to four channels (4,536 segments of Bezier, Catmull-Rom and
linear kinds, alpha from 0 to 1), against each segment's length in
long double by Gauss-Legendre on 16,384 panels of 8 points (the same at
32,768; at 4,096 a segment with a cusp in its speed still moved), the
worst segment's length is 1.8e-12 relative and the worst path's
6.1e-13. A path of one channel is exact (its total variation).

## Amended 2026-10-17: a length the cap ends

A length takes at most 1,024 pairs of rules: where the speed falls to 0
at a span's end, its rounding keeps the two rules apart at every width,
and a span so narrow that the floor on the width lies below the
doubles' spacing would split without end.
The cap ends such a length with the intervals still unagreed accepted
as the 15-point rule gives them. It is reached only where the span lies
within about 2^-30 of a still point, where the speed is its own
rounding, so what it accepts is off by that rounding over a span that
narrow, not by the rules' disagreement.

Measured against exact lengths (binary128 total variations of cubics
moving along a line, two and three channels, with a still point inside,
a still end, or two turns, over spans of every width up to 2^-52 about
those points): 19,727 of 146,756 lengths reached the cap, the worst off
by 9.1e-22 of its segment's length. A point at constant speed stays
within the inverse's own tolerance, 1e-12 of the segment's length;
`test_path` samples a still-ended line at shares 2^-1 to 2^-52 from
each end, 25 of them reaching the cap, and checks each point to that.

No status reports a capped length: its error is nine orders under the
tolerance the point is placed to, and a host could do nothing with it.
The bound is relative to the segment, not to the capped span, which can
be 2^-40 of it and is the part that rounds.

The same measurement found what the cap does not touch: a cubic of two
or more channels that turns back along a line inside a segment, where
the speed has a kink that neither rule sees when none of their points
falls between its two turns. Outside them the speed is a polynomial
both rules integrate exactly, so they agree, and the length misses the
turn: 1.1e-3 of a path's length at worst, on paths of collinear Bezier
keyframes. That is a separate defect, to be fixed on its own.

## Amended 2026-10-17: lengths split at the speed's minima

A length of two or more channels is split where the speed is least
inside its segment, as QUADPACK's qagp splits an integral at the points
its caller names: where v.a, half the derivative of the speed squared,
rises through 0, at most two places. They are found once for a segment's
length and once for a sample at constant speed: the cubic v.a's own
turns bracket each rise, found by Newton's steps kept in the bracket.
A turn back along a line, or near it, then lies at a piece's end, where
the rules disagree until the splitting reaches it, instead of between
their points. Halley's Taylor shortcut is not taken across a minimum.

Options weighed: a test of the minimum's sharpness (split only where
the speed's width about it, |v| / |a|, is under the span) changed no
measured result and saved 17 instructions of 380; splitting always is
the simpler rule. Tightening the agreement does not help: both rules
are exact on the polynomial outside the turns and agree to the last
bit.

Measured against exact lengths:

- Cubics moving along a line (the probe of the amendment above): the
  worst length 9.7e-16 of its segment, where it was 2.0e-3.
- 20,000 random collinear paths through the API, Bezier and uniform
  Catmull-Rom: none off by 1e-9 (1,694 and 1,443 were), the worst
  1.3e-15 and 3.4e-15 relative.
- Near turns, a line's cubic with a constant sideways speed from 2^0
  to 2^-29 of its scale, against long double quadrature split at the
  turns, 120,000 spans: the worst 6.1e-9 of the segment, where it was
  4.2e-3. A piece ending at so sharp a minimum is accepted at the
  rules' 1e-9 agreement, which there is about the error itself rather
  than orders above it (once six times under it): 13,055 spans were off
  by more than 1e-12, where 2,452 were before but up to 4.2e-3.
- 2,000 random paths of two and three channels: the worst path length
  2.5e-10 relative before and after, 9 and 9 paths off by more than
  1e-11. A third of the lengths and points at constant speed move in
  their last bits, the most 4.9e-10 relative.
- The capped lengths of the amendment above: 19,820 of 146,756, the
  worst 4.7e-19 of its segment (9.1e-22 before the split).

The bench's path sample (record mtwn-0058) takes 2,305 instructions,
from 1,925: finding the minima about 160, the extra pieces about 220.
The values part (record mtwn-0055) is 49.3 kB, from 47.5, under its
50.0 kB budget: GCC 14 vectorizes the minima's sum over the channels.
Passing the derivative's coefficients instead of a cubic made the
object smaller and the part 0.3 kB larger, and was left out.
`test_path` measures lines turning back in two to four channels, each
length to 1e-14 relative and each share's point to 1e-11 of the length
against the total variation; it fails on the earlier code.
