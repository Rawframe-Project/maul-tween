# mtwn-0003. Curves with no owner, and a context

Status: Accepted

## Context

Most hosts need a curve or a spring and nothing else; a renderer may
want to bake one into a table. Timelines, players and composition need
storage and an owner.

## Decision

Easings, springs, decay, rubber banding, interpolation of every value
kind and the timing computation are plain structs and functions: no
owner, no allocation, safe from any thread, arrays in caller memory.
Everything else lives in a context, the library's one root object,
created with the host's allocator and advanced by the host's update call
with the host's time.

The curve headers include only `base.h`.

## Consequences

A host that calls only the curves links only them, and the context uses
the same functions, so each curve has one implementation.
