# mtwn-0050. The rivals run as oracles

Status: Accepted

## Context

The library maps its easings' parameters to GSAP's, Penner's and
tween.js's, and its tests are to check those mappings against the rivals
themselves, never against copies of them. Until 0.2.0 the tests held
GSAP's formulas written out again in C, and tween.js had never been run.

## Decision

- `tools/oracle/rivals.mjs` runs GSAP 3.15.0 and tween.js 25.0.0 under
  Node and writes the curves they share with Maul Tween, at 101 even
  progresses, as exact hexadecimal doubles in
  `test/generated/rival_easing.h`. The script is run by hand. With
  `--check` it shows the file is current.
- Each package is fetched by its pinned version and refused unless its
  SHA-512 is the pinned one. Neither is kept in the repository or linked
  into the library.
- `test/test_rival_easing.c` checks every curve against a bound derived
  from the definitions, not fitted to the results:
  - 2^-51 where the curves are defined alike (powers, sine, circ,
    bounce and back, in every mode).
  - `a (1 + a) 2^-10` for elastic, whose rivals miss 1 at the end by up
    to `a 2^-10`.
  - 2^-10 for expo against Penner's (tween.js), and 2^-7 against GSAP
    3's own blend `2^(10 (p - 1)) p + p^6 (1 - p)`.

## Consequences

The mappings the easing header states are checked against the rivals'
own code. Running the oracle found two facts, now in the header: GSAP 3's
expo is not Penner's, and the elastic difference grows with the
amplitude. Updating a rival is a change of its pin and hash, and a rerun.
