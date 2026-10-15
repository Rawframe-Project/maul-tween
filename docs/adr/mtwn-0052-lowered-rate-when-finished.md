# mtwn-0052. A finished player given a lower rate plays again

Status: Accepted

## Context

Web Animations' rate setter keeps the current time by setting it again.
For a finished animation held at its end, that sets only the hold time,
so the start time stays where the old rate put it. The next update
computes the time from that start and the new rate. With a lower rate
the result is short of the end, the hold is released, and the animation
plays again from there. The web-platform-tests check a finished
animation's rate raised and reversed, but never lowered.

Run in a browser through puppeteer (the page is kept in the design
repository): Firefox 157 jumps back both when the end was reached by a
seek and when it was reached by playing. Chrome 154 jumps back after a
seek and keeps an animation that played to its end finished.

## Decision

The library follows the specification's steps, which Firefox agrees
with. A player's rate set while it is held at its end can start it
again. A host that wants a player to stay finished at a new rate sets
the rate before the end. Afterwards, a seek or a finish sets only the
hold time again and keeps the old start, so the player plays again all
the same.

## Consequences

The behavior matches the text and one engine, and the header of
`mtwnPlayer_SetRate` names it. A host porting from Chrome may see a
finished player resume where Chrome would not.
