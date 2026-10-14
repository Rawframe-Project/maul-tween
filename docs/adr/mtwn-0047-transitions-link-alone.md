# mtwn-0047. A program of transitions links no clip code

Status: Accepted

## Context

A user interface that moves its properties by transitions needs no
clips, players, composition, paths or event crossings, but linked them
all: the update called the clips' players' advance directly, the
transitions took their slots from the players' file, and the event
queue shared a file with the clips' sweep. A transitions-only program
linked 96 kB of the library's code; the review of the size targets
asked for the transitions apart, by a build option if need be.

## Decision

- No build option: one library, one build. The clips' players' part of
  an update is reached through the context, a table of three functions
  (`begin`, `advance`, `end`) the first clip's player made installs; a
  program that makes none never references it.
- The players' slots and their creation order move to their own file,
  apart from the clips' players' procedures; the clips' sweep moves out
  of the event queue's file. A linker that keeps or drops whole objects
  drops them, and one that drops sections drops more.
- The cost: an indirect call per clip's player per update, and about 2
  instructions per transition per update.

## Consequences

A transitions-only program links 47 kB (39 kB with the library built
with function sections) instead of 96 kB; the README gives the sizes
by part.
