# mtwn-0073. Outputs on failure

Status: Accepted

## Context

Every public function that can fail returns a status and gives what it
produces through out-parameters. Eleven headers said what a failure
leaves in one (the null id, 0 or zero); the others said nothing. Probed
at 1.0.3, each documented clear held for a refused argument but not for
a missing or busy context: the call returned before the code that
cleared it. The clip getters cleared theirs the same way, undocumented;
the event and update counts did not. The makers, which take no context,
left their outputs unchanged on failure, undocumented, but for one: a
track refused at a later rotation segment had written the earlier
segments' mixes. `mtwnPlayer_GetState` left its state where the target
getters cleared theirs.

COM's memory management rules set out pointers to NULL on failure,
"on function entry", so a caller that cannot clean up is never handed
garbage. Vulkan leaves outputs undefined on error, and its move from
"unmodified" to "undefined" in 1.0.4 was a behavior change its callers
met. The family's libraries differ: handles are null on failure in all,
values "left unchanged" in some and "zeroed" in others, and most say
nothing.

## Decision

Every out-parameter's header says what a failure leaves in it, by one
rule:

- A call on a context clears its outputs on any failure: an id is the
  null id, a node 0, a value or state zero, a count 0. Each clears them
  first, before it checks the context, so a missing or busy context's
  refusal clears them too. A host that ignores a status reads nothing
  stale from the context.
- A call without a context (the makers, sampling, timing, a clip's
  bytes) leaves its outputs as they were: a failed remake keeps the
  caller's last good value, and no failure writes through a pointer the
  call refused.
- An array a call fills is never cleared: on failure an update's report
  and the events and targets read are unwritten, the count beside them
  0. A track's keyframes and mixes and linear()'s points, rewritten in
  place, are as they were: a track tries each rotation mix before it
  writes any, since the longest way between equal rotations refuses
  one.
- A header may name what a failure gives instead, and then that holds:
  a fit too large for its capacity reports the count it needs, as
  snprintf does, so a host can size it and fit again, its points then
  unspecified (a copy kept to restore them would cost what the fit's
  buffer saves).

`test_outputs` fills each output with bytes of 0xAB, makes each call
fail (a missing context, a refused argument, a stale id) and checks the
output cleared or untouched as the rule says; `test_reentry` checks the
busy refusals clear theirs.

## Consequences

Clearing first costs a store per output on every call, success too.
The rule is a 1.x promise under mtwn-0072: an output a failure clears
stays cleared, one it leaves stays left.
