# mtwn-0019. Performance and size targets

Status: Superseded by mtwn-0043

## Context

Measured on one machine (AMD Ryzen 9 5900X) updating 64,000 float tweens
with a quadratic ease out: tweeny 4.1.0 in 1.69 ms at 208 bytes each,
tween.js 25.0.0 in 6.91 ms, anime.js 4.5.0 in 9.88 ms, GSAP 3.15.0 in
10.42 ms, and a bare struct-of-arrays C loop in 0.245 ms.

## Decision

On that machine with GCC 14 at `-O2`:

- 64,000 single-track float transitions update in at most 0.6 ms (0.8 ms
  with the changed-target report) and are created in at most 4 ms, at
  most 96 bytes each with their target.
- A cubic-bezier or spring sample takes at most 30 ns, a Penner sample
  at most 10 ns.
- x86-64 code: the curves at most 24 kB, easing and springs alone at
  most 8 kB, the whole library at most 64 kB.

A target missed is fixed, or changed by a record that says why.

## Consequences

Performance is a measured release criterion, and CI reports the size of
every build.
