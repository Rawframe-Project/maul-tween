# mtwn-0039. Composition: replace and add with weights

Status: Accepted

## Context

Record mtwn-0004 lets many players write one target in composite
order. Web Animations Level 1 combines an effect value with the
underlying value by replace, add or accumulate; CSS Values 4 makes
accumulation addition for every type but lists. Layered motion (an
offset over a loop, a layer faded in and out) also needs weights,
which Web Animations leaves to its host.

## Decision

- A binding names its composite operation, replace or add, and its
  weight in [0, 1]; `mtwnBind` makes one at replace and full weight. A
  player's weight scales its bindings' and `mtwnPlayer_SetWeight`
  changes it while it plays. Accumulate is not offered: for every value
  kind here it is addition.
- The result mixes from the underlying value toward the composited one
  by the weight: replace blends toward the player's value, add adds
  the weight times it. The underlying value is what earlier players
  wrote in this update, or the base for a target not yet written, so an
  add layer never compounds on its own last frame.
- Numbers, vectors and angles add and mix per channel; an integer
  composes unrounded and rounds once; colors add and mix premultiplied
  by alpha, alpha clamped to 1, and a result of alpha 0 has color
  channels 0, as CSS gives; a rotation replaced slerps from the
  underlying toward the effect by the weight, and one added slerps from
  no rotation toward the effect by the weight, then turns the
  underlying by that (the effect in the underlying's frame); a discrete value
  is not additive and steps at half its weight. Velocities compose per
  channel.
- A finished player not kept commits the composited value, what lies
  under it included, as the base.
- Transitions stay replace.

## Consequences

A host layers motion by order and weight without its own blending; a
breathing add layer under a hover transition keeps both.
