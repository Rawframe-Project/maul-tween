# mtwn-0055. Size budgets for the clip players' speed and their tasks

Status: Accepted

## Context

Record mtwn-0051 set each part's budget about 6% over its size at
0.2.0, and allows a part past its budget only by a record that names
the cost. Since 0.2.1 the clip players were made faster: crossings
searched from a guess (mtwn-0053), a track's velocity worked out when
read (mtwn-0054), sibling tracks sharing their timing, kept active
durations, walks remembered within a record, a level's shape kept. In
the bench's clip of 1,000 looping players, the average frame went from
22.5 to 5.9 million instructions and the frame where they repeat from
1,060 to 25.6 million. That used the clips part's margin: 104.2 kB at
0.2.1, 109.9 kB after.

Next, the clip players run on the host's task system (as Maul VFX's
work does), which adds the hooks, a split of each player's update into
work that writes only its own results and a serial step that composes
them, and per-task scratch. That is scope, not speed, and is estimated
at 4 to 6 kB.

## Decision

- Until 0.3.0, the clips part's budget is 118 kB and the whole
  library's 128 kB. The other parts keep theirs.
- At 0.3.0 every budget is restated by mtwn-0051's rule, about 6% over
  each part's size at that release.

## Consequences

The speed work is paid for in size, about 5.7 kB, and named here; the
check on every commit stays. A part growing past 118 kB before 0.3.0
again takes a record.
