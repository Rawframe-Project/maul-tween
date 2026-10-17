# mtwn-0062. A context without records

Status: Accepted

## Context

A clip player's records cost most of a frame where clips repeat. With
1,000 players on clips of their own, each a timeline of four tracks
repeating every second, the repeat frame is 16.6 million instructions
(GCC 14, -O3), a steady frame about 6. Each player makes three
records there (a track's start and end, the timeline's iteration),
and each record takes four counts: the sweep's two ends, then a
search's guess and the double before it (record mtwn-0042's exact
times). Four is the floor for an exact crossing time: the count rounds
through the levels, so a guess is right only once the double before
it does not count. Keeping each node's prepared levels from the clip's
finish would save about an eighth of the frame and double a clip's
memory (about 1 kB a node); it was not taken.

anime.js and GSAP do none of this: they take a timeline's iteration
by division and call back once in a frame where it changed, with no
crossing time and no count; a callback not set costs nothing. Maul
Tween made every record whether a host read them or not: a context of
`eventCapacity` 0 made each and dropped it, counted.

## Decision

A context whose def has `eventCapacity` 0 makes no records and drops
none:

- a clip player's sweep is skipped; the player's swept time still
  follows it, so a context grown to hold records records what is
  crossed from then on, not the time before;
- a player's own records (a seek, a cancel) are not made;
- `mtwnContext_GetDroppedEvents` and a read's drops stay 0.

The sample does not read the sweep's times (it works out its own), so
values are the same bits with records or without.

## Consequences

The bench's layouts ending in `-quiet` count a context of no records:
the repeat frame 3.86 million, the average with a repeat every other
frame 3.88 million and at 1 s 3.85 million, against 16.6, 11.2 and 6.1
with records. `test_grow` plays one clip in two contexts, one with no
records, and checks the same values, nothing made or dropped through a
seek, and after growing to hold records the same records as the other
context's for the next frame. The records' own cost stays at the floor
above.
