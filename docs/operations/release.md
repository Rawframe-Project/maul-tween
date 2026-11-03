# Release

Releases are made by release-please from the conventional commit titles on `main`. Versions follow Semantic Versioning within 1: a feature bumps the minor version and a fix the patch. No change that would break 1.y ships: the family plans no 2.0.0, so such a change is designed to be additive or does not ship, and the pull request title check refuses a breaking title (`!`) or a BREAKING CHANGE footer. Tags are `vX.Y.Z`. The shared library's SOVERSION is major.minor, since a minor release may grow a def, and the CMake package accepts any later version of the same major. Before 1.0.0 a breaking change bumped the minor version, and the SOVERSION and the package took the whole version.

## Steps

1. release-please keeps a draft pull request titled `chore: release X.Y.Z` open. It moves the version between the markers in `include/maul-tween/base.h`, where CMake reads it, updates `.release-please-manifest.json`, and adds an entry to `CHANGELOG.md` from the `feat`, `fix` and `perf` titles.
2. Rewrite that entry in user language, in `CHANGELOG.md` on the release pull request's branch (`release-please--branches--main--components--maul-tween`): what changed for someone using the library, grouped as Added, Changed, Fixed, Removed and Security.
   - A change of results (any bit of a value, velocity or event record) comes first under Changed and says what changes and who sees it, since hosts may replay or compare frames across versions.
   - User documentation (the README and `docs/user/`) gets one short line under Changed when it changes what a user can find or do.
   - Leave out refactors, tests, CI, and the internals and operations documentation.
3. Put the same entry in the pull request's body, in place of the notes between release-please's `---` lines: release-please publishes the GitHub release notes from the body, not from `CHANGELOG.md`. release-please rewrites both whenever anything else lands on `main`, so the curation is the last change before the merge, and nothing else merges to `main` until the release is published. Curate only after the `release` run for `main`'s latest push has finished, and do not dispatch `release.yml` afterwards: a run in flight force-updates the release branch and writes the body again over the curation.
4. Check that the `required` CI job passed on the release pull request, including the code size budgets and the web size report, that the last nightly run on `main` passed, and that the tag `vX.Y.Z` does not exist yet (`git ls-remote --tags origin vX.Y.Z` prints nothing).
5. Mark the pull request ready and merge it. release-please tags the merge commit and publishes the GitHub release with the body's notes. The Pages workflow publishes the documentation from `main`.
6. The `assets` job of `.github/workflows/release.yml` then attaches the source archive `maul-tween-X.Y.Z.tar.gz` (the tagged tree, generated sources and oracle tables included), its `SHA256SUMS`, an SPDX SBOM and a build provenance attestation. Check that they are on the release; a user verifies a download with `sha256sum -c SHA256SUMS` and `gh attestation verify maul-tween-X.Y.Z.tar.gz -R Rawframe-Project/maul-tween`.

## Notes

- A pull request that gives existing inputs a different result is titled `feat`, or `fix` when it corrects a result the documentation already promised, and never carries `!`. It ships in a minor or a patch release as its type gives, and its body and the release notes say what changes. A change that would break the API or the ABI is made additive or does not ship. Before 1.0.0 a change of results carried `!` and a `BREAKING CHANGE:` footer.
- The shared family files must match their canonical copies in the organization's `.github` repository before a release.
- A library that pins this one moves its pin in a pull request of its own.
