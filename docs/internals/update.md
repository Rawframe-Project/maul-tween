# The update

`mtwnUpdate` advances every player to the host's time in the library's floating-point environment, in the order the players were made, later ones writing over earlier ones, then reports the targets that changed ([`src/update.c`](../../src/update.c)). It is the one call a host makes every frame, so it is built to cost next to nothing at rest and to stay within its measured budget with tens of thousands of values moving.

## The hot path

- **Players in creation order.** Every player, transition or clip's player, sits in one pool with a list in creation order, the composite order the update walks ([`src/player_slot.c`](../../src/player_slot.c)). The clips' players also have a chain of their own, so the passes over them before and after an update skip the transitions.
- **A slot for one curve.** A player slot is a union of a clip's player and a transition, a transition holding one curve by its kind. The fields every update reads sit in the slot's first three cache lines: an update reads four cache lines a transition (its target's base, value and velocity, and the transition's times, ends and curve), which for 64,000 transitions is near the machine's cache bandwidth alone.
- **Inline powers.** An integer power easing already normal runs inline in the update, with no call.
- **Velocity when read.** An eased transition of numbers writes its value each update and leaves its velocity to be computed when read (by `mtwnTarget_GetState`, a transition inheriting it, or a player composing over it), by the same arithmetic at the last update's time. Most frames nobody reads a velocity.
- **Changed targets.** A target is marked when its value or base changes and reported once per update, in the order of change, into the caller's array; the total may exceed its capacity. A context at rest reports nothing and does almost nothing.

The budgets come from measurement on the bench, not from guesses: 64,000 eased number transitions created in at most 4 ms and updated in at most 0.75 ms (0.8 ms with the changed-target report) on the reference machine, a cubic-bezier sample at most 30 ns, a Penner easing of arithmetic at most 10 ns, a spring or an exponential easing at most 45 ns. The code's size per part has a budget too; see [development.md](../operations/development.md#code-size).

## Linking only what is used

There is one library and one build, with no feature options. A program of transitions alone must not pay for clips. The clips' players' part of an update is reached through a table of three functions (`begin`, `advance`, `end`) that the first clip's player installs in its context, so a program that makes none never references that code, and its linker drops the objects. The players' slots and creation order live apart from the clips' players' procedures, and the sweep apart from the event queue, so the split falls on object files. The cost is an indirect call per clip's player per update, and about two instructions per transition.

## The host's task system

The context def takes the same pair of functions Maul VFX takes, `enqueueTask` and `finishTask`, so one host task system serves both ([`src/clip_tasks.c`](../../src/clip_tasks.c)). With them, an update first splits the clips' players in creation order into up to 16 groups, each a work item with scratch of its own. A group advances its players (times, finished state, sweep, sample), staging their records and their tracks' results; a player writes only its own state. The serial pass then goes through all players in creation order as before: a transition advances, and a clip's player queues its staged records and composes its staged results into its targets. Records, values, velocities, bases and the changed list are what a serial update makes, bit for bit, whatever the host's split and order.

Staging is fixed when the context is made, so the update never allocates:

- records, up to the queue's capacity and at most 1,024 a group; past a capacity equal to the queue's, a record would be dropped in the serial order anyway, so it is counted as dropped;
- track results, a sixteenth of twice the bindings;
- node times, 128 a group, so a clip of more nodes plays in the serial pass while other players run.

When a player's staging runs out, its state is put back as it was and it plays in the serial pass with the rest of its group; no player is advanced twice. A player with targets of its own, whose results a group would write in place, plays serially before it starts if they might not fit, since what is written in place cannot be undone. Each group runs in the library's floating-point environment on the thread the host gives it.

A target that one player alone writes, with no transition on it, is composed in that player's group, which shortened the serial rest several times. Transitions, and targets several players write, are composed on the calling thread, which bounds the speed-up.

On one worker, the task functions running each group in place, an update through groups costs at most 2% more than the same update without them, counted in instructions on the bench's `--count` layouts of 1,000 clip players of four tracks (GCC 14, Release, the difference of 2,000 updates and none):

| Layout | Without groups | With groups | More |
|---|---|---|---|
| `step`: one clip, every player in step | 1,764,548 | 1,793,270 | 1.6% |
| `spread`: one clip, every player at its own time | 5,880,232 | 5,957,858 | 1.3% |
| `clips`: a clip each | 5,871,983 | 5,954,597 | 1.4% |

What is left is the staging itself: a group stages each player's records and track results and marks and resets its own targets, and the serial pass then queues and composes them. Without groups the player writes them where they go. In step, where the serial pass shares nearly all the rest, this is the largest part of the difference. Removing it would mean composing in place, which only a target one player alone writes allows.

## Players in step

A crowd started together plays one clip at the same time and rate, and would sample it identically many times. Each work (the serial one and each group's) notes the clip and the time of the last player it advanced; only a player near that note compares its keys:

- the sample's: the clip by index and generation, the time, the rate and the direction;
- the sweep's: the clip, the swept time and the time reached, the rate, the host's times, whether it starts from no time, whether it turns, the sign of its rate.

When the results for a key are kept, the player takes them: each bound track's value and timing results, with its own binding's weight and composite, and the records with its own id. Only the second player in step keeps its results, worked out again from the node times its sample left, so a player out of step pays only for the note. A clip of more than 128 nodes, a sweep of more than 64 records, and a track the keeping player left unbound but this one binds are worked out as before. The sharing's memory is `sharedNodeCapacity` and `sharedRecordCapacity` in each work. Before the groups start, the first group's first three players run on the calling thread, by which a crowd in step has its sample and sweep kept, and every other group starts from a copy of that sharing. A crowd split over 16 groups is then worked out once, as in the serial pass, rather than once a group; a work's sharing is valid for whichever player comes next, so the copy changes no result.

## Reduced motion

While reduced motion is on, every transition finishes at its next update, eased, sprung or decayed to its rest, its delay skipped and its records at that update's time; every running clip's player with a finite end finishes as `mtwnPlayer_Finish` does, crossing what lies between once. Defs mark `essential` motion (a fade, a color, motion that carries meaning) to play on, since the library cannot tell motion from a fade. A player with an infinite end cannot finish: it plays on and is counted for the host to handle. Spring tightening and a duration scale were not added: the host chooses its springs and can scale its clock.

## Traps

- The update may not allocate, call the host's code other than easings and the task system, or depend on frame deltas: local times come from integer host time.
- The twin-context suites (`clip_tasks`, `clip_step`) run one script serially and through the task system or the sharing, and require the same bits; a change to either path keeps them equal.
- A field added to a player slot goes after what the update reads first, or it pushes the hot fields into a fourth cache line.
