# mtwn-0049. Reduced motion: what can finish, finishes

Status: Accepted

## Context

The library's scope includes a reduced-motion setting per context, and
0.1.0 shipped without one. Media Queries 5 says the user prefers an
interface that "removes or replaces" non-essential motion. WCAG 2.3.3
exempts essential motion, and does not count changes of color, blur or
opacity as motion. Apple asks for tighter springs and fades in place of
movement. Android multiplies every duration and delay by a scale, and
at 0 an animator skips to its end with its end listeners. Motion turns
off transforms and keeps opacity and color. Maul UI completes every
transition at once. Replacing motion takes knowing what a value is,
which the library does not.

## Decision

- `mtwnContext_SetReducedMotion` and `mtwnContext_IsReducedMotion`; off
  when a context is made. The host reads its platform and sets it.
- While it is on, a transition finishes at its next update: eased,
  spring or decay (at its rest), its delay skipped, its start and end
  records at that update's time, its end committed as the base.
- A running clip's player with a finite end finishes at its next
  update, as `mtwnPlayer_Finish` does, crossing what lies between once
  (its starts too when it had not yet swept), every record at that
  update's time. A paused or idle player is left alone. One with an
  infinite end cannot finish and plays on.
- `essential` in the transition and player defs, in a byte that was
  reserved, plays one on: fades, colors and essential motion.
- No spring tightening (the host chooses its springs) and no duration
  scale (a developer's option; a host can scale its own time).

## Consequences

Two public functions and two def fields, with the layouts unchanged. A
host's state after a reduced update is what it would be had the
motion played, records included. Looping clips stay the host's to
handle. The test costs about 3 instructions per transition per update
(156.8 to 159.8 over 64,000 eased transitions, GCC 14 Release, counted
with `perf stat -e instructions:u`).

## Amended 2026-10-16: the players that play on, shown

A review asked how a host finds the looping players that play on, since
it must still meet the preference for them. The update counts the
clips' players it leaves running under reduced motion though not
essential (`mtwnContext_GetPlayingOnCount`), and each one's state says
so (`mtwnPlayerState.playsOnReduced`). The count is taken in the update's
serial pass, so groups on the host's threads (mtwn-0056) share no
counter. A host that sees a count above 0 finds them by their state and
keeps, seeks or stops each.

