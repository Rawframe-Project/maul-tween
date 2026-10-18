# mtwn-0071. Reserved bytes are zero

Status: Accepted

## Context

Twenty-one public structs carry `reserved` fields, which make their
padding explicit. Before this record nothing read them, so a program
could leave any bytes there. From 1.0 a minor release keeps what a
program built against an earlier one means (the family's source
promise), so no 1.y could give a reserved byte a meaning: a program
that left garbage in it would start asking for something else. Record
mtwn-0059 took one such byte before 1.0.

Interfaces that grow by such fields refuse what they do not know: Linux
requires the bytes of an `openat2` or `clone3` struct past what the
kernel knows to be 0 and returns `EINVAL` otherwise, after system calls
that ignored unknown flags could never use them; Vulkan's valid usage
says a reserved field "must be 0"; Win32 documents "Reserved; must be
zero".

## Decision

A reserved byte is 0 in everything that crosses the interface.

- An input a call takes is refused with `mtwn_errorInvalid` when a
  reserved byte of it, or of a part of it, is not 0, one misuse counted
  where a context takes it: the context, clip, clip track, clip
  timeline and player defs, their timings, positions, keyframes,
  bindings and track defs, the easings and springs inside them, a
  transition def's easing and spring (started or measured), and the
  timing, keyframes and track def `mtwnValidateTiming`, `mtwnMakePath`
  and `mtwnMakeTrack` take.
- The check is made at the call that takes the input in, never in a
  sample: values a sample reads every frame (an easing, a spring, a
  path, a track, a mix) are checked where a context or maker takes them.

## Consequences

A 1.y may give a reserved byte a meaning whose 0 is today's behavior,
and every program that worked before still means the same. A program
that set a reserved byte gets `mtwn_errorInvalid` where it got its
input taken before 1.0. The checks are a few compares per def, outside
the update. `test_reserved` sets each reserved byte of each input in
turn and expects the refusal and its misuse.

## Amended 2026-10-18: what the library writes

The rule's other half, now held by `test_reserved`: what the library
writes has its reserved bytes 0 whatever the caller's buffer held. Its
outputs (`mtwnEvent`, `mtwnPlayerState`, `mtwnTargetState`,
`mtwnTimingResult`, `mtwnClipNodeInfo`), the values its makers produce
(every easing and spring maker, `mtwnMakeColorMix`, `mtwnMakePath`,
`mtwnMakeTrack`) and its default defs, timing and stagger: the test fills
each with 0xFF before the call. A stagger `mtwnComputeStagger` takes is
an input like the others and is refused the same way. Each reserved
field's comment says which of the three it is: refused if not 0, 0 as
made, or written 0.
