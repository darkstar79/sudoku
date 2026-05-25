# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

> Planned as the **1.0.0** release — not yet tagged. The release process is still being established (see [CONTRIBUTING.md](CONTRIBUTING.md#releases)); until then, downstream builds on the openSUSE Build Service use date-stamped snapshots (`YYYYMMDD+git.<sha>`). The heading below will be promoted to `## [1.0.0] — YYYY-MM-DD` when the tag is cut.

First tagged release (in preparation). Adds the custom-puzzle feature suite on top of the generated-puzzle baseline.

### Added — UI

- **Session timer (right of status bar)** — optional indicator showing total time since the app launched, intended as a reminder for long sessions. Toggle via Settings → Display → *Show session timer*. Off by default. Independent of the per-puzzle timer on the left; keeps ticking through menus and pauses. Persists as `display.show_session_timer` in `~/.local/share/sudoku/settings.yaml` (downstream packagers: new YAML key, default `false`, gracefully ignored by older binaries).

### Added — Custom puzzles

- **Paste-string import** — paste an 81-character puzzle string (digits + `.`/`0` for empty) into the import dialog. Input is validated, length-checked, capped at 4 KiB, and rejected if it has multiple solutions.
- **Edit mode** — enter givens manually via `Ctrl+E`, commit with the toolbar's *Done Editing* action. Validation, uniqueness, and difficulty rating run at commit time.
- **Copy puzzle as text** — `Ctrl+Shift+C` writes the current puzzle's givens to the clipboard as an 81-character string (companion to paste-import).
- **Find step by technique** — pick a specific solving technique (e.g. X-Wing, Naked Triple) and the solver returns the next step in the current board that uses it. Read-only; does not consume a hint credit.
- **Automatic difficulty rating** for imported and edited puzzles. Generated puzzles already arrive pre-rated; imported/edited puzzles now do too, populating the same UIState fields (`puzzle_rating`, techniques, requires-backtracking) so statistics and save files capture an accurate rating.

### Changed

- **Repository renamed** from `sudoku-cpp` to `sudoku` (GitHub URL is now `https://github.com/darkstar79/sudoku`; the old path redirects). Downstream packaging recipes that fetch source by URL should switch to the new path.
- **Reverse-DNS app ID** changed from `org.sudoku_cpp.Sudoku` to `io.github.darkstar79.Sudoku` (follows Flathub naming convention for GitHub-hosted projects without a custom domain, derived from the new repo name). The `.desktop`, AppStream metainfo, and SVG icon files are renamed in lockstep; downstream `.spec` and packaging recipes that hardcode the old paths or `Icon=` value need to be updated.
- **Bundled dependencies bumped** for the Flatpak module set and the Conan recipe used by the Windows build: libsodium `1.0.18` → `1.0.21`, yaml-cpp `0.8.0` → `0.9.0` (Conan recipe was already at parity-via-Flatpak for yaml-cpp; both now align).
- **Save file format** bumped from 1.0 → 1.1. A new `PuzzleOrigin` field records whether a puzzle was generated, paste-imported, or entered in edit mode. Loading older 1.0 saves remains supported; missing field defaults to `Generated`.
- **Solver gains budget-bearing overloads** — `solvePuzzle(board, std::chrono::milliseconds)` and a technique-targeted dispatch. The backtracking fallback now honors the deadline so adversarial inputs cannot hang the UI.
- **Hint message clearing** — stale `hintMessage` is cleared on every `getHint` exit path so dismissed hints don't reappear after subsequent moves.
- **Session lifecycle** — entering edit mode finalizes any active statistics session as abandoned (previously orphaned).
- **Language selector** now discovers installed translations automatically. Packagers can ship a new locale by adding `sudoku_<code>.qm` to the translations directory and a matching `.ts` entry in `CMakeLists.txt`; no C++ change is required. Locale codes from `settings.yaml`, the legacy `language.txt`, and the Settings dialog are validated against a BCP 47 subset (`^[a-z]{2,3}(_[A-Z]{2})?$`) before being interpolated into a filesystem path.

### Removed

- **Manual "Analyze Difficulty" menu action.** Auto-rating makes it obsolete — every playable puzzle is now rated at creation time (or loaded with its rating from a save). Retry would have been a placebo anyway since `scoreDifficulty` is deterministic on a given board.

### Fixed

- **Master-difficulty saves now load correctly.** Previously the save loader's range check hardcoded `MAX_DIFFICULTY = 3` (Expert), so every Master save (difficulty 4) was silently rejected with `InvalidSaveData` — permanent data loss for Master-difficulty games. The range now tracks the `Difficulty` enum end-to-end. Existing saves that would not load before this fix will load after it. (#11)
- **AVX-512 dispatch now requires the BITALG sub-feature.** The solver's AVX-512 path issues `_mm512_popcnt_epi16` (in `findMRVCell`), which requires AVX-512 BITALG — present on Zen 5+ and Ice Lake-SP+, but missing on Skylake-X / Cascade Lake / Zen 4. Previously the dispatch gate only checked AVX-512F + AVX-512BW, so these CPUs were selected onto the AVX-512 path and crashed with SIGILL on first popcount. Detection now reads CPUID(7,0).ECX bit 12 and dispatch falls back to AVX2 (or scalar) when BITALG is absent. (#17)

### Internal

- New `IPuzzleAnalyzer` interface (`parseString`, `serializeToString`, `validate`, `checkUniqueness`, `scoreDifficulty`) backs all custom-puzzle workflows.
- Localization updated for new strings (English, German).
- Test coverage: 980 unit / 14 integration / 11 UI; lines 88.3% / functions 89.8% / branches 55.5% (all above 80% / 70% / 55% thresholds).
- `requirements.txt` now also pins `cmake>=3.28` and `ninja>=1.11`, so `pip install -r requirements.txt` (or `uv pip install -r requirements.txt`) brings the full Python-installable build toolchain. The Ninja generator referenced by `CMakePresets.json` needs `ninja.exe` on PATH; this removes one manual setup step for new contributors on Windows.
- Windows build scripts now auto-detect the newest installed Qt6 MSVC 2022 64-bit kit under `C:\Qt\6.*` (proper `[version]` sort, so 6.11 ranks above 6.9) via a new `scripts\find_qt6.ps1` helper. Replaces the previous hardcoded version allow-list (which topped out at 6.10.2 and silently failed on any newer release). `QT6_DIR` still overrides for non-default install locations.
- Windows-setup docs in [README.md](README.md) refreshed to lead with a `uv venv` + `requirements.txt` workflow, mention the MinGW-vs-MSVC kit gotcha explicitly, and document the new Qt6 auto-detect path. [docs/CONAN_PROFILES.md](docs/CONAN_PROFILES.md) rewritten to reflect that the Windows build path uses the auto-detected `default` Conan profile (the previously-documented `msvc-release`/`msvc-debug` named profiles were never shipped in the repo and would have confused new contributors).
- **New [docs/PACKAGING.md](docs/PACKAGING.md)** — upstream-to-downstream-packager interface. Documents the install tree, active migrations (the app-ID rename), recent packager-relevant changes (dynamic locale discovery, bundled-dep bumps), Flathub-prep status, and anticipated future changes (planned `sudoku_cpp` → `sudoku` repo rename). OBS recipes for the four `home:AndnoVember` namespaces should reference this doc going forward.
- **Windows build scripts migrated from `.bat` to PowerShell.** All four `scripts\*.bat` files are replaced by three idiomatic `.ps1` scripts: `build_windows.ps1` (parametrized `-Config Release|Debug`, collapses the previous Release/Debug duplication into one script), `run_tests_windows.ps1`, and `create_installer.ps1`. The migration fixes two bugs that affected the old batch scripts: (1) the build now passes `-s compiler.cppstd=23` explicitly to Conan, so dependency builds (Catch2, etc.) stay ABI-compatible with the project's C++23 code regardless of what the user's auto-detected Conan profile defaulted to — previously this caused `unit_tests.exe` to fail to link with `unresolved external Catch::StringMaker<basic_string_view>::convert` on a fresh Windows install; (2) script failures now propagate exit codes correctly via `$LASTEXITCODE` and `throw`, replacing the broken `exit /b %ERRORLEVEL%` pattern in the batch trampolines (which expanded `%ERRORLEVEL%` at parse time and silently exited 0 on any failure).

### Experimental

The following features are compiled in but hidden by default. Enable them under *Settings → Experimental*. They are **not part of the 1.0 stability commitment** — data formats, UI, and behavior may change or be removed in any 1.x release.

- **Training Mode** — interactive technique trainer with 11 categories of progressive hints, mastery tracking, and per-technique statistics. Reached via *Game → Training Mode* once enabled.
- **Progressive coaching hints** — in-game hint that walks you through the next step in three levels of help before applying it, with a check/apply workflow. Reached via *Help → Get Coaching Hint* (Shift+H) once enabled.

### Platform support

- **Linux (Fedora)** — primary development platform; full interactive use by the maintainer.
- **Linux (Ubuntu 24.04)** — built and full test suite run in CI on every push.
- **Windows 11** — installer artifact attached to the release. Built and tested in CI on `windows-2025` (per push to `main` and on demand); smoke-tested by the maintainer on Win11 before tagging.
- **Linux (openSUSE)** — RPM snapshots produced downstream via the openSUSE Build Service (not built directly by the project).
- **macOS** — not supported in 1.0; out of scope.

See [README.md](README.md#platform-support) for the difference between "tested", "CI-built", and "packaged downstream".

### Known limitations

- No user-facing timer pause. The timer runs continuously while a puzzle is open.
- Open sub-dialogs (Settings, Import Custom Puzzle, Find Step by Technique) do not re-translate when the language is changed from within them — close and reopen the affected dialog to see it in the new language. The main window shell (menu bar, toolbar, button panel, status bar) and the Training Mode widget retranslate immediately.
- No theme system; colors are fixed (light theme).
- Save file format v1.1 is the 1.0 stability boundary. Older v1.0 saves still load (the missing `PuzzleOrigin` field defaults to `Generated`). Pre-1.0 snapshot binaries may not load v1.1 saves; downgrading after upgrading is not supported.
