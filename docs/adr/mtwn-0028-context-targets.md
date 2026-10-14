# mtwn-0028. The context in one block; targets of nine kinds, stored as given

Status: Accepted

## Context

Records mtwn-0003, mtwn-0004 and mtwn-0006 put targets, clips and
players in a context with every capacity named up front. This record
fixes the context's def and memory, and what a target holds.

## Decision

- `mtwnContextDef` opens with a cookie and the ABI version, stamped by
  the inline `mtwnDefaultContextDef`, then the allocator (zeroed means
  the C library's) and the capacities of targets, players, clips,
  events and clip storage. Counts are at most 2^24, storage at most
  2^31 bytes, sizes computed with checked arithmetic. A def of another
  major or minor version is refused before the rest is read.
- The context and its pools are one block, allocated at creation and
  freed in one call; nothing allocates after. The changed-target
  report is the caller's array in the update, bounded by the targets.
- A target is 64 bytes: a kind, base value, current value and
  velocity, each four floats. Kinds are scalar, vec2, vec3, vec4,
  color, angle, rotation, integer and discrete; the kind fixes the
  channels used, and the rest are stored as 0. Values must be finite,
  a rotation not all zero, and are stored as given, checked by their
  bits: creating and setting do no floating-point arithmetic, so they
  give the same result in any floating-point environment.
- A target no animation writes shows its base at once; setting the
  base marks it for the next update's report.
- Ids are a 1-based index and a generation from 1; a slot whose
  generation would wrap is retired. Free slots are reused last in,
  first out, so ids are deterministic. A stale id is refused and
  counted as misuse, except by `mtwnTarget_IsValid`.

## Consequences

The host's allocator sees the whole cost of a context once, which
`mtwnContext_GetMemoryBytes` reports; a target id cannot reach a slot
reused by another target.
