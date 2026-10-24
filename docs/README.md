# Documentation

## Using the library

- [Getting started](user/getting-started.md): requirements, building, installing, samples, the model the API follows, results, defs and ids.
- [Easing](user/easing.md): CSS's and Penner's curves, rough easings, tables and fitted `linear()`.
- [Springs, decay and signals](user/physics.md): springs, flings, landing and bounds, the rubber band, punch and shake.
- [Values and paths](user/values.md): colors, angles, rotations, scales, zoom, keyframe paths and tracks.
- [Transitions](user/transitions.md): a context, targets, transitions, the update, reduced motion.
- [Clips and timelines](user/clips.md): tracks on nested timelines, positions, labels and marks, staggers.
- [Players, events and layers](user/players.md): playing clips, eased rates, scroll, event records, composition.
- [Threads and capacity](user/threads.md): the host's task system, players in step, growing a context.
- [API reference](https://rawframe-project.github.io/maul-tween/api.html), generated from the headers.

## How it works

- [Overview](internals/overview.md): the library profile and how the parts fit.
- [Glossary](internals/glossary.md).
- [Numbers](internals/numbers.md), [Easing](internals/easing.md), [Springs, decay and signals](internals/physics.md), [Values and paths](internals/values.md), [Timing](internals/timing.md), [Clips and composition](internals/clips.md), [Events](internals/events.md), [The update](internals/update.md), [Memory and the API contract](internals/memory-and-contract.md).
- [Maul conventions](internals/maul-conventions.md): the rules every Maul library follows.

## Maintaining it

- [Development](operations/development.md): toolchain, running one test, generated files and oracles, code size, benchmarks, fuzzing.
- [Checks](operations/checks.md): the local tiers, the CI jobs, test labels and the nightly scenes, the time budgets.
- [Release](operations/release.md): how a version is released and what it carries.
