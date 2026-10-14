# mtwn-0014. Colors in a named space, Oklab by default, premultiplied

Status: Accepted

## Context

Interpolating gamma-encoded sRGB gives dark, muddy midpoints; CSS Color
Level 4 makes Oklab the default interpolation space and specifies
premultiplied alpha and hue handling.

## Decision

Colors cross the API as gamma-encoded sRGB, or linear sRGB, with
straight alpha. They interpolate in Oklab (the default), OkLCh with
CSS's shorter, longer, increasing and decreasing hue methods and
powerless hues, linear sRGB or gamma sRGB, premultiplied as CSS Color
Level 4 requires; composition and springs act in that space. Results are
not clamped. The conversion matrices are CSS Color Level 4's published
values, combined with sRGB's by a generator.

## Consequences

Color animations look right by default, match browsers' modern
interpolation, and keep a wide-gamut host's colors.
