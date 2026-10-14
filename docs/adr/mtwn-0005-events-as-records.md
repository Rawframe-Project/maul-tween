# mtwn-0005. Events are counted records in a bounded queue

Status: Accepted

## Context

A callback during an update can start, stop or destroy what the update
is iterating, and the family's libraries deliver completions as records,
never through application code (family record 0018). Browsers dispatch
one iteration event per frame however many iterations a frame crosses;
other libraries compare two render times and lose what lies between.

## Decision

- Each update appends records to the context's event queue: start, end,
  iteration, mark crossed and their backward counterparts, following CSS
  Animations Level 2's phase-change table.
- A mark is crossed when its time, mapped through its ancestors, lies in
  the interval the update swept: `(previous, current]` going forward,
  `[current, previous)` going backward. Crossings through repeating
  ancestors are counted exactly.
- An update makes at most one record per player, node or mark and kind,
  carrying the crossing count, the first and last iteration and the time
  of the first crossing; records are ordered by that time, then
  composite order, then clip order.
- A seek jumps without crossings and leaves a seek record, unless asked
  to sweep.
- The queue's capacity is a named limit; a full queue drops the newest
  record and counts the drop.
- Custom easing functions remain synchronous hooks that may not call
  into the context.

## Consequences

Updates run no host code, events are exact and independent of frame
rate, and a long stall over a fast infinite repeat is one record with
its count.
