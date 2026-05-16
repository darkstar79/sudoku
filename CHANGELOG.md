# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] — 2026-05

First tagged release. Adds the custom-puzzle feature suite on top of the generated-puzzle baseline.

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

### Removed

- **Manual "Analyze Difficulty" menu action.** Auto-rating makes it obsolete — every playable puzzle is now rated at creation time (or loaded with its rating from a save). Retry would have been a placebo anyway since `scoreDifficulty` is deterministic on a given board.

### Internal

- New `IPuzzleAnalyzer` interface (`parseString`, `serializeToString`, `validate`, `checkUniqueness`, `scoreDifficulty`) backs all custom-puzzle workflows.
- Localization updated for new strings (English, German).
- Test coverage: 980 unit / 14 integration / 11 UI; lines 88.3% / functions 89.8% / branches 55.5% (all above 80% / 70% / 55% thresholds).
