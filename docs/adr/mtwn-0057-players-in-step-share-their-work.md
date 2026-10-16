# mtwn-0057. Players in step share their work

Status: Accepted

## Context

A crowd of one clip's players started together stays in step: the same
clip at the same time, rate and direction. Each works out the same node
times, the same track values and the same records; only its targets,
its bindings' weights and composites and its id differ. The bench's
crowd (1,000 players of a clip of four tracks) costs 6 million
instructions a frame, and 26 million in the frame where all of them
repeat at once (record mtwn-0053).

Unreal's Animation Sharing evaluates one pose per state and hands it to
every character in that state, at the cost of the characters taking
the state's playback position. Here the results must stay those of each
player alone, bit for bit.

## Decision

- Each work (the serial one and each group's, record mtwn-0056) notes
  the clip and the time of the last player it advanced. A player of
  both is near it, and only a player near the last checks more; the
  others run the code they ran before, apart from the note.
- A player near the last compares its keys with the ones last noted in
  the work:
  - The sample's: the clip by index and generation, the time, the
    rate and the direction (a held player's direction decides a node's
    phase at its start).
  - The sweep's: the clip, the swept time and the time reached, the
    rate it ran at, the host's times, whether it starts from no time,
    whether it turns, and the sign of its rate.
  When the results for a key are kept, the player takes them. It takes
  each bound track's value and timing results, and works out from its
  own binding the weight, the composite and whether its velocity is
  left to be read. It takes the records with its own id.
- Only the second player in step keeps its results, worked out again
  from the node times its sample left. A player out of step keeps
  nothing, and pays only for the note and, when near, for the keys.
- Not shared:
  - a clip of more than 128 nodes;
  - a sweep of more than 64 records;
  - a track the keeping player left unbound but this one binds.
  Each is worked out as before.

## Consequences

- The bench's crowd in step, in instructions a frame (GCC 14 Release,
  x86-64):

  | frame | before | shared |
  |---|---|---|
  | the average of 300 | 6.02 M | 1.75 M |
  | the repeat | 25.8 M | 2.14 M |

  A crowd a millisecond apart, never near, costs 0.36% more (6.045 M
  against 6.023 M).
- A track's result and its application are marked always inline in
  the sampling loop: left to GCC 14, the sharing's calls to them took
  them out of the loop and cost every player 2% more. The sharing
  applies a result through the library's own out-of-line call, so the
  loop's copy is the only inlined one.
- A work holds 14.5 KB for the sharing: 15 KB more in a default
  context (214 KB), 255 KB more with the task hooks (1.12 MB).
- `test_clip_step` runs twins, one with the sharing off, on the crowd
  in step, a millisecond apart and mixed, serially and on tasks, and on
  a clip made in a destroyed clip's slot, on held players at a track's
  start in both directions, and on each input of a sweep's key apart
  from the last kept; after every update they hold the same bits.

## Amended 2026-10-16: keys by bits

A key's times, rates and ends are compared by their bits, not as
values: 0 and -0 are equal values, so a value compare rests on every
path giving the same bits for both. None was found to differ (a
player's time is its rate times the time since it started, -0 at rate
-0, but an iteration's progress is a difference and reaches an easing
as 0), and `test_clip_share` holds players paused at 0 and -0, with a
host easing that tells them apart, to what each gives alone. Compared
by bits, a shared result is the one its own inputs would give, whatever
a later path does with a sign; the counts are unchanged.

## Amended 2026-10-16: the capacities in the def

`mtwnContextDef` states the sharing's capacities: `sharedNodeCapacity`
(128) and `sharedRecordCapacity` (64), 0 sharing none of that kind and
holding nothing for it. At the defaults a work holds 14,848 bytes for
the sharing, the serial one and each of the 16 groups' with a task
system, so a host with few players or none in step can save 252 KB
of a hooks context. `mtwnGrowContext` takes any, as the sharing is made
again after the move.
