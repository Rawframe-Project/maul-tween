# mtwn-0018. The API shape

Status: Accepted

## Context

The family's rules fix how defs, ids, results and memory look (family
records 0009, 0010, 0014 and 0016); the curves must stay apart from the
context (record mtwn-0003).

## Decision

- Headers: `base.h`; the curves `easing.h`, `spring.h`, `interpolate.h`,
  `timing.h` and `sample.h`, which include only `base.h`; the context
  `context.h`, `target.h`, `clip.h`, `player.h` and `transition.h`.
- Curve values are validated when made and sampled without a status.
- The context def opens with a cookie and the ABI version stamped by a
  `static inline` default and is refused for another minor version
  before anything else is read.
- Targets, clips and players are generation-checked ids; nodes are
  1-based indexes in their immutable clip; a clip is finished before it
  plays and cannot be destroyed while a player uses it.
- Results: success, invalid, capacity, limit, version, stale, busy and
  state; invalid input against a live context counts one misuse on it.

## Consequences

The hot calls carry no status, every refusal is typed, and a host that
includes only the curve headers needs nothing else.
