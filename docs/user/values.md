# Values and paths

`interpolate.h` interpolates every kind of value a target can hold, and `path.h` makes curves through keyframes. Like the easings, they are plain values made once by a function that checks them, and sampled by calls that check them again, by cheaper means, and return a status.

## Value kinds

A target or a track holds one kind of value in up to four float channels (`mtwnVec4`):

| Kind | Channels | Moves |
|---|---|---|
| `mtwn_valueScalar` to `mtwn_valueVec4` | 1 to 4 numbers | Per channel. |
| `mtwn_valueColor` | red, green, blue, alpha | In a color space, premultiplied. |
| `mtwn_valueAngle` | radians in x | By a method of whole turns. |
| `mtwn_valueRotation` | a quaternion x, y, z, w | By slerp. |
| `mtwn_valueInteger` | x | Rounded to nearest, ties away from zero. |
| `mtwn_valueDiscrete` | x, an index the host keeps | Steps at progress 1/2. |
| `mtwn_valueScale` | x, above 0 | In its logarithm. |

## Colors

Colors are gamma-encoded sRGB with straight alpha, as CSS's `rgb()`; channels outside 0 to 1 are extended sRGB, for wide-gamut hosts. They mix as CSS Color 4 interpolates them, in Oklab (CSS's default), OkLCh with CSS's four hue methods, linear sRGB or sRGB:

```c
mtwnColor red = {1.0f, 0.0f, 0.0f, 1.0f};
mtwnColor blue = {0.0f, 0.0f, 1.0f, 1.0f};
mtwnColorMix mix;
mtwnResult made = mtwnMakeColorMix(red, blue, mtwn_colorOklch, mtwn_hueShorter, &mix);
mtwnColor between;
mtwnResult mixed = mtwnMixColor(&mix, 0.5, &between); // half way, in OkLCh
```

A gray's hue in OkLCh is powerless and takes the other color's, as CSS says, so a fade from white to red does not sweep through the hues. Past progress 0 to 1 alpha is clamped and the channels are not. `mtwnColorToSpace` and `mtwnColorFromSpace` convert one color, for a host that springs or blends in a space itself.

## Angles and rotations

`mtwnMixAngle` interpolates radians raw (turns kept, so 0 to 4 pi turns twice) or by CSS's hue methods with a period of a full turn: shorter, longer, increasing, decreasing. It refuses an unknown method, and an angle or progress that is not finite, with `mtwn_errorInvalid`. A rotation mix (`mtwnMakeRotationMix`, `mtwnMixRotation`) slerps unit quaternions at constant angular speed, the shortest way or the longest; its result is unit length to a float's rounding. The longest way between two equal rotations has no axis and is refused.

## Integers and discrete values

`mtwnMixInteger` passes through every integer between its ends, exact at them. `mtwnMixDiscrete` steps from the first value to the second at progress 1/2, as Web Animations steps values it cannot interpolate: an enumeration, a flag, a sprite index. Both refuse a progress that is not finite with `mtwn_errorInvalid`, as every mix does.

## Scales and zoom

A scale (a zoom factor, a size from 1 to 100) mixed linearly seems to slow down, each step adding the same amount. A scale moves in its logarithm instead, so equal steps multiply by equal ratios. As a kind, `mtwn_valueScale`, it does so everywhere: transitions ease it, springs overshoot it by a ratio, tracks curve in the logarithm and layers multiply.

```c
mtwnScaleMix zoom;
mtwnResult made = mtwnMakeScaleMix(1.0, 100.0, &zoom);
double half = 0.0;
mtwnResult mixed = mtwnMixScale(&zoom, 0.5, &half); // 10, not 50.5
```

To move a view (a centre and the width it shows) where both the centre and the zoom change, van Wijk and Nuij's path zooms out as far as the pan needs and back in, so the eye can follow. Its length gives a duration that suits it:

```c
mtwnZoomView from = {0.0, 0.0, 1.0};
mtwnZoomView to = {50.0, 20.0, 2.0};
mtwnZoomMix flight;
mtwnResult made = mtwnMakeZoomMix(from, to, 1.42, &flight);
double length = 0.0;
mtwnResult measured = mtwnZoomLength(&flight, &length);
double seconds = length * 1.42 / sqrt(2.0); // d3's pace
// Halfway it is wider than both ends.
mtwnZoomView view;
mtwnResult mixed = mtwnMixZoom(&flight, 0.5, &view);
```

`rho` trades zoom against pan: the paper's measured `sqrt(2)` is d3's default, 1.42 Mapbox's. Past its ends the path goes on, its width going to 0 as its centre settles on a point.

## Checking a mix

