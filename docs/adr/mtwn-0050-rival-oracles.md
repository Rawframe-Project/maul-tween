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

## Extended: springs and colors

- Motion's spring (motion-dom 14.2.0 and motion-utils 14.1.1, pinned by
  SHA-512) is sampled by stiffness, damping and mass, its rest
  thresholds set so small it never snaps: eight springs, underdamped,
  near and at critical damping and overdamped, with and without a
  starting velocity, every 20 ms for 2 s, in `rival_spring.h`. The
  values agree within 64 ulps of the motion's scale (its offset plus
  its velocity over the undamped frequency), and the velocities within
  that times the frequency.
- CSS Color 4's `conversions.js` and `multiply-matrices.js`, at the
  commit `src/generated/color_matrices.h` was made from and pinned by
  SHA-256, convert 729 gamma sRGB colors (extended channels among them)
  to linear sRGB, Oklab and OkLCh, in `rival_color.h`. The coordinates
  agree within 16 ulps of their size, the hue within that over the
  chroma, and the nine grays are powerless on both sides.
- Each test fails on a change far below what a user would see: a
  stiffness one part in 10^9 off, one channel one float step off.

