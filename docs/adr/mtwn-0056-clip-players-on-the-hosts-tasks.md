# mtwn-0056. Clip players on the host's task system

Status: Accepted

## Context

A frame where many clip players cross boundaries costs several times a
steady one (records mtwn-0053 and mtwn-0055): a hitch the host sees in
wall time. A clip player's update is independent of the others' until
it composes into its targets, which are shared and composed in creation
order (mtwn-0036). LitMotion runs its motions as a parallel job and
applies them afterwards; Maul VFX hands its work to the host's task
system through a hook pair on its context def.

The family's hook has no task index: the host splits `[0, itemCount)`
into ranges, runs them on any thread in any order, and each item writes
only its own memory. A clip player's update needs scratch (the crossing
levels, a clip's node times), so the scratch must belong to items the
library defines.

## Decision

- `mtwnContextDef` takes Maul VFX's pair: `enqueueTask`, `finishTask`
  and `userTaskContext`. Both NULL, the default, runs as before.
- With them, `mtwnUpdate` first hands the host up to 16 groups of the
  clip players in creation order, each group a work item with its own
  scratch. A group advances its players (times, finished state, the
  sweep, the sample), staging their records and their tracks' results,
  each before it is composed. Nothing a player shares is written.
- The serial pass then goes through the players in creation order as
  before: a transition advances, and a clip player queues its staged
  records and composes its staged results into its targets. Records,
  values, velocities, bases and the changed list are what a serial
  update makes, bit for bit, whatever the split.
- Staging is fixed at the context's creation:
  - Records: up to the queue's capacity, at most 1,024. Past a capacity
    equal to the queue's, a record would be dropped in the serial order
    anyway (as many came before it in this update), so it is counted as
    dropped.
  - Track results: a sixteenth of twice the bindings.
  - Node times: 128 a group; a clip of more nodes plays in the serial
    pass.
  When a player's staging runs out, its state is put back as it was
  and it, with the rest of its group, plays in the serial pass. No
  player is advanced twice.
- Each group runs in the library's floating-point environment on the
  thread the host gives it.

## Consequences

- With the hooks the context holds about 41 KB a group at the default
  def (about 660 KB in all) and 24 bytes a player. A host without them
  pays nothing but about 2% more instructions in the serial path, the
  results passing through a staging step.
- A frame's clip players use the host's threads; the composing stays
  serial, in creation order.
- `mtwnGrowContext` takes the def's task system with its capacities.

## Amended 2026-10-16: measured on threads

The gain was first estimated from instructions: each group's and the
whole update's, the groups placed on N workers longest first, gave the
bench's 1,000 players (four tracks, a 1 s loop) a repeat frame 10.8
times faster on 16 workers and a steady frame 5.2. Measured in wall time
on a quiet AMD Ryzen 9 5900X (12 cores, 2 threads each, two 6-core
dies, each with its own last-level cache; load average 5 to 7 on its
24 threads), with a persistent pool whose calling thread helps, the
median update:

| Frame | No hooks | Best | On 16 workers |
| --- | ---: | ---: | ---: |
| Steady, clips of their own | 384 us | 163 us (6 workers, one die) | 223 us |
| Steady, one clip, players 1 ms apart | 366 us | 164 us (6 workers, one die) | 254 us |
| Every player repeating | 1,169 us | 315 us (8 workers) | 326 us |

The groups themselves scale 5.8 to 6.7 times on 16 workers. What
bounds the update is the serial composing: 58 us on one worker, it
grows to 76 to 186 us with more, as it reads every value the groups
staged on other cores (about one cache miss a binding more at 6
workers). On one worker the hooks cost 2% to 8% over none. Sixteen
groups stay: more would shorten only the groups' span, already below
the composing from 8 workers. Composing in the groups what one player
alone writes is the next step to measure.

## Amended 2026-10-16: what one player alone writes is composed in its group

