# Values and paths

How each value kind interpolates, and how paths and tracks are built and measured.

## Colors

Colors cross the interface as gamma-encoded sRGB with straight alpha, unclamped: channels outside 0 to 1 are extended sRGB, sRGB's transfer reflected for negative values as CSS's sample code does. They interpolate as CSS Color 4 does ([`src/color.c`](../../src/color.c)), in Oklab by default, OkLCh, linear sRGB or gamma sRGB, premultiplied by alpha. An OkLCh hue with chroma at most 0.000004 is powerless and takes the other color's; two missing hues are 0; the hue is fixed up by the method and not premultiplied; and where the interpolated alpha is 0 the result is the premultiplied value. A mix holds both ends converted and premultiplied, so a sample costs one conversion back, and its ends are the colors given, exactly.

The conversion matrices are those of CSS's `conversions.js`, multiplied in 80-digit decimal into one linear sRGB to LMS matrix and its inverse and rounded once to binary64 by `tools/gen_color_matrices.py` into `src/generated/color_matrices.h`. Rounding each published matrix separately and multiplying in double would put two roundings between every color and its Oklab.

## Angles and rotations

An angle's far end moves by whole turns, as CSS's hue methods move a hue ([`src/rotation.c`](../../src/rotation.c)), and the end stays exact when it moves by none.

A rotation mix holds its ends normalized in double, the second negated for the shortest way or the longest, and Kahan's angle between them, `2 atan2(|q0 - q1|, |q0 + q1|)`, accurate at every angle where `acos` of the dot product loses half its digits near 0. Slerp's weights are written with `sinc`, which keeps its digits at any argument but 0, so a tiny angle keeps its digits too. Unit ends then give a unit result to an ulp of double, far below a float's, so the result is not normalized again. The longest way between equal rotations turns a full circle about no defined axis and is refused.

## Integers, discrete values, scales

Integers interpolate in double and round to nearest, ties away from zero, so they pass through every integer between and are exact at the ends and within 2^53. Discrete values step at progress 1/2, as Web Animations steps values it cannot interpolate.

A scale is a value moved in its logarithm, not an easing. An easing that knows its ends breaks when a transition is retargeted, when values are composed, and on a track of several keyframes; a value kind lets any easing apply as it is. A scale mix is `from (to / from)^p`, the ratio kept as a difference of logarithms so it cannot overflow ([`src/scale.c`](../../src/scale.c)). As a kind, a scale eases, springs (overshooting by a ratio), composes (multiplying) and curves on tracks in its logarithm.

The zoom is van Wijk and Nuij's optimal path ([`src/zoom.c`](../../src/zoom.c)), written to keep its digits where the paper's form loses them: the pan `u(s)` as `w0 sinh(rho s) / (rho^2 cosh(rho s + r0))` in place of a difference that cancels near 0, `asinh` by its stable form, the terms in units of the first width so views of any scale take the same path, and equal centers as a geometric zoom of length `|log(w1 / w0)| / rho`, the limit of a pan going to 0.

## Paths

A path references the caller's keyframes and, for constant speed, a caller array of cumulative arc lengths it fills; nothing is allocated ([`src/path.c`](../../src/path.c)). A progress picks the last keyframe at or below it (the end segments extend outside [0, 1]), eases the segment's local progress by its easing and evaluates its kind.

- **Catmull-Rom** is the cubic Hermite with Yuksel, Schaefer and Keyser's end tangents for knots spaced by the chord's length to the power alpha, the ends' missing neighbors reflected. Each knot gap is raised to 1e-12 of the largest of the segment's three, so the curve is the same at any scale and coincident keyframes hold still. Centripetal (alpha 0.5) is the default because it is the parameterization with no cusps or self-intersections within a segment.
- **Monotone** segments are a Hermite cubic per channel whose slopes in time are Steffen's: `(sign a + sign b) min(|a|, |b|, |p| / 2)` between secants `a` and `b`, with `p` the parabola's slope through the three keyframes. That is 0 where the channel turns, never more than twice the lesser secant, so the cubic stays between its keyframes (Fritsch and Carlson's condition). At the path's ends and beside a jump, d3's one-sided slope `(3 s - t) / 2`. A segment's tangents are its slopes times its width, so under a linear easing the speed is continuous across keyframes of any spacing.

## Arc length and constant speed

Arc length is adaptive Gauss-Kronrod, 15 points against 7 agreeing to 1e-12 relative, split at the speed's minima, where a segment's speed dips toward a near-cusp and a fixed rule misses the length ([`src/path_length.c`](../../src/path_length.c)); one channel's length is its total variation, exactly. At constant speed a progress is a share of the whole length: the segment is found by binary search and the parameter at a length by safeguarded Halley steps. Each segment's length is within about 1e-11 of the truth, relative.

A table of lengths, as GSAP keeps, is several times cheaper per sample but puts a point up to 7.7e-4 of the length off, so its constant speed speeds up and slows down; the library measures instead.

## Direction

`mtwnPathDirection` follows SVG 2's path directionality ([`src/path_direction.c`](../../src/path_direction.c)): within a segment the first non-zero of the first three derivatives, normalized, so a control on its end or a cusp gives the outgoing direction; at a keyframe the later segment's; a step or a segment of no length takes the direction of the nearest segment with a length before it, else after it. `mtwnPathAngle` uses the library's own arctangent so the angle is the same everywhere. A 3D orientation is a look-at from this direction and an up vector, a rotation's question, and is not built.

## Tracks

A track makes a value kind's keyframes into a path ([`mtwnMakeTrack`](../../include/maul-tween/path.h)). Angles are unwrapped in place, each keyframe and its controls moved by the method's whole turns from the one before; rotations on the shortest path are flipped in place into the hemisphere of the one before. The keyframes are therefore taken by a pointer to modifiable data, and a clip copies them before it makes its track. Rotations take Bezier and Catmull-Rom per component and normalize after, as glTF does. A track tries every rotation mix before it writes any, so a refused track leaves its keyframes and mixes as they were.

## Traps

- Colors and discrete values refuse spline segments, as CSS has no color splines; a new segment kind decides per value kind.
- The color matrices are generated; change `tools/gen_color_matrices.py` and regenerate, never the header.
- Equal keyframe offsets make a jump. A monotone segment beside a jump takes the one-sided slope, not a secant over no width.
