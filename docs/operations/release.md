# Release

Releases are made by release-please from the conventional commit titles on `main`. Versions follow Semantic Versioning; before 1.0.0 a breaking change bumps the minor version, a feature the minor and a fix the patch. Tags are `vX.Y.Z`. While the major version is 0, the shared library's SOVERSION is the whole version and the CMake package accepts only its exact version, since any release may change the ABI.

## Steps

1. release-please keeps a draft pull request titled `chore: release X.Y.Z` open. It moves the version between the markers in `include/maul-tween/base.h`, where CMake reads it, updates `.release-please-manifest.json`, and adds an entry to `CHANGELOG.md` from the `feat`, `fix` and `perf` titles.
2. Rewrite that entry in user language before merging: what changed for someone using the library, grouped as Added, Changed, Fixed, Removed and Security. Leave out refactors, tests, CI and documentation. Say what a caller must change for a breaking change, and name every change of results (any bit of a value, velocity or event record), since hosts may replay or compare frames across versions.
3. Check that the `required` CI job passed on the release pull request, including the code size budgets and the web size report, and that the last nightly run on `main` passed.
4. Mark the pull request ready and merge it. release-please tags the merge commit and publishes the GitHub release with the changelog entry as its notes. The Pages workflow publishes the documentation from `main`.
5. The `assets` job of `.github/workflows/release.yml` then attaches the source archive `maul-tween-X.Y.Z.tar.gz` (the tagged tree, generated sources and oracle tables included), its `SHA256SUMS`, an SPDX SBOM and a build provenance attestation. Check that they are on the release; a user verifies a download with `sha256sum -c SHA256SUMS` and `gh attestation verify maul-tween-X.Y.Z.tar.gz -R Rawframe-Project/maul-tween`.

## Notes

- A change of results is never a patch release.
- The shared family files must match their canonical copies in the organization's `.github` repository before a release.
- A library that pins this one moves its pin in a pull request of its own.
