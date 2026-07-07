# Whole-Application Code Review — 2026-07-06

**Reviewed at:** commit `e6b8516` (main), 2026-07-06.
**Method:** wide survey followed by six parallel deep audits (solver core, non-solver core,
Qt view/viewmodel layer, test suite, CI/CD + build + packaging, determinism). Every finding
below was **verified by reading the actual code**, not pattern-matched. Line numbers refer to
commit `e6b8516`; re-locate by symbol name if the file has drifted.

**Predecessor:** [CODE_REVIEW_2026-05-25.md](CODE_REVIEW_2026-05-25.md) (145 findings, issues
#11–#30). This review is independent but cross-references it where relevant.

---

## How to use this document (for a continuing agent)

- Finding IDs are stable: `SAVE-*` (persistence/restore), `STAT-*` (statistics), `REL-*`
  (release/CI/packaging), `NET-*` (test safety nets), `SOLVER-*`, `UI-*`, `DET-*`
  (determinism/test hygiene), `Q-*` (code quality). Reference them in branch names /
  issue titles (e.g. `feature/save-1-save-file-validation`).
- Severity: **HIGH** = memory safety, user-visible data loss, or release-blocking;
  **MED** = wrong behavior users can hit or systemic risk; **LOW** = latent/hygiene.
- Per project workflow: TDD mandatory (failing test first), no auto-commits, feature
  branches `feature/*`, squash merges, CHANGELOG entry in the same commit for anything
  user- or packager-visible (most SAVE/STAT/UI fixes qualify; NET/DET test-only fixes do not).
- **Do-not-resurrect list** (previously adjudicated false alarms — do NOT re-open):
  - Fish cover overlap (issue #21): base-disjointness is the invariant; cover-overlap
    rejection would weaken the solver. Documented in-code in `mutant_fish_strategy.h`.
  - Grouped Nice Loop per-cell guard and Continuous Nice Loop (issue #23 items 4/5):
    empirically sound, guard rejected.
  - Cell Forcing Chain mask-diff fix (issue #23 item 2): unsound, won't-fix.
- Verification harness contract: any solver-*logic* change ⇒ run the nightly command
  (`cmake --preset relwithdebinfo -DSUDOKU_ENABLE_COVERAGE=OFF && cmake --build --preset
  relwithdebinfo && ./build/RelWithDebInfo/bin/tests/unit_tests "[strategy]"`, ~2h,
  RelWithDebInfo, coverage OFF — never Debug/coverage). Rating/metadata-only changes cannot
  move that harness; don't burn the run on them. Last-resort-tier strategies are NOT reached
  by `solvePuzzle` — probe `findStep` per-state (the issue-#59 lesson; PR #58's "0/500" was a
  false negative).

---

## 1. Strengths (verified — keep these properties intact)

- **Solver soundness architecture.** `CandidateGrid` = structural `ConstraintState` + monotone
  per-cell elimination overlay (`src/core/candidate_grid.h:72-74`); persistent grid in
  `solvePuzzleImpl` matches the harness replay model; backtracking fallback operates on the raw
  board (`src/core/sudoku_solver.cpp:258,359-363`) so wrong eliminations degrade to
  stall+recover, never a wrong solution. All historical bestiary fixes confirmed present
  (memo-cache truncation guards `solution_counter.cpp:218-335,458,515,613`; Sue de Coq
  disjointness `sue_de_coq_strategy.h:188-226`; Medusa per-component colors
  `three_d_medusa_strategy.h:348,414`; Franken/Mutant base dedup; Hidden UR
  `isDiagPartnerForced`; Hidden Triple/Quad value validation). W-Wing and UniqueLoop Type 1
  re-derived from theory during this review: sound.
- **Persistence hardening (where designed in):** `file_utils::atomicWriteFile` (tmp+fsync+rename,
  `src/core/file_utils.cpp:76`) used by saves/stats/ledger; corrupt files archived
  (`.corrupt-<ts>-N`), never deleted; zlib bomb cap (`save_manager_compression.cpp:44`);
  correct libsodium usage (fresh salt+nonce, `sodium_memzero`); fail-closed `PlayTimeLedger`.
- **CI layering:** format → determinism grep → i18n → coverage (absolute gcovr + per-PR
  diff-cover ≥80%) → ASan/UBSan unit job per PR → diff-scoped clang-tidy with nightly parity;
  `check-nightly-skip` re-asserts last real nightly failure; documented ccache flavor keys +
  cleanup workflow; top-level `contents: read`, write only on release job.
- **Determinism:** gate passes; all engine RNG caller-seeded `mt19937`; Zobrist keys
  fixed-constant; 11/13 `determinism-ok` markers fully justified (see DET-7); `MockTimeProvider`
  used properly, incl. `test_backtracking_solver.cpp:170-193` proving the injected clock is
  consulted.
- **Observable re-entrancy** (`src/core/observable.h:190-268`): reentrant set/update queued and
  coalesced, snapshot iteration, RAII notify guard, bounded drain. ViewModel layer has zero Qt
  includes (but see UI-4). Undo/redo capture-replay records peer-note deltas rather than
  re-deriving them.

---

## 2. Cluster A — Save/restore (the weakest subsystem; fix before 1.0.0)

### SAVE-1 — HIGH — OOB memory write from unvalidated move positions in save files
- `src/core/save_manager_serialization.cpp:412-414` reads `move.position.row/col` as raw ints
  into `Position` (`size_t` fields — negative YAML wraps to huge index; large positives pass).
- `restoreGameState` copies history verbatim (`src/view_model/game_view_model.cpp:265`);
  undo/redo call `GameState::setValue`/`setNotes` (`src/core/game_state.cpp:163-166,207-210`)
  which — unlike `addNote`/`toggleNote` — have **no bounds check** and index `BoardData`'s raw span.
- Scenario: open corrupt/hand-crafted save (import is a supported feature), press Undo →
  OOB write into `values_`/`notes_data_`.
- Fix: range-check row/col/value/type during deserialization; reject with `InvalidSaveData`.

### SAVE-2 — HIGH — Elapsed time lost on every load/resume; re-save destroys the stored value
- Found independently by two audits. `restoreGameState`
  (`src/view_model/game_view_model.cpp:197-295`) never applies `saved_game.elapsed_time`;
  `GameState` has **no elapsed-time setter** (`src/core/game_state.h:210`; only `resetTimer()`);
  `startTimer()` restarts from now (`game_state.cpp:44-48`).
- Resume shows 00:00:00; next auto-save overwrites `elapsed_time` with the small new value —
  **irreversible data loss**; best-time stats skewed for completed restored games. The Load
  dialog *displays* the stored time (`main_window.cpp:1312`), so users can watch it get destroyed.
- `mistakes`/`hints_used`/`moves_made` are persisted as zeros (`saveCurrentGame` never populates
  them) and never restored — write-only fields.
- Fix: add `GameState::setElapsedTime()`, apply in `restoreGameState`, populate counters on save.
  Regression test: save → restore → `getFormattedTime() != "00:00:00"`.

### SAVE-3 — HIGH — Restored games never start a statistics session (hints stuck at 0, completions dropped)
- `restoreGameState` never calls `startGameSession()` (only `startNewGame`, `resetGame`, two
  import paths do — `game_view_model_hints.cpp:291,417`). With `current_game_session_ == 0`:
  `getHintCount()` returns 0 (`game_view_model_hints.cpp:516-519`) → toolbar badge "0",
  Help→Get Hint inert (`main_window.cpp:445`); `recordMove` skipped
  (`game_view_model_undo.cpp:208`); completion's `endGameSession(true)` is a no-op.
- **This is the default launch path** whenever an auto-save exists (`src/main.cpp:245-258`):
  play → quit → relaunch → resume ⇒ no hints for that puzzle, completion never counted.
- Fix: start (or resume) a session in `restoreGameState`, or decouple hint accounting from the
  stats session. Coordinate with SAVE-2 counters.

### SAVE-4 — MED — Undo history restored misaligned; undo-after-load corrupts board state
- `Move::previous_value`/`previous_notes`/`peer_note_delta` are not serialized
  (`save_manager_serialization.cpp:136-146`); `restoreGameState` ignores saved
  `current_move_index`, pinning the cursor to `size-1` (`game_view_model.cpp:266`).
- Scenario: undo N moves → save → load → cursor claims all moves applied → Undo "reverts"
  moves not on the board using default `previous_value=0`/empty notes → wipes user notes,
  clears legitimately filled cells.
- Fix: serialize the previous-state fields (save-format change → CHANGELOG) **or** truncate
  history at the saved cursor on save.

### SAVE-5 — MED — `listSaves()` freezes the UI: Argon2id-MODERATE per file, parsed twice
- Manual saves are encrypted (`game_view_model.cpp:334` sets `encrypt=true`); `deriveKey` uses
  `OPSLIMIT_MODERATE`/`MEMLIMIT_MODERATE` ≈ 0.7 s + 256 MiB per call
  (`src/core/encryption_manager.cpp:217-218`). `listSaves()` parses each file twice —
  `isValidSaveFile()` → `deserializeFromYaml`, then again in the loop
  (`src/core/save_manager.cpp:258-262`). `getSaveInfo` == `loadGame`
  (`save_manager.cpp:279-281`) despite its "quick info" doc.
- A dozen saves ≈ 15–20 s hard freeze in a single-threaded Qt app every time the save/load
  dialog opens.
- Fix: parse once (drop the pre-check); consider `OPSLIMIT_INTERACTIVE` or an unencrypted
  metadata sidecar.

### SAVE-6 — MED — Machine-bound encryption strands saves; rename silently decrypts
- Key derives from hostname + username + machine-id (`encryption_manager.cpp:284-308`):
  hostname change / OS reinstall ⇒ all encrypted manual saves permanently `DecryptionFailed`,
  no user-facing recovery hint.
- Inconsistent: `renameSave`/`importSave` re-save with default `SaveSettings` (`encrypt=false`,
  `src/core/i_save_manager.h:111`) — renaming an encrypted save rewrites it **plaintext**.
  Header comment "encrypt save file (future feature)" is stale — it's live.
- Decision needed before 1.0.0 (save-format/packager-visible → CHANGELOG): document
  machine-binding + make rename/import consistent, or drop encryption for manual saves.

### SAVE-7 — MED — Hostile note/board values: UB shift, no range validation
- `deserializeNotes` feeds raw ints into `CellNotes::add` → `valueToBit` → `1 << value`
  (`src/core/board_serializer.cpp:73`, `include/core/constants.h:57-59`) — UB for value ≥ 31 or
  negative; 10–15 set phantom mask bits. `deserializeIntBoard` (`board_serializer.cpp:36`)
  accepts any int; out-of-range values flow into `GameState`.
- Fix: validate 0–9 / 1–9 at the deserialization boundary. **Single choke point for
  SAVE-1/-4/-7:** a `validate()` pass in `deserializeFromYaml` + a corrupt-save test corpus.

### SAVE-8 — LOW — Save-id path traversal
- `getSavePath` concatenates the id unsanitized (`save_manager.cpp:444-446`); `save_id` comes
  from file *content* via `listSaves`, so a planted `save_id: ../../foo` redirects
  `loadGame`/`deleteSave`/`saveGame` outside the saves dir. Low exploitability (only `loadGame`
  UI-reachable). Fix: reject ids that aren't exactly 16 hex chars.

---

## 3. Cluster B — Statistics

### STAT-1 — HIGH — Master games silently excluded from all statistics
- `isValidDifficulty` accepts only `0..3` (`src/core/statistics_manager.cpp:669-673`; comment
  predates Master=4, cf. `src/core/i_puzzle_generator.h:41`). `startGame(Master,…)` →
  `InvalidDifficulty`, swallowed by `GameViewModel::startGameSession`
  (`game_view_model_state.cpp:352`) ⇒ no session, no aggregates, no best time for any Master game.
- Same stale-bound family as fixed #11/BL-8. Fix:
  `diff_val < static_cast<int>(DIFFICULTY_COUNT)`. **Grep the codebase for other `<= 3` /
  `< 4` difficulty bounds while at it.**

### STAT-2 — HIGH — OOB indexing from unvalidated difficulty in `game_sessions.yaml`
- `src/core/statistics_serializer.cpp:257` does `static_cast<Difficulty>(as<int>())` with no
  range check; `updateAggregateStats` (`statistics_manager.cpp:565+`) indexes seven
  `std::array<…,5>` members with it → OOB **writes** during `recalculateAggregateStats()`
  (runs at startup when `aggregate_stats.yaml` is missing). OOB read at
  `statistics_serializer.cpp:443` (`csv_difficulty_names[difficulty]`).
- Fix: validate at deserialization; reject the session (archive path already exists).

### STAT-3 — MED — `flushSessions()` drops sessions on write failure
- `statistics_manager.cpp:780-800`: encrypted mode — `atomicWriteFile` failure only warns, then
  falls through to `pending_sessions_.clear()`. Plain mode `std::ignore`s each
  `saveGameSession` result before clearing.
- Fix: clear pending only on confirmed write.

---

## 4. Cluster C — Release / CI / packaging (1.0.0 tag risk)

### REL-1 — HIGH — Windows (a shipped platform) has zero per-PR gating
- `ci.yml:468` — MSVC `/W4 /WX` build+test is `workflow_dispatch`-only; macOS likewise
  (`ci.yml:584`). Any MSVC-fatal construct merged since the last manual dispatch surfaces
  first in the tag-triggered NSIS job, i.e. during the release.
- Fix: per-PR Windows job path-filtered to `src/**, tests/**, CMakeLists.txt, conanfile.py`
  (or minimum weekly schedule). Interim: full `workflow_dispatch` of CI now and immediately
  before tagging.

### REL-2 — HIGH — Packaging pipelines never exercised between releases
- `packaging.yml:3-7` triggers only on `v*` tags + dispatch. Flatpak (KDE 6.10 runtime),
  AppImage (linuxdeploy `continuous`), NSIS all rot silently; release job hard-depends on all
  three (`needs: [flatpak, appimage, installer]`, `packaging.yml:294`).
- Fix: weekly scheduled dry-run (no release job) so tag day is a rerun, not a first run.

### REL-3 — HIGH — Version triplicated; no tag↔version guard; metainfo date stale
- Declared single source `CMakeLists.txt:13` (1.0.0), but `conanfile.py:7` hardcodes
  `version = "1.0.0"` and `resources/linux/io.github.darkstar79.Sudoku.metainfo.xml:95`
  hardcodes `<release version="1.0.0" date="2026-03-20">` — date already stale (tag not cut).
  `test_version.cpp` guards only the header.
- Artifact naming split: AppImage from git ref (`packaging.yml:18,44`), NSIS from
  `PROJECT_VERSION` (`CMakeLists.txt:387`) → a mismatched tag ships two version numbers in one
  release. CONTRIBUTING "Cutting a release" omits conanfile + metainfo.
- Fix: release-guard step in `packaging.yml` asserting
  `github.ref_name == "v" + PROJECT_VERSION`, conanfile match, metainfo top `<release>` match +
  real date (~20 lines of shell). Extend CONTRIBUTING checklist.

### REL-4 — MED — No appstream/desktop-file validation anywhere
- No `appstreamcli validate` / `desktop-file-validate` in `.github/`, `scripts/`, `flatpak/`.
  Exactly the class of error that would catch REL-3's stale date; packager-visible across all
  four OBS namespaces; a Flathub submission would fail on it.
- Fix: add both as a step in an existing Linux job (seconds of runtime).

### REL-5 — MED — Supply chain: actions pinned to mutable tags; unverified binary fetches
- All actions tag-pinned incl. third-party `ilammy/msvc-dev-cmd@v1`,
  `jurplel/install-qt-action@v4`, `softprops/action-gh-release@v2` (the latter in the only
  `contents: write` job, `packaging.yml:305`). linuxdeploy fetched from `continuous`, no
  checksum (`packaging.yml:100-103`); PMD zip unverified (`nightly.yml:412`).
- Fix: SHA-pin + Dependabot `github-actions`; pin linuxdeploy to a dated release + sha256.

### REL-6 — MED — CI toolchain unpinned in every job
- `pip3 install conan` / `gcovr` fetch latest (`ci.yml:93,161,262,371` + nightly/packaging);
  `requirements.txt` pins unused by CI. A Conan 3.0 release breaks every workflow at once.
  (clang-format IS pinned to 22.1.5 — `ci.yml:50` — follow that precedent.)

### REL-7 — MED — cppstd divergence across pipelines
- Linux CI passes `-s compiler.cppstd=gnu23`; AppImage omits it (`packaging.yml:86` → profile
  default gnu17); Windows CI/packaging omit except Catch2-only `cppstd=17` (`ci.yml:506`,
  absent `packaging.yml:160`); local `build_windows.ps1:163` uses 23. Repo already documented
  an ABI break from this class (Catch2 `string_view` StringMaker, `ci.yml:501-505`).
- Fix: pass `compiler.cppstd` uniformly; also declare a Qt minimum —
  `find_package(Qt6 REQUIRED …)` (`CMakeLists.txt:112`) declares none although
  `qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES …)` needs Qt ≥ 6.7 (REL-8 adjunct:
  shipped artifacts span Qt 6.8.3/KDE 6.10/ubuntu-24.04 archive Qt).

### REL-9 — MED — `cancel-in-progress: true` applies to `main` pushes
- `ci.yml:35-37`: two quick merges cancel the older commit's run — per-commit signal on `main`
  lost, weakening "every commit on main is green" + bisectability.
- Fix: `cancel-in-progress: ${{ github.ref != 'refs/heads/main' }}`.

### REL-10 — LOW — Misc build/CI hygiene
- Dead clang-tidy wiring: `CMakeLists.txt:187-191` sets `CXX_CLANG_TIDY` via
  `set_source_files_properties` — it's a **target** property; silent no-op. Delete or fix.
- `file(GLOB_RECURSE)` without `CONFIGURE_DEPENDS` (`CMakeLists.txt:148-161`, tests same) —
  known local pitfall (the `touch tests/unit/CMakeLists.txt` workaround).
- Mixed `upload-artifact` majors (`@v7` at `ci.yml:218`, `@v4` at `:564,572,636`).
- ~6× copy-pasted Conan/ccache setup blocks (~40 lines each) across ci/nightly — extract a
  `setup-cpp-build` composite action like the existing `install-qt-deps`. Per-commit
  `-${{ github.sha }}` cache saves on `main` never cleaned (LRU only).
- Windows drift: CI pins Qt 6.8.3 (`ci.yml:481`), `build_windows.ps1:147` says 6.11.1;
  packaging `installer` builds `with_tests=False` and never tests the shipped configuration
  (acceptable only once REL-1 is fixed).

---

## 5. Cluster D — Test safety-net holes

### NET-1 — HIGH — Livelock regression sentinels never run anywhere
- `tests/unit/test_sudoku_solver_timeout.cpp:86,108` ("bails out on AI Escargot", "propagates
  deadline into backtracking fallback") tagged `[.integration]` — hidden; nothing in `.github/`
  or `scripts/` invokes `[timeout]`. The guards for the resolved AI-Escargot livelock
  (`5e27631`/`e4e4411`/`e21fa22`) are dormant.
- Fix: add a `"[timeout]"` invocation to nightly.yml (naming a tag selects hidden tests) or
  retag.

### NET-2 — HIGH — Last-resort audit + kraken mining not wired into any schedule
- `tests/unit/test_last_resort_audit.cpp:250` (`[.][lastresort][audit]`) and
  `test_kraken_fish_strategy.cpp:453` (`[.][kraken][mining]`) manual-only; `nightly.yml:92,103`
  runs only `[strategy]` + `[bucket_migration]`. The exact tier that produced #40/#59 has no
  standing automated run.

### NET-3 — HIGH — Correctness harness skips every backtracking puzzle
- `tests/unit/test_strategy_correctness.cpp:194-209`: `used_backtracking` → `continue` (:196)
  or falls outside the `else if (!used_backtracking)` guard (:203) — zero verification.
  `persistentCandidateReplay` runs only in the solver-*failed* branch (:180).
- Blind spot: a wrong elimination that stalls the chain but leaves the puzzle
  backtracking-solvable is invisible to the ground-truth gate — the exact bug class the
  harness exists for.
- Fix (cheap): run `persistentCandidateReplay` for `used_backtracking` puzzles too (replay
  halts at chain stall; no backtracking involved).

### NET-4 — MED — AVX-512 tests silently SKIP on all GitHub runners
- `tests/unit/test_simd_constraint_state.cpp:370-373` SKIPs without AVX-512; standard
  `ubuntu-24.04` runners are Zen 3 (no AVX-512) → the alignment-SIGSEGV regression test (the
  project's own bestiary class; cited as justification at `ci.yml:245`) has likely never
  executed in CI. SKIP is indistinguishable from pass in logs. `src/core/simd_avx512_ops.cpp`
  has no other test reference.
- Fix: CI summary check that warns/fails when all `[avx512]` tests SKIP; longer-term an
  on-demand run on AVX-512 hardware (maintainer's Zen 5 box qualifies).

### NET-5 — MED — Strategy chain hand-duplicated with no parity guard
- Found independently by two audits. The 55-entry chain in
  `tests/helpers/strategy_test_utils.h:309` mirrors `SudokuSolver::initializeStrategies()`
  (`src/core/sudoku_solver.cpp:106-161`); verified **currently identical in set and order**,
  but `strategies_` is private (`sudoku_solver.h:76`) and nothing enforces sync. A strategy
  added to the solver only silently escapes the persistent replay AND the last-resort audit.
- Fix: single factory in core consumed by both, or a test-visible accessor + parity test
  asserting `createStrategyChain()` matches in content and order.

### NET-6 — LOW — ctest false green + no-op tag filters
- `ctest -R "^test_"` matches only UI tests; without `-DSUDOKU_ENABLE_UI_TESTS=ON` it matches
  nothing and exits 0. Fix: `--no-tests=error` on every ctest invocation (`ci.yml:536,632`,
  docs).
- `~[slow]~[pathological]` filters (`scripts/coverage.sh:76`, ci sanitizers job,
  `nightly.yml:194`) are no-ops today (no `[slow]` tag exists; `[pathological]` already
  hidden) — the first `[slow]`-tagged test will silently vanish from coverage AND both
  sanitizer runs. Delete the filters or create the tags deliberately.

### NET-7 — LOW — Wall-clock latency assertions in default-run tests
- `test_solver_infinite_loop_reproduction.cpp:62-69` (+~9 sites), `test_puzzle_analyzer.cpp:199-201`,
  `test_backtracking_solver.cpp:191-193` assert `elapsed < N s` on real `steady_clock` — they
  run inside the per-PR ASan job where instrumentation eats the headroom. Most likely future
  "flakes" in the codebase. Fix: generous runaway bounds or move behind a nightly tag.

### NET-8 — LOW — Test coverage gaps (ranked by risk)
1. Backtracking-fallback puzzles in the soundness gate (= NET-3).
2. AVX-512 ops (= NET-4).
3. View rendering logic: `src/view/board_painter.cpp`, `board_render_data.cpp`,
   `training_number_pad.cpp` — zero test references; `.gcovr.cfg:26` excludes `src/view/.*` so
   the gap is invisible to metrics. `board_render_data` (conflicts/notes/highlights mapping) is
   testable offscreen.
4. Timeout/deadline propagation (= NET-1).
5. Real `QtClipboardProvider` (only the fake is exercised).
- Also: UI assertions are weak/tautological (`tests/ui/test_view_model_binding.cpp:67-70`
  `hint_count >= 0`; :112-113 non-empty; :73-76 non-null); UI tests drive unseeded
  `startNewGame()` (different puzzle each run — only safe because assertions are loose).
- Docs bug: CLAUDE.md mandates `isValidBoard()` as a fixture precondition, but no such symbol
  exists anywhere — real API is `GameValidator::validateBoard`. Fix the doc, and have strategy
  fixtures actually assert validity.
- PR-time soundness idea: reuse `tests/data/fixtures/{Hard,Expert,Master}` (currently
  kraken-mining-only) for a fast ~50-puzzle mini-correctness replay per PR, so soundness signal
  isn't nightly-only.

---

## 6. Cluster E — Solver core

### SOLVER-1 — MED — Registration order contradicts re-anchored SE ratings → systematic rating skew
- `src/core/sudoku_solver.cpp:106-161` still reflects pre-SE-re-anchor internal ratings while
  `solving_technique.h` was re-anchored (stories 0b.3/0b.4). Puzzle rating =
  `max(step.rating)` over the first-fire path (`puzzle_rater.cpp:66-73`), so inversions change
  ratings. Confirmed inversions: NakedSingle 2.3 before HiddenSingle 1.5 (:107-108);
  PointingPair 2.6/BoxLine 2.8 after NakedTriple 3.6/HiddenPair 3.4 (:110-114); Swordfish 3.8
  after XYWing 4.2 (:118-119); FinnedXWing/SashimiXWing 3.4 after UR/UniqueLoop 4.5 (:124-128);
  XCycles 6.6 before Jellyfish 5.2 (:133-134).
- Previously flagged (CODE_REVIEW_2026-05-25.md:307,559), now load-bearing since 0b.4 shipped.
- Fix: re-sort by `getTechniqueRating()`; add a fast unit test asserting non-decreasing
  rating across the chain; quantify skew with a fixed-seed corpus sweep. Rating-only → the
  `[strategy][correctness]` nightly CANNOT move on this; don't spend it.

### SOLVER-2 — MED — AvoidableRectangle unreachable in `solvePuzzle` → zero ground-truth coverage
- `solvePuzzleImpl` (:219) and `findAllApplicableSteps` (:370) never call
  `setGivensFromPuzzle`, so `hasGivensInfo()` is false and AR bails
  (`avoidable_rectangle_strategy.h:41`). AR fires only on the hint path
  (`findNextStep(board, original_puzzle)`), which has **no backtracking net behind it** — and
  it's below the 7.0 cutoff of the last-resort audit. Verification blind spot identical in
  shape to pre-#59 Kraken.
- Strategy logic itself read fully: live branches sound (the `v3 == val_a` row-path branch
  :199-204 is dead on valid boards, not unsound).
- Fix options: (a) call `candidates.setGivensFromPuzzle(board)` at `solvePuzzleImpl` entry
  (entry cells are effective givens — sound; this IS solver-logic change → full nightly run);
  (b) add per-state `findStep` probing with givens set to the audit harness.

### SOLVER-3 — MED (low likelihood) — BUG+1 parity test is a relaxation of the BUG theorem
- `bug_strategy.h:52-79,192-217` uses XOR parity — cannot distinguish "exactly 2 per unit"
  (the theorem's precondition) from 4 or 6. In-code note (:39-41): N>1 produced 18 false
  positives across 15k puzzles before clamping to N=1; the parity relaxation is a plausible
  root cause and still exists for N=1, empirically unobserved.
- Fix: count occurrences (uint8), require exactly 0 or 2 per digit per unit — strictly
  tightens. Then: deterministic `[regression]` fixture with a 4-occurrence even-parity
  pseudo-BUG state asserting no fire; nightly run (solver-logic change).

### SOLVER-4 — LOW — `ValueSelectionStrategy::MostConstrained` not implemented in BacktrackingSolver
- `solveRecursive` picks the first empty cell (`backtracking_solver.cpp:62-70,128`); comments
  (:100-103) and enum doc (`backtracking_solver.h:39`) claim MCV. Only `SolutionCounter` has
  real MCV (`solution_counter.cpp:234-264` — mirror it). Not a correctness bug; the fallback is
  the historical livelock hot path, so first-empty leans on the deadline.
- Fix: implement MCV cell selection or correct the docs.

### SOLVER-5 — LOW — Edge-case hygiene in the solver loop
- Harness replica `MAX_ITERATIONS = 300` (`test_strategy_correctness.cpp:102`) vs solver's 200
  (`sudoku_solver.cpp:223`) — attribution can diverge near the cap.
- `solvePuzzleImpl:214` returns success for a complete-but-**invalid** input board
  (`isComplete` checks emptiness only, `sudoku_solver.cpp:349-351`).

### SOLVER-6 — LOW — `PuzzleRater::ratePuzzle` error switch falls through
- `puzzle_rater.cpp:41-51`: no default, no return after the switch; a future `SolverError`
  enumerator falls into `result.value()` on an errored expected → `bad_expected_access` at
  runtime. Add `std::unreachable()` or a default return.

---

## 7. Cluster F — UI layer (view / view_model)

### UI-1 — HIGH — = SAVE-3 (restore never starts a session). Listed here for the view-side symptoms: hint badge "0", inert hint menu, "No coaching hints remaining".

### UI-2 — HIGH — = SAVE-2 (elapsed time). View-side: Load dialog shows the stored time that restore then discards.

### UI-3 — MED — Runtime language switch hides experimental menu entries + leaks menus
- Settings observer (`main_window.cpp:748-767`) applies `experimental_*` visibility to current
  `QAction`s; `applyLocale → installTranslator` posts async `LanguageChange`;
  `retranslateUi()` (`main_window.cpp:1687-1693`) does `menuBar()->clear() + setupMenuBar()`,
  recreating `training_mode_action_`/`coaching_hint_action_` hidden
  (`main_window.cpp:370,456`); nothing re-applies settings.
- Scenario: enable Training Mode, switch language → "Training Mode" and "Get Coaching Hint"
  vanish until some other settings change fires the observer. Side effect: `clear()` removes
  actions but not `addMenu`'d menus → small per-switch leak.
- Fix: re-apply settings-driven visibility inside `retranslateUi()` (4 lines); reuse menus
  instead of rebuilding.

### UI-4 — MED — Qt include inside core: `src/core/i18n_helpers.h:22` includes `<QCoreApplication>`
- The single violation of "core is Qt-free"; transitively drags QtCore into all five ViewModel
  TUs despite the clean `src/view_model` grep. Blocks the "Model/Core Qt-free" claim and the
  post-1.0.0 engine extraction.
- Fix: move to a presentation-layer i18n adapter (infrastructure/), before the extraction locks
  it in.

### UI-5 — MED — Dead code that is regression-shaped
- `checkSolution()` (`game_view_model_undo.cpp:125-178`) called from nowhere (decl
  `game_view_model.h:313`); unlike the live `checkGameCompletion`
  (`game_view_model_state.cpp:368-386`) it does NOT pause the timer, does NOT clear the
  auto-save (re-introducing the story-6.9 stale-auto-save bug if ever wired up), and
  auto-starts a new game. Also dead: `updateTimeDisplay()` decl (`game_view_model.h:389`),
  `MainWindow::event()` pure passthrough (`main_window.cpp:884-886`).
- Fix: delete all three.

### UI-6 — MED — Pause / no-game / complete states conflated
- `isGamePaused()` ≡ `!isTimerRunning()` (`game_view_model_state.cpp:186-188`) — true at app
  start with no game. `getFormattedTime()` returns "00:00:00" for paused-but-incomplete games
  (`game_view_model_state.cpp:332-340`); `updateStatusBar` shows "Ready" + blank timer for a
  paused game (`main_window.cpp:1014-1017`). Comment at `main_window.cpp:1065-1067` admits the
  trap.
- Fix: `enum class GamePhase { NoGame, Running, Paused, Complete }` on UIState, derived once in
  `updateUIState`; paused branch in `getFormattedTime()`.

### UI-7 — MED — Qt's own dialog buttons stay English in a localized UI
- `applyLocale` loads only `sudoku_<locale>.qm` (`main_window.cpp:856`); no `qtbase_<locale>.qm`
  translator. All `QMessageBox` standard buttons (Yes/No/Cancel/Save) render untranslated.
- Fix: install a `QTranslator` for `qtbase_<locale>` from `QLibraryInfo::TranslationsPath`.

### UI-8 — LOW/MED — Time-limit force-close absorbed by open modal dialogs
- `clock_timer_` fires inside `dialog->exec()`; if the limit expires under a modal,
  `forceCloseForLimit()` (`main_window.cpp:822-834`) hides the main window but the app keeps
  running under the modal until dismissed — enforcement degrades silently. Also
  `setPlayLimitController` double-connects the tick on a second call (`main_window.cpp:777`).
- Fix: on limit close, `reject()` open modal children first; guard against double-connect.

### UI-9 — LOW — `WA_DeleteOnClose` + `exec()` on statistics (`main_window.cpp:1375,1511`) and
settings (`:1734,2006`) dialogs — documented-fragile pattern; stack-allocate + `exec()` (as
`showSaveDialog` correctly does) or use `open()`.

### UI-10 — LOW — Observable/CompositeObserver lifetime one refactor from UB
- Tokens capture raw `Observable*` (`observable.h:133`). Today safe only because `~MainWindow`
  explicitly calls `observer_.unsubscribeAll()` (`main_window.cpp:130`) while members alive —
  but `main()` clears the DIContainer before `main_window` destruction (`main.cpp:275`), and
  `observer_` (`main_window.h:96`) is destroyed AFTER the shared_ptrs in member order. Remove
  the explicit call and `~CompositeObserver` dereferences destroyed Observables.
- Also: `setViewModel()` calls `observer_.unsubscribeAll()` (`main_window.cpp:624`) — would
  kill settings/training subscriptions on any re-bind; `getSubscriptionCount()` counts expired
  weak_ptrs (`observable.h:151`).
- Fix: declare `observer_` last member; narrow `setViewModel`'s unsubscribe.

### UI-11 — LOW — Hint-limit message hardcodes "(0/10 used)"
- `game_view_model_hints.cpp:136` — while `max_hints` is configurable 1–50
  (`main_window.cpp:1742-1745`); badge initialized to hardcoded "10" (`main_window.cpp:535`).

### UI-12 — LOW — Accessibility absent
- No `setAccessibleName`/`QAccessible` under `src/view` (grep-verified); custom-painted board
  has no accessible interface; mistakes are color-only (`sudoku_board_widget.cpp:373`); toasts
  visual-only; Exit hardcodes `Alt+F4` (`main_window.cpp:404`) instead of
  `QKeySequence::Quit` (wrong on macOS).

---

## 8. Cluster G — Determinism & test hygiene

### DET-1 — MED — `listSaves()` unstable sort, 1-second key granularity
- `std::ranges::sort` (unstable) keyed only on `last_modified` (`save_manager.cpp:268-269`);
  `last_modified` round-trips YAML at 1 s granularity (`save_manager_serialization.cpp:297`);
  input order is `directory_iterator` (OS-dependent, `save_manager.cpp:252`). Latent
  Load-dialog / `saves[0]` ordering flake.
- Fix: tie-break `(last_modified desc, save_id asc)`. `stable_sort` alone is NOT sufficient
  (input order itself nondeterministic).

### DET-2 — MED — Unit test touches the real `~/.local/share/sudoku/saves`
- `tests/unit/test_save_manager_deserialization.cpp:484-493`: `SaveManager("")` resolves to the
  real default dir (`save_manager.cpp:52-55`); constructor **creates** it; `listSaves()` reads
  real user saves — result depends on machine state.
- Fix: redirect `XDG_DATA_HOME`/`HOME` to a `TempTestDir` (env helpers exist in
  `tests/helpers/test_utils.cpp`).

### DET-3 — MED — cwd-relative integration fixtures (+ one fixed shared name)
- `test_save_manager_integration.cpp:32`, `test_statistics_manager_integration.cpp:35`,
  `test_save_origin_integration.cpp:34`, `test_save_rating_model_compat_integration.cpp:40-41`
  use `./test_*_<ts>` in cwd (explains stray `test_saves_undo_pencil/` etc. in the repo root);
  `test_save_manager_integration.cpp:400-411` uses the FIXED name
  `./non_existent_dir_12345/saves` — deterministic collision between concurrent runs.
- Fix: switch all to `sudoku::test::TempTestDir` (as `test_game_view_model_integration.cpp:424`
  already does).

### DET-4 — LOW/MED — `TempTestDir` uniqueness is a bare clock tick
- `tests/helpers/test_utils.cpp:33-35`: name = `system_clock` count only; same-tick or
  parallel-process collision → second dtor `remove_all`s the first's live dir. Same pattern
  `tests/ui/test_fixture.h:45-46`, `test_session_timer.cpp:54`, `test_experimental_gating.cpp:58`.
- Fix: append PID + `static std::atomic<int>` counter in `TempTestDir` (single choke point).

### DET-5 — LOW — Observable callback order = `unordered_map` iteration order
- `observable.h:262-267` (container :176): multiple subscribers fire in implementation-defined
  order; `CallbackId` is monotonic (:129) — subscription order available but discarded.
- Fix: `std::map<CallbackId, CallbackFn>` (small N documented at :247-249).

### DET-6 — LOW — Training-stats YAML written in `unordered_map` order
- `training_statistics_serializer.cpp:41` — persisted technique order nondeterministic;
  blocks golden-file tests, churns saved files. Fix: emit sorted by enum value.

### DET-7 — Marker review (13 `determinism-ok` markers): 11 justified. Two follow-ups:
- `game_view_model.cpp:312` (`created_time = system_clock::now()`) — weakly justified;
  GameViewModel holds no `ITimeProvider` while SaveManager routes `last_modified` through one
  (`save_manager.cpp:81,291,340`). Fix: inject `ITimeProvider` into GameViewModel, drop marker.
- `solution_counter.cpp:87,138,185,389` — wall-clock timeout returns 1 ("assume unique",
  :122-124) ⇒ with `ensure_unique=true` the same seed can yield a different puzzle on a slower
  machine (H3 seed test sidesteps via `ensure_unique=false`). Tracked deferral from #24; durable
  fix = injected deadline or deterministic recursion budget.
- Also (non-marker): `GameState` elapsed time uses `systemNow()` not `steadyNow()`
  (`game_state.cpp:35-42`) — NTP/DST jump skews the in-game timer; interface already offers
  monotonic.

---

## 9. Cluster H — Code quality (no behavior change)

### Q-1 — Magic dimensions violating the shared-constants rule
- `src/core/sudoku_solver.cpp:349-351` (`row < 9`, `col < 9`, `== 0`);
  `src/core/candidate_grid.h:241,260,286` (`== 0` vs `EMPTY_CELL`);
  `src/core/puzzle_analyzer.cpp:37-38` (re-defines `kCellsPerBoard=81`, `kRowSize=9`);
  `src/core/puzzle_generator.cpp:228` (`cell < 0 || cell > 9`);
  `src/view_model/game_view_model_hints.cpp:105-113` (`row < 9`, `col < 9`, `val <= 9`);
  `game_view_model_board.cpp:40,96` (`number < 1 || number > 9` — while `:241` correctly uses
  `MIN_VALUE`/`MAX_VALUE`).

### Q-2 — Duplication hotspots
- Row/col/box triplication is the dominant pattern: `w_wing_strategy.h::tryStrongLink` (3×~15
  near-identical lines); `CandidateGrid::findSingleCandidateInRow/Col/Box`
  (`candidate_grid.h:235-297`) — note `ConstraintState` already has the `Region`-templated
  consolidation (`constraint_state.h:172-174`); CandidateGrid regressed to copy-paste. A
  `forEachUnit` helper in `StrategyBase` would delete hundreds of lines.
- Fish family: basic/finned/sashimi × wing/swordfish/jellyfish = 9 files × ~230-260
  structurally identical lines despite `fish_helpers.h`; `// CPD-OFF` markers acknowledge the
  debt. Size-parameterized implementation would collapse them.
- `archiveCorruptSave` ≈ `archiveUnreadableSessions` (`save_manager.cpp:460` /
  `statistics_manager.cpp:706`); `tryOrdering` lambda duplicated verbatim in
  `removeCluesToCreatePuzzle` and `runRecompletionPhase`; difficulty-name arrays in 3 files.

### Q-3 — Complexity hotspots (size × NOLINT density)
- `unique_rectangle_strategy.h` (693 lines, Types 1–6 in one class — extract per-type before
  touching under pressure), `grouped_x_cycles_strategy.h` (516),
  `grouped_nice_loop_strategy.h` (489), `mutant_fish_strategy.h` (484).
- `MainWindow` 2,009 lines with settings/statistics/save/load dialogs (~600 lines) built
  inline — extract dialogs. `GameViewModel` ~2,500 lines across 5 TUs, ~60 public ops —
  hint/coaching (~800 lines + `CoachingState`) is a coherent `CoachingViewModel` extraction.
- Header-only strategy bloat: 15.5k lines included into `sudoku_solver.cpp` (55 includes) and
  again per strategy-test TU; move `findStep` bodies into the compiled lib at engine-extraction
  time.

### Q-4 — API surface issues
- `[[nodiscard]]` missing on interface methods while overrides have it: `ISaveManager::saveGame`,
  `autoSave`, `loadAutoSave`, `cleanupOldSaves` (`i_save_manager.h:139,150,154,212`); a few on
  `IStatisticsManager`. Calls through the interface pointer (the normal case) get no protection.
- Write-only/dead API: `SavedGame.moves_made/hints_used/mistakes` (see SAVE-2);
  `puzzle_seed` persisted but VM passes 0 to `startGame` although the generator records real
  seeds; `PuzzleGenerator::removeCluesToTarget` dead (`puzzle_generator.cpp:469-478`);
  `Move::is_note` duplicates `move_type`.
- `SettingsManager` writes non-atomically (`settings_manager.cpp:345-350`) — crash truncates
  `settings.yaml`, silently resetting parental play limits (policy bypass). Use
  `atomicWriteFile` like everything else.
- `ToastWidget::show(QString,int)` shadows non-virtual `QWidget::show()`; toast never
  repositioned on parent resize. `onAutoSave` log claims "30s interval" regardless of config
  (`main_window.cpp:619`). `TrainingWidget::rebuildPages()` on LanguageChange resets to page 0
  without re-syncing VM phase (`training_widget.cpp:95-136`). Mixed include styles
  (`"../core/x.h"` vs `"core/x.h"`) e.g. `game_view_model.h:19-31`.

---

## 10. Priority roadmap

**P0 — before the 1.0.0 tag (bugs users hit + tag-day safety):**
1. SAVE-1/-4/-7 via one `validate()` choke point in `deserializeFromYaml` + corrupt-save corpus.
2. SAVE-2 + SAVE-3 (elapsed time setter, session on restore, populate counters) — default
   launch path.
3. STAT-1 + STAT-2 (`DIFFICULTY_COUNT` bounds; grep for other stale `<= 3`).
4. REL-3 release-guard step + CONTRIBUTING checklist update; REL-4 appstream/desktop validate.
5. REL-1/REL-2 interim: one full `workflow_dispatch` of CI (Windows+macOS) and packaging NOW,
   repeat immediately before tagging.
6. SAVE-6 decision (encryption story) — save-format/packager-visible.

**P1 — cheap, high leverage (before or right after tag):**
7. NET-1/NET-2: one line each in nightly.yml. NET-6: `--no-tests=error`, delete no-op filters.
8. REL-6 pin conan/gcovr; REL-9 main-branch cancel guard; UI-3 retranslate fix; UI-5 delete
   dead code; SOLVER-6 unreachable default.
9. NET-3 backtracking-puzzle replay; NET-5 chain parity test.
10. DET-1..DET-4 (sort tie-break, test-dir hygiene, TempTestDir pid+counter).

**P2 — structural (post-tag):**
11. REL-1 per-PR Windows job; REL-2 weekly packaging dry-run; REL-5 SHA-pinning + Dependabot;
    REL-7 uniform cppstd + Qt minimum; REL-10 composite action.
12. SOLVER-1 registration re-sort + monotonicity test + corpus skew sweep; SOLVER-2 AR
    reachability (nightly run required if solvePuzzleImpl gains givens); SOLVER-3 BUG exact
    count; SOLVER-4 MCV or doc fix.
13. UI-4 move i18n out of core (BEFORE engine extraction); UI-6 GamePhase enum; UI-7 qtbase
    translator; SAVE-5 single-parse listSaves; UI-8/9/10 lifetime hardening.
14. NET-4 AVX-512 visibility; NET-8 board_render_data tests + PR-time mini-replay from fixtures.
15. UI-12 accessibility pass; Q-2/Q-3 dedup + extraction (fish family, forEachUnit, dialogs,
    CoachingViewModel).

**Doc fixes:** CLAUDE.md `isValidBoard()` → `GameValidator::validateBoard`; stale "future
feature" comment on save encryption; BacktrackingSolver MCV comments (if SOLVER-4 resolved as
doc-fix).
