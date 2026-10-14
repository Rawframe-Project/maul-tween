# mtwn-0013. Timelines resolve positions when built; labels and marks are host keys

Status: Accepted

## Context

GSAP's position parameter (absolute, relative to the end, to a label, to
the previous child's start or end) is the friendliest way to build a
timeline; append, join and insert are its subset.

## Decision

Children are added to a timeline at one of those positions, which become
start times as they are added, so a built clip holds numbers only.
Sequences and groups are timelines filled at the end or at the previous
child's start. Labels (times) and marks (instants that make events) are
named by a `uint32_t` host key; a player seeks to a label by its key.

## Consequences

Updates do no lookups, and no strings enter the library.
