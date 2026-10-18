# mtwn-0066. Snapped values stay the host's

Status: Accepted

## Context

Web animation libraries snap the values they write: GSAP's SnapPlugin
rounds every value of a tween to an increment, to the nearest of a
list, or to a list's values within a radius (its ModifiersPlugin runs
any function per value per tick); DOTween's tweens take a `snapping`
flag rounding to whole numbers; anime.js 4 takes its `utils.round` and
`utils.snap` as property modifiers. Each writes the property itself,
so the snap is part of its write.

This library writes no property: the update reports the targets whose
value changed, and the host shows them. A target's value is the
composite of every writer bound to it, in order, and its velocity
drives springs and transitions that retarget from it.

## Decision

No snap is added to targets. A host showing a value on a grid rounds
it where it shows it and skips the write when the rounded value is the
one already shown: one line, next to the shown value it compares with,
and a list or a radius is its own rule. The target keeps the exact
value, so composition, velocities and springs never see the rounding.
A count stepping by whole numbers is an integer target, which rounds
as a kind.

## Consequences

The update still reports a target whose exact value changed while its
snapped one did not; the host's comparison drops it. The guide's update
section says so.
