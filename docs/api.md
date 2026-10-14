# Maul Tween API reference

Generated from the public headers by `tools/gen_api.py`. The headers
are the source of truth; this file mirrors them.

## `base.h`

The base of the Maul Tween API: the library version, the export and attribute macros, and the result codes every fallible function returns.

```c
mtwnVersion mtwnGetVersion(void);
```
Returns the version of the library that was linked, which may differ from the MTWN_VERSION macros a program was compiled with.  @return The library version. @par Thread safety Safe from any thread.

```c
const char* mtwnResultName(mtwnResult result);
```
Returns the name of a result code, for diagnostics.  @param result  Any value; an unknown one is named as such. @return A static, NUL-terminated string such as "mtwn_errorCapacity". @par Thread safety Safe from any thread.

## `easing.h`

Easing: curves that map a timing's progress to eased progress (record mtwn-0011). An easing is a value made by a function that validates it and sampled by mtwnEase with no status to check: Penner's families, which land exactly on 0 and 1, and CSS Easing Level 2 entire. Every kind has the same four modes.

```c
mtwnEasing mtwnLinearEasing(void);
```
Returns the linear easing, whose eased progress is its progress.  @return The easing. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePowerEasing(mtwnEaseMode mode, double exponent, mtwnEasing* easingOut);
```
Makes the power easing p^k: quad for k = 2, cubic 3, quart 4, quint 5. GSAP's power(n) is k = n + 1. Integer exponents up to 8 are computed by multiplication.  @param mode        The mode. @param exponent    k, finite and above 0. @param easingOut   Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown mode or an exponent outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakePennerEasing(mtwnEasingKind kind, mtwnEaseMode mode, mtwnEasing* easingOut);
```
Makes one of Penner's easings without parameters: sine, expo, circ or bounce. Each lands exactly on 0 at 0 and on 1 at 1; expo is normalized to do so and differs from Penner's by at most 2^-10.  @param kind       mtwn_easingSine, mtwn_easingExpo, mtwn_easingCirc or mtwn_easingBounce. @param mode       The mode. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, another kind or an unknown mode. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeBackEasing(mtwnEaseMode mode, double overshoot, mtwnEasing* easingOut);
```
Makes the back easing p^2 ((s + 1) p - s), normalized to land on 1. GSAP's back(s) and Penner's in and out use s as given, 1.70158 by default; Penner's and tween.js's in-out use 1.525 s, which is this easing with that s.  @param mode       The mode. @param overshoot  s, finite and at least 0; 0 is the cubic. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown mode or an overshoot outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeElasticEasing(mtwnEaseMode mode, double amplitude, double period, mtwnEasing* easingOut);
```
Makes the elastic easing, GSAP's elastic(amplitude, period): out is 1 + a 2^(-10 p) sin((p - c) 2 pi / P) with c = P asin(1 / a) / (2 pi), normalized to land on 0 and 1, and in its reflection. An amplitude below 1 is 1 with the period divided by it, as GSAP does. GSAP's defaults are amplitude 1 with period 0.3, or 0.45 in-out; Penner's is amplitude 1.  @param mode       The mode. @param amplitude  a, finite and above 0. @param period     P, finite and above 0, in units of progress. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown mode or a parameter outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeHostEasing(mtwnEaseMode mode, mtwnEaseFunction function, void* context, mtwnEasing* easingOut);
```
Makes an easing of a host function, called with progress clamped to 0 to 1, under the mode's reflections. The library neither checks nor forces its ends.  @param mode       The mode. @param function   The function; it must be safe to call from every thread that samples the easing. @param context    Passed to the function; may be NULL. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null function or easingOut or an unknown mode. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeCubicBezier(double x1, double y1, double x2, double y2, mtwnEasing* easingOut);
```
Makes CSS's cubic-bezier(x1, y1, x2, y2), solved by safeguarded Newton to |x(t) - x| <= 1e-14 (record mtwn-0009): exactly 0 at 0 and 1 at 1, and outside 0 to 1 extended along CSS's end tangents.  @param x1         The first control point's x, from 0 to 1. @param y1         Its y, finite. @param x2         The second control point's x, from 0 to 1. @param y2         Its y, finite. @param easingOut  Receives the easing, in mode mtwn_easeIn. @return mtwn_success; mtwn_errorInvalid for a null pointer or a point outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSteps(uint32_t count, mtwnStepPosition position, mtwnEasing* easingOut);
```
Makes CSS's steps(count, position), with CSS's before flag: in the before phase, a progress landing exactly on a step takes the step below.  @param count      The steps: at least 1, or 2 for mtwn_jumpNone. @param position   Where it jumps. @param easingOut  Receives the easing, in mode mtwn_easeIn. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown position or a count outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeCssEasing(mtwnCssKeyword keyword, mtwnEasing* easingOut);
```
Makes the easing a CSS keyword names.  @param keyword    The keyword. @param easingOut  Receives the easing, in mode mtwn_easeIn. @return mtwn_success; mtwn_errorInvalid for a null pointer or an unknown keyword. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeLinearEasing(mtwnLinearPoint* points, uint32_t count, mtwnEasing* easingOut);
```
Makes CSS's linear() from its points, canonicalizing them in place as CSS does: a first input not given is 0 and a last one 1 or the largest before it; an input below an earlier one is raised to it; runs not given are spread evenly between their neighbors. The easing references the points, which must outlive it and not change. Outside the points' inputs it extends their end segments.  @param points     The points, in order: at least 2, each output finite and each input finite or NaN. @param count      The number of points. @param easingOut  Receives the easing, in mode mtwn_easeIn. @return mtwn_success; mtwn_errorInvalid for a null pointer or points outside the above, leaving the points as they were. @par Thread safety Safe from any thread; the points are used by one thread at a time.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnSetEaseMode(mtwnEasing* easing, mtwnEaseMode mode);
```
Changes an easing's mode, keeping its curve.  @param easing  An easing made by this library. @param mode    The mode. @return mtwn_success; mtwn_errorInvalid for a null pointer or an unknown mode, leaving the easing as it was. @par Thread safety Safe from any thread.

