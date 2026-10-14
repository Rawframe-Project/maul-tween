# mtwn-0020. Every part is checked against an independent reference

Status: Accepted

## Context

Agreement with a reference is evidence only when the reference is
computed another way than the code under test.

## Decision

- Long double references for the math functions, CSS easing, Penner
  curves, springs (by numerical integration), slerp and arc length.
- Oracles in `tools/oracle/` that run, never copy, other
  implementations: GSAP's and tween.js's easings, Motion's spring and
  CSS Color Level 4's sample conversions under Node, and an independent
  Python implementation of Web Animations' timing steps.
- A brute-force crossing counter for events.
- A determinism hash of a fixed scenario, the same constant on GCC,
  Clang, clang-cl and WebAssembly.
- Mutation testing on every module, and a randomized invariant test of
  the builder and the timing model under the sanitizers.

## Consequences

Each behavior is tested against something that could disagree with it.
