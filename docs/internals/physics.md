# Springs, decay and signals

Springs, decay, inertia, the rubber band, punch and shake are closed forms of time from a start. None is stepped: a frame-by-frame integrator gives a different path at 30 and at 144 frames a second, and drifts. Sampled at the time since its start, a motion gives the same value at any frame rate, and a retarget starts a new motion from the position and velocity at that time.

## One spring formula

With decay rate `a = zeta w0` and damped frequency `w = w0 sqrt(|1 - zeta^2|)`, a start `(x0, v0)` moves as

```text
x = e^(-a t) (x0 C + (v0 + a x0) S)
v = e^(-a t) (v0 C - (a v0 + w0^2 x0) S)
```

where `C` and `S` are `cos(w t)` and `sin(w t) / w` below critical damping, and `cosh(w t)` and `sinh(w t) / w` from it ([`src/spring.c`](../../src/spring.c)). At critical damping `w` is 0 and `S` is `t`. The regimes must meet without a jump as the damping ratio crosses 1, which naive forms break by dividing by a vanishing `w`:

- `sin(w t) / w` keeps its digits however small `w` is, as the sine does.
- `sinh(w t) / w` is `t` times a series below `w t = 1/2`.
- Above that, an overdamped spring is written in its two rates, the slow one as `w0^2 / (a + w)`, so nothing cancels or overflows however heavy the damping.

The motion over one time is a 2 by 2 matrix taking a start's offset and velocity to those at the time (`mtwnComputeSpringMotion`). A value of four channels computes it once and costs four multiplications a channel. A spring stores the natural frequency, the damping ratio and what its samples need, whichever way it was made: stiffness, damping and mass; hertz and a ratio; or Apple's duration and bounce, mapped as SwiftUI, Motion and anime.js map them (frequency `2 pi / duration`, ratio `1 - bounce`, or `1 / (1 + bounce)` below 0).

## Settling

`mtwnComputeSpringSettlingTime` returns when bounds on the offset and the velocity both stay below their tolerances for good. Where a bound is a pure exponential, the time is in closed form. Otherwise the bound's logarithm is concave, so Newton's steps from past its peak approach the crossing from the right. Where the bound is nearly flat at its tolerance, rounding can still send a step before the peak, even before 0; such a step goes halfway to the peak instead, so every step stays at or after it. The host snaps the spring to the target then, which is what a transition does.

The time is for `mtwnSampleSpring`'s values, which are rounded, not only for the motion: where a bound is tight (critical damping with both terms of one sign, an overdamped velocity from rest), the motion meets its tolerance at the bound's time and the sample lands a few ulps either side. So each bound is solved for its tolerance less a margin `m`, and the time it gives is certified: the bound's excess at it, plus a bound on that excess's own rounding, is at most `-m`, or the time steps on until it is. The margin covers the sample's error against the bound, relatively `u (60 (1 + L)^2 + 30 (1 + L) r t)` with `u = 2^-53`, `r` the bound's rate and `L` a factor near critical damping from above (at most about 750, 0 otherwise), plus what the sample's results below the normal doubles can add, half the least subnormal at each step, times what multiplies it after. As the error grows with `t` far slower than the bound falls past its peak, a margin taken at the time holds after it too. The derivation is in [`src/spring.c`](../../src/spring.c), by regime. For tolerances of everyday size the time moves later by a few 1e-14 relatively, at most about 3e-11 where the bound is flat at its tolerance. Where the underflowed steps could reach a quarter of a tolerance, as with tolerances among the subnormals, the time is the later of the bounds' and the time every exponential in the sample is 0, `747 / r`, from which the sample is exactly 0. A start at rest keeps the bounds' time, as its sample is always exactly 0. Where `747 / r` is infinite (undamped, or a rate below about 4e-306), no later time serves: a bound within at once keeps its time of 0, and any other gets infinity. The sample's steps below the normal doubles are multiplied by the start (a half-step in `e^(-a t)` times an `x0` of 1e300 is 2.5e-24), so they are not confined to subnormal tolerances.

A start far enough out overflows a bound: a coefficient such as `w0^2 x`, two products with opposite signs, or the bound on Newton's way for a slow spring. A bound and its tolerance divided by the same `2^e` cross at the same time, so such a start is divided by `2^e`, with `e` the exponent of its larger part, and the tolerances' logarithms move by `e ln 2`. Its coefficients are then at most `decay + w0^2` and the time is the true one. A start that is not finite still gets infinity.

