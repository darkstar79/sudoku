# Contributing

Thank you for your interest in sudoku!

## Bug Reports and Feature Requests

Bug reports and feature requests are welcome via [GitHub Issues](../../issues). When filing a bug report, please include:

- Steps to reproduce the issue
- Expected vs actual behavior
- Build configuration (compiler, OS, build type)
- Relevant log output or error messages

## Code Contributions

This project does not currently accept pull requests. It is maintained as a single-author project.

You are welcome to fork the repository under the terms of the [GPLv3 license](LICENSE).

## Releases

> **Status:** the release process is still being established. The first tagged release (`1.0.0`) is in preparation; cadence and policies will evolve as the project matures. Until tagging begins, pre-release builds are available on the [openSUSE Build Service](https://software.opensuse.org/package/sudoku-cpp) as date-stamped snapshots (`YYYYMMDD+git.<sha>`).

The project follows a **trunk-based workflow**:

- All work happens on short-lived branches off `main`.
- `main` must always build and pass the full test suite (unit + integration + UI).
- Released versions are marked with annotated `vX.Y.Z` tags on `main`.
- There is no long-lived `develop` branch — feature branches merge directly to `main`.

### Versioning

The version number is declared once, in the `project(... VERSION X.Y.Z ...)` clause of [CMakeLists.txt](CMakeLists.txt). At configure time CMake substitutes that value into `sudoku::kAppVersion` via [src/version.h.in](src/version.h.in); the running application, packaging metadata, and the smoke tests in [tests/unit/test_version.cpp](tests/unit/test_version.cpp) all read that one constant. Do not hardcode the version anywhere else.

[Semantic Versioning](https://semver.org/) applies in spirit. Until `1.0.0` is tagged, surfaces that user code or save files depend on (save-file schema, settings keys, CLI flags, packaging layout) may still shift between snapshots — packagers and downstream consumers should track tags rather than `main`.

### Cutting a release

1. Audit the working tree on `main`: full test suite green, [CHANGELOG.md](CHANGELOG.md) `[Unreleased]` section reflects the actual diff since the previous tag.
2. Bump `project(VERSION ...)` in [CMakeLists.txt](CMakeLists.txt) if a new MAJOR/MINOR/PATCH is appropriate, and reconfigure to regenerate `sudoku/version.h`.
3. Promote the changelog's `[Unreleased]` heading to `[X.Y.Z] — YYYY-MM-DD`.
4. Coordinate with the downstream packager *before* tagging — particularly when transitioning off the date-stamped snapshot scheme, which may require an RPM `Epoch:` bump.
5. Tag `vX.Y.Z` on `main` (annotated, signed where applicable) and push the tag.

## Security Issues

See [SECURITY.md](SECURITY.md).

## Questions

For questions about the codebase or architecture, feel free to open a GitHub Issue with the "question" label.
