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
