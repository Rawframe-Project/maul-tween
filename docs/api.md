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
Tells whether an easing never decreases, as a timeline's easing must: linear, power, sine, expo, circ and steps in every mode, a cubic bezier with both y from 0 to 1, linear() with outputs that never decrease; not back, elastic or bounce, and not a host function, which the library cannot inspect.  @param easing  An easing made by this library. @return Whether it never decreases. @par Thread safety Safe from any thread.

```c
double mtwnEase(const mtwnEasing* easing, double progress, bool before);
```
Samples an easing. CSS's curves follow CSS Easing Level 2 outside 0 to 1; Penner's curves and host functions see progress clamped to 0 to 1. The modes reflect the curve, and the before flag reaches it where it samples progress unreflected.  @param easing    An easing made by this library. @param progress  The progress, finite. @param before    Whether the timing is in its before phase; it moves only curves that jump at their start. @return The eased progress: exactly 0 at 0 and 1 at 1 for Penner's curves, linear and cubic bezier; what CSS defines for steps and linear(). @par Thread safety Safe from any thread.

---

15 functions across 2 headers.
