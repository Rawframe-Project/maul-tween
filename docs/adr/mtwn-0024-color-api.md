# mtwn-0024. Colors as CSS Color 4 interpolates them

Status: Accepted

## Context

Hosts compare animated colors against the web platform, and CSS Color 4
specifies interpolation exactly: the conversions, which hue counts as
powerless, how missing hues fill in, the four hue methods, and
premultiplication with what to do at alpha 0. A transition samples one
pair of colors every frame, so converting both ends every time would
waste most of the work.

## Decision

- `mtwnColor` is gamma-encoded sRGB with straight alpha, unclamped:
  channels outside 0 to 1 are extended sRGB, sRGB's transfer reflected
  for negative values as CSS's sample code does.
- `mtwnColorMix` holds two colors converted to Oklab, OkLCh, linear
  sRGB or gamma sRGB and premultiplied; `mtwnMixColor` interpolates and
  converts back. The steps are CSS's: an OkLCh hue with chroma at most
  0.000004 is powerless and takes the other color's, two missing hues
  are 0, the hues are fixed up by the method (shorter, longer,
  increasing, decreasing), the hue is not premultiplied, and at an
  interpolated alpha of 0 the result is the premultiplied value. The
  ends are the colors given, exactly; past them alpha is clamped and
  the channels are not.
- `mtwnColorToSpace` and `mtwnColorFromSpace` convert one color, for a
  host that springs or composes in a space.
- The matrices are those of CSS's `conversions.js`, multiplied in
  80-digit decimal into one linear sRGB to LMS matrix and its inverse
  and rounded once to binary64 by `tools/gen_color_matrices.py`.

## Consequences

Colors match CSS's sample code to an ulp or two, and every space gives
the same floats back after a round trip.

## Amendment (2026-10-14)

OkLCh's hue is in radians, from 0 to 2 pi, at `mtwnColorToSpace` and
`mtwnColorFromSpace`, as every angle in the library is (record
mtwn-0025); CSS's degrees are converted by the host at its CSS edge.
