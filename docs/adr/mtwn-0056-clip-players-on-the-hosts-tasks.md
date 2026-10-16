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
