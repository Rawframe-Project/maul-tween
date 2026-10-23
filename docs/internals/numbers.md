# Numbers

Results are the same bits on every platform the library builds for, for the same calls in the same order. Three things make that hold: the library computes in a floating-point environment of its own, it writes its own elementary functions, and it fixes where values are rounded to float.

## The floating-point environment

A host may run with flush-to-zero, denormals-are-zero, another rounding mode or traps enabled, as games and audio code often do. Every public call that computes ([`src/fpenv.c`](../../src/fpenv.c)) saves the thread's control register (MXCSR on x86, FPCR on AArch64), sets round to nearest with subnormals kept and exceptions masked, runs, and restores the host's register. The accesses are volatile and the work sits in out-of-line functions, so the compiler cannot move arithmetic across the switch. WebAssembly has one environment, the one the library needs. Any other architecture is refused at build time rather than computed in an unknown environment, and so is a compiler whose float arithmetic does not round to float (`FLT_EVAL_METHOD` other than 0, as x87 gives).

Calls that do no arithmetic skip the switch: creating a target and setting its base store the value as given, checked by its bits, so they give the same result in any environment.

## Own elementary functions

C libraries' `exp`, `log`, `sin`, `atan2` and `pow` differ in their last bits between glibc, musl, Apple's libm, Microsoft's CRT and Emscripten. The library writes its own ([`src/elementary.c`](../../src/elementary.c), and in binary32 [`src/elementary_float.c`](../../src/elementary_float.c)) from the published methods: an exact argument reduction, a polynomial on the reduced range, and reconstruction by `ldexp`. Each is within 1 ulp over the range the library uses, stated beside it and tested against the C library's long double functions where they are wider. Only the functions IEEE 754 rounds exactly (`sqrt`, `floor`, `ceil`, `round`, `fmod`, `frexp`, `ldexp`, `copysign`, `fabs`) are taken from `<math.h>`, and `tools/check_source.py` refuses any other.

The cost is speed: springs and the exponential easings run several times slower than with a C library's functions, and about 12 kB of every program's code is these functions. The same bits under every C library are worth it for replays, lockstep games and frame tests across platforms.

These files and their generator, `tools/gen_math_constants.py`, are shared by the Maul family and synced from the organization's canonical copy; they are changed there first.

## Float values, double computation

Values are binary32 at the interface and in targets, computed in binary64 and rounded once where they become a float. Host time is integer nanoseconds; a player's local time is the integer difference between the host time and its start, converted once and multiplied by its rate, so nothing is summed from frame deltas and a long run does not drift. A pause, a seek or a rate change rebases the start.

## Values held within the floats

A number past the floats' range is held at the nearest finite float, `0x1.fffffep127` either sign, and a number within it is unchanged. Each value is held where it becomes a float: a transition's eased or sampled value, a track's sample, the composition, the incremental repeat. A value's four channels are converted together and then clamped together, which equals holding the doubles first and compiles to a packed conversion, a minimum and a maximum. A velocity is taken from the values held, so a number held at a bound is still. Clamping at each conversion, rather than once where a target is written, keeps the sums before it in double where they do not overflow, and leaves no infinity to subtract from another (infinity minus infinity is NaN, which no later clamp undoes). Every target's value and velocity is therefore finite.

## Mixes

Every interpolation of two numbers goes through one helper, `(1 - t) a + t b`, and returns `a` when `a = b`. The weighted sum keeps both ends exact. Far past its ends its error is an ulp of `t a` beside a result of at least `t |b - a|`, so between distinct floats its relative error stays near 2^-29 at any `t`; only equal ends would lose their value past `t = 2^53`, which the equality test fixes. Plain `a + t (b - a)` loses the exact end at `t = 1`. P0811's `std::lerp` was measured and set aside: it costs about 3% more on an Oklab mix, for a monotonicity and boundedness the exact ends already give within [0, 1].

## Traps

- Never call a `<math.h>` function outside `tools/libm-allowed.txt`; use the library's own. A new use of one that rounds exactly needs a line there.
- A public function that computes must run inside the environment switch, or a host's flush-to-zero changes its bits. The `transition` and `clip` suites run under a host environment of flush-to-zero and rounding toward zero.
- The determinism suite's hash covers every part; a change that moves any bit changes it, and must say why in its pull request.
