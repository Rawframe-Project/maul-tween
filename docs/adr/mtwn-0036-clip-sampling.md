# mtwn-0036. Clips sampled into targets

Status: Accepted

## Context

Record mtwn-0035 gives players their time; record mtwn-0013 gives clips
their tree. A player's clip must reach its targets each update, with a
velocity for transitions that interrupt it, and a player not kept must
leave its targets where it filled them.

## Decision

- The update walks a player's nodes in the order they were added. The
  root passes the player's current time, held at its ends; a timeline
  gives its children its transformed progress times its iteration
  duration (Web Animations Level 2), its active time when that
  duration is infinite. Time runs backwards below reversed iterations,
  negative rates and players played backwards.
- A track with an effect writes its slot's target; later tracks write
  over earlier ones and later players over earlier ones. A target bound
  to a clip's player that no player writes in an update shows its
  base; a destroyed player's targets show theirs at once.
- A running player's velocity is its values' change over the last
  millisecond, as a transition's is; an integer's from the unrounded
  curve; at rest otherwise.
- A finished player not kept commits its values as its targets' bases
  (what it fills forward, as `commitStyles()` keeps) and is freed.
- Bindings name slots the clip's tracks use and live targets of their
  kinds.

## Consequences

Default node timing fills none, as Web Animations' does: a host that
wants a clip's end to stay sets fill forwards on its tracks. Composite
operations and weights per binding come with composition.
