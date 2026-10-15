# mtwn-0051. Size budgets, checked on every commit

Status: Accepted

## Context

The library's size budgets were to be checked at each release. Both
releases on 2026-10-15 shipped without the check. 0.2.0 links 112.5 kB
in all, over the 112 kB budget, and nothing said so. The per-part
budgets also dated from before the parts were measured by the public
headers (record mtwn-0047), so they no longer compared like with like.

## Decision

- `tools/code_size.py --budget` fails when a part's code is over its
  budget, measured on a Release build with GCC 14, x86-64, `-O2`,
  unused sections dropped. The lane's gate runs it on every commit, so
  the check no longer depends on remembering it at a release.
- The budgets, about 6% over each part at 0.2.0: curves 23 kB, values
  48 kB, transitions 50 kB, clips 110 kB, everything 120 kB.
- What 0.2.0 added: staggers 2.1 kB in the values part, and reduced
  motion about 1.4 kB in the players and the context. Both are features
  in the scope, so the budget grows with them, recorded here, not
  silently.

## Consequences

A commit that grows a part past its budget fails locally, and the
budget is raised only by a record that names the cost. The README's
table shows each part's size beside its budget.
