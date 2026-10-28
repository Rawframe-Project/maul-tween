# Events

Events are records in the context's bounded queue, never callbacks. A callback would run host code in the middle of an update, which may then call the context back, and its count and order would depend on frame rate. A record is counted exactly however far the time moved, read when the host likes, and makes no call.

## Crossings counted exactly

What a player crosses between two updates is found by counting, not by stepping through time ([`src/crossing.c`](../../src/crossing.c)). A clip that repeats a thousand times inside one frame, nested three deep, costs a few counts, not a thousand iterations walked.

Each timeline's child time runs along a path: its iterations in the order its rate takes them, each from its start to its end; repeat delays hold the path. A point below (a mark, a node's active start or end, an iteration start) is crossed once each time the path covers it. Where an iteration's end and the next one's start share a time (no repeat delay between, or going backwards) both are reached at that time.

The count of crossings a level has reached by a local time is the constants it keeps for its first, full forward, full reversed and last iterations, times their numbers, plus the iteration it reaches partly, whose moving end is counted at the level below. A count is one walk down the chain of timelines; the constants are kept bottom up once per point counted.

A sweep forwards counts `(previous, current]`, backwards `[current, previous)`, so a point at an update's exact time is crossed once, in the update that reaches it. A sweep from no time (just played, rewound) takes its own start. Each crossing's direction is the point's own, its child time rising or falling.

## Times of the first and last crossing

A record carries the host and clip time of its first crossing. That time is found by bisection on the count over the doubles in their order as 64-bit integers: exact to the double, at most 64 halvings, each one count. The search starts from a guess, the last crossing the sweep found, or a record's first crossing for its last: records in one frame mostly share a time, and a right guess costs two counts. The invariant is kept, so the result is still a double where the count flips; where it flips more than once, the guess picks the same one of those few doubles on every platform.

## The sweep

Each update sweeps a clip's player from the time its last sweep reached to its current time ([`src/sweep.c`](../../src/sweep.c)). A start crossed forwards is a start record, backwards an end; an end crossed forwards is an end, backwards a start; as CSS Animations' phase table gives them.

Most of a frame is quiet, so the sweep passes over a subtree whose timelines all stayed in one iteration without their time moving, unless the sweep ends where that time's hold meets its motion. A node's events are not counted when its parent's child time moved one way within one iteration of every timeline above, both ends off those iterations' boundaries by more than a margin of 1e-6, and the instant lies outside the span covered by more than the margin: its counts are then 0. Each node's active duration is kept when its clip is sealed for this test.

A context made with `eventCapacity` 0 skips the sweep altogether: the player's swept time still follows it, so a context grown later to hold records records what is crossed from then on, not the time before. The sample works out its own times and never reads the sweep's, so values are the same bits with records or without.

## The queue

An update makes at most one record per player, node or mark, kind and direction, with the count of crossings and the iterations of the first and last. Records are pushed in the order made (players in creation order, a player's nodes in clip order, then start, iteration, end) and sorted, stably, by time once the update is done ([`src/event.c`](../../src/event.c)). A record arriving at a full queue is dropped and counted, so a full queue keeps the first an update made. Nothing is lost silently: a read reports the drops since the last read.

Seeks jump, leaving a seek record; `mtwnPlayer_Sweep` and finishing cross what lies between, their records at once. A cancel of a player with a time, and a transition replaced, give cancel records.

## Checked by brute force

`tools/oracle/exact_oracle.py` counts crossings in exact rationals, two levels deep, on times that are not dyadic. The `crossing` suite compares the engine with an oracle that samples the timing itself, splitting each sweep wherever a level changes iteration or phase, and the `sweep` suite plays random clips on twin contexts, one counting every record and one taking the shortcuts, and requires the same records bit for bit.

## Traps

- An endless node's end lies at no time and is never crossed, not even by the children of a reversed endless timeline at infinity.
- The margin of the sweep's shortcut is far above rounding and far below a frame; a change to how child times are computed must keep the twin-context check passing, or the shortcut drops a record.
- A new record kind needs its place in the sort order and in CSS's phase table, both ways.
