# mtwn-0044. The defaults, sized together

Status: Accepted

## Context

The default context def allowed 64 clips but its 64 KiB of clip storage
held two clips of the default clip def (32 nodes, 64 keyframes, about
25 KB each). A host could not tell what a clip def takes. DOTween sizes
200 tweeners and 50 sequences by default and grows past them, "which
can cause hiccups"; this library never grows by itself.

## Decision

- The default clip def is for a small clip: 16 nodes, 32 keyframes and
  8 slots, about 13 KB.
- The default context def holds 16 clips, with 208 KiB of clip storage:
  16 default clips. Its other capacities are unchanged (1024 targets,
  256 players, 256 events, 1024 bindings).
- `mtwnGetClipBytes(def, &bytes)` gives the clip storage a clip def
  takes, so a host sizes `clipStorageBytes` for the clips it makes.

## Consequences

A test holds the default context to its clip capacity of default clips,
so a change to the clip's layout that breaks it is seen. A host with
more or larger clips states its storage, or grows the context.
