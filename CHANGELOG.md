# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

> Planned as the **1.0.0** release — not yet tagged. The release process is still being established (see [CONTRIBUTING.md](CONTRIBUTING.md#releases)); until then, downstream builds on the openSUSE Build Service use date-stamped snapshots (`YYYYMMDD+git.<sha>`). The heading below will be promoted to `## [1.0.0] — YYYY-MM-DD` when the tag is cut.

First tagged release (in preparation). Adds the custom-puzzle feature suite on top of the generated-puzzle baseline.

### Added — Custom puzzles

- **Paste-string import** — paste an 81-character puzzle string (digits + `.`/`0` for empty) into the import dialog. Input is validated, length-checked, capped at 4 KiB, and rejected if it has multiple solutions.
- **Edit mode** — enter givens manually via `Ctrl+E`, commit with the toolbar's *Done Editing* action. Validation, uniqueness, and difficulty rating run at commit time.
- **Copy puzzle as text** — `Ctrl+Shift+C` writes the current puzzle's givens to the clipboard as an 81-character string (companion to paste-import).
- **Find step by technique** — pick a specific solving technique (e.g. X-Wing, Naked Triple) and the solver returns the next step in the current board that uses it. Read-only; does not consume a hint credit.
- **Automatic difficulty rating** for imported and edited puzzles. Generated puzzles already arrive pre-rated; imported/edited puzzles now do too, populating the same UIState fields (`puzzle_rating`, techniques, requires-backtracking) so statistics and save files capture an accurate rating.

### Changed

- **Save file format** bumped from 1.0 → 1.1. A new `PuzzleOrigin` field records whether a puzzle was generated, paste-imported, or entered in edit mode. Loading older 1.0 saves remains supported; missing field defaults to `Generated`.
- **Solver gains budget-bearing overloads** — `solvePuzzle(board, std::chrono::milliseconds)` and a technique-targeted dispatch. The backtracking fallback now honors the deadline so adversarial inputs cannot hang the UI.
- **Hint message clearing** — stale `hintMessage` is cleared on every `getHint` exit path so dismissed hints don't reappear after subsequent moves.
- **Session lifecycle** — entering edit mode finalizes any active statistics session as abandoned (previously orphaned).
- **Language selector** now discovers installed translations automatically. Packagers can ship a new locale by adding `sudoku_<code>.qm` to the translations directory and a matching `.ts` entry in `CMakeLists.txt`; no C++ change is required. Locale codes from `settings.yaml`, the legacy `language.txt`, and the Settings dialog are validated against a BCP 47 subset (`^[a-z]{2,3}(_[A-Z]{2})?$`) before being interpolated into a filesystem path.

### Removed

- **Manual "Analyze Difficulty" menu action.** Auto-rating makes it obsolete — every playable puzzle is now rated at creation time (or loaded with its rating from a save). Retry would have been a placebo anyway since `scoreDifficulty` is deterministic on a given board.

### Internal

- New `IPuzzleAnalyzer` interface (`parseString`, `serializeToString`, `validate`, `checkUniqueness`, `scoreDifficulty`) backs all custom-puzzle workflows.
- Localization updated for new strings (English, German).
- Test coverage: 980 unit / 14 integration / 11 UI; lines 88.3% / functions 89.8% / branches 55.5% (all above 80% / 70% / 55% thresholds).

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
