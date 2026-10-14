# mtwn-0038. Events: counted records of every crossing, in the context's queue

Status: Accepted

## Context

Records mtwn-0004 and mtwn-0013 make events records in a bounded queue,
one per player, node or mark and kind per update, counted; record
mtwn-0037 counts crossings exactly. The host needs a player's and a
transition's start and end, a node's start, iterations and end as CSS
Animations names them, marks, seeks and cancels.

## Decision

- `mtwnEvent` carries the kind (start, end, iteration, mark, seek,
  cancel), the player, the node (1 for the clip's root, 0 for a
  transition) and a mark's key, the count of crossings, whether they
  went backwards, the host's and the player's time of the first
  crossing, and the node's iteration (a mark's parent's) at the first
  and last. `mtwnReadEvents` takes the oldest. The queue holds the
  context def's `eventCapacity` records; one arriving at a full queue
  is dropped and counted, never silently: `mtwnReadEvents` reports the
  drops since the last read and `mtwnContext_GetDroppedEvents` all of
  them. Records arrive in the order made (players in creation order,
  nodes in clip order), so a full queue keeps the first an update made;
  those kept are sorted.
- Each update sweeps a clip's player from the time its last sweep
  reached to its current time and counts, along the timelines above
  each node, its active start and end, its iteration starts, and the
  marks. A start crossed forwards is a start, backwards an end; an end
  crossed forwards is an end, backwards a start (backwards), as CSS
  Animations' phase table gives. A subtree whose timelines all stayed
  in one iteration without moving is passed over.
- A player from no time (just played, rewound) takes its starting
  point; a sweep that turns back takes its start again, as the point
  is passed once more.
- Seeks jump, leaving a seek record; `mtwnPlayer_Sweep` and finishing
  cross what lies between, their records at once. A cancel of a player
  with a time, and a transition replaced, give cancel records. A
  transition's start comes when its delay ends, its end when it
  commits.
- Records an update makes are sorted by host time, then the players'
  creation order, then the place along the sweep, then the order made
  (clip order; start, iteration, end).

## Consequences

A stall over an endless fast repeat is one iteration record with its
count; records never call host code, so the update never meets a host
destroying what it walks.
