# mtwn-0041. Growing a context, when the host asks

Status: Accepted

## Context

Record mtwn-0028 gives a context fixed capacities, allocated at once in
one block; a full pool refuses. The family's allocator rule lets an
object allocate again only when its caller explicitly grows it, and a
library growing by itself mid-frame is a known source of hiccups.

## Decision

- `mtwnGrowContext(&context, def)` grows a context to a def's
  capacities, each at least its own, with its own allocator. It
  allocates one new block, moves every pool with its free list (ids
  stay valid; freed slots are reused first as before), the bindings,
  the queued records in order and the changed list, frees the old
  block and writes the new address. Clips' storage does not move.
- On a refusal or an allocation failure nothing changes.

## Consequences

The host keeps ids, not pointers into the context, so growing updates
one pointer. Nothing in the library grows by itself.
