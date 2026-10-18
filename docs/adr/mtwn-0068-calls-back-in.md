# mtwn-0068. Calls back in from the host's code

Status: Accepted

## Context

A context's functions run the host's code: a host easing
(`mtwnMakeHostEasing`) wherever an eased timing or segment is sampled,
which is in the update and also in adding a clip's track and finishing
the clip; the allocator in growing and destroying the context; the task
system's functions in the update. Nothing said whether that code may call
a function of the context it serves, and nothing stopped it: a host
easing destroying a player while the update walks the players, or
updating inside the update, corrupts the context without a sign.

Box2D v3 locks its world for a step, during which its callbacks run;
every function changing the world refuses a locked one (an assert in
debug builds, an early return in release). GLFW documents on its
allocator's functions that they "should not call any GLFW function" and
checks nothing. PrimeTween fixed "rare edge cases found by extensive
fuzz testing of the public API", mostly its API called "recursively from
user-defined callbacks" (1.5.0), as its completion callbacks exist to
change tweens; this library's events are records read after the update
(mtwn-0038), so it has no such callback to make work.

## Decision

A call back in is refused. A context is busy while one of its functions
taking it non-const runs; such a function called while another runs
returns `mtwn_errorState` at once and counts one misuse, and
`mtwnDestroyContext` returns without destroying. The headers state the
rule on the host easing, the allocator and the task system, which call
no function of the context they serve; the functions taking a const
context are not refused but read it in the middle of a call.

The flag is set by every such function, not only by those that run host
code: which calls reach a host easing is not plain from the code (adding
a track and finishing a clip measure eased timings, found by the fuzz
check below), and a rule on every call needs no list kept true.
`mtwnGrowContext` enters the context it is given and leaves the one it
returns; `mtwnDestroyContext` stays busy until the context's block is
freed.

The misuse count is atomic, incremented relaxed: a refusal may come from
the task system's code on a worker while the update's thread waits.

## Consequences

A call back in, from any host code and any call, is a counted refusal
and not a corruption. `test_reentry` calls every function taking a
context from a host easing in the update, from the allocator in growing
and destroying, and from the task system's functions, each with
arguments that are refused otherwise as invalid or stale, so the state
error is the busy refusal alone; `fuzz_api` samples a host easing that
fails the run whenever its context is not busy, which found the two
clip calls. The cost: 2.3 kB of code (155.4 to 157.7 kB for
everything), a load, a store and a branch on entry and a store on leave
for each call, 0.015% on the bench's churn frame and none measurable on
the update's frames.
