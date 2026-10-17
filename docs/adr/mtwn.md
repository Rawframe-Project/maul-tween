# Maul Tween records

The design records that apply to Maul Tween only. The family records
are listed in [README.md](README.md).

| Number | Title | Status |
|---|---|---|
| [mtwn-0001](mtwn-0001-library-profile.md) | Library profile | Accepted |
| [mtwn-0002](mtwn-0002-scope.md) | The whole scope, and what stays outside | Accepted |
| [mtwn-0003](mtwn-0003-two-layers.md) | Curves with no owner, and a context | Accepted |
| [mtwn-0004](mtwn-0004-targets-clips-players.md) | Targets, clips and players, with transitions as the short path | Accepted |
| [mtwn-0005](mtwn-0005-events-as-records.md) | Events are counted records in a bounded queue | Accepted |
| [mtwn-0006](mtwn-0006-fixed-capacities.md) | Every capacity in the def, allocated at creation | Accepted |
| [mtwn-0007](mtwn-0007-numbers-and-time.md) | Float values, double computation, integer host time | Accepted |
| [mtwn-0008](mtwn-0008-own-math-functions.md) | The library writes its own transcendental functions | Accepted |
| [mtwn-0009](mtwn-0009-cubic-bezier-solver.md) | Cubic-bezier easing by safeguarded Newton | Accepted |
| [mtwn-0010](mtwn-0010-springs-and-decay.md) | Springs and decay in stable closed form | Accepted |
| [mtwn-0011](mtwn-0011-easing-catalogue.md) | The easing catalogue, landing exactly | Accepted |
| [mtwn-0012](mtwn-0012-timing-model.md) | Web Animations timing on every node; synchronous players | Accepted |
| [mtwn-0013](mtwn-0013-timelines.md) | Timelines resolve positions when built; labels and marks are host keys | Accepted |
| [mtwn-0014](mtwn-0014-colors.md) | Colors in a named space, Oklab by default, premultiplied | Accepted |
| [mtwn-0015](mtwn-0015-rotations-and-angles.md) | Rotations by slerp with Kahan's angle; angles with hue methods | Accepted |
| [mtwn-0016](mtwn-0016-tracks-and-paths.md) | Keyframe tracks, centripetal Catmull-Rom, constant speed | Accepted |
| [mtwn-0017](mtwn-0017-punch-and-shake.md) | Punch and shake from a seed | Accepted |
| [mtwn-0018](mtwn-0018-api-shape.md) | The API shape | Accepted |
| [mtwn-0019](mtwn-0019-performance-and-size.md) | Performance and size targets | Superseded by mtwn-0043 |
| [mtwn-0020](mtwn-0020-proof.md) | Every part is checked against an independent reference | Accepted |
| [mtwn-0021](mtwn-0021-easing-api.md) | The easing API: one value type, makers that validate, a mode for every kind | Accepted |
| [mtwn-0022](mtwn-0022-sampling.md) | Sampling: even tables, and linear() fitted to a tolerance | Accepted |
| [mtwn-0023](mtwn-0023-spring-api.md) | Springs as a motion matrix; decay and rubber band | Accepted |
| [mtwn-0024](mtwn-0024-color-api.md) | Colors as CSS Color 4 interpolates them | Accepted |
| [mtwn-0025](mtwn-0025-angle-rotation-api.md) | Angles with CSS's hue methods; rotations by Kahan-angle slerp | Accepted |
| [mtwn-0026](mtwn-0026-integers-discrete-punch-shake.md) | Integers, discrete values, punch and shake as values | Accepted |
| [mtwn-0027](mtwn-0027-timing-api.md) | The timing API: Web Animations' steps, rate, repeat delay | Accepted |
| [mtwn-0028](mtwn-0028-context-targets.md) | The context in one block; targets of nine kinds, stored as given | Accepted |
| [mtwn-0029](mtwn-0029-transitions-update.md) | Transitions with easings, the update, the floating-point guard | Accepted |
| [mtwn-0030](mtwn-0030-spring-decay-transitions.md) | Spring and decay transitions; velocity handed over | Accepted |
| [mtwn-0031](mtwn-0031-paths.md) | Paths: keyframe curves in the values layer | Accepted |
| [mtwn-0032](mtwn-0032-tracks.md) | Tracks: every value kind over keyframes | Accepted |
| [mtwn-0033](mtwn-0033-clips.md) | The clip: one block of storage, tracks at positions, then finished | Accepted |
| [mtwn-0034](mtwn-0034-timelines.md) | Timelines: nested groups, labels and marks as nodes | Accepted |
| [mtwn-0035](mtwn-0035-players.md) | Players: Web Animations' procedures on the host's clock | Accepted |
| [mtwn-0036](mtwn-0036-clip-sampling.md) | Clips sampled into targets | Accepted |
| [mtwn-0037](mtwn-0037-crossings.md) | Crossings: exact counts through every ancestor's iterations | Accepted |
| [mtwn-0038](mtwn-0038-events.md) | Events: counted records of every crossing, in the context's queue | Accepted |
| [mtwn-0039](mtwn-0039-composition.md) | Composition: replace and add with weights | Accepted |
| [mtwn-0040](mtwn-0040-incremental-tracks.md) | Incremental tracks: each iteration moves on | Accepted |
| [mtwn-0041](mtwn-0041-growing-a-context.md) | Growing a context, when the host asks | Accepted |
| [mtwn-0042](mtwn-0042-the-update-hot-path.md) | The update's hot path, the same bits | Accepted |
| [mtwn-0043](mtwn-0043-targets-measured.md) | Performance and size targets, measured | Accepted |
| [mtwn-0044](mtwn-0044-defaults-sized-together.md) | The defaults, sized together | Accepted |
| [mtwn-0045](mtwn-0045-css-reversal.md) | Shortened reversals, as CSS Transitions shortens them | Accepted |
| [mtwn-0046](mtwn-0046-randomized-invariants.md) | Randomized invariants, and what they found | Accepted |
| [mtwn-0047](mtwn-0047-transitions-link-alone.md) | A program of transitions links no clip code | Accepted |
| [mtwn-0048](mtwn-0048-stagger.md) | Stagger offsets, one pure function | Accepted |
| [mtwn-0049](mtwn-0049-reduced-motion.md) | Reduced motion: what can finish, finishes | Accepted |
| [mtwn-0050](mtwn-0050-rival-oracles.md) | The rivals run as oracles | Accepted |
| [mtwn-0051](mtwn-0051-size-budgets.md) | Size budgets, checked on every commit | Accepted |
| [mtwn-0052](mtwn-0052-lowered-rate-when-finished.md) | A finished player given a lower rate plays again | Accepted |
| [mtwn-0053](mtwn-0053-crossing-search-from-a-guess.md) | A crossing's time searched from a guess | Accepted |
| [mtwn-0054](mtwn-0054-clip-velocity-when-read.md) | A clip track's velocity worked out when read | Accepted |
| [mtwn-0055](mtwn-0055-size-budgets-for-clip-speed-and-tasks.md) | Size budgets for the clip players' speed and their tasks | Accepted |
| [mtwn-0056](mtwn-0056-clip-players-on-the-hosts-tasks.md) | Clip players on the host's task system | Accepted |
| [mtwn-0057](mtwn-0057-players-in-step-share-their-work.md) | Players in step share their work | Accepted |
| [mtwn-0058](mtwn-0058-values-speed-targets.md) | The values layer's speed targets, measured | Accepted |
| [mtwn-0059](mtwn-0059-scales-and-zooms.md) | Scales and zooms | Accepted |
| [mtwn-0060](mtwn-0060-distances-for-speed.md) | Distances for durations set by speed | Accepted |
| [mtwn-0061](mtwn-0061-rough-easings.md) | Rough easings, seeded | Accepted |
| [mtwn-0062](mtwn-0062-a-context-without-records.md) | A context without records | Accepted |
