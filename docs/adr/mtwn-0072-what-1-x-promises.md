# mtwn-0072. What 1.x promises

Status: Accepted

## Context

1.0.0 is the first release whose interface is promised. The family's
rules (conventions section 15) give what every Maul library promises
within a major: the source interface only grows, every format an
earlier 1.y wrote is read, results are deterministic within a version
and a change to them is a changelog line, and the binary interface holds
within a minor, a def of another minor refused. Maul Tween has no file
format (it defines none), and a few promises of its own follow from its
records and are stated here so a 1.y keeps them.

## Decision

Within 1.x, beyond the family's promises:

- **Limits only rise.** `MTWN_MAX_COUNT`, `MTWN_MAX_CLIP_STORAGE`,
  `MTWN_MAX_FIT_POINTS`, `MTWN_RATE_LIMIT` and `MTWN_MAX_CLIP_DEPTH`
  may grow in a 1.y, never shrink: what was accepted stays accepted.
- **Enumerations only grow.** Every enumerator keeps its explicit value;
  a 1.y adds values, and a call refuses one it does not know.
- **Reserved bytes** (record mtwn-0071): an input's reserved bytes stay
  0, refused otherwise, until a 1.y gives one a meaning whose 0 is
  today's behavior; outputs write them 0; the reserved bytes of
  easings, springs, color mixes, paths and tracks stay padding.
- **Results** are the same bits on every platform the library builds for
  (x86 with SSE2 arithmetic, AArch64, WebAssembly; records mtwn-0008 and
  mtwn-0029), for the same calls in the same order. A 1.y may change
  them only to fix a defect or tighten a bound, each change a line under
  Changed or Fixed, the determinism scenario's hash with it.
- **Refusals stay refusals** unless a 1.y gives the refused input a
  meaning: a call that returned an error for an input does not start
  doing something else with it silently.

## Consequences

A host can build against 1.0's headers and move to any 1.y by
recompiling, keeping what its calls mean; a host that ships the shared
library pairs it with programs of its minor only (`SOVERSION`
major.minor, the package's check SameMajorVersion). What 1.x cannot
change (a function's name or meaning, a field's place, a limit lowered)
waits for 2.0.
