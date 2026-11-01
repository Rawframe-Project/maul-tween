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
mtwnResult timed = mtwnComputeSpringSettlingTime(&spring, -1.0, 0.0, 1e-3, 1e-2, &settled);
```

`mtwnComputeSpringSettlingTime` gives the time after which `mtwnSampleSpring`'s offset and velocity stay within both tolerances of the target and of rest, by bounds that never end it early, with room for the sample's rounding; the host then snaps it to the target. For tolerances near the subnormals, where that rounding is a large share of a tolerance, the time can be noticeably later than the motion's. An undamped spring that does not start at rest never settles and gets infinity.

Every call that takes a spring, a decay or a bounded decay returns a status and gives its results through pointers, left as they were on failure. It refuses a struct no maker produced (a spring's precomputed fields are derived again from its frequency and ratio, bit for bit), a start that is not finite, a time below 0 or NaN, and a tolerance or stop speed not above 0.

To retarget a moving spring, sample it at the time of the change and start again from there: the velocity carries over and the offset is measured from the new target. `mtwnComputeSpringMotion` gives the motion over one time as four coefficients, so a value of several channels costs one evaluation and four multiplications a channel.

`mtwnMakeSpringEasing` turns a spring into an easing over a duration that lands exactly on 1, for CSS `linear()` (through `mtwnFitLinearEasing`) or for a track timed by its keyframes.

## Decay and inertia

A decay coasts from a velocity that falls exponentially, as a fling does: `mtwnMakeDecay` takes the rate a second, `mtwnMakeDecayFromDeceleration` iOS's share of velocity kept each millisecond (0.998 normal, 0.99 fast). `mtwnComputeDecayStopTime` says when its speed falls to a stop speed, and `mtwnComputeDecayCrossingTime` when it passes a position, where a host hands it to a spring.

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
mtwnResult stopped = mtwnComputeBoundedDecayStopTime(&scroll, 40.0, 1500.0, 0.5, 1.0, &still);
```

A start already past a bound is the spring's from the start. Either bound may be infinite, for none.

`mtwnApplyRubberBand` gives iOS's resistance when content is dragged past its edge, `c x d / (c x + d)` with iOS's coefficient `c` of 0.55, and `mtwnInvertRubberBand` the drag that moves content a given distance past it. Both give their result through a pointer and return a status: they refuse a dimension or a coefficient that is not above 0, and a NaN drag or offset, where they used to give 0.

## Punch and shake

A punch (`mtwnMakePunch`) is a sine fading out over a duration; a shake (`mtwnMakeShake`) is smooth gradient noise, one per channel, fading over the same envelope, the same for a seed on every platform. Each sample is a factor from -1 to 1 the host scales by each channel's amplitude and adds to the value, exactly 0 at the start, at the end and outside them: a hit or a rumble that leaves nothing behind. A sample gives its factor through a pointer and returns a status: it refuses a NaN time, and a punch or a shake whose fields are not ones its maker takes.

A camera shake driven by trauma, as Eiserloh's talk on game cameras has it, takes the noise alone (`mtwnSampleShakeNoise`) on one clock that runs on through every hit, and multiplies it by the trauma squared, which the host raises at each hit and lets decay. The noise never jumps where the amplitude changes:

```c
// A hit raises the trauma (Eiserloh's camera shake), up to 1.
trauma = fmin(trauma + 0.4, 1.0);
```

```c
// Each frame: trauma decays 0.5 a second; the shake is its square, on
// one clock that runs through every hit, so the noise never jumps.
trauma = fmax(trauma - 0.5 * dt, 0.0);
double shake = trauma * trauma;
double n[3];
double roll = 0.0; // degrees
double dx = 0.0;   // pixels
double dy = 0.0;
if (mtwnSampleShakeNoise(&noise, 0, now, &n[0]) == mtwn_success &&
    mtwnSampleShakeNoise(&noise, 1, now, &n[1]) == mtwn_success &&
    mtwnSampleShakeNoise(&noise, 2, now, &n[2]) == mtwn_success)
{
    roll = 10.0 * shake * n[0];
    dx = 20.0 * shake * n[1];
    dy = 20.0 * shake * n[2];
}
```

## Not obvious

- A spring's velocity and a decay's are per second, in the value's own units; a spring easing's start velocity is in units of the whole change a second.
- `mtwnMakeSpringEasing` refuses a spring that is back at its start at the duration, since it could not be scaled to land on 1. A spring settled before the duration is stretched by the distance it had left.
- A shake's noise is 0 on its lattice points, `frequency` of them a second, and outside its clock's range (the frequency times the time below 2^53).
- The library adds no rotation shake: the noise's channels are the angles (a roll, or yaw, pitch and roll), and turning a camera by them is the host's.
