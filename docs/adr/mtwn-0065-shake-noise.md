# mtwn-0065. A shake's noise on its own clock

Status: Accepted

## Context

Camera shake in games follows Eiserloh's "trauma" (GDC 2016, "Math for
Game Programmers: Juicing Your Cameras With Math"): an amount from 0 to
1 raised at each hit and decaying linearly, the shake its square, as
smooth noise per channel; Bevy's screen shake example is one rendering
of it. A shake (record mtwn-0026) is seeded 1D gradient noise times an
envelope (1 - t / duration)^exponent, one clock for both: exponent 2 is
a single hit's trauma, but a second hit means a new shake, whose noise
starts again at 0, a visible jump.

## Decision

`mtwnShakeNoise` gives a shake's noise alone at any time from 0 while
its frequency times the time is below 2^53 (0 elsewhere): a factor from
-1 to 1 per channel, 0 on the lattice. The host keeps one clock through
every hit and multiplies by its trauma squared, or any envelope it
likes. `mtwnSampleShake` is that noise times its envelope, bit for bit.

Rotation shake is not added: the noise's channels are the angles (a 2D
roll, or 3D yaw, pitch and roll), and turning a camera by them is the
host's composition, as in Bevy's example and DOTween's DOShakeRotation.

## Consequences

`test_signal` checks the shake against its noise times its envelope
bit for bit, the noise running on past the duration, its zeros on the
lattice and outside its range, and its spread. The guide's shake shows
the trauma recipe, run by `test_guide`.