The serial composing bounded the update on threads: every binding was
composed on the calling thread from values staged on other cores, its
target marked unwritten before and shown its base after, each read of a
target a cache miss. Bevy's animation composes in parallel per target
because a target has one player by rule; LitMotion applies serially.
Here the property is a fast path, not a rule:

- A clip's player's binding is its target's alone when no other binding
  names the target and no transition is on it. Who writes a target
  changes only by calls between updates (a player made or destroyed, a
  transition started or ended, a target destroyed, the context grown),
  so the update counts the bindings again only after such a call, never
  on a steady frame.
- A group marks its players' alone targets unwritten, composes and
  commits them in place and shows their bases where nothing wrote them,
  staging only each target's index for the changed list. The serial
  pass puts those indexes on the list where it would have, between the
  results it composes for targets that several write; with no such
  target it walks no player's bindings.
- A group cannot undo what it wrote in place, so a player with alone
  targets goes to its group only if all it can make fits the group's
  staging (a result per node, six records a node); otherwise it is left
  to the serial pass before it starts.
- Records, values, velocities, bases and the changed list stay the same
  bits as without the hooks; the twin tests now give the players
  targets of their own beside shared ones, and empty each staging in
  turn.

Measured under load (load average 160 to 200, the builds alternated),
the serial rest of the bench's update fell from 96 to 219 microseconds
to 27 to 38 at 1, 8 and 16 workers. The context holds about 12 KB more
with the hooks at the default def (the staging 36 bytes a player, 4
more a sixteenth of twice the bindings); the earlier figure of about
660 KB in all was stale, the measure now about 900 KB. Without the
hooks the bench's frames cost up to 0.9% more instructions. The clips'
code grew 4.0 kB (mtwn-0055, amended).

## Amended 2026-10-16: measured again on threads

The same harness as the first measure, the builds before and after the
composing in the groups run in turn, three rounds, load average 9.7 to
11.5 on 24 threads (medians, microseconds; "rest" is the update outside
the groups' span):

| Layout | Workers | Before: update, rest | After: update, rest | Speed-up after |
| --- | ---: | ---: | ---: | ---: |
| clips | 1 | 407.4, 58.6 | 416.1, 16.7 | 0.92 |
| clips | 8 | 190.3, 121.5 | 89.0, 25.4 | 4.32 |
| clips | 16 | 226.6, 167.9 | 79.0, 28.6 | 4.87 |
| spread | 8 | 193.8, 126.6 | 98.7, 26.5 | 3.80 |
| repeat | 1 | 1201.5, 58.1 | 1188.6, 17.1 | 0.97 |
| repeat | 8 | 291.3, 121.9 | 189.1, 25.8 | 6.08 |
| repeat | 16 | 331.4, 168.9 | 183.1, 28.8 | 6.28 |

The rest is 17 to 29 microseconds at every worker count, and 16 workers
are now faster than 8. On one worker the hooks still cost more than
updating without them: 5.5% more instructions for a steady frame and
2.7% for the repeat (5.1% and 2.5% before), the staging's bookkeeping
for each player; making that cheaper is its own work.

## Amended 2026-10-16: the advances left serial, counted

`mtwnContext_GetSerialAdvances` counts the clip players' advances a
context with a task system ran on the calling thread since it was
made: a clip of more than 128 nodes, targets of a player's own that
might not fit its group's staging, a staging that ran out, and the
players after those in their group. Counted in the serial pass, once an
advance; 0 without a task system. As Jolt Physics names the capacity to
raise when an update finds a cache full (`EPhysicsUpdateError`), a host
seeing the count grow raises its binding or event capacities or splits
a clip.

## Amended 2026-10-17: the bookkeeping on one worker

On one worker the hooks cost 5.5% more instructions than none for a
steady frame and 2.7% for the repeat, against a bound of 2%. Most of it
was looking up, three times a player (marking, sampling, showing
bases), targets whose ids are known alive: a binding is alone only as
the writers were counted, and a destroyed target its player binds
marks the count stale, which is redone before any group runs. In a
group an alone binding's target is now taken from the binding; outside
a group (one player left, where nothing is counted) it is looked up as
before, and a test destroys an alone target there and makes another in
its slot. On one worker the bench's steady layouts cost 0.6% more than
without the hooks, the repeat 0.1%; without the hooks 0.4% fewer, as a
track's binding is read once.

Players of one clip in step cost 13.9% more on one worker: each of the
16 groups samples its first player of the clip in full, as a group
shares only what it worked out itself. With workers those samples run
side by side. The bench's `-tasks` count layouts bound all four in the
gate.

A player's first update under reduced motion sweeps twice (to where it
was first, then to the end it is finished at), so the fit check counts
twice the records for it. Each sweep makes at most six records a node.