A start far enough in underflows one instead: a product such as `w0^2 x` or a bound's `b t` rounds to a subnormal or to 0, losing that bound's digits or the whole term. A sum that stays at least the least normal double is still within an ulp, so only a bound whose `b`, or whose value `a + b t` at its time, falls below it is redone from the divided start, which brings both up. A spring whose `w0^2` is itself subnormal, with `w0` below `2^-511`, would keep its product with a start below 1 there, so its start is left `2^53` larger; its decay is below `2^513`, so nothing overflows. An overflow redoes both bounds, an underflow only the bound it reached, and every other time keeps its bits.

A rate below the normal doubles, damping so slight that the bound decays only past any time a double holds, overflows both `1 / rate` and `a / b`, whose difference is NaN. The peak is then 0 where `a rate >= b`, the bound falling from its start, and the cap otherwise.

## Decay and inertia

A decay at rate `k` moves `velocity (1 - e^(-k t)) / k` from its start ([`src/decay.c`](../../src/decay.c)). For a small `y = k t`, `1 - e^(-y)` loses its digits, so below `y = 1/2` the factor comes from its series; the crossing time `-ln(1 - f) / k` is likewise `2 atanh(f / (2 - f)) / k` by the atanh series for `f <= 1/2`. iOS's per-millisecond deceleration `r` is the rate `-1000 ln r`.

Each result keeps to the doubles on the way wherever it lies within them itself, by a second form for where the first leaves them; where the first does not, its bits are what they were:

- **The crossing's share** `f = d k / v` comes from the operands' mantissas and exponents, so `d k` neither overflows nor underflows. A distance `target - position` past the doubles is taken halved. Whether the target is ahead comes from the signs, as a share below the least subnormal rounds to 0. Where `s = f / (2 - f)` falls below the normal doubles, `2 s / k` is `2 (d / v) / (2 - f)`.
- **The stop time** `ln(speed / stop) / k` is `(ln speed - ln stop) / k` where the ratio overflows.
- **The travel** is `v (1 - e^(-y)) / k` where `v t` or `y` overflows, and from half of `v t` where `v t` overflows at a small `y`. A travel past the doubles from a start on the other side may end within them: the position then comes from half the start and half the travel.
- **The velocity** `v e^(-y)` is `v e^(-y/2) e^(-y/2)` where `e^(-y)` is subnormal, past `y = 708`.

Inertia ([`src/inertia.c`](../../src/inertia.c)) builds on the decay without changing it:

- **Landing.** The decay that comes to rest on a chosen end with the start's velocity has rate `velocity / (end - position)`. The natural rest is the decay at infinite time, iOS's projection, so the snapping rule (the nearest page, a grid, a list) stays the host's.
- **Bounds.** A bounded decay is the decay while it would come to rest within its bounds. Where it would rest past one, it is the decay until the exact time it reaches that bound (the crossing time, not the frame after it), then the spring from the bound with the velocity it has there; a start past a bound is the spring's from the start. Velocity is continuous at both hand-overs and the path is the same at any frame rate, as Flutter's BouncingScrollSimulation and Motion's inertia with bounds give it, with the crossing found exactly.

A transition's decay of a scale runs in its logarithm, its start velocity a share of the value a second, so a zoom coasts by ratios.

## Punch and shake

Punch is `sin(2 pi f t) (1 - t / duration)^e`. Shake is Perlin's improved gradient noise in one dimension ([`src/signal.c`](../../src/signal.c)): 0 at each lattice point, and between two it blends their gradients' lines by the quintic fade `6u^5 - 15u^4 + 10u^3`, whose first two derivatives vanish at the lattice, so it is smooth. Each gradient comes from SplitMix64 of the seed, the channel and the lattice index: no table, no state, the same bits on every platform for a seed, and any channel and time sampled in any order. Both are exactly 0 at the start, at the end and outside them, so a host adds them and nothing is left behind.

`mtwnSampleShakeNoise` is the noise alone, on a clock that runs on through every hit, for shakes whose amplitude the host drives (trauma squared); `mtwnSampleShake` is that noise times its envelope, bit for bit.

## Not built

- A wiggle as an easing: a wiggle is a displacement that ends where it began, as punch and shake are, not a progress.
- SlowMo: GSAP's alone among the libraries surveyed, and a host easing gives it exactly.
- Spring tightening under reduced motion, or a global duration scale: the host chooses its springs and can scale its own clock.
