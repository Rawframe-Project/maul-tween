# Releasing Maul Tween

A release follows the family's release checklist (`docs/conventions.md`,
section 15): the changelog's section with a line saying what the release
is, the version macros in `include/maul-tween/base.h` and the README's
status, the family drift check, one commit `release: X.Y.Z` green in
every CI job, an annotated tag `vX.Y.Z` and a release on the hosting site
with the changelog's section as its notes. Maul Tween takes these steps
of its own between the checklist's steps 4 and 5, before the commit:

1. **The sizes.** Build the library three ways, Release: GCC 14,
   GCC 14 with `-ffunction-sections -fdata-sections`, and Clang 20, and
   measure each with `python3 tools/code_size.py <libmaul-tween.a>`
   (`--cc clang-20` for Clang's). The README's table takes the three
   columns.
2. **The budgets, at a minor release only.** Each part's budget in
   `tools/code_size.py` is restated about 6% over its GCC 14 size
   (records mtwn-0051 and mtwn-0055). Record what each part grew since
   the last release and why, by object (`code_size.py --map <part>` on
   the last release's build and on this one), in mtwn-0055's amendment
   for the release, so a restatement is a reviewed decision rather than
   a reset; `RELEASED` takes the release's sizes in bytes, the ceiling
   `--budget` holds the next budgets to (10% over, unless `RAISED`
   names a record), and `RAISED` empties. A patch release keeps the
   budgets; a part over its budget then takes a recorded decision first.
3. **The ABI.** A patch release changes no public declaration or layout
   (`git diff vLAST -- include` shows only comments); otherwise the
   release is a minor one.
4. **The pins.** A library or plan that names a Maul Tween version to
   pin is told the new one where it is recorded.
