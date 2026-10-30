# Memory and the API contract

What a context holds and how, and the rules every call keeps: ids, defs and versions, refusals, what a failure leaves in an output, and calls back in from the host's code. The family rules these refine are in [maul-conventions.md](maul-conventions.md).

## One block

The context def names every capacity: targets, players (transitions included), clips, event records, bindings, the sharing's nodes and records, and clip storage ([`src/context.c`](../../src/context.c)). Creation checks the def and allocates the context, its pools, its queue and its list of changed targets in one block through the host's allocator; destruction frees it, with its clips' blocks, in one call. Counts are at most 2^24 each and clip storage at most 2^31 bytes, and sizes are computed with checked arithmetic, so no def can overflow a size. A full pool refuses with `mtwn_errorCapacity` and a capacity past its maximum with `mtwn_errorLimit`; nothing grows behind the host's back, and an update never allocates.

A game knows its budget, and a pool that grows in the middle of a frame stalls it or fails where no one checks. When the host does outgrow a context, `mtwnContext_Grow` grows it at a moment the host chooses: it allocates one new block, moves every pool with its free list (ids stay valid, freed slots reused in the same order as before), the bindings, the queued records in order and the changed list, frees the old block and writes the new address. Clips' storage does not move. On a refusal or an allocation failure nothing changes.

The defaults are sized together: a clip def for a small clip (16 nodes, 32 keyframes, 8 slots, 64 points of `linear()`, about 14 KB), and a context def holding 16 such clips in 224 KiB of clip storage, 1024 targets, 256 players with 64 points of `linear()` for each transition, 256 events and 1024 bindings. `mtwnContext_GetClipBytes` gives the clip storage a clip def takes; it takes the context, so every def but the context's is read under a context that passed the version check.

A target is 64 bytes: a kind and its base value, current value and velocity, four floats each. Values are stored as given and checked by their bits, so creating and setting a target do no floating-point arithmetic.

Bindings live in the context's `bindingCapacity` as runs taken first fit and coalesced when given back ([`src/runs.c`](../../src/runs.c)).

## Ids

Targets, players and clips are named by a 1-based index and a generation from 1 ([`src/pool.c`](../../src/pool.c)). A slot whose generation would wrap is retired, so an old id can never come back to life. Free slots are reused last in, first out, so the ids a sequence of calls hands out are the same on every run. A stale id is refused with `mtwn_errorStale` and counted as misuse, except by the `_IsValid` functions, which ask.

## Defs and versions

Every def opens with a cookie, set by its inline default function; a def not made by it fails the cookie. The context def, the root every other object lives under, also carries `MTWN_ABI_VERSION`, stamped by its default from the headers the program includes: `mtwnCreateContext` and `mtwnContext_Grow` check the cookie first and the version second, before they read anything else. The other defs are taken by a context that already passed, so they carry only their cookie. The version is the whole release before 1.0 and the major and minor version from 1.0 on, with the top bit set so a zeroed value never matches. Starting from the default is what lets a def grow: a field added later is set by the new default, and a program built against old headers is refused rather than misread.

## Padding

Public structs have no reserved fields and no fields that only spell out padding: a struct gains a field in the release that needs it, and `MTWN_ABI_VERSION` refuses a program built with another release's headers. Where a struct has padding, its bytes carry nothing. A call never reads them, and an output with padding that a call clears is cleared with `memset`, so a refused call leaves every byte of it 0. The library stores no bytes of its own (no snapshot, tape or asset), so the family's rule for stored bytes has nothing to apply to here.

## Refusals and misuse

A refused call changes nothing. A call on a live context refused for an invalid argument, an argument past a named bound (`mtwn_errorLimit`), a stale id or a call back in counts one misuse, and one refused for room (`mtwn_errorCapacity`) does not (`mtwnContext_GetMisuse`); the count is atomic, incremented relaxed, since a refusal may come from the task system's code on a worker while the update's thread waits. A host asserts the count stays 0, and a fuzz target asserts it moves exactly when a call is refused.

Caller input is never checked by an assertion: it is validated and refused with a status in every build. `MTWN_ASSERT` ([`src/invariant.h`](../../src/invariant.h)) checks only states that cannot happen unless the library itself is wrong. It traps in builds without `NDEBUG` (Debug, the sanitizer builds, the fuzz builds) and compiles out of Release builds, whose update loop must not pay for it.

## Outputs on failure

Every out-parameter's header says what a failure leaves in it, by one rule:

- A call on a context clears its outputs on any failure, first, before it checks the context: an id is null, a node 0, a value or state zero, a count 0. A host that ignores a status reads nothing stale.
- A call without a context (the makers, sampling, timing, a clip's bytes) leaves its outputs as they were, so a failed remake keeps the caller's last good value and no failure writes through a pointer the call refused.
- An array a call on a context fills is cleared too, as far as a success could write it: an update's report to the null id up to the context's target capacity, the events read to zero records up to its event capacity, the targets read to `count` zero values. Two refusals leave it as it was. A call back in (`mtwn_errorBusy`) leaves it, since it may be the array the call in progress fills. A null context leaves an update's report and the events, whose reach only the context knows. The targets read are cleared without a context too, since a success writes `count` values. Keyframes, mixes and `linear()` points rewritten in place are as they were.
- A header may name what a failure gives instead: a fit too large for its capacity reports the count it needs, as `snprintf` does, its points then unspecified.

## Calls back in

The host's code that the library runs (a host easing, the allocator, the task system's functions and tasks) must not call the context it serves. A context is busy while any of its functions taking it non-const runs; such a function called during another returns `mtwn_errorBusy` at once and counts one misuse, and `mtwnDestroyContext` returns without destroying. The flag is set by every such function, not only by those that run host code: which calls reach a host easing is not plain from the code (adding a track and sealing a clip measure eased timings), and a rule on every call needs no list kept true. Functions taking a const context are not refused, but read it in the middle of a call. `mtwnContext_Grow` enters the context it is given and leaves the one it returns.

## ABI

The headers are the common subset of C17, C23 and C++17. Every public struct's layout is ABI, and a def or a struct it holds grows only with a new ABI version. While the major version is 0, any release may change the API and the ABI; the version the context def carries refuses a mismatch instead of misreading it, and the other defs make objects only inside a context.

## Traps

- A def built by hand, or zeroed, is refused; tests and samples always start from the default.
- A new call that takes the context non-const must enter and leave the busy state, or a host easing could call back in through it.
- A new output follows the rule above, and `test_outputs` covers it: each output filled with a pattern and each call made to fail.