```c
bool mtwnIsEasingMonotonic(const mtwnEasing* easing);
```
Tells whether an easing never decreases, as a timeline's easing must: linear, power, sine, expo, circ and steps in every mode, a cubic bezier with both y from 0 to 1, linear() with outputs that never decrease; not back, elastic, bounce or a spring, and not a host function, which the library cannot inspect.  @param easing  An easing made by this library. @return Whether it never decreases. @par Thread safety Safe from any thread.

```c
double mtwnEase(const mtwnEasing* easing, double progress, bool before);
```
Samples an easing. CSS's curves follow CSS Easing Level 2 outside 0 to 1; Penner's curves, springs and host functions see progress clamped to 0 to 1. The modes reflect the curve, and the before flag reaches it where it samples progress unreflected.  @param easing    An easing made by this library. @param progress  The progress, finite. @param before    Whether the timing is in its before phase; it moves only curves that jump at their start. @return The eased progress: exactly 0 at 0 and 1 at 1 for Penner's curves, springs, linear and cubic bezier; what CSS defines for steps and linear(). @par Thread safety Safe from any thread.

## `interpolate.h`

Interpolation of values that are not plain numbers (records mtwn-0014, mtwn-0015, mtwn-0024 and mtwn-0025): colors in a named space as CSS Color 4 interpolates them, angles with CSS's hue methods, and rotations by slerp.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeColorMix(mtwnColor from, mtwnColor to, mtwnColorSpace space, mtwnHueMethod hue, mtwnColorMix* mixOut);
```
Makes the interpolation between two colors in a space, following CSS Color 4: a powerless OkLCh hue (chroma at most 0.000004) takes the other color's, the hues are fixed up by the method, and the colors are premultiplied, the hue excepted.  @param from    The color at progress 0: finite, alpha from 0 to 1. @param to      The color at progress 1, likewise. @param space   The space to interpolate in. @param hue     The hue method, read for OkLCh only. @param mixOut  Receives the mix. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown space or method, or a color outside the above. @par Thread safety Safe from any thread.

```c
mtwnColor mtwnMixColor(const mtwnColorMix* mix, double progress);
```
Samples a mix: its colors exactly at 0 and 1, between them the premultiplied coordinates interpolated and divided by the interpolated alpha, or kept as they are where that alpha is 0 or below. Progress outside 0 to 1 extends the line, with alpha clamped to 0 to 1; the channels are not clamped.  @param mix       A mix made by this library. @param progress  The progress, finite. @return The color. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnColorToSpace(mtwnColor color, mtwnColorSpace space, double coordinatesOut[4]);
```
Converts a color to a space's coordinates and its alpha, for a host that springs or composes in the space.  @param color           Finite, alpha from 0 to 1. @param space           The space. @param coordinatesOut  Receives the three coordinates and the alpha; an OkLCh hue in degrees from 0 to 360, NaN where it is powerless. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown space or a color outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnColorFromSpace(const double coordinates[4], mtwnColorSpace space, mtwnColor* colorOut);
```
Converts a space's coordinates and an alpha back to a color.  @param coordinates  The three coordinates and the alpha; a NaN OkLCh hue counts as 0, as CSS's missing hue does. @param space        The space. @param colorOut     Receives the color, alpha clamped to 0 to 1. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown space, or a coordinate or alpha not finite, a NaN hue excepted. @par Thread safety Safe from any thread.

```c
double mtwnMixAngle(double from, double to, mtwnAngleMethod method, double progress);
```
Interpolates an angle in radians: the end at progress 1 is moved by whole turns as the method asks, then the two are interpolated linearly. At 0 the result is from exactly, and at 1 to exactly when no turn was added.  @param from      The angle at progress 0, finite. @param to        The angle at progress 1, finite. @param method    The method; an unknown one is raw. @param progress  The progress, finite; outside 0 to 1 extends. @return The angle. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeRotationMix(mtwnQuat from, mtwnQuat to, mtwnRotationPath path, mtwnRotationMix* mixOut);
```
Makes the spherical interpolation between two rotations, at constant angular speed, the shortest way or the longest.  @param from    The rotation at progress 0: finite, not zero; it is normalized. @param to      The rotation at progress 1, likewise. @param path    Which way. @param mixOut  Receives the mix. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown path, a rotation outside the above, or the longest way between rotations so close (|q0 + q1| under 1e-6 once the path's sign is taken) that it turns a full circle about no defined axis. @par Thread safety Safe from any thread.

```c
mtwnQuat mtwnMixRotation(const mtwnRotationMix* mix, double progress);
```
Samples a rotation mix: its rotations exactly at 0 and 1 (the second negated when the path needed it), between and beyond them slerp, normalized.  @param mix       A mix made by this library. @param progress  The progress, finite. @return The rotation. @par Thread safety Safe from any thread.

## `sample.h`

Sampling (record mtwn-0022): an easing turned into points in caller memory, as an even table for a GPU or a host lookup, or as the points of a CSS linear() fitted to a tolerance. Nothing is allocated.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnSampleEasing(const mtwnEasing* easing, uint32_t count, float* valuesOut);
```
Samples an easing evenly over progress 0 to 1: valuesOut[i] is the eased progress at i / (count - 1), rounded to float.  @param easing     An easing made by this library. @param count      The number of samples, at least 2. @param valuesOut  Receives count values. @return mtwn_success; mtwn_errorInvalid for a null pointer or a count below 2. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnFitLinearEasing(const mtwnEasing* easing, double tolerance, mtwnLinearPoint* pointsOut, uint32_t capacity, uint32_t* countOut);
```
Fits CSS linear() points to an easing over progress 0 to 1: their interpolation is within the tolerance of the eased progress at every probe, 32 inside each segment, and is not proven so between them; the points lie on the curve, the first at 0 and the last at 1. A segment narrower than 2^-24 is kept as it is, so a host function's jump costs points and may stay over the tolerance within 2^-24 of it. Steps are refused: CSS writes them as steps().  @param easing     An easing made by this library, not steps. @param tolerance  The largest error allowed, finite and above 0. @param pointsOut  Receives the points, with their inputs; may be NULL when capacity is 0. @param capacity   The number of points pointsOut holds. @param countOut   Receives the number of points the fit has, also when they do not fit. @return mtwn_success; mtwn_errorCapacity when more than capacity points are needed, the contents of pointsOut then unspecified; mtwn_errorInvalid for a null easing or countOut, a NULL pointsOut with a capacity, steps or a tolerance outside the above. @par Thread safety Safe from any thread.

## `spring.h`

Springs, decay and the rubber band (records mtwn-0010 and mtwn-0023): a damped spring pulling a value to its target, and a velocity decaying to rest, both solved in closed form from where they start, so the same times give the same values at every frame rate; and iOS's rubber band for dragging past a limit. A spring's start is an offset from the target and a velocity, in the value's units (per second); a retarget keeps the velocity and changes the offset.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpring(double stiffness, double damping, double mass, mtwnSpring* springOut);
```
Makes a spring from its physical parameters: natural frequency sqrt(stiffness / mass), damping ratio damping / (2 sqrt(stiffness mass)).  @param stiffness  Finite and above 0. @param damping    Finite and at least 0. @param mass       Finite and above 0. @param springOut  Receives the spring. @return mtwn_success; mtwn_errorInvalid for a null pointer or a parameter outside the above or giving a frequency outside the finite range above 0. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpringFromFrequency(double hertz, double dampingRatio, mtwnSpring* springOut);
```
Makes a spring from its natural frequency in hertz and its damping ratio: 1 is critical, below it overshoots, 0 never settles.  @param hertz         Finite and above 0. @param dampingRatio  Finite and at least 0. @param springOut     Receives the spring. @return mtwn_success; mtwn_errorInvalid for a null pointer or a parameter outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpringFromDuration(double duration, double bounce, mtwnSpring* springOut);
```
Makes a spring from Apple's perceptual duration and bounce, as SwiftUI, Motion and anime.js map them: natural frequency 2 pi / duration, damping ratio 1 - bounce for a bounce from 0, 1 / (1 + bounce) below it.  @param duration   Seconds, finite and above 0. @param bounce     Above -1 and at most 1: 0 is critical, 1 never settles, below 0 is overdamped. @param springOut  Receives the spring. @return mtwn_success; mtwn_errorInvalid for a null pointer or a parameter outside the above. @par Thread safety Safe from any thread.

```c
mtwnSpringMotion mtwnSpringMotionAt(const mtwnSpring* spring, double seconds);
```
Computes how a spring moves any start over a time, for applying to every channel of a value.  @param spring   A spring made by this library. @param seconds  The time since the start, at least 0; infinity gives the rest, all four zero when the spring is damped. @return The motion. @par Thread safety Safe from any thread.

```c
void mtwnSampleSpring(const mtwnSpring* spring, double offset, double velocity, double seconds, double* offsetOut, double* velocityOut);
```
Samples a spring from a start.  @param spring       A spring made by this library. @param offset       The start's offset from the target. @param velocity     The start's velocity, per second. @param seconds      The time since the start, at least 0. @param offsetOut    Receives the offset then; may be NULL. @param velocityOut  Receives the velocity then; may be NULL. @par Thread safety Safe from any thread.

```c
double mtwnSpringSettlingTime(const mtwnSpring* spring, double offset, double velocity, double offsetTolerance, double velocityTolerance);
```
Returns the time after which a spring from a start stays within both tolerances of its target and of rest, by bounds on its offset and velocity that never end it early; the host then snaps it to the target.  @param spring             A spring made by this library. @param offset             The start's offset from the target. @param velocity           The start's velocity, per second. @param offsetTolerance    How far from the target is at rest, above 0. @param velocityTolerance  How fast is at rest, per second, above 0. @return Seconds from the start: 0 when it starts within both; infinity for an undamped spring that does not, or a tolerance not above 0. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeSpringEasing(const mtwnSpring* spring, double duration, double velocity, mtwnEaseMode mode, mtwnEasing* easingOut);
```
Makes an easing of a spring over a duration: the spring from an offset of -1 with a start velocity, sampled at progress times the duration and divided by its value at the duration, so it is exactly 0 at 0 and 1 at 1; a spring settled by then is changed by the distance it had left. For CSS linear() (mtwnFitLinearEasing) and for tracks timed by duration.  @param spring     A spring made by this library. @param duration   Seconds, finite and above 0. @param velocity   The start velocity in units of the whole change a second, finite: 0 from rest. @param mode       The mode. @param easingOut  Receives the easing. @return mtwn_success; mtwn_errorInvalid for a null pointer, an unknown mode, a duration or velocity outside the above, or a spring back at its start at the duration. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeDecay(double rate, mtwnDecay* decayOut);
```
Makes a decay whose velocity falls as e^(-rate t).  @param rate      The rate a second, finite and above 0. @param decayOut  Receives the decay. @return mtwn_success; mtwn_errorInvalid for a null pointer or a rate outside the above. @par Thread safety Safe from any thread.

```c
MTWN_NODISCARD MTWN_API mtwnResult mtwnMakeDecayFromDeceleration(double perMillisecond, mtwnDecay* decayOut);
```
Makes a decay from iOS's deceleration rate, the share of velocity kept each millisecond (0.998 normal, 0.99 fast): the rate a second is -1000 ln of it.  @param perMillisecond  Above 0 and below 1. @param decayOut        Receives the decay. @return mtwn_success; mtwn_errorInvalid for a null pointer or a share outside the above. @par Thread safety Safe from any thread.

```c
void mtwnSampleDecay(const mtwnDecay* decay, double position, double velocity, double seconds, double* positionOut, double* velocityOut);
```
Samples a decay from a start: the position is position + velocity (1 - e^(-k t)) / k, the velocity velocity e^(-k t).  @param decay        A decay made by this library. @param position     The start's position. @param velocity     The start's velocity, per second. @param seconds      The time since the start, at least 0; infinity gives the rest. @param positionOut  Receives the position then; may be NULL. @param velocityOut  Receives the velocity then; may be NULL. @par Thread safety Safe from any thread.

```c
double mtwnDecayStopTime(const mtwnDecay* decay, double velocity, double stopSpeed);
```
Returns the time a decay's speed falls to a stop speed.  @param decay      A decay made by this library. @param velocity   The start's velocity, per second. @param stopSpeed  The speed counted as stopped, per second. @return Seconds from the start: 0 when the start is no faster; infinity for a stop speed not above 0. @par Thread safety Safe from any thread.

```c
double mtwnDecayCrossingTime(const mtwnDecay* decay, double position, double velocity, double target);
```
Returns the time a decay reaches a position, where a host hands it over to a spring back from a limit.  @param decay     A decay made by this library. @param position  The start's position. @param velocity  The start's velocity, per second. @param target    The position to reach. @return Seconds from the start: 0 when it starts there; infinity when it never gets there, moving away or coming to rest short of it or exactly on it. @par Thread safety Safe from any thread.

```c
double mtwnRubberBand(double distance, double dimension, double coefficient);
```
iOS's rubber band: how far content moves when dragged a distance past a limit, in a dimension (the view's size along the drag): c x d / (c x + d), with slope c at the limit, never reaching d. Signed: a negative distance moves the other way.  @param distance     The drag past the limit; may be infinite. @param dimension    The view's size along it. @param coefficient  c, 0.55 on iOS. @return The distance moved; 0 for a dimension or coefficient not above 0 or NaN. @par Thread safety Safe from any thread.

```c
double mtwnRubberBandInverse(double offset, double dimension, double coefficient);
```
The inverse of mtwnRubberBand: the drag that moves content an offset past a limit, d o / (c (d - o)).  @param offset       The distance moved, signed. @param dimension    The view's size along it. @param coefficient  c, 0.55 on iOS. @return The drag; infinite, signed, for an offset of at least the dimension; 0 for a dimension or coefficient not above 0 or NaN. @par Thread safety Safe from any thread.

---

38 functions across 5 headers.
