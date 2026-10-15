# mtwn-0048. Stagger offsets, one pure function

Status: Accepted

## Context

The library's scope includes staggers: offsets spread over items from
a side, the centre or a grid point, eased. 0.1.0 shipped without them.
GSAP, anime.js and Motion all make a stagger a function from an item
to a number that the caller uses as a delay or a value, and their
arithmetic differs. GSAP's grid spread is a heuristic. Motion's row
never reaches the end of its easing. anime.js does not subtract the
nearest distance, so from the centre of an even count the first items
start half a step late. Only anime.js can repeat a random order, given
a seed.

## Decision

- One pure function, `mtwnComputeStagger`, and a plain value,
  `mtwnStagger`, in `maul-tween/timing.h`. It takes no context, pool or
  clip node; the host puts the offsets where a time or a value goes.
- Item i sits at column i mod columns, row i / columns. The origin is
  the first item, the last item, the centre, an index, or a point given
  as ratios of the last column and row. The distance is Euclidean, or
  along one axis. Edges measure to the nearest edge. Random is a
  permutation from a seed by SplitMix64.
- `t = (d - min) / (max - min)` (0 when all distances are equal), and
  the offset is `start + span * ease(t)`. The span is the amount, or the
  spacing times `max - min`, so items one cell apart are the spacing
  apart in a row and in a grid. A negative span reverses the order.

## Consequences

Two public functions. Staggers serve transitions as well as clips. The
offsets are the same bits on every platform, random ones included. Jitter,
modifiers, measured layouts and units stay with the host.
