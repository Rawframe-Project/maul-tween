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

## Amended 2026-10-16: a subtree passed over only away from a hold's edge

A subtree whose parent's time was the same at both ends of a sweep was
passed over even when the parent's time was held at one end of the
sweep and moving at the other within it, or on its active start or
end, or on an iteration's start (the end of the iteration before, in
other time): a point at the held time is then crossed. A track at the
start of a timeline delayed by 0.25 s did not start in the frame
landing on 0.25 s; played backwards, a timeline held after its end did
not end, backwards, in the frame landing on its end. Counts then
depended on where the frames fell, against record mtwn-0037.

Now a subtree is passed over only when no timeline above it, at either
end of the sweep, lies within the margin Missed leaves (record
mtwn-0042) of its active start or end or, in its active interval, of
an iteration's start. A test plays 304 random clips in frames of 1/64
s, which land on their edges, and in irregular ones, and checks the
same crossings by node, kind and way; it found every case here, and
20,000 such clips agree. Clip frames cost about 2% more instructions:
the bench's players on clips of their own 5.77 to 5.91 million a
frame.

## Amended 2026-10-16: starts and ends take the node's own iteration

A record's iterations were looked up by walking the timelines above the
node at the crossing's time. Where a timeline above repeats exactly at
the node's active start or end, that time lies on both sides of the
repeat, and the walk could land on the far side: a track of 0.2 s, 2
iterations, ending where its 0.4 s timeline repeats reported its first
iteration at its end, and a track's start, crossed backwards there, its
second. A record of several crossings could mix the two.

Now a start or end record takes the node's iteration at its active start
or end from its own timing, filled both ways: its first at the start,
its last at the end, at every crossing, as CSS Animations' start and end
belong to the first and last iteration. A node of no length is in its
after phase there, as the walk found it. Both are worked out when the
clip is finished, next to the active duration, so a node holds 16 bytes
more and the sweep does no timing for them; marks and iteration records
still walk. A test plays the determinism scenario's nesting with tracks
ending where their timeline repeats, delayed, and of no length, in short
frames and in frames that cross each point several times; 54 of its 178
starts and ends were in another iteration before. Without the walk the bench's 1,000 players on
clips of their own, repeating every other frame, take 11.0 million
instructions a frame on average, from 11.8, and their repeat frame at 1
s 16.5 million, from 18.1; the clips' code is 0.1 kB smaller.
