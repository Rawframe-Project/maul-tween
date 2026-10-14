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

Easing: curves that map a timing's progress to eased progress (record mtwn-0011). An easing is a value made by a function that validates it and sampled by mtwnEase with no status to check. Penner's families land exactly on 0 and 1; every kind has the same four modes.

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
MTWN_NODISCARD MTWN_API mtwnResult mtwnSetEaseMode(mtwnEasing* easing, mtwnEaseMode mode);
```
Changes an easing's mode, keeping its curve.  @param easing  An easing made by this library. @param mode    The mode. @return mtwn_success; mtwn_errorInvalid for a null pointer or an unknown mode, leaving the easing as it was. @par Thread safety Safe from any thread.

```c
bool mtwnIsEasingMonotonic(const mtwnEasing* easing);
```
Tells whether an easing never decreases, as a timeline's easing must: linear, power, sine, expo and circ in every mode; not back, elastic or bounce, and not a host function, which the library cannot inspect.  @param easing  An easing made by this library. @return Whether it never decreases. @par Thread safety Safe from any thread.

```c
double mtwnEase(const mtwnEasing* easing, double progress, bool before);
```
Samples an easing. Penner's curves and host functions see progress clamped to 0 to 1.  @param easing    An easing made by this library. @param progress  The progress, finite. @param before    Whether the timing is in its before phase; it moves only curves that jump at their start. @return The eased progress, exactly 0 at 0 and 1 at 1 for every kind but a host function. @par Thread safety Safe from any thread.

---

11 functions across 2 headers.
