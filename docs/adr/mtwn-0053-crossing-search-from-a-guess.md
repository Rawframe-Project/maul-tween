# mtwn-0053. A crossing's time searched from a guess

Status: Accepted. Amends mtwn-0037's "exact to the double".

## Context

A record's first and last crossing times are found by halving the
doubles between the sweep's ends, each halving a full count
(mtwn-0037): at most 64 counts a search, two searches a record. When a
looping clip repeats, every track's end and start are crossed at the
same time as the loop's repeat. A frame of 1,000 players of a looping
four-track clip then cost 1.06 billion instructions, a hundred steady
frames, once a second.

The halving keeps one invariant: the count does not reach one at the
near end of its bracket and does at the far end. It ends at two
neighbouring doubles. Random nested timelines show that the count's
own rounding can flip it more than once within a few doubles; the
halving's path then picks one of those flips. mtwn-0037's "exact to the
double" holds where the count rises monotonically, which is nearly
everywhere, and not at those few doubles.

## Decision

- The search can start from a guess. A guess inside the bracket is
  tested, then its neighbour toward the near end when it counts, and
  the halving goes on between whatever they leave. The invariant is
  kept, so the result is still a double where the count flips. A right
  guess costs two counts.
- A sweep guesses the last crossing it found, and a record's last
  crossing guesses its first. Records in one frame mostly share a time.
- The time found is a double where the count flips. Where it flips more
  than once, the guess can pick another of those few doubles than a
  search from none would. It picks the same one on every platform.

## Consequences

The frame where the loop repeats costs 180 million instructions
instead of 1.06 billion. The determinism scenario's hash and the event
tests are unchanged. A test compares searches from nine guesses with
searches from none over random nested timelines. Every result flips the
count and lies within 1e-9 s of the search from none. 64 of 461,862
picked another flip a few doubles away.

## Amended 2026-10-16: the path of frames

A review asked whether events depend only on the times swept. Which
crossings happen, and how many, do: counts are exact and add up over
any cut of the time into sweeps (mtwn-0042). A crossing's time does
not, at the last few doubles: it is a double where the count flips,
and where there are several, the one found follows the guess, which
comes from the crossings the same sweep found before. Playing to a
time in frames, in one update or by scrubbing cuts it into different
sweeps, so a record's time can differ by a few doubles between the
paths: within 1e-9 s, as the test above bounds. Two records keep their
order whatever the path unless their crossings lie within those few
doubles of each other. A test plays one clip by three paths and checks
both: boundaries 1e-12 s apart keep their order, and each record's
time agrees within 1e-9 s.


## Amended 2026-10-16: one crossing, one search

A record of one crossing searched its last as well as its first. The
two searches flip on different counts, the last on one that includes
its start and the first on one that does not; through nested levels'
rounding they can step a double apart, so the last could land in the
iteration before the first's: in a track in a repeating timeline in
another, 29 of 59 single crossings reported a last iteration other
than their first. The last of one crossing is now its first, searched
once: the record consistent, and the bench's 1,000 players on clips of
their own, all repeating in one frame, 22.5 to 17.75 million
instructions.

## Amended 2026-10-16: a guess past the sweep's end

A guess the interpolation put a double past the sweep's end was
dropped, and the search halved the whole sweep: some 50 counts, a
frame of 1,000 players at 42 million instructions instead of 17. A
guess past the far end is now the far end, which counts, and its
neighbour is tested as a right guess's is. Stepping out from a guess a
few doubles off was measured and dropped: no such guess showed, and it
cost 0.8 kB of code.
