# Springs, decay and signals

`spring.h` holds the physically based motions: damped springs, decay for flings, inertia that lands or bounces, and iOS's rubber band. `interpolate.h` holds punch and shake, offsets that end where they began. All of them are closed forms of time: a sample takes a start and a time since it, never a frame's step, so the same times give the same values at any frame rate.

## Springs

A spring is made from stiffness, damping and mass (`mtwnMakeSpring`), from a natural frequency in hertz and a damping ratio (`mtwnMakeSpringFromFrequency`: 1 is critical, below it overshoots, 0 never settles), or from Apple's perceptual duration and bounce (`mtwnMakeSpringFromDuration`, as SwiftUI, Motion and anime.js map them: 0 is critical, 1 never settles, below 0 is overdamped). It is sampled from a start's offset from its target and its velocity:

```c
mtwnSpring spring;
mtwnResult made = mtwnMakeSpringFromDuration(0.4, 0.2, &spring); // 0.4 s, a little bounce
double offset = 0.0;
double speed = 0.0;
// 0.1 s after starting 1 short of the target, at rest.
mtwnResult sampled = mtwnSampleSpring(&spring, -1.0, 0.0, 0.1, &offset, &speed);
double settled = 0.0;
mtwnResult timed = mtwnSpringSettlingTime(&spring, -1.0, 0.0, 1e-3, 1e-2, &settled);
```

`mtwnSpringSettlingTime` gives the time after which the motion stays within both tolerances of its target and of rest, by bounds that never end it early; the host then snaps it to the target. An undamped spring that does not start at rest never settles and gets infinity.

Every call that takes a spring, a decay or a bounded decay returns a status and gives its results through pointers, left as they were on failure. It refuses a struct no maker produced (a spring's precomputed fields are derived again from its frequency and ratio, bit for bit), a start that is not finite, a time below 0 or NaN, and a tolerance or stop speed not above 0.

To retarget a moving spring, sample it at the time of the change and start again from there: the velocity carries over and the offset is measured from the new target. `mtwnSpringMotionAt` gives the motion over one time as four coefficients, so a value of several channels costs one evaluation and four multiplications a channel.

`mtwnMakeSpringEasing` turns a spring into an easing over a duration that lands exactly on 1, for CSS `linear()` (through `mtwnFitLinearEasing`) or for a track timed by its keyframes.

## Decay and inertia

A decay coasts from a velocity that falls exponentially, as a fling does: `mtwnMakeDecay` takes the rate a second, `mtwnMakeDecayFromDeceleration` iOS's share of velocity kept each millisecond (0.998 normal, 0.99 fast). `mtwnDecayStopTime` says when its speed falls to a stop speed, and `mtwnDecayCrossingTime` when it passes a position, where a host hands it to a spring.

A fling that snaps lands where the host chooses. The natural rest is the decay's sample at infinite time; `mtwnMakeDecayToRest` makes the decay that comes to rest on a chosen end, such as the nearest page, with the same start velocity. A bounded decay coasts between two bounds; where it would come to rest past one, a spring takes it from the exact time it reaches the bound, with the velocity it has there, as a scroll view bounces:

```c
mtwnDecay coast;
mtwnResult made = mtwnMakeDecayFromDeceleration(0.998, &coast);
double rest = 0.0;
// Where it would stop: its sample at infinite time.
mtwnResult sampled = mtwnSampleDecay(&coast, 40.0, 1500.0, HUGE_VAL, &rest, NULL);
double page = 320.0 * round(rest / 320.0); // the nearest page
mtwnDecay snap;
made = mtwnMakeDecayToRest(40.0, 1500.0, page, &snap);

mtwnSpring bounce;
made = mtwnMakeSpring(500.0, 10.0, 1.0, &bounce);
mtwnBoundedDecay scroll;
made = mtwnMakeBoundedDecay(&coast, &bounce, 0.0, 600.0, &scroll); // content 0 to 600
double at = 0.0;
mtwnResult bounced = mtwnSampleBoundedDecay(&scroll, 40.0, 1500.0, 0.5, &at, NULL);
double still = 0.0;
mtwnResult stopped = mtwnBoundedDecayStopTime(&scroll, 40.0, 1500.0, 0.5, 1.0, &still);
```

A start already past a bound is the spring's from the start. Either bound may be infinite, for none.

`mtwnRubberBand` gives iOS's resistance when content is dragged past its edge, `c x d / (c x + d)` with iOS's coefficient `c` of 0.55, and `mtwnRubberBandInverse` the drag that moves content a given distance past it.

## Punch and shake

A punch (`mtwnMakePunch`) is a sine fading out over a duration; a shake (`mtwnMakeShake`) is smooth gradient noise, one per channel, fading over the same envelope, the same for a seed on every platform. Each sample is a factor from -1 to 1 the host scales by each channel's amplitude and adds to the value, exactly 0 at the start, at the end and outside them: a hit or a rumble that leaves nothing behind.

A camera shake driven by trauma, as Eiserloh's talk on game cameras has it, takes the noise alone (`mtwnShakeNoise`) on one clock that runs on through every hit, and multiplies it by the trauma squared, which the host raises at each hit and lets decay. The noise never jumps where the amplitude changes:

```c
// A hit raises the trauma (Eiserloh's camera shake), up to 1.
trauma = fmin(trauma + 0.4, 1.0);
```

```c
// Each frame: trauma decays 0.5 a second; the shake is its square, on
// one clock that runs through every hit, so the noise never jumps.
trauma = fmax(trauma - 0.5 * dt, 0.0);
double shake = trauma * trauma;
double roll = 10.0 * shake * mtwnShakeNoise(&noise, 0, now); // degrees
double dx = 20.0 * shake * mtwnShakeNoise(&noise, 1, now);   // pixels
double dy = 20.0 * shake * mtwnShakeNoise(&noise, 2, now);
```

## Not obvious

- A spring's velocity and a decay's are per second, in the value's own units; a spring easing's start velocity is in units of the whole change a second.
- `mtwnMakeSpringEasing` refuses a spring that is back at its start at the duration, since it could not be scaled to land on 1. A spring settled before the duration is stretched by the distance it had left.
- A shake's noise is 0 on its lattice points, `frequency` of them a second, and outside its clock's range (the frequency times the time below 2^53).
- The library adds no rotation shake: the noise's channels are the angles (a roll, or yaw, pitch and roll), and turning a camera by them is the host's.
