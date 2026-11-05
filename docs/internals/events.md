# Events

Events are records in the context's bounded queue, never callbacks. A callback would run host code in the middle of an update, which may then call the context back, and its count and order would depend on frame rate. A record is counted exactly however far the time moved, read when the host likes, and makes no call.

## Crossings counted exactly

What a player crosses between two updates is found by counting, not by stepping through time ([`src/crossing.c`](../../src/crossing.c)). A clip that repeats a thousand times inside one frame, nested three deep, costs a few counts, not a thousand iterations walked.

Each timeline's child time runs along a path: its iterations in the order its rate takes them, each from its start to its end; repeat delays hold the path. A point below (a mark, a node's active start or end, an iteration start) is crossed once each time the path covers it. Where an iteration's end and the next one's start share a time (no repeat delay between, or going backwards) both are reached at that time.

A count is the exact number of points the path covers between the sweep's ends. A timeline's iterations are counted as offsets from its first, `floor(iterationStart)`. Its position at a local time is the start's fraction plus the scaled time over the duration, each step rounded as the timing model's own steps round it (the time less the delay, times the rate, over the duration; with repeat delays, the cycle and the time into it), but never added to the iteration index. Its iterations are the timing model's: the last ends where the fraction plus the iterations, rounded once, ends. So a count does not depend on how large `iterationStart` is, every iteration is exact up to 2^53 offsets from the first, and only the roundings of the time can move a point across a sweep's end, by an ulp of the time. The fields a record reports (`firstIteration`, `lastIteration`) are what the timing model reports, which rounds at the magnitude of `iterationStart`. Near an iteration's boundary, within an ulp of it (past about 2^52 iterations, within half an iteration), a count can be exact where the reported iteration is the nearest double to the exact one. Neither is a fault of the other.

What a local time reaches never falls as it grows, so no count is negative, and a sweep's count is the sum of its parts': `(a, b]` and `(b, c]` together count `(a, c]`.

A count between two local times is one walk down the chain of timelines with both ends. Where both lie in one iteration of a level, what came before that iteration is the same for both, and the walk goes on below between their child times. Where they lie in different iterations, the count is the rest of the earlier one's iteration, the constants the level keeps for its first, full forward, full reversed and last iterations, times the numbers of those whole iterations between, and the later one's iteration up to it. No count from the clip's start is formed, so a sweep's count does not depend on how much the clip crossed before it: a few crossings at a sweep's end are counted exactly after 2^60 of them, where a difference of two such totals would have lost them. Where both ends of an endless node are past the doubles, it is held there, as the timing model holds it, and nothing lies between. The constants are kept bottom up once per point counted.

A sweep forwards counts `(previous, current]`, backwards `[current, previous)`, so a point at an update's exact time is crossed once, in the update that reaches it. A sweep from no time (just played, rewound) takes its own start. Each crossing's direction is the point's own, its child time rising or falling.

## Times of the first and last crossing

A record carries the host and clip time of its first crossing. That time is found by bisection on the count over the doubles in their order as 64-bit integers: exact to the double, at most 64 halvings, each one count. The search starts from a guess, the last crossing the sweep found, or a record's first crossing for its last: records in one frame mostly share a time, and a right guess costs two counts. The invariant is kept, so the result is still a double where the count flips; where it flips more than once, the guess picks the same one of those few doubles on every platform.

## The sweep

Each update sweeps a clip's player from the time its last sweep reached to its current time ([`src/sweep.c`](../../src/sweep.c)). A start crossed forwards is a start record, backwards an end; an end crossed forwards is an end, backwards a start; as CSS Animations' phase table gives them.

Most of a frame is quiet, so the sweep passes over a subtree whose timelines all stayed in one iteration without their time moving, unless the sweep ends where that time's hold meets its motion. A node's events are not counted when its parent's child time moved one way within one iteration of every timeline above, both ends off those iterations' boundaries by more than a margin of 1e-6, and the instant lies outside the span covered by more than the margin: its counts are then 0. Each node's active duration is kept when its clip is sealed for this test.

A context made with `eventCapacity` 0 skips the sweep altogether: the player's swept time still follows it, so a context grown later to hold records records what is crossed from then on, not the time before. The sample works out its own times and never reads the sweep's, so values are the same bits with records or without.

## The queue

An update makes at most one record per player, node or mark, kind and direction, with the count of crossings and the iterations of the first and last. Records are pushed in the order made (players in creation order, a player's nodes in clip order, then start, iteration, end) and sorted, stably, by time once the update is done ([`src/event.c`](../../src/event.c)). A record arriving at a full queue is dropped and counted, so a full queue keeps the first an update made. Nothing is lost silently: a read reports the drops since the last read.

Seeks jump, leaving a seek record; `mtwnClipPlayer_Sweep` and finishing cross what lies between, their records at once. A cancel of a player with a time, and a transition replaced, give cancel records.

## Checked by brute force

`tools/oracle/exact_oracle.py` counts crossings in exact rationals, two levels deep, on times that are not dyadic. The `crossing` suite compares the engine with an oracle that samples the timing itself, splitting each sweep wherever a level changes iteration or phase, and the `sweep` suite plays random clips on twin contexts, one counting every record and one taking the shortcuts, and requires the same records bit for bit.

## Traps

- An endless node's end lies at no time and is never crossed, not even by the children of a reversed endless timeline at infinity.
- The margin of the sweep's shortcut is far above rounding and far below a frame; a change to how child times are computed must keep the twin-context check passing, or the shortcut drops a record.
- A new record kind needs its place in the sort order and in CSS's phase table, both ways.
