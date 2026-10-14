# mtwn-0017. Punch and shake from a seed

Status: Accepted

## Context

Game hosts rely on punch and shake effects; libraries that draw shake
from a global random generator cannot repeat one.

## Decision

Punch is `A sin(2 pi f t) (1 - p)^e`. Shake is one-dimensional gradient
noise per channel at frequency f and amplitude A under the same
envelope, its lattice gradients drawn from SplitMix64 of the seed, the
channel and the lattice index. Both are exactly 0 at their ends and
composite add by default.

## Consequences

A shake repeats exactly from its seed on every platform.