## Amended 2026-10-17: who writes a target, kept as it changes

Counting the writers again over every binding (three passes, each
looking every target up) after any call that changed them was free on
a steady frame but paid in full by a host whose players come and go:
with a tenth of the bench's 1,000 players made and freed each frame,
the hooks cost 9.3% more instructions than none, the count most of it.
Bevy keeps its single writer as a relation set when it is made
(`AnimationTarget { player }`), never counted; here a target may have
several writers, so it keeps their number:

- A target's writers are counted as a player is made (its bindings'
  targets are alive) and freed (a target still alive, by its id's
  generation), up to 65,535, where the count stays. The live targets
  whose writers need an order, two bindings or one beside a transition,
  are counted as each count or transition changes and as a target is
  destroyed.
- Each change advances an epoch. A group works out its players'
  bindings' alone (target alive, one writer, no transition) only when
  the epoch moved since the groups last did; counts and transitions
  change only between updates, so the groups read them safely. The
  targets with ordered writers are marked by the serial pass after the
  groups, which never touch them.

The churn costs 1.3% more with the hooks than without, a steady frame
0.8%, the repeat 0.1% (bench `--count` with `-tasks`, one worker); the
gate bounds `churn` and `churn-tasks`.

## Amended 2026-10-17: one worker in time

Measured again at load 6.9 to 9.2, 0.4.0 against the build before the
last two amendments, the builds in turn: on one worker the bench's
players on clips of their own still take about 8% longer than without
the hooks (9% before), those a millisecond apart 2.5% (5.7%), the
repeat 2.0%, though their instructions are within 0.8%. The hooks'
update misses the second-level cache twice as often; the instruction
cache and the TLB do not move. At 8 workers the speed-ups are 4.2, 3.8 and 6.1 as before.

## Amended 2026-10-17: a clip of one player in its own node times

The doubled misses on one worker were demand fills from the shared
cache, about 4.5 K more cache lines an update (counted alone with
`perf stat`, each event in its own run, as several at once are
multiplexed). Built with the hooks but running no groups, the update
fetched as many as without the hooks, so the data was the groups'. A
group samples and sweeps a clip in its own scratch of node times,
because players of one clip may run in two groups at once; the serial
update writes each clip's own times, which lie beside its nodes, so its
walk through a clip stays in one place. With the group taking the
clip's own times when the clip has one player (no other group can then
touch them), the fills came back to within about 10% of the update
without the hooks, the instructions the same. Clips with players in
several groups keep the scratch; the twin tests poison every clip's
own times and require that groups touch only those of a clip with one
player, a clip of two included.

## Amended 2026-10-17: a clip of one player past 128 nodes

A group keeps node times for 128 nodes, so a player of a larger clip
was left to the calling thread. A clip's one player now samples in the
clip's own node times, so its size no longer keeps it from its group;
a larger clip with other players still plays on the calling thread.
The group's other bounds hold as before: a player whose results or
records might not fit its group's staging is left there too. The twin
tests run a 240-node clip of one player in its group, with the same
bits.