`mtwnMixColor`, `mtwnMixRotation`, `mtwnMixScale`, `mtwnMixZoom` and `mtwnZoomLength` give their result through a pointer and return a status. Each refuses a null pointer, a progress that is not finite, and a mix whose fields are outside their ranges, leaving the output as it was. A mix's fields are the library's, but the host holds it and could write anything there, so each call checks the ranges. It does not make the mix again, which would cost as much as the sample. A mix that passes the check gives no undefined behavior and no NaN; it is not promised to equal what a maker would give. A color or rotation is finite, held within the floats as a target holds it; a scale or zoom reaches infinity or 0 only past the doubles' range, as a maker's mix does. A track checks the mix of the segment it samples the same way.

## Paths

A path runs through two or more keyframes, each a value at an offset from 0 to 1 and the segment that starts there, with that segment's easing. Segments are linear, step, cubic Bezier (through the keyframe's `out` control and the next one's `in`), Catmull-Rom (centripetal by default: no cusps or loops within a segment) or monotone. `mtwnMakePath` references the keyframes, which must outlive the path and not change; given a lengths array, it measures the arc length and the path travels at constant speed, the offsets and easings no longer timing it. `mtwnSamplePath` gives the value, and the tangent if asked, through pointers and returns a status: it refuses a progress that is not finite, a path whose own fields no maker gives, and the segment it reads unless that segment's kind is known (at constant speed, not a step) and, timed by its keyframes, its easing is a maker's. Keyframes or lengths changed otherwise after making give an unspecified value, perhaps not finite, but nothing is read outside the arrays; what a keyframe's easing references (a host function, its context, `linear()`'s points and their count) stays the host's, as every easing's does, and changing it is the host's error, as changing the keyframes pointer is.

A Catmull-Rom curve is a point's path, smooth in space. A monotone one is a value's path over time, as curve editors' clamped tangents are: each channel stays between its keyframes, flat where it turns, its speed continuous across keyframes however far apart.

Something moving along a path turns with it: `mtwnPathDirection` gives the unit direction as CSS's `offset-rotate: auto` and SVG's rules take it, holds and cusps included, and `mtwnPathAngle` its angle in the plane, the same bits on every platform. Both refuse what a sample refuses.

A curve editor's key has handles in time as well as in value (Blender's F-curve handles, Unity's weighted tangents). Its segment is a Bezier through the handles' values, timed by a cubic-bezier easing of the handles' times as shares of the span, with y controls 1/3 and 2/3 so the easing gives the curve's own parameter. A handle reaching past the span is shortened along itself first, as Blender does. The handles of a key at 0 s and value 0 and one at 2 s and value 10, at (0.5 s, 6) and (1.2 s, 10):

```c
mtwnKeyframe keys[2] = {0};
keys[0].kind = mtwn_segmentBezier;
keys[0].out = (mtwnVec4){6.0f, 0, 0, 0}; // the out handle's value
keys[1].offset = 1.0;
keys[1].value = (mtwnVec4){10.0f, 0, 0, 0};
keys[1].in = (mtwnVec4){10.0f, 0, 0, 0}; // the in handle's value
// Their times, 0.5 s and 1.2 s, as shares of the 2 s span.
mtwnResult made = mtwnMakeCubicBezier(0.25, 1.0 / 3.0, 0.6, 2.0 / 3.0, &keys[0].easing);
```

A segment's channels share its easing, so handles timed apart per channel are one track a channel, as Blender's F-curves are.

## Tracks

A track gives any value kind its keyframes (`mtwnMakeTrack` from an `mtwnTrackDef`, `mtwnSampleTrack`): numbers and vectors are their path; integers round it; discrete values step at an eased 1/2; angles unwrap by their method; colors mix linear segments in their space; rotations slerp linear segments and take Bezier and Catmull-Rom per component, normalized; scales are a path over their logarithms. `mtwnSampleTrack` gives the value through a pointer and refuses as a path's sample does, and a track whose own fields no maker gives. Clips copy their keyframes and make their tracks themselves, so a host makes tracks directly only to sample them on its own.

## Not obvious

- `mtwnMakeTrack` rewrites the keyframes in place for angles (unwrapped) and for rotations on the shortest path (flipped into one hemisphere). One array must not feed two tracks with different angle methods or rotation paths.
- Colors and discrete values take only linear and step segments, as CSS has no color splines. Constant speed is for numbers, vectors, angles, integers and scales.
- A track's sample is held within the floats' range, as a target holds its value: a value past the largest float comes back as the largest float, never infinity.
- Equal offsets make a jump. Past 0 and 1 a path extends its first and last segments.
