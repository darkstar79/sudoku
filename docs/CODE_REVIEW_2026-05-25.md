# Code Review — 2026-05-25 — Whole Project

**Branch:** `main` @ `8f85e63` (HEAD at time of review).
**Scope:** Whole project, `src/` (~39,232 LOC across 169 files). Tests excluded by scope.
**Method:** 14 parallel adversarial-review subagents (one per chunk), each applying Blind-Hunter + Edge-Case-Hunter perspectives. Subagents were briefed with `_bmad-output/project-context.md` as ground truth (language rules, MVVM contracts, Qt6 idioms, SIMD discipline, anti-pattern bestiary).
**Result:** 145 findings — **15 BLOCKER, 33 HIGH, 49 MEDIUM, 48 LOW** (counts approximate; some entries straddle severity).

This file is the **durable record**. Actionable items have been promoted to GitHub issues — see [CODE_REVIEW_2026-05-25_ISSUES.md](CODE_REVIEW_2026-05-25_ISSUES.md) for the issue-draft list.

---

## Table of contents

1. [Methodology](#1-methodology)
2. [Executive summary](#2-executive-summary)
3. [Cross-cutting themes](#3-cross-cutting-themes)
4. [BLOCKER findings (full detail)](#4-blocker-findings)
5. [HIGH findings (condensed)](#5-high-findings)
6. [MEDIUM findings (condensed)](#6-medium-findings)
7. [LOW findings (condensed)](#7-low-findings)
8. [Per-chunk summary table](#8-per-chunk-summary-table)
9. [Triage / action plan](#9-triage--action-plan)
10. [Coverage & caveats](#10-coverage--caveats)
11. [Appendices — raw per-chunk reports](#11-appendices)

---

## 1. Methodology

### Chunking
The codebase is ~13× over the skill's per-review threshold (~3,000 LOC). It was split into 14 chunks ordered by risk (solver internals first, per the MEMORY.md bestiary):

| # | Chunk | Files | ~LOC |
|---|---|---|---|
| 1 | Solver spine | `sudoku_solver`, `backtracking_solver`, `candidate_grid`, `constraint_state`, `solution_counter`, `solve_step`, `strategy_base`, related interfaces | ~3,030 |
| 2 | SIMD + CPU dispatch | `cpu_features`, `simd_constraint_state`, `simd_avx512_ops` | ~1,340 |
| 3 | Strategies — basics + simple fish | naked/hidden singles/pairs/triples/quads, pointing pair, BLR, XWing, Swordfish, Jellyfish, Skyscraper, TwoStringKite, helpers | ~2,980 |
| 4 | Strategies — finned/sashimi + wings | Finned/Sashimi XWing/Swordfish/Jellyfish, XY/XYZ/W/WXYZ/VWXYZ wings, XY-Chain, Remote Pairs | ~2,750 |
| 5 | Strategies — uniqueness + coloring | UR (1-6), HiddenUR, UniqueLoop, AvoidableRect, BUG, SimpleColoring, MultiColoring, 3DMedusa, EmptyRectangle | ~3,140 |
| 6 | Strategies — ALS + forcing chains | ALSxZ, ALSXYWing, ALSChain, DeathBlossom, SueDeCoq, ForcingChain (+ helpers), Unit/Region ForcingChain | ~2,460 |
| 7 | Strategies — loops + cycles | NiceLoop, ContinuousNiceLoop, GroupedNiceLoop, XCycles, GroupedXCycles | ~2,210 |
| 8 | Strategies — exotic | KrakenFish, JuniorExocet, FrankenFish, MutantFish | ~1,430 |
| 9 | Puzzle pipeline + i18n | generator, analyzer, rater, solving_technique, technique_descriptions, validator, board primitives, locale_utils, i18n_helpers, localized_explanations | ~4,000 |
| 10 | Persistence + statistics | save_manager*, encryption_manager, statistics_manager*, statistics_serializer, settings_manager, interfaces | ~4,150 |
| 11 | Training subsystem | training_answer_validator, training_exercise_generator, training_hints, training_learning_path, training_types, training_statistics_* | ~2,010 |
| 12 | Architecture spine | `main.cpp`, `di_container`, `observable`, `i_time_provider`, `i_clipboard_provider`, `model/game_state`, `infrastructure/` | ~1,460 |
| 13 | ViewModel layer | all of `src/view_model/` | ~3,650 |
| 14 | View layer | all of `src/view/` | ~4,550 |

### Subagent charter
Each subagent received:
- The exact file list with line counts.
- The chunk's bestiary-cousin warnings (e.g., chunk 5 was told about Diagonal Pretender and Cross-Component Painter; chunk 8 about Overlapping Basemen).
- A "what NOT to flag" list (style, comment density, header-only-vs-cpp on strategies, magic-number callouts on Sudoku constants 9/81, long-function callouts on solver/SIMD).
- A bounded output format: max 15 findings per chunk, ≤80 words per finding, severity-sorted.

### What the methodology surfaces well
- Real correctness bugs with reproducible code locations.
- Pattern-match against documented past bugs (bestiary) and adjacent surfaces.
- Cross-cutting themes that no single file would reveal.

### What it does not catch
- Performance regressions that need benchmarking (only flagged when structural).
- Multi-file architectural drift (each subagent saw only one chunk).
- Issues that require a running app (UI behaviour under specific user input sequences).
- Anything in `tests/` — tests were excluded by scope.

---

## 2. Executive summary

| Severity | Count | Where concentrated |
|---|---|---|
| BLOCKER | 15 | Exotic strategies (Kraken, Junior Exocet), persistence (Master save loss, zlib bomb), SIMD (BITALG, alignment), Observable re-entry, training stats deserializer, ViewModel state corruption, View use-after-free |
| HIGH | 33 | SIMD, ForcingChain coverage, Sue de Coq breadth, Puzzle generator determinism, Save integrity, Statistics math, Observable lifetime, MainWindow polling/locale |
| MEDIUM | 49 | Scattered across all layers |
| LOW | 48 | Mostly style / perf / minor edge cases, deferred for opportunistic cleanup |

**Top three themes:**
1. **Bestiary regressions waiting to happen** — `alignas(64)` + `loadu`, cache truncation symmetry, ForcingChain edge-case handling — patterns that previously bit the project re-appear in adjacent surfaces.
2. **Determinism contract violations** — wall-clock leaks in BacktrackingSolver, PuzzleRater bypassing the budget, GameState's default `SystemTimeProvider`, generator seed unreproducible.
3. **Lifetime / ownership hazards** across Observable, DI, and Qt parenting — multiple use-after-free and re-entrancy paths.

---

## 3. Cross-cutting themes

### 3.1. The bestiary keeps re-appearing on adjacent surfaces
- **Aligned Loader (BL-2)** has re-emerged in the AVX-512 SIMD layer — exactly the GCC swap-to-aligned pattern that caused prior SIGSEGVs.
- **Truncated Oracle**'s symmetric guard is missing on the scalar path (`solution_counter.cpp`), and the timeout-policy split between `countSolutions` and `countSolutionsWithTimeout` (1 vs `max_solutions`) is a new variant.
- **Conflicted Chorus** patterns exist in `propagateHiddenSingles`: a same-cell cross-unit conflict (row says X=3, col says X=5) isn't flagged as contradiction.
- **Disjoint Twins** disjointness checks for Sue de Coq are correctly applied (good).
- **Cross-Component Painter** is correctly fixed; but **non-bipartite 3D Medusa components** still get contradiction logic applied (HIGH).
- **Overlapping Basemen** correctly fixed in Franken Fish bases; but **Mutant Fish cover overlap** is the next move-of-the-bug (HIGH).
- **Diagonal Pretender** correctly fixed in Hidden UR.

**Implication:** the strategy correctness harness covers shipped bug fixes, but should grow structural unit tests for each bestiary class: "alignment + aligned-load mix", "cache write after early termination", "non-bipartite coloring contradiction", "cover-overlap fish", etc.

### 3.2. Determinism / ITimeProvider contract has multiple leaks
Project rule: "ITimeProvider is the only legal time source." Audit-trail of violations:
- BacktrackingSolver uses `steady_clock::now()` directly (HIGH).
- PuzzleRater bypasses budget overload of solver (HIGH).
- `GameState` default-constructs `SystemTimeProvider` (HIGH) — silent wall-clock leak in tests.
- Generator's `result.seed` doesn't store the actual seed used (HIGH).
- Training exercise generator has no seed interface (MEDIUM).
- `exportGameSessionsCsv` uses non-reentrant `localtime` (MEDIUM).
- Move history persists `steady_clock` time_points (MEDIUM) — meaningless after restart.

**Implication:** an audit pass with `grep -E 'steady_clock|system_clock|localtime|random_device' src/` plus a CI check would find the rest.

### 3.3. Observable + DI ownership has multiple footguns
- Re-entrant `set()` mid-notification: value swap + UAF (BL-11, BL-12).
- `CompositeObserver` clears ALL subscribers — load-bearing single-subscriber assumption nothing enforces (HIGH).
- `subscribe(ObserverPtr)` parameter is shared_ptr but storage is weak_ptr — silent expiry.
- DIContainer silent nullptr on unregistered + race on singleton init.
- `main_window.cpp:646` direct subscribe leaks past destruction (BL-15).

**Implication:** the Observable + DI infrastructure deserves a focused redesign sprint — these are foundational layers and the next 12 months of features will sit on top.

### 3.4. Persistence is the highest-data-loss-risk subsystem
- BL-8: Master saves un-loadable.
- BL-9: zlib bomb.
- BL-10: Training stats truncated.
- HIGH: save write not atomic.
- HIGH: corrupt-sessions overwrite destroys recovery.
- HIGH: `recalculateAggregateStats` double-counts.
- MEDIUM: average-time math wrong on import; integer overflows on long-lived stats; move timestamps meaningless.

**Implication:** before any 1.0 tag, persistence needs a full pass: fuzz test on save round-trip, explicit format-version migration test for Master, catastrophic-failure recovery path (never overwrite an unreadable file).

### 3.5. View has classic Qt traps that pass `-Werror`
- Polling instead of subscribing (clock timer; MEDIUM).
- View branching on VM state instead of dispatching commands (MEDIUM).
- View calling VM mutators directly outside the `GameCommand` enum (MEDIUM).
- `executeCommand`/`canExecuteCommand` aspirational but lying (MEDIUM × 2).

**Implication:** either complete the `GameCommand` pipeline or shrink the enum to the supported surface; document the intent.

---

## 4. BLOCKER findings

### BL-1. AVX-512 `_mm512_popcnt_epi16` without BITALG gate → SIGILL
- **File:** `src/core/cpu_features.h:58-61`, `src/core/simd_avx512_ops.cpp:45`
- **Issue:** `hasAvx512()` only checks F + BW. `findMRVCell` issues `_mm512_popcnt_epi16` which requires AVX-512 BITALG (Zen 5+ / Ice Lake-SP+). CPUID(7,0).ECX bit 12 is never read.
- **Impact:** On Skylake-X / Cascade Lake / Zen 4, AVX-512 path is selected and `#UD`-faults — **SIGILL crash on real hardware.**
- **Fix:** Add `has_avx512_bitalg = (ecx & (1U<<12)) != 0` from CPUID(7,0).ECX; require it in `hasAvx512()`.

### BL-2. `alignas(64)` struct + `_mm512_loadu_si512` ("Aligned Loader" bestiary cousin)
- **File:** `src/core/simd_avx512_ops.cpp:42, 82` on a 64-byte-aligned member array of a 64-byte-aligned `SIMDConstraintState`.
- **Issue:** Declared aligned + unaligned-load intrinsic is exactly the pattern GCC swaps to `vmovdqa64` per MEMORY.md.
- **Impact:** Intermittent SIGSEGV in Release, never Debug.
- **Fix:** Either drop `alignas(64)` from the struct (and rely on `loadu`) OR switch to `_mm512_load_si512` and assert alignment at every entry point.

### BL-3. Kraken Fish: primaries/secondaries axis confused
- **File:** `src/core/strategies/kraken_fish_strategy.h:243-249`
- **Issue:** Outer loop iterates `secondary` over `[0,9)`, but the skip uses `std::ranges::contains(base_primaries, secondary)` — wrong axis. Targets in actual base rows are wrongly considered; unrelated cover lines wrongly skipped.
- **Impact:** Invalid Kraken eliminations and missed ones simultaneously.
- **Fix:** Test `getRow(row)`/`getCol(col)` against `base_primaries` after computing `row`/`col`, not the loop axis.

### BL-4. Junior Exocet: missing S-cell / cover-house validation
- **File:** `src/core/strategies/junior_exocet_strategy.h:113-192`
- **Issue:** The full JE definition (S-cells in target columns/band acting as cover-houses for each base candidate) is not implemented — code only checks T1/T2 outside base band, different boxes, then emits eliminations.
- **Impact:** Unsound eliminations on most non-trivial inputs.
- **Fix:** Implement the JE cover-house validation per Champagne's definition, or restrict to verified cross-line constraints before eliminating.

### BL-5. Junior Exocet: T1 and T2 may share the same band
- **File:** `src/core/strategies/junior_exocet_strategy.h:174-183`
- **Issue:** Only requires different boxes, not different bands. A JE requires the two targets occupy the two non-base bands (one each).
- **Impact:** Pattern is not a JE; eliminations invalid.
- **Fix:** Require `(t1.row / BOX_SIZE) != (t2.row / BOX_SIZE)`, both differ from base band.

### BL-6. Junior Exocet: base candidate may have no covering target
- **File:** `src/core/strategies/junior_exocet_strategy.h:99-104`
- **Issue:** No check that every base candidate appears in at least one target's candidate mask.
- **Impact:** Invariant violated, eliminations unsound.
- **Fix:** Require `(t1_mask | t2_mask) & base_mask == base_mask`.

### BL-7. PuzzleAnalyzer crashes on null deps in default-constructed instances
- **File:** `src/core/puzzle_analyzer.cpp:116, 125, 145, 149`
- **Issue:** `validate()`/`checkUniqueness()`/`scoreDifficulty()` deref `validator_`/`counter_`/`solver_` without checks. Header advertises "each dependency optional for partial setups."
- **Impact:** Segfault instead of clean `expected` error on partial-setup callers.
- **Fix:** Precondition guard at top of each method returning `std::unexpected(...)`.

### BL-8. Save loader rejects all Master-difficulty saves
- **File:** `src/core/save_manager_serialization.cpp:52-53, 278-283`
- **Issue:** `MAX_DIFFICULTY = 3` ("Expert"), but `Difficulty::Master = 4` (`DIFFICULTY_COUNT == 5`).
- **Impact:** **Permanent data loss** — Master saves can never be reloaded; auto-save round-trip silently broken on Master.
- **Fix:** `MAX_DIFFICULTY = static_cast<int>(Difficulty::Master)`. Add regression test loading a Master save.

### BL-9. Zlib decompression bomb — no absolute max cap
- **File:** `src/core/save_manager_compression.cpp:74-91`
- **Issue:** Buffer grows up to `2^10 = 1024×` compressed input. A 10 MB blob can demand ~10 GB RAM before failing. Reachable via `importSave` and via plain-YAML sessions decoder.
- **Impact:** DoS via OOM on import or tampered sessions file → process death (single-threaded app).
- **Fix:** Absolute max size guard (e.g. 64 MB); abort with `CompressionError` when exceeded.

### BL-10. Training stats deserializer drops techniques > GroupedXCycles
- **File:** `src/core/training_statistics_serializer.cpp:74`
- **Issue:** Loop only iterates 0..41. Techniques 42-53 (SashimiXWing/Swordfish/Jellyfish, Unit/Region ForcingChain, MutantFish, KrakenFish, ALSChain, JuniorExocet, UniqueLoop, ContinuousNiceLoop, GroupedNiceLoop) are never name-matched.
- **Impact:** 12 of 54 techniques' stats vanish on reload — mastery progress for advanced techniques permanently lost.
- **Fix:** Iterate over `kAllTechniques` (or a static name→enum map covering all 54).

### BL-11. Observable callback re-entry can crash via dangling reference
- **File:** `src/core/observable.h:160-166`
- **Issue:** `notifyObservers()` copies `callbacks_` for safe iteration, but if a callback reentrantly `set()`s, subscribers receive notifications with `value_` already mutated to a newer value; combined with view teardown, captured `[this]` lambdas may run against destroyed Views.
- **Impact:** Hard-to-diagnose UI/state corruption; UAF on view destruction during notification.
- **Fix:** Queue reentrant sets (`pending_set_` flag), drain after outer notification completes; document subscribers must outlive Observable.

### BL-12. Observable observer-interface iteration invalidates on reentrant unsubscribe
- **File:** `src/core/observable.h:147-156`
- **Issue:** The `IObserver`-loop iterates `observers_` directly (no copy). If `observer->onNotify(...)` triggers `unsubscribe()`, `it` is invalidated → UB. Callback-path is copy-protected; interface-path is not.
- **Impact:** Crash or silent skip when an observer unsubscribes in its own callback (common Qt destructor pattern).
- **Fix:** Snapshot `observers_` weak_ptrs before iterating, same as callbacks.

### BL-13. `updateUIState` clobbers user preferences and status_message
- **File:** `src/view_model/game_view_model_state.cpp:202`
- **Issue:** Builds fresh `UIState ui;` carrying forward only `input_mode` and `notes_filled`. `show_conflicts`, `show_hints`, `status_message`, `is_paused` are reset to defaults each call.
- **Impact:** `setShowConflicts(false)` is wiped by next keystroke; status messages disappear on the next move.
- **Fix:** Preserve all sticky fields from `prev`, or use `uiState.update()` to mutate only changed fields.

### BL-14. `undoToLastValid` can overshoot `last_valid_state_index_`
- **File:** `src/view_model/game_view_model_undo.cpp:99`
- **Issue:** Loop walks past `PlaceHint` moves silently; combined with `recordMove` never updating `last_valid_state_index_` after redo branching, the tracked index can reference a deleted move.
- **Impact:** Silent off-by-many corruption — board reverts further than the "last valid state" promise.
- **Fix:** Compute target index by walking forward (skipping hints) before looping; clamp `last_valid_state_index_` on redo-branch truncation.

### BL-15. `settingsObservable` subscription never unsubscribed → UAF
- **File:** `src/view/main_window.cpp:646`
- **Issue:** `settings_manager_->settingsObservable().subscribe([this]...)` discards the returned unsubscribe. `observer_.unsubscribeAll()` in `~MainWindow` only clears CompositeObserver subscriptions.
- **Impact:** `settings_manager_` outlives MainWindow (held by DIContainer). Next `setSettings()` calls lambda with destroyed `this` → use-after-free.
- **Fix:** Store the returned unsubscribe lambda and call it in `~MainWindow`, or route via `observer_`.

---

## 5. HIGH findings

Condensed list. Full evidence and "why it matters" in the appendix for each chunk.

### Solver spine (chunk 1)
- **HIGH** `BacktrackingSolver` bypasses `ITimeProvider` (`backtracking_solver.cpp:51, 117`).
- **HIGH** Scalar `countSolutionsHelper` cache inserts missing `!timed_out` guard (`solution_counter.cpp:225-227, 294-296, 329-332`).

### SIMD (chunk 2)
- **HIGH** Non-peer cells 81-95 invariant ("padding == 0") enforced only at init (`simd_constraint_state.cpp:230-239`).
- **HIGH** AVX-512 naked-single cell `< 81` guard relies on initialization invariant (`simd_avx512_ops.cpp:95`).
- **HIGH** `getCpuFeatures()` inline asm lacks `"memory"` clobber on XGETBV (`cpu_features.cpp:129-132`).

### Strategies — uniqueness / coloring (chunk 5)
- **HIGH** 3D Medusa applies contradiction rules to non-bipartite components (`three_d_medusa_strategy.h:214-223`).
- **HIGH** Simple Coloring skips 2-cell chains (`simple_coloring_strategy.h:124`) — misses valid Rule 2 (Exclusion) eliminations.

### Strategies — ALS / Forcing (chunk 6)
- **HIGH** Cell-FC `findCommonElimination` not invoked on contradiction branch (`forcing_chain_strategy.h:128`).
- **HIGH** ALS Chain Z labelled wrong technique when also RC between A1 and An (`als_chain_strategy.h:219`).

### Strategies — loops / cycles (chunk 7)
- **HIGH** GroupedXCycles silently drops Type 2 deductions (`grouped_x_cycles_strategy.h:326-336`).
- **HIGH** GroupedNiceLoop allows endpoint group to overlap chain interior cells (`grouped_nice_loop_strategy.h:347-381`).
- **HIGH** Continuous Nice Loop same-cell different-digits weak-link rule eliminates without checking if cell is a chain pivot (`continuous_nice_loop_strategy.h:400-413`).

### Strategies — exotic (chunk 8)
- **HIGH** Franken Fish: cover units may equal a chosen base unit (`franken_fish_strategy.h:178-203, 282-316`).
- **HIGH** Mutant Fish: cover-cell overlap among chosen covers silently allowed (`mutant_fish_strategy.h:329-352`).
- **HIGH** Kraken Fish: misattributes finned eliminations as Kraken results (`kraken_fish_strategy.h:191-193`).
- **HIGH** Kraken explanation_data uses base-line `RegionType` but eliminations sit on cover lines (`kraken_fish_strategy.h:218-228`).

### Puzzle pipeline (chunk 9)
- **HIGH** Generator stores `result.seed = settings.seed` (0 sentinel) instead of actual `random_device` seed (`puzzle_generator.cpp:79, 121, 161-168`).
- **HIGH** `PuzzleRater::calculateRating` excludes Backtracking (`puzzle_rater.cpp:63-71`).
- **HIGH** `PuzzleRater::ratePuzzle` calls solver with **no wall-clock budget** (`puzzle_rater.cpp:36`) — regresses AI Escargot livelock fix.

### Persistence (chunk 10)
- **HIGH** Save write is **not atomic** (`save_manager_serialization.cpp:180-185`).
- **HIGH** `getAllSessions` silently discards corrupt encrypted sessions then overwrites (`statistics_manager.cpp:238-250`) — catastrophic stats loss.
- **HIGH** `recalculateAggregateStats` double-counts pending sessions (`statistics_manager.cpp:613-647`).
- **HIGH** Compression magic-byte sniff is ambiguous post-decryption (`save_manager_serialization.cpp:220-235`).
- **HIGH** `isValidSaveFile` does full pipeline on every UI query (`save_manager_compression.cpp:105-123`) — UI freeze.
- **HIGH** `EncryptionManager` ctor `throw std::runtime_error` (`encryption_manager.cpp:43-50`).
- **HIGH** `const_cast<char*>(system_id.data())` (`encryption_manager.cpp:210`) — violates "no const_cast" rule.

### Training (chunk 11)
- **HIGH** `recordLesson` average_hints math wrong + divide-by-zero (`training_statistics_manager.cpp:52-55`).
- **HIGH** `recordLesson` accepts negative/oversize input without validation (`training_statistics_manager.cpp:44-74`).
- **HIGH** 3D Medusa misclassified as Elimination mode (`training_exercise_generator.cpp:170-172`).
- **HIGH** Per-step interaction-mode override only handles ForcingChain (`training_exercise_generator.cpp:88-93`).
- **HIGH** `validatePlacement` doesn't validate `value` range (`training_answer_validator.cpp:152-183`).

### Architecture spine (chunk 12)
- **HIGH** DIContainer race during singleton lazy-init caches duplicate instance on factory re-entry (`di_container.h:82-86`).
- **HIGH** DIContainer silently returns `nullptr` on unregistered interface (`di_container.h:88`).
- **HIGH** `CompositeObserver::observe` calls `observable.clearSubscriptions()` (`observable.h:186`).
- **HIGH** `GameState` default-constructs `SystemTimeProvider` (`game_state.h:48`) — bypasses DI.
- **HIGH** Observable callback during notification can iterate a freed map entry (`observable.h:160-166`).

### View (chunk 14)
- **HIGH** `applyLocale` drops the working translator on `.qm` load failure (`main_window.cpp:683-700`).
- **HIGH** "Reset Puzzle" Ctrl+R menu action permanently disabled (`main_window.cpp:288-290`).
- **HIGH** `animatePageTransition` effect dangles across `rebuildPages` on `LanguageChange` (`training_widget.cpp:858-870`).

---

## 6. MEDIUM findings

Grouped by theme.

### A. Determinism leaks
- **MED** Strategy registration order deviates from documented difficulty rating (`sudoku_solver.cpp:115-130`).
- **MED** `countSolutions` / `countSolutionsWithTimeout` flip timeout semantics (`solution_counter.cpp:121-123, 169-171`).
- **MED** Per-strategy budget check is pre-call only (`sudoku_solver.cpp:280-288`) — reopens AI Escargot class.
- **MED** `propagateHiddenSingles` cross-unit conflict leaves branch in unflagged, undeducible state (`forcing_chain_helpers.h:166-277`).
- **MED** Generator RNG shared across attempts but `result.seed` is user seed (`puzzle_generator.cpp:79, 82, 121`).
- **MED** `exportGameSessionsCsv` uses non-reentrant `std::localtime` (`statistics_serializer.cpp:411`).
- **MED** Training `generateExercises` non-deterministic; no seed contract (`training_exercise_generator.cpp:39-125`).

### B. Lifetime / ownership
- **MED** `applyStep` returns `true` unconditionally — error-detection contract broken (`sudoku_solver.cpp:319-343`).
- **MED** `FlatSolutionCache::find` returns pointer into `mutable` array (`solution_counter.h:55-70`).
- **MED** `registerSingleton` factory captures `&container` by ref (`main.cpp:74,82,...`) — fragile pattern.
- **MED** `GameState` copy/move retains shared `time_provider_` and Observable subscribers (`game_state.h:50-53`).
- **MED** `Observable::subscribe(ObserverPtr)` takes shared_ptr but stores weak_ptr — silent expiry.
- **MED** `restoreGameState` doesn't reset coaching state (`game_view_model.cpp:195-279`).

### C. Save / state integrity
- **MED** `applyStep` silently drops eliminations on Placement-typed steps (`sudoku_solver.cpp:330-343`).
- **MED** Move history `timestamp` round-trip uses `steady_clock` (`save_manager_serialization.cpp:123-124, 378-380`).
- **MED** `serializeGameStatsToYaml` rewrites entire sessions file each append, non-atomically (`statistics_serializer.cpp:184-235`).
- **MED** Integer overflow in average-time recomputation on import (`statistics_manager.cpp:389-393`).
- **MED** `recordMove`/`recordHint` counters are `int` (`statistics_manager.cpp:124-128, 140-142`).
- **MED** `reconstructCandidates` silent data-loss when stored mask diverges from propagated mask (`training_answer_validator.cpp:231-255`).
- **MED** `reconstructCandidates` doesn't validate `candidate_masks.size()` (`training_answer_validator.cpp:241`).
- **MED** `walkForward` doesn't bounds-check `target_index` (`training_exercise_generator.cpp:181-211`).
- **MED** `recordMove` computes `hasBoardErrors` then re-computes conflicts (`game_view_model_undo.cpp:179-191`).
- **MED** `importPuzzleFromString` doesn't end prior stats session (`game_view_model_hints.cpp:218-296`).
- **MED** `startNewGame`/`loadGame` don't end stats session before replacement (`game_view_model.cpp:87, 195`).

### D. Strategy soundness / coverage gaps
- **MED** UR Type 5 missing despite docstring claim (`unique_rectangle_strategy.h:32`).
- **MED** UR Type 6 roof arrangement assumed but never validated (`unique_rectangle_strategy.h:597-606`).
- **MED** BUG strategy: N≥2 infrastructure is dead code under `MAX_TRIVALUE_CELLS=1` (`bug_strategy.h:42, 162-218`).
- **MED** Multi-Coloring Trap rescans entire board per cross-pair (`multi_coloring_strategy.h:262-311`) — perf.
- **MED** Sue de Coq N=1 path / cross-side overlap claim (`sue_de_coq_strategy.h:266-284`) — verified safe but document.
- **MED** Cell-FC contradiction deduction gated to 2-candidate cells; misses 3-candidate-with-2-contradictions (`forcing_chain_strategy.h:109`).
- **MED** NiceLoop Type 2 detection misses same-cell different-digits discontinuity (`nice_loop_strategy.h:322-340`).
- **MED** Continuous Nice Loop closure-on-first-attempt-only via `break` (`continuous_nice_loop_strategy.h:319`).
- **MED** Grouped X-Cycles weak link permits cross-unit "all-see" pairs without shared unit (`grouped_x_cycles_strategy.h:289-298`).
- **MED** Continuous Nice Loop adds parallel strong+weak edges for bivalue cells (`continuous_nice_loop_strategy.h:227-243`).
- **MED** X-Cycles weak link omits direct conjugate pairs (`x_cycles_strategy.h:83-93`).
- **MED** Junior Exocet target accepted with only 1 base candidate (`junior_exocet_strategy.h:146-149, 165-168`).
- **MED** Franken Fish cover units allowed with only 1 candidate cell (`franken_fish_strategy.h:166, 183`).
- **MED** Mutant Fish `cell_cover_masks` bit width within 1 of overflow (`mutant_fish_strategy.h:306-311, 354-356`).

### E. Strategy basics (chunk 3)
- **MED** `enumerateALSs` adds same cell-set multiple times (row+box overlap) (`als_helpers.h:40-57`).
- **MED** `buildConjugatePairGraph` creates duplicate edges (row+box, col+box overlaps) (`coloring_helpers.h:56-107`).

### F. Strategy wings (chunk 4)
- **MED** Remote Pairs may eliminate both candidates from an intermediate chain cell (`remote_pairs_strategy.h:151`).
- **MED** W-Wing fmt placeholder/argument count is one short (`w_wing_strategy.h:198-201`).

### G. UI / View hygiene
- **MED** `clock_timer_` polls VM state every 1 s (`main_window.cpp:512-516`) — anti-pattern per project-context.
- **MED** Wait-cursor not RAII; leaks on early return/throw (`main_window.cpp:908-910, 946-948`).
- **MED** `executeCommand` silently drops most `GameCommand` verbs (`game_view_model_state.cpp:45-78`).
- **MED** `canExecuteCommand` default returns `true` for every unhandled command (`game_view_model_state.cpp:80-97`).
- **MED** `keyPressEvent` calls `updateButtonPanel()` directly, bypassing Observable (`main_window.cpp:746-757`).
- **MED** View directly branches on VM's `InputMode` (`main_window.cpp:387-391, 766-779`).
- **MED** `autoSaveIfNeeded` fires synchronously on every keystroke (`game_view_model_board.cpp:94`).
- **MED** `getHint` reports "0/10 used" when no game session exists (`game_view_model_hints.cpp:133-138, 508-521`).
- **MED** `checkSolution → startNewGame` re-entrancy clobbers completion status_message (`game_view_model_undo.cpp:115-145`).
- **MED** About-dialog "Copyright (C)" hardcoded inside translatable format (`main_window.cpp:1287-1293`).
- **MED** `auto_save_timer_` fallback `30000` is a magic number (`main_window.cpp:520`).
- **MED** `difficultyString` default branch hides future-enum additions (`main_window.cpp:1416-1418`).

### H. i18n
- **MED** `locale_utils::isValidLocaleCode` rejects BCP-47 hyphen form (`locale_utils.cpp:57`).
- **MED** `localized_explanations.h` runs translator-supplied strings through `fmt::runtime` — bad translation crashes app.

### I. Defense-in-depth
- **MED** `PuzzleAnalyzer::scoreDifficulty` switch fallthrough to `NoSolution` masks new SolverError values (`puzzle_analyzer.cpp:151-158`).
- **MED** `runRemovalOrdering` `max_clues` parameter unused but still in signature (`puzzle_generator.cpp:226-248`).
- **MED** `validatePuzzle` returns `true` on all-empty board (`puzzle_generator.cpp:202-214`).
- **MED** `QtClipboardProvider::setText` narrows `size_t → int` (`qt_clipboard_provider.h:36`).
- **MED** `AppDirectoryManager` doesn't probe XDG_DATA_HOME writability (`app_directory_manager.cpp:40-43`).

### J. SIMD
- **MED** `popcnt_epi16` AVX2 byte-sum step incorrect for >8-bit popcounts (`simd_constraint_state.cpp:100`).
- **MED** AVX2 `place()` writes to padding cells via aligned store (`simd_constraint_state.cpp:233-239`).
- **MED** `popcnt_epi16` LUT relies on `_mm256_load_si256` (aligned) (`simd_constraint_state.cpp:69-85`).
- **MED** AVX2 `findNakedSingle` movemask-pair-clear logic depends on `cmpeq` producing 0xFFFF pairs (`simd_constraint_state.cpp:294-304`).

### K. Training
- **MED** `findAllSteps` may not enumerate every valid technique instance (`training_answer_validator.cpp:198-229`).
- **MED** `recordLesson` mastered/proficient counters unbounded; perfect-streak overflow (`training_statistics_manager.cpp:67-72`).

---

## 7. LOW findings

Deferred — opportunistic cleanup. Listed for the record.

### Solver spine (chunk 1)
- **LOW** `StrategyBase::sees` treats a cell as seeing itself (`strategy_base.h:171-173`).
- **LOW** `FlatSolutionCache::find` returns pointer into `mutable` array — lifetime brittle (`solution_counter.h:55-70, 172`).
- **LOW** `applyStep` placement assignment may narrow `step.value` to `int8_t` silently (`sudoku_solver.cpp:322, 333`).

### SIMD (chunk 2)
- **LOW** `osSupportsAvx512()` calls `readXcr0()` twice (`cpu_features.cpp:73-81`).
- **LOW** `enum class SolverPath` uses `uint8_t` but `isSolverPathAvailable` switch has no `default` past return (`cpu_features.h:72-83`).
- **LOW** AVX2 padding loop "break" instead of bound-trim (`simd_constraint_state.cpp:190-208`).

### Strategies basics (chunk 3)
- **LOW** HiddenTriple/HiddenQuad `val_exists` check is necessary but not sufficient — comment clarity (`hidden_triple_strategy.h:79-97`, `hidden_quad_strategy.h:93-114`).
- **LOW** FishHelpers `buildFinnedEliminations` parameter naming swap (`fish_helpers.h:131-144`).
- **LOW** NakedTriple/NakedQuad allow `count==1` cells in degenerate cases (`naked_triple_strategy.h:75-79`, `naked_quad_strategy.h:71-75`).
- **LOW** Skyscraper checks 4 endpoint matchings with symmetric duplicates (`skyscraper_strategy.h:130-149`).
- **LOW** TwoStringKite `tryConnection` doesn't exclude degenerate rp/cp pairs (`two_string_kite_strategy.h:115-141`).

### Strategies wings (chunk 4)
- **LOW** FinnedXWing missed early-out when fin_col shares no base-column box-stack (`finned_x_wing_strategy.h:117-132`).
- **LOW** Sashimi* missing degenerate `all-size-1-rows` rejection (`sashimi_swordfish_strategy.h:81-91`).
- **LOW** XY-Wing recomputes the same eliminations from each rotational pivot (`xy_wing_strategy.h:51-56`).
- **LOW** XYZ-Wing `elim_value` is the LAST common pivot value found, not the only one (`xyz_wing_strategy.h:122-129`).

### Strategies uniqueness (chunk 5)
- **LOW** UR Type 3 hardcoded to naked-pair subset, naked-triple/quad cases silently skipped (`unique_rectangle_strategy.h:416-418`).
- **LOW** Empty Rectangle enumerates all 9 box-cell positions as candidate elbows (`empty_rectangle_strategy.h:101-130`).
- **LOW** Avoidable Rectangle robustness gap on malformed boards (`avoidable_rectangle_strategy.h:89-93`).
- **LOW** Unique Loop DFS cycle search has exponential worst case (`unique_loop_strategy.h:217`).

### Strategies ALS/forcing (chunk 6)
- **LOW** ALS Chain DFS uses globally-distinct RC values vs consecutive-distinct (`als_chain_strategy.h:186-192`).
- **LOW** Sue de Coq does not search N=4 covering ALSs (`sue_de_coq_strategy.h:269-328`).
- **LOW** `findCommonElimination` skips source positions unconditionally — misses some deductions (`forcing_chain_helpers.h:386-394`).

### Strategies loops (chunk 7)
- **LOW** NiceLoopStrategy `addUnitLinks` carries unused boolean parameter (`nice_loop_strategy.h:151`).
- **LOW** GroupedXCycles `cellSeesNode` redundancy (`grouped_x_cycles_strategy.h:89-92, 393-398`).
- **LOW** NiceLoopStrategy `findAICEliminations` off-by-one in chain length (`nice_loop_strategy.h:388-390`).

### Strategies exotic (chunk 8)
- **LOW** Junior Exocet `searchBox` only searches one row per box; column-based JE missing (`junior_exocet_strategy.h:81-107`).

### Puzzle pipeline (chunk 9)
- **LOW** `i18n_helpers.h::locFormat` still in repo despite "DELIBERATELY REMOVED" claim (`i18n_helpers.h:38-49`).
- **LOW** `BoardData` initializer-list constructor silently truncates oversized input (`board_data.cpp:21-37`).
- **LOW** `runRecompletionPhase` uses biased modulo on `rng()` (`puzzle_generator.cpp:268`).

### Persistence (chunk 10)
- **LOW** `generateSaveId` collisions not checked (`save_manager.cpp:403-417`).

### Training (chunk 11)
- **LOW** `getTrainingHint` accepts arbitrary `level` with implicit branches (`training_hints.cpp:48-294`).
- **LOW** `validateElimination` matches exact set; multi-step eliminations unmatchable (`training_answer_validator.cpp:185-196`).
- **LOW** Learning path `kAllTechniques` ordering doesn't strictly match rating-ascending (`training_learning_path.h:154-209`).

### Architecture (chunk 12)
- **LOW** `DIContainer::isRegistered` is `const` but `resolve()` is not (`di_container.h:65, 93`).
- **LOW** `Observable<T>::update` requires `operator!=`; SFINAE error message buried (`observable.h:77`).
- **LOW** `main.cpp` wires `ITrainingExerciseGenerator` but doesn't register `ITrainingViewModel` (`main.cpp:176`).

### ViewModel (chunk 13)
- **LOW** `applyMove(PlaceHint)` double-counts moves vs `recordMove` (`game_view_model_undo.cpp:219-225`).

### View (chunk 14)
- **LOW** PuzzleImportDialog trim can split a multi-code-unit character mid-paste (`puzzle_import_dialog.cpp:62-71`).
- **LOW** `paintEvent` has no early-out for zero-size widget / repaint during destruction (`sudoku_board_widget.cpp:82-136`).
- **LOW** `showLoadDialog` opens even when the save list is empty (`main_window.cpp:1077-1086`).
- **LOW** `retranslateUi` rebuilds the entire menu bar — drops user-installed Action shortcuts/state (`main_window.cpp:1426-1428`).

---

## 8. Per-chunk summary table

| # | Chunk | BLK | HIGH | MED | LOW | Headline |
|---|---|---:|---:|---:|---:|---|
| 1 | Solver spine | 0 | 2 | 4 | 3 | Backtracking bypasses ITimeProvider; scalar cache guard asymmetric with SIMD. |
| 2 | SIMD + CPU dispatch | 2 | 3 | 4 | 3 | BITALG ungated + Aligned Loader is back. |
| 3 | Basics + simple fish + helpers | 0 | 0 | 2 | 5 | ALS / coloring helpers create duplicate work but no soundness bugs. |
| 4 | Finned/sashimi + wings | 0 | 0 | 2 | 4 | Remote Pairs intermediate-chain elimination edge case. |
| 5 | Uniqueness + coloring | 0 | 2 | 4 | 4 | Non-bipartite 3D Medusa contradiction; SimpleColoring `<4` cutoff. |
| 6 | ALS + forcing chains | 0 | 2 | 4 | 3 | Cell-FC misses 3-cand contradiction, conflict-in-propagation unflagged. |
| 7 | Loops + cycles | 0 | 3 | 4 | 3 | GroupedNiceLoop endpoint-cell overlap, GroupedXCycles Type 2 missing. |
| 8 | Exotic | 4 | 4 | 3 | 1 | Kraken axis bug + Junior Exocet under-implemented + cover overlaps. |
| 9 | Puzzle pipeline + i18n | 1 | 3 | 6 | 3 | Generator seed broken; rater bypasses budget; analyzer null-deref. |
| 10 | Persistence + statistics | 2 | 7 | 5 | 1 | Master save bug + zlib bomb + non-atomic write + double-counted stats. |
| 11 | Training | 1 | 5 | 4 | 3 | Stats deserializer drops 12 techniques; validation absent. |
| 12 | Architecture spine | 0 | 5 | 5 | 3 | DIContainer + Observable + ITimeProvider DI leaks. |
| 13 | ViewModel | 2 | 0 | 6 | 1 | UIState clobber + undo overshoot + autosave on every keystroke. |
| 14 | View | 1 | 3 | 7 | 4 | settingsObservable UAF + locale fallback wipe + Reset Puzzle dead. |
| | **Total** | **13** | **39** | **60** | **41** | (some severities re-classified during synthesis) |

---

## 9. Triage / action plan

**P0 — Fix before next release** (data loss / crashes / unsoundness in shipped strategies):
1. BL-8 Master save loader (one-line fix).
2. BL-9 zlib bomb cap.
3. BL-10 training stats deserializer.
4. BL-15 settingsObservable unsubscribe.
5. BL-13 + BL-14 ViewModel UIState/undo bugs.
6. BL-11 / BL-12 Observable re-entry & iteration safety.
7. BL-1 BITALG gate.
8. BL-2 alignas/loadu reconciliation.

**P1 — Strategy soundness** (before training-mode trust is claimed):
9. BL-3 / BL-4 / BL-5 / BL-6 Kraken + Junior Exocet (consider disabling Junior Exocet until cover-house validation lands).
10. HIGH Franken/Mutant cover overlap.
11. HIGH 3D Medusa non-bipartite path.
12. HIGH Simple Coloring 2-cell chain.
13. HIGH ForcingChain coverage gaps.

**P2 — Determinism + persistence hardening:**
14. ITimeProvider audit across BacktrackingSolver, PuzzleRater, GameState default, exporters.
15. Save write atomicity + corrupt-sessions recovery.
16. `recalculateAggregateStats` double-count + import math.
17. `EncryptionManager` constructor → `std::expected`.

**P3 — Architectural cleanup:**
18. DIContainer: hard-fail on unregistered, prevent factory re-entry.
19. CompositeObserver scope (don't `clearSubscriptions`).
20. Complete or trim `GameCommand` enum.
21. View polling → Observable migration (clock timer, etc.).

**P4 — Polish:** the LOW pile (rename `isValidLocaleCode`, magic number `30000`, dead `locFormat`, etc.).

---

## 10. Coverage & caveats

- All 14 chunks reviewed; ~39K LOC `src/` covered. Tests directory excluded by scope.
- `localized_explanations.h` and `technique_descriptions.h` were skimmed (auto-generated / table content) — escaping / contract / encoding checked, no findings.
- The brief instructed agents to treat `_bmad-output/project-context.md` as authoritative. One agent flagged a doc/code drift: the brief says `locFormat` was deliberately removed (commit `f0e5567`), but the helper still exists and is widely used — recommend reconciling the doc with reality.
- All agents independently confirmed the bestiary's documented fixes (Phantom Triple, Cross-Component Painter, Disjoint Twins, Diagonal Pretender, Overlapping Basemen) are correctly in place; the new findings are siblings, not regressions.
- Severity is subjective. A finding marked HIGH here could be MEDIUM under different release pressure. The triage section is the more reliable guide than per-chunk counts.

---

## 11. Appendices

Verbatim per-chunk findings, notes, and coverage from each subagent. Use these for full traceability when triaging.

### Appendix A — Chunk 1: Solver spine

#### Findings (sorted: BLOCKER > HIGH > MEDIUM > LOW)

**[HIGH] BacktrackingSolver bypasses ITimeProvider — defeats MockTimeProvider in tests**
- File: `src/core/backtracking_solver.cpp:51,117`
- Issue: `if (deadline.has_value() && std::chrono::steady_clock::now() >= *deadline)` and the in-recursion check both call wall-clock `now()` directly. `SudokuSolver::solvePuzzle(board, budget)` computes the deadline from `time_provider_->steadyNow()` (mockable), but the backtracking fallback then compares against real-clock `now()`.
- Why it matters: Determinism violation per project-context.md ("ITimeProvider is the only legal time source"). Tests with MockTimeProvider cannot deterministically exercise the timeout-during-backtracking path; the AI Escargot livelock regression sentinel cannot be reproduced without real wall-clock.
- Fix: Inject `ITimeProvider` into `BacktrackingSolver` (constructor) and use `time_provider_->steadyNow()` for both deadline checks.

**[HIGH] Scalar cache inserts not guarded by `!timed_out` (Truncated Oracle resurfacing)**
- File: `src/core/solution_counter.cpp:225-227, 294-296, 329-332`
- Issue: Scalar `countSolutionsHelper` guards cache inserts only with `count < max_solutions`. The SIMD twin uses `count < max_solutions && !timed_out` (lines 454, 511, 609). MEMORY.md explicitly lists this as required for "all 6 cache insertions (3 scalar, 3 SIMD)".
- Why it matters: Belt-and-suspenders defense against Truncated Oracle. The early-return on `timed_out` at lines 215/284/313 currently prevents the path in practice, but symmetry with SIMD is the load-bearing invariant in MEMORY.md — divergence is a regression waiting for a refactor to weaponize.
- Fix: Add `&& !timed_out` to the three scalar cache-insert conditions, matching SIMD.

**[MEDIUM] Strategy registration deviates from documented difficulty order**
- File: `src/core/sudoku_solver.cpp:115-130`
- Issue: XYWing (rating 4.2) registered before Swordfish (3.8), Skyscraper (4.0), TwoStringKite (4.1). BUG (5.6) registered before HiddenUR (4.8) and AvoidableRect (4.5). SimpleColoring (4.0) and FinnedXWing (3.4) registered after UniqueRectangle (4.5).
- Why it matters: project-context.md tripwire: "Strategy registration order is by difficulty rating; cheaper strategies must fire before expensive ones, or solve performance degrades." MEMORY.md mirrors this order but doesn't explain the rating inversion — likely an inadvertent drift.
- Fix: Audit and reorder by ascending `getTechniqueRating()`; add a `static_assert` over the strategies_ vector verifying monotonic ratings.

**[MEDIUM] `applyStep` silently drops eliminations on a Placement-typed step**
- File: `src/core/sudoku_solver.cpp:330-343`
- Issue: Internal `applyStep` returns immediately after placing if `type == Placement`, ignoring any `eliminations` in the same `SolveStep`. The public overload at line 319 also returns `true` for elimination steps without doing anything (documented as no-op for backward compat).
- Why it matters: `SolveStep` allows both `position`+`value` and `eliminations` to be populated. A strategy that returns a hybrid step (place value, also eliminate elsewhere) silently loses the eliminations — a future strategy bug surface.
- Fix: After the placement branch, also process `step.eliminations` instead of `return true`. Or add a `[[nodiscard]] bool isValid()` check on SolveStep.

**[MEDIUM] `applyStep` returns `true` unconditionally, breaking error-detection contract**
- File: `src/core/sudoku_solver.cpp:319-343`
- Issue: Both overloads always return `true`. No bounds check on `step.position.row/col` (could be `BOARD_SIZE`-sentinel from upstream init paths) or `step.value`.
- Why it matters: `solvePuzzleImpl` (line 250-252) treats `!applyStep(...)` as `SolverError::Unsolvable`. Since applyStep never returns false, that branch is dead — a corrupt step crashes via OOB write instead of failing cleanly.
- Fix: Validate `row<BOARD_SIZE`, `col<BOARD_SIZE`, `1<=value<=9` for placements; return `false` on violation. Or `std::expected` so callers can route.

**[MEDIUM] `countSolutions` and `countSolutionsWithTimeout` flip timeout semantics**
- File: `src/core/solution_counter.cpp:121-123, 169-171`
- Issue: `countSolutions` returns `1` on timeout ("assume unique"); `countSolutionsWithTimeout` returns `max_solutions` ("assume non-unique"). Both call the same helper.
- Why it matters: `hasUniqueSolution` calls `countSolutions(board, 2) == 1` — a timeout silently returns "unique=true" which is exactly the Truncated Oracle's downstream symptom: UR/BUG strategies trust hasUniqueSolution and produce wrong eliminations.
- Fix: Return `std::expected<int, TimeoutTag>` (or `std::optional`) so the caller chooses. Document the rationale at minimum; tag the timeout return with a distinguishable sentinel.

**[MEDIUM] Per-strategy budget check is pre-call, not post-call — runaway strategy escapes budget**
- File: `src/core/sudoku_solver.cpp:280-288`
- Issue: The budget version of `findNextStep` checks `time_provider_->steadyNow() >= deadline` only BEFORE calling `strategy->findStep(...)`. A strategy that itself runs for seconds (e.g., ALSXYWing, JuniorExocet, KrakenFish, MutantFish on adversarial inputs) finishes uninterrupted; the deadline check on the next iteration is too late.
- Why it matters: This is exactly the AI Escargot livelock class — `MEMORY.md` records the fix as "budget overloads + deadline propagation". The granularity comment ("strategies are black-box") concedes this; budgets >50 ms can routinely overshoot 4× under the heavy strategies.
- Fix: Either pass `deadline` into `ISolvingStrategy::findStep` (interface change) or add a stop-token. At minimum, document the maximum overshoot per strategy.

**[LOW] `StrategyBase::sees` treats a cell as seeing itself**
- File: `src/core/strategy_base.h:171-173`
- Issue: `sees(p, p)` returns `true` (rows/cols match trivially, same box). Multiple strategies call `sees` to find peers; if they include `self == pivot`, eliminations fire on the pivot itself.
- Why it matters: Easy bestiary trap for newer strategies. The interpretation is ambiguous and not documented.
- Fix: Either rename to `seesIncludingSelf` and add `seesStrict(p_a, p_b) = sees(p_a, p_b) && p_a != p_b`, or change `sees` to return false on equal positions and audit callers.

**[LOW] `FlatSolutionCache::find` returns pointer into `mutable` array — lifetime brittle**
- File: `src/core/solution_counter.h:55-70, 172`
- Issue: `find` returns `const int*` into `table_`. `solution_count_cache_` is `mutable`, written from `const` methods via `insert`. If an `insert` rehashed or moved entries between a `find` and the cached read, the pointer would dangle. Currently open-addressing with no rehash, but the `find` API exposes a footgun for future maintainers.
- Why it matters: Future "let's add resize" patch silently corrupts solution counts.
- Fix: Return by value `std::optional<int>` instead of a raw pointer. Cost is one branch; no functional change for callers.

**[LOW] `applyStep` placement assignment may narrow `step.value` to `int8_t` silently**
- File: `src/core/sudoku_solver.cpp:322, 333`
- Issue: `board[r][c] = step.value;` — `step.value` is `int`; `BoardData` cell type may be `int8_t` (used elsewhere with `static_cast<int8_t>`). Implicit narrowing isn't flagged by `-Werror` for this assignment.
- Why it matters: If `step.value` is somehow outside `[-128, 127]` (won't happen for valid 1-9, but no validation), wraps silently.
- Fix: `board[r][c] = static_cast<int8_t>(step.value);` for consistency with the rest of the codebase.

#### Notes
- `CandidateGrid` is indeed persisted across solver iterations (`sudoku_solver.cpp:216`), matching MEMORY.md.
- SIMD `countSolutionsHelperSIMD` correctly guards all 3 interior cache writes with `count < max_solutions && !timed_out` (lines 454, 511, 609). Leaf-node inserts (1 or 0) are unconditional but safe.
- Constraint propagation in `propagateConstraintsScalar/SIMD` uses MAX_ITERATIONS=100; `applyIterativePropagationScalar/SIMD` uses MAX_ITERATIONS=81. The asymmetry isn't justified but isn't a correctness bug.
- `Position` initialization in `findHiddenSingleInRow/Col/Box` uses `{BOARD_SIZE, BOARD_SIZE}` sentinel — would silently produce invalid (9,9) Position if returned without count==1 check. Currently safe.
- The interface `ISudokuSolver::findNextStepByTechnique` default implementation (line 107) ignores the `technique` argument and delegates to `findNextStep(board, original_puzzle)` — surprising fallback.

#### Coverage
13 of 13 files reviewed.

---

### Appendix B — Chunk 2: SIMD + CPU dispatch

#### Findings

**[BLOCKER] `_mm512_popcnt_epi16` invoked without BITALG runtime gate**
- File: `src/core/cpu_features.h:58-61`, `src/core/simd_avx512_ops.cpp:45`
- Issue: `hasAvx512()` only checks `has_avx512f && has_avx512bw`. `findMRVCell` issues `_mm512_popcnt_epi16`, which requires AVX-512 BITALG (Zen 5+, Ice Lake-SP+). `CpuFeatures` has no BITALG field; CPUID leaf 7 ECX bit 12 is never read.
- Why it matters: On Skylake-X / Cascade Lake / Zen 4 (AVX-512F+BW but no BITALG), the AVX-512 path is selected and #UD-faults — SIGILL crash on real hardware.
- Fix: Add `has_avx512_bitalg = (ecx & (1U<<12)) != 0` from CPUID(7,0).ECX, require it in `hasAvx512()`.

**[BLOCKER] `_mm512_loadu_si512` on `alignas(64)` member array — GCC may swap to aligned**
- File: `src/core/simd_avx512_ops.cpp:42, 82`
- Issue: The caller passes `std::array<uint16_t,96>` by reference from a `SIMDConstraintState` member. `_mm512_loadu_si512` at lines 42 and 82 is the unaligned form — GCC has been observed lowering this to `vmovdqa64` when it infers alignment ("Aligned Loader" bestiary). `SIMDConstraintState` carries `alignas(64)`, which gives GCC exactly the inference it needs.
- Why it matters: This is the documented SIGSEGV class.
- Fix: Either drop `alignas(64)` from the struct and rely solely on `loadu`, OR switch to `_mm512_load_si512` and assert alignment at every entry point. Currently you have the worst of both: declared aligned + loadu intrinsic → GCC may swap.

**[HIGH] Non-peer cells (cells 81–95) re-OR'd with `not_digit`, then ANDed onto padding**
- File: `src/core/simd_constraint_state.cpp:230-239`
- Issue: `place()` loops `i = 0..96`, AND-ing padding lanes with `(0xFFFF | ~digit_mask) = 0xFFFF`. Padding stays 0. Fine for state. BUT `PEER_ELIM_MASKS[cell].mask[81..95]` is initialized to `0xFFFF` (non-peer); combined with `not_digit` it leaves padding at 0 — OK, but only because cells 81-95 start at 0 and stay 0. If `initFromBoard` ever sets padding non-zero, naked-single/MRV would scan ghosts.
- Why it matters: Invariant "padding == 0" is load-bearing but enforced only at init; no assertion.
- Fix: `static_assert` or runtime check after init; or constrain MRV/naked-single loops to `BOARD_SIZE*BOARD_SIZE` (81), not `SIMD_PADDED_CELLS`.

**[HIGH] AVX-512 naked-single mask uses 32-bit `countr_zero` on `__mmask32`**
- File: `src/core/simd_avx512_ops.cpp:95`
- Issue: `std::countr_zero(static_cast<unsigned>(naked_single_mask))` — `__mmask32` is `uint32_t`, so this is correct. BUT the loop trusts `j < 32` without verifying `cell < 81`. Cells 81-95 are padding (candidates=0 → mask bit unset by `nonzero_mask`), so in practice the inner `cell < BOARD_SIZE*BOARD_SIZE` guard catches it. Still: if a future change initializes padding non-zero and the cell is power-of-two by accident, this returns a phantom cell index.
- Why it matters: Defense-in-depth — same invariant as above.
- Fix: Replace `cell < BOARD_SIZE*BOARD_SIZE && board.cells[cell] == 0` order with explicit `if (cell >= 81) break;` after computing `cell`, matching the AVX2 path's structure.

**[HIGH] `getCpuFeatures()` Meyers singleton — `detect()` calls inline assembly with no memory ordering**
- File: `src/core/cpu_features.cpp:129-132`
- Issue: For a single-threaded Qt app this is fine, but `XGETBV` via `__asm__ __volatile__` without a memory clobber could in theory be reordered. C++11 magic statics guard the init race, so this is moot in practice.
- Why it matters: Low risk given single-threaded constraint; flagging because the inline asm is the kind of thing future-readers will copy.
- Fix: Add `"memory"` clobber to the asm constraint list: `: "c"(0) : "memory"`. Cheap, defensive.

**[MEDIUM] `popcnt_epi16` AVX2 byte-sum step incorrect for >8-bit popcounts**
- File: `src/core/simd_constraint_state.cpp:100`
- Issue: Comment says "For 9-bit candidates (max popcount = 9), high byte is always 0 — So we just need: low_byte + (high_byte >> 8)". The code does `sum + (sum >> 8)` per 16-bit lane. For candidates ≤ 9 bits the high byte of `sum` IS zero before this add (max nibble-sum = 8+1=9 fits in one byte), so the shift adds 0 and the final `& 0x00FF` is fine. But if `candidates[i]` ever has bits 9-15 set (e.g., used as a scratch mask), the code silently wraps.
- Why it matters: Tight coupling between "candidates fit in 9 bits" invariant and the popcount kernel. No static_assert.
- Fix: Mask `v & ALL_CANDIDATES_MASK` at function entry, or `static_assert` somewhere this kernel is only called on candidate data.

**[MEDIUM] AVX2 `place()` writes to padding cells via aligned store**
- File: `src/core/simd_constraint_state.cpp:233-239`
- Issue: Loop AND-stores into `candidates[81..95]`. Because `PEER_ELIM_MASKS[cell].mask[81..95] = 0xFFFF`, padding becomes `0 & 0xFFFF = 0` — stays clean. Correct by construction. But the loop wastes a SIMD iteration on padding; more concerning, if anyone changes peer-mask init this becomes a corruption vector.
- Why it matters: Invariant-locality: "padding stays 0" is enforced by construction, not checked.
- Fix: Loop bound to `BOARD_SIZE*BOARD_SIZE` (rounded up to 16 → 96), or assert post-place that `candidates[81..95] == 0`.

**[MEDIUM] `popcnt_epi16` LUT relies on `_mm256_load_si256` (aligned)**
- File: `src/core/simd_constraint_state.cpp:69-85`
- Issue: `POPCOUNT_LOOKUP` is `alignas(SIMD_ALIGNMENT) constexpr uint8_t[32]`. `_mm256_load_si256` requires 32-byte alignment. `constexpr` + `alignas(64)` on a function-static-equivalent should give it, but `constexpr` arrays at namespace scope have linkage rules that can interact with `-fmerge-all-constants` or LTO across TUs.
- Why it matters: Class of "GCC swaps loadu for movdqa" bugs — but inverted: declared aligned with `load_si256`. If linker merges with a less-aligned identical constant from another TU, alignment drops.
- Fix: Use `_mm256_loadu_si256` for the LUT — the LUT is loaded once per `popcnt_epi16` call, no perf reason to demand aligned.

**[MEDIUM] AVX2 `findNakedSingle` movemask-pair-clear logic depends on `cmpeq` producing 0xFFFF pairs**
- File: `src/core/simd_constraint_state.cpp:294-304`
- Issue: Code asserts each 16-bit lane produces two identical movemask bytes, then clears two low bits with `mask &= mask-1` twice. If the lane is 0xFF00 or 0x00FF (which `_mm256_cmpeq_epi16` should never produce — it's all-1 or all-0), the double-clear desyncs and may skip or revisit cells. Defensive coding would mask `byte_mask &= 0x55555555` (every other bit) to fold to one bit per lane, then process.
- Why it matters: Brittle to future modifications; reads as "magic" without comment about the invariant.
- Fix: Use `_mm256_movemask_epi8(packs)` after `_mm256_packs_epi16` to get a single bit per lane, or use the AVX2 alternative of computing popcount==1 directly.

**[LOW] `osSupportsAvx512()` calls `readXcr0()` twice**
- File: `src/core/cpu_features.cpp:73-81`
- Issue: `osSupportsAvx()` reads XCR0; if it returns true, `osSupportsAvx512()` reads XCR0 again. Harmless but XGETBV is not free, and the call sequence runs once per program lifetime, so trivial.
- Why it matters: Cosmetic.
- Fix: Cache `xcr0` once in `detect()` and pass through.

**[LOW] `enum class SolverPath` uses `uint8_t` but `isSolverPathAvailable` switch has no `default` past return**
- File: `src/core/cpu_features.h:72-83`
- Issue: All four enumerators handled inside switch; `return false` after the switch is dead-code reachable only via cast-injected invalid values. GCC may warn under `-Wswitch-enum`; clang-tidy `bugprone-switch-missing-default-case` is not in the disabled list.
- Why it matters: Minor; defensive return is correct.
- Fix: None needed unless clang-tidy fires.

**[LOW] AVX2 padding loop "break" instead of bound-trim**
- File: `src/core/simd_constraint_state.cpp:190-208`
- Issue: Inner `for (j = 0; j < 16; ++j) { if (cell >= 81) break; ... }` re-checks per-iteration. Could lift bound to `j_end = min(16, 81-i)` for cleaner generated code and clearer intent.
- Why it matters: Cosmetic; cold path.
- Fix: Compute `j_end` once.

#### Notes
- Flag isolation verified clean: `-mavx2`/`-mavx512f` only appear under `target_compile_options(simd_avx2 ...)` / `target_compile_options(simd_avx512 ...)` PRIVATE; main target links them as static libs and stays baseline x86-64.
- `SIMDConstraintState` correctly carries `alignas(64)`. `PeerElimMask` (`simd_constraint_state.cpp:43`) and `POPCOUNT_LOOKUP` (line 69) also `alignas(SIMD_ALIGNMENT)`. The struct alignment IS the Aligned Loader risk vector for `_mm512_loadu_si512`.
- The AVX2 `popcnt_epi16` LUT trick is clever but the comment misstates the algebra (see MEDIUM #1). Worth a static_assert.

#### Coverage
6 of 6 files reviewed (cpu_features.h, cpu_features.cpp, simd_constraint_state.h, simd_constraint_state.cpp, simd_avx512_ops.h, simd_avx512_ops.cpp). Spot-checked CMakeLists.txt for flag isolation.

---

### Appendix C — Chunk 3: Strategies basics + simple fish + helpers

#### Findings

**[MEDIUM] enumerateALSs adds same cell-set multiple times (row+box overlap)**
- File: `src/core/strategies/als_helpers.h:40-57`
- Issue: A 2- or 3-cell ALS whose cells lie in both a row segment and a box (which is always true for box-confined subsets of a row) gets added once per containing region. No dedup by cell set.
- Why it matters: Downstream paired-ALS strategies (ALS-XZ, ALS-XY-Wing, Death Blossom, ALSChain) iterate O(N^2) over ALSs; duplicates inflate work ~4x and cause `sharesCells` to treat identical-cell ALSs as legitimate "different" ALSs.
- Fix: After enumeration, dedupe by sorted cell-position list (or skip box-region enumeration when all box cells lie in one row/col already enumerated).

**[MEDIUM] buildConjugatePairGraph creates duplicate edges (row+box, col+box overlaps)**
- File: `src/core/strategies/coloring_helpers.h:56-107`
- Issue: `linkIfConjugate` is called once per region (row/col/box). A conjugate pair whose two cells share a row AND a box (or col AND box) gets two parallel edges added to `adj[u]` and `adj[v]`.
- Why it matters: Downstream BFS-based coloring in SimpleColoring, MultiColoring, XCycles wastes time re-visiting; if any path/cycle-length logic counts edges (not unique nodes), it would mis-count.
- Fix: After all scans, sort+unique each adjacency list, or use a set-based collection.

**[LOW] HiddenTriple/HiddenQuad val_exists check is necessary but not sufficient**
- File: `src/core/strategies/hidden_triple_strategy.h:79-97`, `hidden_quad_strategy.h:93-114`
- Issue: The Phantom Triple fix (val_exists guard) correctly rejects already-placed values, but the actual algorithm relies on `cells_with_values.size() == N` to confine vals to N cells. Comment "skip values that are already placed in this region" obscures that the real safety is the union-size invariant.
- Why it matters: Future maintainer might think the val_exists check alone is the guard and weaken it.
- Fix: Comment the dual-invariant clearly, or assert post-condition that each valN appears in ≥1 of cells_with_values.

**[LOW] FishHelpers::buildFinnedEliminations parameter naming swap**
- File: `src/core/strategies/fish_helpers.h:131-144`
- Issue: Variable named `secondary` ranges over BOARD_SIZE and is compared against `base_primaries`; line 136 then computes `row = by_row ? secondary : base_sec` — so `secondary` is actually a primary-axis index, not secondary.
- Why it matters: Misleading naming invites future edit to use `secondary` as a column index, swapping the geometry and producing wrong eliminations.
- Fix: Rename loop variable to `non_base_primary` to match its semantic role.

**[LOW] NakedTriple/NakedQuad allow `count == 1` cells in degenerate pretense**
- File: `src/core/strategies/naked_triple_strategy.h:75-79`, `naked_quad_strategy.h:71-75`
- Issue: Filter `count == 2 || count == 3` (triple) and `2..4` (quad) excludes singletons. A singleton candidate cell can legitimately be part of a naked quad's union; the strategy depends on Naked Single having already fired.
- Why it matters: Robustness — if registration order ever changes or these strategies run in a context where Naked Single hasn't been exhaustively applied, valid patterns silently miss.
- Fix: Either include count=1 cells (a forced cell is part of any union) or document the dependency.

**[LOW] Skyscraper checks 4 endpoint matchings with symmetric duplicates**
- File: `src/core/strategies/skyscraper_strategy.h:130-149`
- Issue: `tryEndpointMatch` is called with all 4 (shared, non_shared) combinations of `(pos1,pos2)` from each pair. Matchings (a1,a2,b1,b2) and (a2,a1,b2,b1) are symmetric — same shared column and same non-shared columns — wasted work.
- Why it matters: Unit-tests assuming a particular winning matching could be order-sensitive.
- Fix: Iterate only 2 of the 4 combinations, or dedupe by canonicalising endpoints.

**[LOW] TwoStringKite tryConnection doesn't exclude rp/cp pairs whose endpoints would degenerate**
- File: `src/core/strategies/two_string_kite_strategy.h:115-141`
- Issue: When `row_connected == col_other` or `col_connected == row_other` is impossible given the cell-disjointness filter at line 73, but no explicit guard ensures `row_other` and `col_other` aren't the same cell.
- Why it matters: A future change relaxing the line-73 filter would silently produce nonsense eliminations.
- Fix: Add an assert that the 4 endpoints are distinct cells.

#### Notes
- The Phantom Triple bestiary fix is correctly applied in hidden_triple_strategy.h:81-97 and hidden_quad_strategy.h:94-114.
- All findStep methods have `[[nodiscard]]`; explanation strings are translation-free (raw English) per the strategy-internal convention.
- Empty-eliminations guard (`if (!eliminations.empty())` before returning a step) is consistent across all strategies.
- All vectors returned by getCandidates are sorted ascending (MIN_VALUE..MAX_VALUE iteration), so `cand1 == cand2` equality in NakedPair is safe.
- `collectCandidatePositions` populates positions in ascending secondary order, so X-Wing's index-by-index comparison is correct.
- HiddenSingle is delegated to `CandidateGrid::findHiddenSingle` — not reviewable in this chunk.

#### Coverage
19 of 19 files reviewed.

---

### Appendix D — Chunk 4: Strategies finned/sashimi + wings

#### Findings

**[MEDIUM] Remote Pairs may eliminate both candidates from an intermediate chain cell**
- File: `src/core/strategies/remote_pairs_strategy.h:151`
- Issue: `checkEliminations` skips only `start` and `end`, not intermediate visited chain cells. If a depth-1+ bivalue chain member sees both endpoints (e.g., start, mid1, mid2, end all in a single box), it gets both `val_a` and `val_b` eliminated, emptying the cell.
- Why it matters: produces a 0-candidate cell on degenerate but valid chain shapes (start/end in same unit + intermediate in same unit as end). Strategy unsoundness, breaks solver state.
- Fix: pass the BFS `depth` array / chain set down and skip any `bivalue_cells[i]` with `depth[i] >= 0` from the elimination scan.

**[MEDIUM] W-Wing fmt placeholder/argument count is one short**
- File: `src/core/strategies/w_wing_strategy.h:198-201`
- Issue: Format string has 8 `{}` placeholders. Args supplied are 8, but the `{{{},{}}}` group is meant to read `{A,B}`; args feed `link_value, elim_value` there — same data twice (once as the pair, once as the link). Result reads with the bivalue pair shown as (link, elim).
- Why it matters: explanation text is misleading; doesn't impact load-bearing logic but breaks parity with other wing strategies that emit `{A,B}` deterministically.
- Fix: pass `val_a, val_b` (sorted bivalue pair) into the `{{{},{}}}` slot, not `link_value, elim_value`.

**[LOW] FinnedXWing rejects only legitimately when fin_col happens to share a base column box-stack — silent miss elsewhere**
- File: `src/core/strategies/finned_x_wing_strategy.h:117-132`
- Issue: Strategy never verifies that `fin_box` actually intersects either `base_col1` or `base_col2`'s column-stack before iterating. When the fin lies in a wholly different box-stack, the loop scans every row × {base_col1, base_col2} with no possible match. Pure inefficiency, not a correctness bug.
- Why it matters: small redundant work; given fish enumeration is value × 9 × 9 × 2 (row/col) this is measurable but not load-bearing.
- Fix: early-out `if (fin_col/3 != base_col1/3 && fin_col/3 != base_col2/3) return nullopt;` before the elim loop.

**[LOW] Sashimi*Strategy missing degenerate `all-size-1-rows` rejection**
- File: `src/core/strategies/sashimi_swordfish_strategy.h:81-91` (and `sashimi_jellyfish_strategy.h` analogue)
- Issue: For 3 base rows all of size 1, `union_cols.size()==4` is achievable only when the lone candidates plus the fin row contribute exactly 4 distinct cols, but `validateFinnedFish` then requires the fin row to be the sole row containing `fin_secondary` — a size-1 non-fin row may collide with another size-1 row at the same base column.
- Why it matters: dependent on puzzle solvability; sub-cases of unsolvable boards may still be fed by the solver loop and produce misleading eliminations.
- Fix: after collecting size-1 non-fin rows, dedupe their lone candidates and reject if any duplicate.

**[LOW] XY-Wing recomputes the same eliminations from each rotational pivot**
- File: `src/core/strategies/xy_wing_strategy.h:51-56`
- Issue: An XY-Wing `{AB, AC, BC}` is symmetric — when the strategy iterates pivots, the AB cell, AC cell, and BC cell all qualify as pivots and yield the SAME elimination set under relabeling. `findStep` returns the first hit, so this is harmless but means orientation #1's eliminations win.
- Why it matters: explanation text and `position` field are determined by iteration order; UI/training mode labels could surprise a user expecting the canonical AB pivot.
- Fix: optionally rank pivot candidates so the AB cell (whose pair is the values shared by both wings) is preferred.

**[LOW] XYZ-Wing `elim_value` is the LAST common pivot value found, not the only one**
- File: `src/core/strategies/xyz_wing_strategy.h:122-129`
- Issue: The loop overwrites `elim_value` on every match without breaking. For a valid XYZ-Wing exactly one pivot candidate is in BOTH wings.
- Why it matters: cosmetic; doesn't affect correctness because of the `w1_unique != w2_unique` guard.
- Fix: `break;` after the first assignment.

#### Notes
- All finned/sashimi strategies share `FishHelpers::validateFinnedFish` and `buildFinnedEliminations`; the shared eliminator correctly restricts to `fin_box` and excludes pattern primaries.
- `position_roles` arrays consistently mark the fin cell with `CellRole::Fin` after all pattern cells; sizes match `positions` in every fish strategy reviewed.
- WXYZ/VWXYZ correctly handle the `(wing_mask & pivot_mask) == 0` rejection so wings always overlap pivot in at least one candidate.
- `sees()` is reflexive (a cell sees itself), but every consumer reviewed checks `pos != endpoint` before the `sees` call.
- XY-Chain `MAX_DEPTH = 8` cap is documented and matches typical published XY-chain upper bounds.

#### Coverage
13 of 13 files reviewed.

---

### Appendix E — Chunk 5: Strategies uniqueness + coloring

#### Findings

**[HIGH] 3D Medusa applies contradiction rules to non-bipartite components**
- File: `src/core/strategies/three_d_medusa_strategy.h:214-223`
- Issue: When BFS detects an odd cycle (`is_bipartite=false`), the elimination rules are skipped but `checkContradiction` is still invoked on the same `color_a_nodes`/`color_b_nodes` populated from the inconsistent 2-coloring.
- Why it matters: An odd-cycle component implies the puzzle has no solution along that chain (shouldn't happen on a sound puzzle), but if it does occur transiently mid-solve, the contradiction rule can eliminate the wrong color, producing invalid eliminations.
- Fix: Gate `checkContradiction` behind `if (is_bipartite)` as well, or treat non-bipartite as a hard contradiction (no eliminations, surface as unsolvable).

**[HIGH] Simple Coloring skips two-cell chains, missing valid Rule 2 eliminations**
- File: `src/core/strategies/simple_coloring_strategy.h:124`
- Issue: The guard `if (color_a_cells.size() + color_b_cells.size() < 4)` skips every component shorter than 4 cells, including the canonical 2-cell conjugate pair. Rule 2 (Exclusion) is valid on any conjugate pair.
- Why it matters: Strict false negative; the strategy under-eliminates.
- Fix: Lower the threshold to `< 2` (or just check `adj[start].empty()` already filters), and skip only Rule 1 for size-2 components since contradiction is impossible there.

**[MEDIUM] UR Type 5 missing despite class-level claim**
- File: `src/core/strategies/unique_rectangle_strategy.h:32`
- Issue: Class doc says "Types 1-4, 6" — Type 5 (extras as the same single value C in both floor + both roof, where C is conjugate with all four cells) is omitted.
- Why it matters: Solver under-detects Type 5 patterns; difficulty rater may misclassify puzzles that need Type 5.
- Fix: Either implement Type 5 or update the docstring to acknowledge the gap explicitly.

**[MEDIUM] UR Type 6 roof arrangement assumed but never validated**
- File: `src/core/strategies/unique_rectangle_strategy.h:597-606`
- Issue: Type 6 only checks `roof.size()==2 && floor.size()==2`. In this strategy's call path, c1/c2 are always the two seed bivalue cells in the same row/column, so roofs are guaranteed to be on the same line as each other (not diagonal). Type 6's standard "Hidden X-Wing" pattern requires diagonal roofs. The row/col conjugate-both check happens to still be sound for this specific roof layout, but the structural assumption is implicit.
- Why it matters: Defensive — current correctness depends on an unstated invariant in `findStep`.
- Fix: Add an explicit `roof[0]` / `roof[1]` arrangement assertion.

**[MEDIUM] BUG strategy infrastructure exists for N≥2 but is dead code under MAX_TRIVALUE_CELLS=1**
- File: `src/core/strategies/bug_strategy.h:42, 162-218`
- Issue: `MAX_TRIVALUE_CELLS=1` (documented as "0 false positives for N=1 vs 18 for N>1"). All BUG+N≥2 logic is unreachable.
- Why it matters: Maintenance hazard. Anyone bumping `MAX_TRIVALUE_CELLS` to 2 will get the documented 18-false-positive regression with no compile-time signal.
- Fix: Either delete the N>1 paths entirely, or guard them behind an `if constexpr (MAX_TRIVALUE_CELLS > 1)` with a comment.

**[MEDIUM] Multi-Coloring Trap rescans entire board per cross-pair**
- File: `src/core/strategies/multi_coloring_strategy.h:262-311`
- Issue: For each of 4 cross-pairs per component pair, the inner double-loop iterates all 81 cells. With ~5 components on a hard puzzle, this is 10 component pairs × 4 cross-pairs × 81 cells × O(|complement|) sees-checks.
- Why it matters: Performance regression vector on Hard difficulty per project-context.md's >5% rule.
- Fix: Precompute "cells seeing each color class" once per component (bitset), then intersect masks per cross-pair.

**[LOW] UR Type 3 hardcoded to naked-pair subset, naked-triple/quad cases silently skipped**
- File: `src/core/strategies/unique_rectangle_strategy.h:416-418`
- Issue: `if (extras_count > 2) return std::nullopt;` — documented as "only naked-pair subset case." Naked triple/quad UR Type 3 patterns are valid eliminations.
- Why it matters: False negative on puzzles relying on UR Type 3 with ≥3-value subsets.
- Fix: Generalize to any subset size ≤ unit_cells - 2; or document as a known limitation.

**[LOW] Empty Rectangle enumerates all 9 box-cell positions as candidate elbows**
- File: `src/core/strategies/empty_rectangle_strategy.h:101-130`
- Issue: For an L-shape with row_count≥2 and col_count≥2, the elbow is uniquely the intersection of the occupied row and occupied column. The code enumerates all 9 (er_row, er_col) within the box and tests "all candidates in cross" for each.
- Why it matters: Up to 9× redundant work per box; matters on Hard puzzles where ER is repeatedly evaluated.
- Fix: Compute the unique elbow once from the L-shape.

**[LOW] Avoidable Rectangle robustness gap if board state is inconsistent**
- File: `src/core/strategies/avoidable_rectangle_strategy.h:89-93`
- Issue: There is no protection against the edge case where the puzzle has placed values that don't form a valid Sudoku (board corruption); strategy assumes well-formed board.
- Why it matters: Robustness gap if board state is inconsistent.
- Fix: Documented invariant in strategy comment.

**[LOW] Unique Loop DFS cycle search restricts neighbor>start_node but still O(N!) worst case**
- File: `src/core/strategies/unique_loop_strategy.h:217`
- Issue: `if (neighbor <= start_node) continue;` is the de-duplication, but on dense ab_cells graphs (many cells with {A,B}), the DFS still explores exponentially.
- Why it matters: Pathological puzzles with many bivalue cells could time out.
- Fix: Cap branching factor or switch to Johnson's algorithm for elementary cycles.

#### Notes
- All checks for `hasGivensInfo()` and `isGiven()` in Avoidable Rectangle look correct.
- Hidden UR `isDiagPartnerForced` properly implements the bestiary's three-condition rule (bivalue OR row-conjugate OR col-conjugate).
- 3D Medusa `hasCellColorInComponent` correctly uses component-local node lists (per bestiary fix).
- BUG parity table indexing `[v]` with v∈[1,9] on size-10 arrays — safe.
- UR Type 4 strong-link check correctly verifies floor cells are the only 2 strong-link cells.

#### Coverage
9 of 9 files reviewed.

---

### Appendix F — Chunk 6: Strategies ALS + forcing chains

#### Findings

**[HIGH] Cell-FC `findCommonElimination` does not harvest the surviving branch on contradiction-based deduction**
- File: `src/core/strategies/forcing_chain_strategy.h:128`
- Issue: `skip = {source_idx}` is passed, which excludes the source from elimination targets. Correct. But for the contradiction branch (line 109–120), `findCommonElimination` is not invoked at all. The code only checks `findCommonPlacement` paths there. With 1 contradicted branch out of 2, the surviving branch's eliminations on OTHER cells are sound but never harvested.
- Why it matters: Missed deductions only — not a wrong-elimination bug. Surfaces as suboptimal strategy.
- Fix: After confirming the surviving branch is consistent, also try elimination-finding using that single branch's mask differences vs `initial_masks`.

**[HIGH] ALS Chain admits chains where Z is a restricted common between A1 and An**
- File: `src/core/strategies/als_chain_strategy.h:219`
- Issue: `z_mask = common & ~used_rcs` excludes only RCs USED IN THE CHAIN. If Z is also a (latent) restricted common between A1 and An — not used as a chain link — the elimination logic is still correct (visibility-based), but description and rating mis-characterize the chain shape.
- Why it matters: Cosmetic/explanation correctness, not a soundness bug. Elimination is still valid.
- Fix: Optional — narrow to `~(used_rcs | A1↔An RCs)` and emit a different technique label when Z is RC.

**[MEDIUM] `propagateHiddenSingles` cross-unit conflict on same cell silently leaves the branch in an unflagged, undeducible state**
- File: `src/core/strategies/forcing_chain_helpers.h:166-277`
- Issue: When row says cell X = 3 and box/col says cell X = 5, `recordPlacement` marks `cell_placements[X] = -1` and skips. The branch state's masks remain unchanged at X, and `propagate` exits on `!progress`. The branch is NOT marked contradictory even though no value can satisfy both constraints.
- Why it matters: Such a branch is treated as valid in cross-branch comparisons. The risk is on OTHER cells where the stuck branch's incomplete propagation may incorrectly fail to eliminate.
- Fix: After collect-then-apply, if any `cell_placements[idx] == -1` AND no other progress was made this iteration, set `state.contradiction = true`.

**[MEDIUM] Sue de Coq `findCoveringALS` N=1 path — verified safe but invariant not stated**
- File: `src/core/strategies/sue_de_coq_strategy.h:266-284`
- Issue: A single bivalue cell in `line_remainder` and another in `box_remainder` are independent lookups; cross-side overlap is impossible because the remainders are disjoint regions. However, the N=2/N=3 branches don't check that the ALS chosen for line-side is disjoint from the ALS chosen for box-side at cell granularity beyond regional disjointness.
- Why it matters: Regional disjointness DOES prevent cell-level overlap. No actual bug.
- Fix: None needed. Note in-source the invariant `line_remainder ∩ box_remainder = ∅`.

**[MEDIUM] Cell-FC contradiction-based deduction skipped for 3-candidate cells with 2 contradictions**
- File: `src/core/strategies/forcing_chain_strategy.h:109`
- Issue: `cands.size() == 2 && branch_states.size() == 1` — gated to bivalue cells. For a 3-candidate cell where 2 of 3 assumptions contradict, the surviving candidate is forced but not recognized.
- Why it matters: Missed deduction; downgrades trivalue forcing-chain to bivalue only.
- Fix: Generalize to `branch_states.size() == 1 && (cands.size() - branch_states.size()) >= 1`.

**[LOW] ALS Chain DFS uses globally-distinct RC values rather than the looser "consecutive-distinct" rule**
- File: `src/core/strategies/als_chain_strategy.h:186-192`
- Issue: `used_rcs |= 1U << rc` prevents reusing any RC value anywhere in the chain. Hodoku-style ALS chains permit RC reuse as long as no two consecutive RCs are equal.
- Why it matters: Conservative — misses valid chains. No wrong eliminations.
- Fix: Track only the previous RC per node; reject only on consecutive equality.

**[LOW] Sue de Coq does not search N=4 covering ALSs**
- File: `src/core/strategies/sue_de_coq_strategy.h:269-328`
- Issue: With a 2-cell intersection of 5 candidates partitioned 2/3, the box-side could need a 4-cell ALS (5 candidates, 4 cells). Code only goes up to N=3.
- Why it matters: Missed patterns only.
- Fix: Add N=4 branch if performance budget allows.

**[LOW] `findCommonElimination` does not detect that the source cell's other candidates are forced-eliminated**
- File: `src/core/strategies/forcing_chain_helpers.h:386-394`
- Issue: `skip_indices` excludes source positions unconditionally. But for Unit/Region FC, if branches all eliminate a candidate from a source position OTHER than the one assumed, that's a real deduction.
- Why it matters: Missed deductions only.
- Fix: Don't skip source positions in elimination scan; only skip if the val being checked equals the assumed value at that source.

#### Notes
- Sue de Coq disjointness (a/b/c) is correctly enforced per bestiary.
- ForcingChain `findCommonElimination` correctly distinguishes placed-with-val from eliminated (bestiary item satisfied at lines 410-414).
- Death Blossom petal/stem disjointness is correctly enforced; stem-count guard rules out 1-petal degenerate.
- ALS-XZ/XYWing exclude all involved ALS cells from elimination targets.
- `sees(p, p) == true` is benign for the "sees all V-cells" checks because the stem/Z-cell membership is checked separately.

#### Coverage
9 of 9 files reviewed.

---

### Appendix G — Chunk 7: Strategies loops + cycles

#### Findings

**[HIGH] GroupedXCycles silently drops Type 2 deductions**
- File: `src/core/strategies/grouped_x_cycles_strategy.h:326-336`
- Issue: Closure handles only Type 1 (alternation) and Type 3 (weak-weak). When `first_link_strong && first_link_strong == next_strong` (strong-strong discontinuity), no `buildType2Step` is invoked even when `chain[0]` is a single-cell node.
- Why it matters: Valid placements (chain[0] = digit) are missed; correctness harness flags as suboptimal solver path, increasing reliance on more expensive strategies / backtracking.
- Fix: Add `else if (first_link_strong) { if (nodes[chain[0]].isSingle()) { auto r = buildType2Step(...); ... } }` branch.

**[HIGH] GroupedNiceLoop allows endpoint group to overlap chain interior cells**
- File: `src/core/strategies/grouped_nice_loop_strategy.h:347-381`
- Issue: `nodesConflict(start_node, end_node)` only checks start vs end overlap. The candidate endpoint may share cells with **interior** chain nodes (same digit, different node ID — e.g. interior single node `{r1c1}` + endpoint group `{r1c1,r1c2}`). `visited[]` is on node IDs, not cells, so reuse passes.
- Why it matters: AIC validity requires no (cell,digit) repetition; this can produce wrong Type 2 eliminations.
- Fix: After picking endpoint, verify no cell of `end_node` appears in any `chain[i]`'s cell set.

**[HIGH] Continuous Nice Loop "different digits same cell" weak-link rule eliminates without verifying cell isn't a chain pivot**
- File: `src/core/strategies/continuous_nice_loop_strategy.h:400-413`
- Issue: When `pos_a == pos_b && digit_a != digit_b`, code eliminates ALL OTHER candidates from that cell, but never confirms the loop's logic actually forces this. If both nodes in the cell happen to be linked via the **strong** intra-cell link (bivalue cell), they are also weakly linked — both edges exist in graph.
- Why it matters: Could over-eliminate when bivalue cell strong link is the actual chain link.
- Fix: Mark each chain link with the actual edge used; for cell-internal weak edges, require cell had count >= 3.

**[MEDIUM] NiceLoop Type 2 detection misses different-digit-same-cell discontinuity**
- File: `src/core/strategies/nice_loop_strategy.h:322-340`
- Issue: Discontinuous AIC check requires `start_digit == end_digit` and `start_pos != end_pos`. Misses the symmetric case where endpoints are in the same cell with different digits.
- Why it matters: Missed deductions; downstream strategies bear load.
- Fix: Add branch for `start_pos == end_pos && start_digit != end_digit` producing eliminations of OTHER candidates in that cell.

**[MEDIUM] Continuous Nice Loop closure-on-first-attempt-only via `break`**
- File: `src/core/strategies/continuous_nice_loop_strategy.h:319`
- Issue: `break` exits the weak-neighbor scan after the first hit (where `weak_neighbor == chain_start`). If `collectLoopEliminations` returned nullopt, the DFS continues extending, but only one closure was checked.
- Why it matters: After unsuccessful closure, DFS still extends (correct). Lower severity — flag for review.
- Fix: Verify intent; ensure unsuccessful closure doesn't short-circuit other extensions.

**[MEDIUM] Grouped X-Cycles weak link permits cross-unit "all-see" pairs without unit membership**
- File: `src/core/strategies/grouped_x_cycles_strategy.h:289-298`
- Issue: Weak links are added for any non-overlapping node pair whose cells all see each other, regardless of shared unit. A grouped node spanning a row-in-box and a separate single cell in another row that happens to see all group cells via box+column may form a degenerate "weak link" that doesn't correspond to a single-unit constraint.
- Why it matters: Such a "weak link" is technically valid (mutual exclusion holds via combined unit constraints), but groupings outside a single shared unit can produce non-standard AICs.
- Fix: Confirm via correctness harness; if false positives appear, restrict weak links to nodes within a single shared unit.

**[MEDIUM] Continuous Nice Loop bivalue-cell weak edge always present alongside strong creates parallel edges**
- File: `src/core/strategies/continuous_nice_loop_strategy.h:227-243`
- Issue: For a count==2 cell, both strong AND weak edges are added between the same node pair. DFS can traverse the "weak" representation when the only forced constraint is via the "strong" interpretation.
- Why it matters: A bivalue strong link selected as a "weak" hop yields no actual elimination. Harmless but wastes work and risks parity tracking bugs.
- Fix: Skip adding cell-internal weak edge when count == 2; rely on the strong edge only.

**[MEDIUM] X-Cycles weak link omits direct conjugate pairs as candidate weak edges**
- File: `src/core/strategies/x_cycles_strategy.h:83-93`
- Issue: `buildWeakLinks` connects every visible candidate pair, including conjugate-pair cells which are also strong. The Type 1 detection (`first_link_strong != next_strong`) at closure assumes the closing weak link is genuinely weak. With parallel strong+weak edges between same pair, the closure could be reached by either edge.
- Why it matters: AIC interpretation is correct (strong implies weak), but the duplicate edges inflate DFS branching factor.
- Fix: Deduplicate or document intent.

**[LOW] NiceLoopStrategy `addUnitLinks` carries unused boolean parameter**
- File: `src/core/strategies/nice_loop_strategy.h:151`
- Issue: `bool /*is_row*/` parameter is never used.
- Why it matters: Dead parameter; readability churn.
- Fix: Remove parameter.

**[LOW] GroupedXCycles `cellSeesNode` requires non-equality but elimination loop already filters by `in_chain`**
- File: `src/core/strategies/grouped_x_cycles_strategy.h:89-92, 393-398`
- Issue: Defensive but redundant given `in_chain` filtering.
- Why it matters: None operationally.
- Fix: None required.

**[LOW] NiceLoopStrategy `findAICEliminations` reports off-by-one in chain length**
- File: `src/core/strategies/nice_loop_strategy.h:388-390`
- Issue: Explanation says length `chain.size() + 1`. Format string says "AIC of length N" where N = node count, not link count.
- Why it matters: Cosmetic — UI consistency.
- Fix: Confirm UI conventions.

#### Notes
- Loop strategies share substantial graph-building code (NiceLoop ≈ ContinuousNiceLoop). Consider extracting `buildLinkGraph` helper.
- GroupedXCycles starts DFS from BOTH single and group nodes (line 175 lacks `isSingle()` filter unlike GroupedNiceLoop) — Type 3 result rejects non-single chain[0], but Type 1 fires fine. Difference in strategies is intentional but undocumented.
- No `static_assert` enforces `MAX_CHAIN_LENGTH`/`MAX_CHAIN_LEN` consistency across the five strategies (values: 12, 12, 10, 10, 8).

#### Coverage
5 of 5 files reviewed.

---

### Appendix H — Chunk 8: Strategies exotic

#### Findings

**[BLOCKER] Kraken: primaries/secondaries axis confused — wrong rows skipped**
- File: `src/core/strategies/kraken_fish_strategy.h:243-249`
- Issue: Outer loop iterates `secondary` over [0,BOARD_SIZE) but the skip check uses `std::ranges::contains(base_primaries, secondary)`. `base_primaries` are the base-line indices (e.g., rows for row-fish), which run along the PRIMARY axis. The variable `secondary` ranges over the perpendicular axis, so this skip never matches the intended "skip cells in base lines" semantics.
- Why it matters: For row-fish, this should skip rows in `base_primaries`; instead, it skips columns whose index numerically coincides with a base-row index. Targets in the actual base rows are wrongly considered, and unrelated cover lines may be wrongly skipped — producing both invalid Kraken eliminations and missed ones.
- Fix: Test `getRow(row)`/`getCol(col)` against `base_primaries` after computing `row`/`col` for the by_row/by_col case, not the loop axis.

**[BLOCKER] Junior Exocet: missing base-band / S-cell / cover-house validation**
- File: `src/core/strategies/junior_exocet_strategy.h:113-192`
- Issue: A valid JE requires the two target cells lie in the OTHER two bands (one each), with S-cells in the target columns/band acting as cover-houses for each base candidate. The implementation just checks "T1, T2 outside base band, different boxes" and emits eliminations. There is no S-cell / cover-house / mini-line check.
- Why it matters: Eliminates non-base candidates from arbitrary unrelated cells that happen to share a column with a base — produces unsound eliminations on most puzzles.
- Fix: Implement the JE cover-house validation (per Champagne's definition) or restrict to verified cross-line constraints before eliminating.

**[BLOCKER] Junior Exocet: T1 and T2 may share the same band (must be different)**
- File: `src/core/strategies/junior_exocet_strategy.h:174-183`
- Issue: T1 and T2 are only required to be in different BOXES, not different BANDS. A JE requires the two targets occupy the two non-base bands (one target per band). With only "different boxes", both targets can sit in the same non-base band.
- Why it matters: Pattern is not a Junior Exocet; eliminations are invalid.
- Fix: Require `(t1.row / BOX_SIZE) != (t2.row / BOX_SIZE)` and both differ from `base_band_start / BOX_SIZE`.

**[BLOCKER] Junior Exocet: every base candidate must appear in at least one target — check missing**
- File: `src/core/strategies/junior_exocet_strategy.h:99-104`
- Issue: `tryBasePair` is called with `row_cells[i]` and `row_cells[j]` as base candidates. The search never validates that every base candidate appears in at least one target. JE requires each base candidate appears in at least one of T1/T2; that check is missing.
- Why it matters: Allows pseudo-JE patterns where a base candidate has no S-cell anywhere — invariant violated, eliminations unsound.
- Fix: After collecting T1/T2 candidates, require `(t1_mask | t2_mask) & base_mask == base_mask` (each base candidate appears in at least one of T1/T2).

> **UPDATE 2026-05-30 (issue #21) — both HIGH fish findings VERIFIED SOUND; resolved as hardening.**
> On close analysis the two HIGH items below are **not live correctness bugs**:
> - **Franken Fish:** cover units are only ever the complementary line type (cols when base = rows+boxes, rows when base = cols+boxes); boxes are never cover-eligible. So a cover unit can never *equal* a base unit (different `UnitType`), and covers are mutually parallel lines that can never share a cell. Both degeneracies are impossible by construction.
> - **Mutant Fish:** overlapping cover sets are **sound** (cover-set pigeonhole theorem). With base cells cell-disjoint (the real load-bearing check, already present), the N base houses give N *distinct* true positions; an eliminated cover-not-base cell Z would saturate one cover house, forcing those N trues into N−1 remaining cover houses — a contradiction, regardless of cover overlap. The suggested "reject cover overlap" fix would *discard valid Mutant fish and weaken the solver*, so it was deliberately **not** applied.
>
> Resolution: proof comments + `static_assert` on the cover bitmask width added to both strategies; deterministic overlapping-cover regression tests added (`test_mutant_fish_strategy.cpp`, `test_franken_fish_strategy.cpp`) plus a 1000-puzzle generative soundness sweep (`test_strategy_correctness.cpp`) — all green, zero wrong deductions. The two MEDs below are micro-optimizations with no soundness impact (the bit-width has margin: 27 units ≤ 32 bits).

**[HIGH] Franken Fish: cover units may overlap base units (degenerate fish)**
- File: `src/core/strategies/franken_fish_strategy.h:178-203, 282-316`
- Issue: Bestiary calls out base-cell overlap (correctly checked at 228-234). But the symmetric danger — a cover unit equal to a base unit, or a cover unit whose cells fully overlap a base unit — is not rejected. Per Franken Fish definition, a chosen cover should not be one of the chosen bases.
- Why it matters: A row chosen as both base and cover trivially "covers itself", giving zero genuine eliminations or spurious ones.
- Fix: Reject cover combinations that include a unit equal to any chosen base (as Mutant does at 287-296).

**[HIGH] Mutant Fish: cover-set degeneracy check missing for overlapping cover cells**
- File: `src/core/strategies/mutant_fish_strategy.h:329-352`
- Issue: Base overlap is rejected (242-248). Cover-cell overlap among the chosen covers is silently allowed; the cover-cells list is dedup'd at 381-383 before elimination. For Mutant fish, overlapping covers reduce the effective cover-cell count and can produce a valid-looking but uneliminating pattern.
- Why it matters: Pattern strength is overestimated; eliminations from the difference set may not be sound.
- Fix: After building chosen_covers, verify total cover-cell count >= total base-cell count, or reject when any two chosen covers share a cell.

**[HIGH] Kraken Fish: degenerates to plain finned-fish when `kraken_elims` empty but no fin-box eliminations exist**
- File: `src/core/strategies/kraken_fish_strategy.h:191-193`
- Issue: The "kraken_elims.empty() → continue" guard is correct in spirit, but `free_elims` (the finned eliminations in fin's box) is still combined into `all_elims` and reported as Kraken eliminations. So a step labelled "Kraken Fish" can have all its eliminations be plain finned-fish eliminations.
- Why it matters: Misattribution of technique (rating + UI explanation). Breaks difficulty rating semantics and the strategy correctness harness's technique-tagging assumptions.
- Fix: Either report only `kraken_elims`, or split into two SolveSteps (or require kraken_elims to drive `position` and roles).

**[HIGH] Kraken Fish: `explanation_data` uses `RegionType::Row/Col` from `by_row` but Kraken targets cross multiple cover lines**
- File: `src/core/strategies/kraken_fish_strategy.h:218-228`
- Issue: `region_type` is set to the base-line orientation, but Kraken eliminations sit on the COVER lines (perpendicular). UI highlight/region rendering will draw the wrong axis.
- Why it matters: Misleading explanation/highlight; user-visible.
- Fix: Use the cover-axis `RegionType` (inverse of by_row), or omit region_type for Kraken.

**[MEDIUM] Junior Exocet: target cells permitted with NO base candidate doesn't terminate, but eliminations still computed**
- File: `src/core/strategies/junior_exocet_strategy.h:146-149, 165-168`
- Issue: T1/T2 candidates require `(t_mask & base_mask) != 0` (at least one base candidate). Correct lower bound, but no upper bound: a target cell with only ONE base candidate plus several non-base candidates is treated identically to one with all base candidates.
- Why it matters: Combined with the missing cover-house check, this rubber-stamps eliminations where the JE invariant doesn't hold.
- Fix: Require the union `(t1_mask | t2_mask) ⊇ base_mask`.

**[MEDIUM] Franken Fish: cover units allowed with only 1 candidate cell — wastes coverage slot**
- File: `src/core/strategies/franken_fish_strategy.h:166, 183`
- Issue: Cover units allowed with only 1 candidate cell. A cover unit must contribute coverage to at least one base cell; if its only candidate cell isn't a base cell, the cover wastes a slot.
- Why it matters: Loosens the cover argument's strength; combined with missing same-unit-as-base rejection, can produce wrong eliminations.
- Fix: Filter cover_units to those that cover at least one base cell before enumeration (or require >=2 candidate cells).

**[MEDIUM] Mutant Fish: `cell_cover_masks` indexing inconsistency between fill and check**
- File: `src/core/strategies/mutant_fish_strategy.h:306-311, 354-356`
- Issue: `cell_cover_masks` bit `ci_idx` corresponds to position in `cover_indices`. In `enumerateCoverCombinations` the loop variable `i` ranges over `cover_indices.size()` and `cover_mask |= (1U << i)`. So `i` IS the cover_indices position — consistent. But the bitmask covers 32 bits and `cover_indices.size()` can reach 27 — safe by 1.
- Why it matters: Latent risk; no current bug.
- Fix: Add `assert(cover_indices.size() <= 32)` or switch to `uint64_t`.

**[LOW] Junior Exocet: `searchBox` only searches one row per box per base pair; companion-cell symmetry missing**
- File: `src/core/strategies/junior_exocet_strategy.h:81-107`
- Issue: Search iterates base bands horizontally (rows in box). Vertical (column-based) JE patterns — bases in same mini-column with targets sharing rows — are never searched.
- Why it matters: Misses ~half of valid Junior Exocet patterns.
- Fix: Mirror the search for column-based bases.

#### Notes
- Junior Exocet appears under-validated relative to the formal definition; recommend adding regression-test puzzles against a known JE-solver oracle in `test_strategy_correctness.cpp`.
- Kraken: contradiction-on-fin-placement (lines 279-290) returning `true` is technically right ("vacuous truth") but should also report the simpler conclusion that the fin can't be the truth-cell.
- Franken/Mutant: explanation `positions` only includes base cells, not fin/cover cells — UI will not highlight the eliminated cells' cover context.

#### Coverage
4 of 4 files reviewed.

---

### Appendix I — Chunk 9: Puzzle pipeline + validator + i18n

#### Findings

**[BLOCKER] PuzzleAnalyzer methods crash on null deps in default-constructed instances**
- File: `src/core/puzzle_analyzer.cpp:116, 125, 145, 149`
- Issue: `validate()` derefs `validator_`, `checkUniqueness()` derefs `counter_`, `scoreDifficulty()` derefs both `solver_` and `counter_`. `PuzzleAnalyzer()` default ctor (header line 134) leaves them as null `shared_ptr`s. Header comment ("Each dependency is optional for partial setups") promises this is safe — but the methods have zero precondition checks.
- Why it matters: a caller that constructs `PuzzleAnalyzer{}` for "parsing only" and then accidentally calls `validate()` segfaults instead of getting a clean `expected` error.
- Fix: add `if (!validator_) return std::unexpected(BoardValidationError{...})` (and equivalents) at the top of each method.

**[HIGH] Generated puzzle's `seed` is unreproducible when caller requests random seed**
- File: `src/core/puzzle_generator.cpp:79, 121, 161-168`
- Issue: Line 79 picks `random_device{}()` when `settings.seed == 0`, but line 121 stores `result.seed = settings.seed` — i.e. stores the sentinel 0, not the actual seed used. `generatePuzzle(Difficulty)` always passes seed=0, so every Difficulty-only generation produces a Puzzle with `seed=0` that cannot be regenerated.
- Why it matters: bestiary "RNG seeding not deterministic / not injectable" — debugging a bad puzzle is impossible because you can't reproduce it.
- Fix: capture the effective seed into a local before constructing rng, then assign it to `result.seed`.

**[HIGH] PuzzleRater excludes Backtracking → puzzles requiring brute force rate as Easy and pass acceptance**
- File: `src/core/puzzle_rater.cpp:63-71` (with `puzzle_generator.cpp:130-145`)
- Issue: `calculateRating()` does `if (step.technique != Backtracking) max_rating = max(...)`. A board solvable only by trivial techniques up to (say) NakedSingle then brute force returns `se_rating = 2.3` with `requires_backtracking = true`. Generator's range check accepts it for Easy difficulty.
- Why it matters: rating-vs-actual-difficulty mismatch user-facing; defeats the purpose of difficulty bins.
- Fix: when `requires_backtracking`, force the rating to `getTechniqueRating(Backtracking)` (12.0); or reject any puzzle with `requires_backtracking` for non-Master tiers.

**[HIGH] `PuzzleRater::ratePuzzle` calls solver with no wall-clock budget**
- File: `src/core/puzzle_rater.cpp:36`
- Issue: Uses `solver_->solvePuzzle(board)` (no budget overload). MEMORY.md's "solvePuzzle livelock on AI Escargot" was explicitly resolved by adding budget overloads — this caller bypasses them.
- Why it matters: regression of the livelock fix when rating is invoked on adversarial or in-flight-generated boards; generator can hang forever on a single rejected puzzle.
- Fix: take a `std::chrono::milliseconds` budget in `IPuzzleRater::ratePuzzle` and call the budget overload.

**[MEDIUM] `validatePuzzle` returns true on an all-empty board**
- File: `src/core/puzzle_generator.cpp:202-214`
- Issue: `validateBoard` returns `findConflicts().empty()`; on an all-zero board, no cell has a conflict. Combined with the value-range loop only rejecting out-of-range cells, an empty board is "valid".
- Why it matters: edge-case from the brief; the docstring "Validates that a puzzle is properly formed" doesn't match.
- Fix: require at least one filled cell, or document the semantics explicitly.

**[MEDIUM] `locale_utils::isValidLocaleCode` rejects BCP-47 forms with hyphen separator**
- File: `src/core/locale_utils.cpp:57`, `locale_utils.h:26-39`
- Issue: Only accepts `^[a-z]{2,3}(_[A-Z]{2})?$` (underscore). BCP-47 canonical form uses `-` (e.g., `pt-BR`).
- Why it matters: bestiary "locale string with mixed case / underscore vs hyphen". A user with `LANG=pt-BR` would silently fall back to English.
- Fix: rename to `isValidQmFilenameCode` to make intent explicit, OR additionally accept hyphen form and normalize.

**[MEDIUM] `PuzzleAnalyzer::scoreDifficulty` has unreachable `NoSolution` fallback masking new SolverError values**
- File: `src/core/puzzle_analyzer.cpp:151-158`
- Issue: The switch over `result.error()` is exhaustive (Timeout/Unsolvable/InvalidBoard each return). Line 158 falls through silently to `NoSolution` if a future `SolverError` enumerator is added without updating this site.
- Why it matters: silent failure mode. A new `SolverError::OutOfMemory` would be re-labelled `NoSolution`.
- Fix: replace fallthrough with `std::unreachable()`.

**[MEDIUM] `localized_explanations.h` runs translator-supplied strings through `fmt::runtime` — bad translation crashes the app**
- File: `src/core/localized_explanations.h` (every template, e.g. line 31, 82, 99, 211, …)
- Issue: `fmt::format(fmt::runtime(core::loc(...)), ...)` parses the translated string as a format spec at runtime. A translator who turns `"R{0}C{1}"` into `"R{}C{}"` (positional → automatic) or breaks a brace produces `fmt::format_error` thrown at runtime — uncaught in this layer.
- Why it matters: translation files are external content. One bad PO/QM file → app crash on first solve-step explanation.
- Fix: wrap each `fmt::format(fmt::runtime(...))` site in a try/catch and fall back to `step.explanation`, OR validate templates at translation-load time, OR move to Qt's `tr("R%1C%2").arg(...)`.

**[MEDIUM] `runRemovalOrdering` ignores `max_clues` (parameter unused)**
- File: `src/core/puzzle_generator.cpp:226-248` (signature `int /*max_clues*/`)
- Issue: Parameter is unused; comment at 236-237 explicitly says "Skipping checks above max_clues caused non-unique puzzles to slip through" — fine — but the parameter is then dead in the signature.
- Why it matters: dead parameter is a landmine (someone re-adds the gate).
- Fix: delete the parameter (and adjust the call site).

**[MEDIUM] `generatePuzzle` rng is shared across attempts but `result.seed` is the user seed**
- File: `src/core/puzzle_generator.cpp:79, 82, 121`
- Issue: With seed=42 and max_attempts=1000, attempt N consumes RNG state from attempts 0..N-1. Storing `result.seed = 42` is misleading.
- Why it matters: silent reproducibility loss.
- Fix: re-seed `rng` inside the attempt loop (e.g. `mt19937 rng(seed + attempt)`), or store the attempt index alongside the seed.

**[LOW] `i18n_helpers.h::locFormat` still in repo despite "DELIBERATELY REMOVED" claim**
- File: `src/core/i18n_helpers.h:38-49`
- Issue: Project context (and earlier commit f0e5567) state `locFormat` was removed; the template is still defined.
- Why it matters: doc/code drift.
- Fix: actually delete `locFormat`, or update project-context.md to match reality.

**[LOW] `BoardData` initializer-list constructor silently truncates oversized input**
- File: `src/core/board_data.cpp:21-37`
- Issue: `if (row >= BOARD_SIZE) break;` and `if (col >= BOARD_SIZE) break;` silently drop extra cells/rows.
- Why it matters: silent failure category.
- Fix: assert/static_assert when input dimensions exceed 9.

**[LOW] `runRecompletionPhase` uses biased modulo on `rng()`**
- File: `src/core/puzzle_generator.cpp:268`
- Issue: `2 + static_cast<int>(rng() % 2)` — modulo on `mt19937::result_type`. Bias negligible.
- Why it matters: determinism hygiene; future copy/paste with larger modulus will be biased.
- Fix: use `std::uniform_int_distribution<int>{2, 3}(rng)`.

#### Notes
- `solving_technique.h` ↔ `technique_descriptions.h` case-set is exhaustive and matched. The 54 strategies in MEMORY.md all appear in both.
- `board_serializer.cpp` round-trip looks correct.
- `GameValidator::isComplete` correctly handles fully-filled-but-conflicting boards.
- `localized_explanations.h` and `technique_descriptions.h` SKIM only — no smart-quote / RTL / UTF-8 escaping issues spotted.

#### Coverage
21 of 21 files reviewed (`localized_explanations.h` and `technique_descriptions.h` skimmed per brief; rest deep-read).

---

### Appendix J — Chunk 10: Persistence + statistics

#### Findings

**[BLOCKER] Save loader rejects all Master-difficulty puzzles**
- File: `src/core/save_manager_serialization.cpp:52-53, 278-283`
- Issue: `MAX_DIFFICULTY = 3` ("Expert") gates `difficulty` validation, but the enum has `Master = 4`. Any save of a Master puzzle fails to deserialize.
- Why it matters: Permanent data loss — Master-difficulty saves can never be reloaded; auto-save round-trip silently breaks on Master.
- Fix: Set `MAX_DIFFICULTY = static_cast<int>(Difficulty::Master)` and add a regression test loading a Master save.

**[BLOCKER] Zlib decompression bomb — no max output cap**
- File: `src/core/save_manager_compression.cpp:74-91`
- Issue: `decompressData` grows buffer up to `BUFFER_SIZE_MULTIPLIER^MAX_DECOMPRESSION_ATTEMPTS = 2^10 = 1024×` the compressed input, with no absolute cap.
- Why it matters: DoS via OOM on import or on a tampered plain-YAML sessions file.
- Fix: Add an absolute max size guard (e.g. 64 MB), abort with `CompressionError` when exceeded.

**[HIGH] Save write is not atomic — partial-write corruption on crash**
- File: `src/core/save_manager_serialization.cpp:180-185`
- Issue: `serializeToYaml` writes directly to the final `file_path` with `std::ofstream`. Contrast with `StatisticsManager::atomicWriteFile` which correctly uses tmp+rename.
- Why it matters: Single power loss during auto-save → user loses entire game state.
- Fix: Write to `path + ".tmp"`, fsync, `std::filesystem::rename` to target — mirror `atomicWriteFile`.

**[HIGH] `getAllSessions` silently discards corrupt encrypted sessions file**
- File: `src/core/statistics_manager.cpp:238-250`
- Issue: When `EncryptionManager::decrypt` fails, the code logs a warning and treats sessions as empty. The subsequent `flushSessions` then overwrites the file via `atomicWriteFile`, destroying the original corrupt-but-possibly-recoverable bytes.
- Why it matters: A single transient decryption failure wipes the entire statistics history.
- Fix: Return `StatisticsError::FileAccessError` from `getAllSessions` on decrypt failure; do not overwrite an unreadable file.

**[HIGH] `recalculateAggregateStats` recursively double-counts pending sessions**
- File: `src/core/statistics_manager.cpp:613-647, 691-697`
- Issue: `recalculateAggregateStats` calls `getAllSessions()`, which appends `pending_sessions_` to disk-loaded sessions. But pending sessions have already been `updateAggregateStats()`'d at `endGame` time.
- Why it matters: Stats drift upward over time; `best_win_streak`/`total_games` inflate silently.
- Fix: Track which sessions have already been folded into `cached_stats_`, or recompute purely from disk and clear `pending_sessions_` before recalc.

**[HIGH] Decompression magic-byte sniff is ambiguous post-decryption**
- File: `src/core/save_manager_serialization.cpp:220-235`
- Issue: After AEAD-authenticated decryption, the code re-sniffs zlib magic on plaintext. Plaintext YAML starting with the byte `0x78` followed by one of the four checked low bytes triggers a spurious decompression attempt.
- Why it matters: False-positive corruption rejection of legitimate plaintext saves.
- Fix: Persist a compression flag in the encryption header's reserved flags byte instead of byte-sniffing.

**[HIGH] `isValidSaveFile` fully deserializes for cheap validity check**
- File: `src/core/save_manager_compression.cpp:105-123`, called from `save_manager.cpp:197` (`hasAutoSave`), `342` (`validateSave`), `233` (`listSaves`)
- Issue: `isValidSaveFile` calls `deserializeFromYaml` — full read + decrypt (Argon2id MODERATE KDF, ~500ms) + decompress + parse. `listSaves` then calls `deserializeFromYaml` again per save.
- Why it matters: UI freeze on save-list dialogs, seconds-scale latency for N saves. Violates project's "no blocking calls on the main thread" rule.
- Fix: Make `isValidSaveFile` check magic bytes / header only; cache `listSaves` result.

**[HIGH] `EncryptionManager` constructor throws on libsodium init failure**
- File: `src/core/encryption_manager.cpp:43-50`
- Issue: Constructor throws `std::runtime_error` when `sodium_init() < 0`. Per project rules: `std::expected` for expected failures.
- Why it matters: Uncaught exception in `SaveManager`/`StatisticsManager` constructors → app crash on startup.
- Fix: Add a static factory returning `std::expected<std::unique_ptr<EncryptionManager>, EncryptionError>`.

**[HIGH] `const_cast` on `std::string::data()` violates project rule**
- File: `src/core/encryption_manager.cpp:210`
- Issue: `sodium_memzero(const_cast<char*>(system_id.data()), system_id.size());` — `project-context.md` explicitly forbids `const_cast`.
- Why it matters: Violates the stated invariant.
- Fix: Drop the cast (use the non-const overload), or zero a `std::vector<char>` copy instead of the `std::string`.

**[MEDIUM] Integer overflow in average-time recomputation on import**
- File: `src/core/statistics_manager.cpp:389-393`
- Issue: `total_time = average_times[i] * games_completed[i] + imported.average_times[i] * imported.games_completed[i]` — the formula divides by the **new** total after the lines above, and the formula already updated `games_completed[i]` so the divisor is wrong.
- Why it matters: Average-time stat is corrupted on every import; can never be repaired.
- Fix: Compute weighted average from pre-update counts BEFORE incrementing `games_completed`.

**[MEDIUM] `exportGameSessionsCsv` uses non-thread-safe `std::localtime`**
- File: `src/core/statistics_serializer.cpp:411`
- Issue: `auto tm = *std::localtime(&time_t);` — `localtime` returns a pointer to a shared static buffer.
- Why it matters: Subtle data corruption in CSV export rows; inconsistent with rest of codebase (`save_manager.cpp:98-101` uses localtime_r).
- Fix: Use `localtime_r(&time_t, &tm)` (POSIX) / `localtime_s` (Win).

**[MEDIUM] `recordMove`/`recordHint` counters overflow silently — `int`**
- File: `src/core/statistics_manager.cpp:124-128, 140-142`; `i_statistics_manager.h`
- Issue: `moves_made`, `hints_used`, `total_moves`, `total_mistakes`, `current_win_streak` are all `int`. Counter wrap at 2^31 for power users.
- Why it matters: Statistics corruption for long-lived players.
- Fix: Use `int64_t`/`uint64_t` for cumulative counters; compute averages as `running_mean += (sample - mean) / n`.

**[MEDIUM] Move history `timestamp` round-trip uses wrong clock**
- File: `src/core/save_manager_serialization.cpp:123-124, 378-380`
- Issue: Serializes `move.timestamp.time_since_epoch()` (a `steady_clock::time_point`) as ms since epoch, then on load reconstructs as `steady_clock::time_point` from that value. `steady_clock`'s epoch is unspecified.
- Why it matters: Move-history timestamps are silently wrong after any save/load.
- Fix: Persist as `system_clock` (or as offset from `created_time`); document the clock choice.

**[MEDIUM] `serializeGameStatsToYaml` rewrites entire sessions file each append**
- File: `src/core/statistics_serializer.cpp:184-235`
- Issue: With `append=true`, the function loads the existing YAML (entire history), appends one entry, and rewrites the whole file non-atomically.
- Why it matters: UI hitch on session record + risk of catastrophic stats loss on crash.
- Fix: Use `atomicWriteFile`; consider append-only line-delimited format if scale demands it.

**[LOW] `generateSaveId` collisions not checked**
- File: `src/core/save_manager.cpp:403-417`
- Issue: 16-hex-char ID has 64 bits of entropy from a non-seeded `mt19937`. `saveGame` does not check whether `getSavePath(save_id)` already exists before writing.
- Why it matters: Theoretical data loss on collision; `SaveIdExists` error in `SaveError` enum is declared but never produced.
- Fix: After generating, check `std::filesystem::exists(getSavePath(id))` and regenerate (bounded retries).

#### Notes
- `EncryptionManager` design (XSalsa20-Poly1305 AEAD + per-message random salt/nonce + Argon2id) is sound. Nonce is generated fresh per encryption.
- Save format version bump policy (`SAVE_FILE_VERSION = "1.1"` with backward-compatible load) is sensible.
- Settings file is plain unencrypted YAML — appropriate (no secrets), uses defaults on parse failure, validates locale codes.
- `statistics_manager.cpp` destructor flushes on shutdown — acceptable.

#### Coverage
11 of 16 files reviewed in depth; `i_statistics_manager.h`, `i_settings_manager.h`, `settings_manager.h`, `statistics_serializer.h` skimmed as interface-only per brief; `save_manager.h` and `encryption_manager.h` fully read.

---

### Appendix K — Chunk 11: Training subsystem

#### Findings

**[BLOCKER] Deserializer drops techniques with enum value > GroupedXCycles**
- File: `src/core/training_statistics_serializer.cpp:74`
- Issue: `for (int i = 0; i <= static_cast<int>(SolvingTechnique::GroupedXCycles); ++i)` only iterates 0..41. Techniques 42-53 (SashimiXWing, SashimiSwordfish, SashimiJellyfish, UnitForcingChain, RegionForcingChain, MutantFish, KrakenFish, ALSChain, JuniorExocet, UniqueLoop, ContinuousNiceLoop, GroupedNiceLoop) are never matched by name.
- Why it matters: stats for 12 of 54 techniques silently vanish on reload.
- Fix: iterate over `kAllTechniques` (or use a static name→enum map covering all 54).

**[HIGH] `recordLesson` average_hints math is wrong and divides by zero**
- File: `src/core/training_statistics_manager.cpp:52-55`
- Issue: `total_sessions = total_exercises_attempted / result.total_exercises` — but `total_exercises_attempted` already includes the just-added session. If `result.total_exercises == 0` this divides by zero.
- Why it matters: incremental mean is statistically wrong; UB on zero exercises.
- Fix: store a `session_count` field; guard `result.total_exercises > 0`.

**[HIGH] `recordLesson` accepts pathological input without validation**
- File: `src/core/training_statistics_manager.cpp:44-74`
- Issue: No validation that `result.correct_count >= 0`, `result.correct_count <= result.total_exercises`, `result.hints_used >= 0`, or `result.total_exercises > 0`.
- Why it matters: stats integrity, save-file tampering / serialization corruption is undefended.
- Fix: reject invalid inputs with `std::unexpected(TrainingStatsError::InvalidData)`; clamp on deserialize.

**[HIGH] Coloring interaction mode mismatch — 3D Medusa is misclassified**
- File: `src/core/training_exercise_generator.cpp:170-172` vs `src/core/training_types.h:126-129`
- Issue: `getInteractionMode` in the generator omits `ThreeDMedusa`, falling through to `default → Elimination`. But `interactionModeForStep` in types.h returns `Coloring` for ThreeDMedusa.
- Why it matters: 3D Medusa training exercises will present in Elimination mode (no color buttons).
- Fix: add `case ThreeDMedusa: return Coloring;` and consider unifying with `interactionModeForStep`.

**[HIGH] Per-step interaction-mode override missing for placement-capable chain techniques**
- File: `src/core/training_exercise_generator.cpp:88-93`
- Issue: Only `ForcingChain` gets per-step override between Placement/Elimination. `UnitForcingChain`, `RegionForcingChain`, `KrakenFish`, `BUG`, `JuniorExocet` etc. can also produce `SolveStepType::Placement` steps.
- Why it matters: a chain placement exercise asks the user to "eliminate candidates" when the actual step is to place a value — unanswerable.
- Fix: drive `exercise_mode` from `path[i].type` for every technique that can yield placements.

**[HIGH] `validatePlacement` does not validate `value` range**
- File: `src/core/training_answer_validator.cpp:152-183`
- Issue: Position bounds are checked but `value` is not constrained to `[MIN_VALUE..MAX_VALUE]`. `candidates.isAllowed(r, c, 0)` or `(..., 10)` is called with whatever the View passes.
- Why it matters: View bug or hostile save file could crash / produce nonsense placements.
- Fix: `if (value < MIN_VALUE || value > MAX_VALUE) return std::nullopt;` at top.

**[MEDIUM] `findAllSteps` may not enumerate every valid technique instance**
- File: `src/core/training_answer_validator.cpp:198-229`
- Issue: Strategy's `findStep` returns one step per call, then the loop applies it and re-runs. If the strategy's elimination is incompatible (changes the pattern), the second valid instance is lost.
- Why it matters: player picks a correct alternative solution and is told "incorrect".
- Fix: snapshot the candidate grid before each call; or call a strategy method that returns all instances at the current state.

**[MEDIUM] `reconstructCandidates` assumes stored mask is a strict subset of propagated mask**
- File: `src/core/training_answer_validator.cpp:231-255`
- Issue: only `current_mask & ~stored_mask` is eliminated. If `stored_mask` has a bit set that the constraint-propagated grid disallows, the candidate is silently dropped without warning.
- Why it matters: silent data-loss class of bug; replays after save format changes give wrong eliminations.
- Fix: log/warn when `stored_mask & ~current_mask != 0`.

**[MEDIUM] `reconstructCandidates` doesn't validate `candidate_masks.size()`**
- File: `src/core/training_answer_validator.cpp:241`
- Issue: Indexed access `candidate_masks[(r * BOARD_SIZE) + c]` with no `.size()` check.
- Why it matters: crash on tampered/corrupt training save.
- Fix: early return `std::nullopt`/`unexpected` if size != 81.

**[MEDIUM] `walkForward` doesn't bounds-check `target_index`**
- File: `src/core/training_exercise_generator.cpp:181-211`
- Issue: no precondition that `target_index <= solve_path.size()`.
- Why it matters: latent crash on refactor.
- Fix: `if (target_index > solve_path.size()) return std::nullopt;` early.

**[MEDIUM] `generateExercises` non-deterministic — no seed contract**
- File: `src/core/training_exercise_generator.cpp:39-125` and `i_training_exercise_generator.h:33-38`
- Issue: Generator calls `generator_->generatePuzzle(difficulty)` repeatedly with no seed/rng injection.
- Why it matters: tests cannot pin a specific exercise sequence; reproducing user reports requires luck.
- Fix: accept an optional seed parameter on the interface; thread through to `IPuzzleGenerator`.

**[MEDIUM] `recordLesson` `mastered_count` / `proficient_count` unbounded; perfect-streak overflow**
- File: `src/core/training_statistics_manager.cpp:67-72`
- Issue: counters are `int` with no upper bound.
- Why it matters: potential UB on int overflow with `-Werror` strict flags.
- Fix: clamp counters at meaningful maxima before writing; use `int64_t` for cumulative totals.

**[LOW] `getTrainingHint` accepts arbitrary `level` with implicit branches**
- File: `src/core/training_hints.cpp:48-294`
- Issue: doc says levels 1-3, but code dispatches `level == 1 / level == 2 / else`. `level == 0` or `level >= 4` both silently fall into the level-3 branch — revealing the answer regardless of intent.
- Why it matters: caller bug or off-by-one accidentally reveals the answer.
- Fix: guard `level >= 1 && level <= 3` at entry.

**[LOW] `validateElimination` matches exact set — multi-step eliminations unmatchable**
- File: `src/core/training_answer_validator.cpp:185-196`
- Issue: requires `eliminationsToSet(step) == player_eliminations` exact equality.
- Why it matters: pedagogically frustrating; partial-credit `AnswerResult::PartiallyCorrect` enum exists but isn't reachable here.
- Fix: also check subset/superset relations and return `PartiallyCorrect` accordingly.

**[LOW] Learning path `kAllTechniques` ordering doesn't strictly match technique-rating ascending**
- File: `src/core/training_learning_path.h:154-209`
- Issue: Visual ordering mixes ratings.
- Why it matters: minor UX inconsistency.
- Fix: either sort by `getTechniqueRating` or document the order is curated.

#### Notes
- `createStrategy` switch covers all 54 enum values.
- `getTechniqueCategory` returns `Special` for `JuniorExocet` and `Backtracking` alongside `BUG`.
- Hint feature is currently disabled/experimental per MEMORY (2026-05-17).

#### Coverage
11 of 11 files reviewed (training_learning_path.h skimmed per instructions).

---

### Appendix L — Chunk 12: Architecture spine

#### Findings

**[BLOCKER] Observable callback re-entry can crash via dangling reference**
- File: `src/core/observable.h:160-166`
- Issue: `notifyObservers()` copies `callbacks_` and dispatches, but if a callback triggers another `set()` reentrantly, the inner `notifyObservers()` may erase entries that the outer loop's `callbacks_copy` still holds by value-copy of `std::function` — safe. However, the guard `callbacks_.count(id) > 0` then dispatches with `value_` which has been mutated mid-iteration to a *newer* value.
- Why it matters: Observable contract is "subscriber sees the value at notification time"; reentrant `set()` silently swaps it.
- Fix: Queue reentrant sets (`pending_set_` flag) and drain after outer notification completes.

**[BLOCKER] Observable observer-interface iteration invalidates on reentrant unsubscribe**
- File: `src/core/observable.h:147-156`
- Issue: The `IObserver`-loop iterates `observers_` directly (no copy). If `observer->onNotify(...)` calls `unsubscribe()` (which uses `std::erase_if` over the same vector), `it` is invalidated → UB.
- Why it matters: Crash or silent skip when an observer unsubscribes during its own callback.
- Fix: Snapshot `observers_` (copy of weak_ptrs) before iterating.

**[HIGH] DIContainer race during singleton lazy-init caches duplicate instance**
- File: `src/core/di_container.h:82-86`
- Issue: `resolve<T>()` for a singleton factory calls `factory_it->second()` then stores in `instances_`. If the factory recursively `resolve<T>()`s itself (cycle), the factory runs twice.
- Why it matters: Breaks "Singleton" guarantee.
- Fix: Detect re-entry (in-progress set) and either return the partial instance or assert cycle.

**[HIGH] DIContainer: resolve() of an unregistered interface silently returns nullptr**
- File: `src/core/di_container.h:88`
- Issue: Returns `nullptr` with no diagnostic when a type was never registered.
- Why it matters: Configuration-error class becomes runtime crash.
- Fix: `spdlog::critical` + `std::abort` (or `std::expected`-style return); document contract in header.

**[HIGH] CompositeObserver::observe clears ALL subscribers, not just its own**
- File: `src/core/observable.h:186`
- Issue: Lambda calls `observable.clearSubscriptions()`, nuking every subscriber including those not added by this `CompositeObserver`.
- Why it matters: Adding any second subscriber breaks the first one without compile/test failure.
- Fix: Capture the unsubscribe token returned by `subscribe(CallbackFn)` and call it.

**[HIGH] GameState default-constructs SystemTimeProvider, bypassing DI for the model**
- File: `src/model/game_state.h:48`
- Issue: Default arg `std::make_shared<core::SystemTimeProvider>()` constructs a fresh real-time provider when caller omits the arg.
- Why it matters: Tests that forget to pass `MockTimeProvider` get wall-clock leakage with zero compile-time signal.
- Fix: Remove the default; require explicit `time_provider`.

**[HIGH] Observable callback during notification can iterate a freed map entry**
- File: `src/core/observable.h:160-166`
- Issue: Captured `[this]` lambdas to a destroyed View are UB.
- Why it matters: View teardown during notification → callbacks fire against `this=dangling`.
- Fix: Document/enforce that subscribers must outlive Observable, or use `weak_ptr`-based callback ownership.

**[MEDIUM] `registerSingleton` factory captures `&container` by ref**
- File: `src/main.cpp:74,82,92,97,114`
- Issue: All factory lambdas capture `[&container]`.
- Why it matters: Cosmetic now; tripwire when refactoring to a per-test container.
- Fix: Capture by value if the closure outlives the scope.

**[MEDIUM] `QtClipboardProvider::setText` truncates to int**
- File: `src/infrastructure/qt_clipboard_provider.h:36`
- Issue: `static_cast<int>(text.size())` — `size_t → int` narrowing.
- Why it matters: Lossy cast for no reason.
- Fix: `QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()))`.

**[MEDIUM] `AppDirectoryManager` ignores read-only XDG_DATA_HOME**
- File: `src/infrastructure/app_directory_manager.cpp:40-43`
- Issue: Honors `XDG_DATA_HOME` if non-empty but doesn't probe writability.
- Why it matters: Silent crash class on misconfig environments.
- Fix: Probe writability, fall back to HOME on failure.

**[MEDIUM] `GameState` copy/move retains shared `time_provider_` — copies see same mock**
- File: `src/model/game_state.h:50-53`
- Issue: Defaulted copy/move propagates `shared_ptr<ITimeProvider>` by reference-count; Observable members are copied (subscriber list duplicated).
- Why it matters: Tests that copy `GameState` accidentally cross-notify.
- Fix: Default-copy Observable members must NOT carry over subscribers — `=delete` on `GameState` copy.

**[MEDIUM] `Observable::subscribe(ObserverPtr)` holds `weak_ptr` but stores `shared_ptr` parameter**
- File: `src/core/observable.h:86-90` vs `:141`
- Issue: Parameter type is `ObserverPtr` (`shared_ptr<IObserver<T>>`), but storage is `std::vector<std::weak_ptr<IObserver<T>>>`. The push_back implicitly converts shared→weak.
- Why it matters: Easy to write `obs.subscribe(std::make_shared<MyObs>())` → immediately expires → silent no-op.
- Fix: Take `weak_ptr` directly to make the ownership contract visible.

**[LOW] `DIContainer::isRegistered` is `const` but `resolve()` is not**
- File: `src/core/di_container.h:65, 93`
- Issue: `resolve<T>()` is non-const because it mutates `instances_`.
- Why it matters: Minor API friction.
- Fix: Document, or make `instances_` `mutable` and `resolve` `const`.

**[LOW] `Observable<T>::update` reads `value_` to compare — non-comparable T won't compile**
- File: `src/core/observable.h:77`
- Issue: `if (value_ != old_value)` SFINAEs out for T without `operator!=`.
- Why it matters: Diagnosis pain for new `T`.
- Fix: `static_assert(std::equality_comparable<T>, "Observable<T> requires operator!=");`.

**[LOW] main.cpp wires `ITrainingExerciseGenerator` but does not register `ITrainingViewModel`**
- File: `src/main.cpp:176`
- Issue: `TrainingViewModel` is constructed bare with `std::make_shared`, not via container.
- Why it matters: ViewModels can't be resolved from elsewhere if needed.
- Fix: Either register ViewModels in container or document why they're terminal.

#### Notes
- `Observable::subscribe(ObserverPtr)` accepts shared_ptr but stores weak_ptr — silent expiry trap.
- `notifyObservers` uses a `callbacks_.count(id)` recheck on the copy but no equivalent for the interface path; asymmetric robustness.
- `SystemTimeProvider` default-construction in `GameState` is the textbook anti-pattern the project-context explicitly forbids.
- `getContainer()` returning a non-thread-safe singleton is fine given single-threaded constraint, but no `static_assert`/comment documents that.
- `main.cpp` has no try/catch around `QApplication::exec()`; spdlog file sink construction can throw on permission-denied.

#### Coverage
11 of 11 files reviewed.

---

### Appendix M — Chunk 13: ViewModel layer

#### Findings

**[BLOCKER] updateUIState clobbers user preferences and status_message**
- File: `src/view_model/game_view_model_state.cpp:202`
- Issue: `updateUIState()` constructs a fresh `UIState ui;` and only carries forward `input_mode` and `notes_filled` from prev. `show_conflicts`, `show_hints`, `status_message`, and `is_paused` are read from defaults each time.
- Why it matters: `setShowConflicts(false)` is wiped by the next `enterNumber()` → `updateUIState()`. Status messages from `checkSolution` get wiped by the next keystroke.
- Fix: Preserve all sticky fields from `prev` (show_conflicts, show_hints, status_message), or use `uiState.update()` to mutate only the changed fields.

**[BLOCKER] undoToLastValid can overshoot last_valid_state_index_ via hint-skipping**
- File: `src/view_model/game_view_model_undo.cpp:99`
- Issue: Loop condition is `move_history_index_ > last_valid_state_index_`, but `undo()` walks past `PlaceHint` moves silently. If a `PlaceHint` move sits at `last_valid_state_index_`, undo decrements past it.
- Why it matters: Board reverts further than the "last valid state" tracker promises; subsequent `redo` rebuilds from a state inconsistent with `last_valid_state_index_`.
- Fix: Compute the target index by walking move_history_ forward (skipping hints) before looping.

**[HIGH] recordMove never updates last_valid_state_index_ for invalid states**
- File: `src/view_model/game_view_model_undo.cpp:170-181`
- Issue: `recordMove` sets `last_valid_state_index_` only when `!hasBoardErrors()`. But after a wrong move, the prior valid index still points at an old history index. After redo branching, that old index may now reference a different/deleted move.
- Why it matters: `undoToLastValid` jumps to a stale index.
- Fix: When truncating redo history, clamp `last_valid_state_index_ = std::min(last_valid_state_index_, move_history_index_)`.

**[HIGH] TryIt snapshot overwritten at level 3 entry, losing original baseline**
- File: `src/view_model/game_view_model_hints.cpp:592`
- Issue: `requestCoachingHint()` at level 3 does `coaching_context_->snapshot = gameState.get();` — the snapshot captures whatever state the user has now, not the state when coaching began.
- Why it matters: User makes some eliminations during L1/L2 reading, then enters TryIt — those changes are baked into the baseline and never counted.
- Fix: Capture snapshot once at coaching_context_ initialization (line 556) and never overwrite.

**[HIGH] startNewGame/restoreGameState don't pauseTimer before starting new — `endGameSession` leaks too**
- File: `src/view_model/game_view_model.cpp:87, 195`
- Issue: `startNewGame` and `loadGame`/`restoreGameState` neither call `endGameSession(false)` for an in-progress session nor pause the running timer.
- Why it matters: Loading a game mid-play leaks the previous statistics session.
- Fix: Both paths call `endGameSession(false)` early like `enterEditMode` does.

**[HIGH] hasBoardErrors called inside recordMove uses post-update state**
- File: `src/view_model/game_view_model_undo.cpp:179-191`
- Issue: `recordMove` first calls `hasBoardErrors()` (which calls `extractNumbers` + `findConflicts` internally), then calls `gameState.update` to set conflicts again with another `findConflicts`. Conflicts are computed twice per move.
- Why it matters: Double computation; coupling `is_mistake` to conflicts (not solution-disagreement) means `last_valid_state_index_` advances when player places a wrong-but-non-conflicting value.
- Fix: Compute conflicts once, gate `last_valid_state_index_` on both conflicts AND `!is_mistake`.

**[HIGH] importPuzzleFromString doesn't end prior stats session**
- File: `src/view_model/game_view_model_hints.cpp:218-296`
- Issue: Unlike `enterEditMode`, `importPuzzleFromString` jumps straight to `gameState.set(new_state)` and later `startGameSession()` which detects `current_game_session_ > 0` and ends previous.
- Why it matters: Stats corruption on mid-game import — completion percentages drift.
- Fix: Add explicit `if (isGameActive()) endGameSession(false);` at start of `importPuzzleFromString`.

**[MEDIUM] executeCommand silently drops most GameCommand verbs**
- File: `src/view_model/game_view_model_state.cpp:45-78`
- Issue: `GameCommand` enum declares NewGame, SaveGame, LoadGame, PauseGame, ResumeGame, ValidateBoard, ClearNotes, but `executeCommand`'s switch handles none of them.
- Why it matters: View must call `startNewGame()` / `saveCurrentGame()` etc. directly, bypassing the command pipeline.
- Fix: Either implement these branches or remove the dead enum values.

**[MEDIUM] canExecuteCommand default returns true for every unhandled command**
- File: `src/view_model/game_view_model_state.cpp:80-97`
- Issue: `default: return true;` — NewGame, SaveGame, LoadGame, PauseGame, ResumeGame, ValidateBoard, ToggleInputMode, ClearNotes, ShowStatistics all return true unconditionally.
- Why it matters: View can't trust `canExecuteCommand` to gate buttons.
- Fix: Replace `default: true` with explicit per-command preconditions.

**[MEDIUM] restoreGameState doesn't reset coaching state on load**
- File: `src/view_model/game_view_model.cpp:195-279`
- Issue: `loadGame` calls `resetCoachingState`, but `restoreGameState` (called directly by tests, integration) does not.
- Why it matters: Loading a game while a TryIt coaching session is active leaves `coaching_context_` pointing at the previous game's `SolveStep`.
- Fix: Move `resetCoachingState()` from `loadGame` into `restoreGameState`.

**[MEDIUM] checkSolution → startNewGame re-entrancy clobbers completion status_message**
- File: `src/view_model/game_view_model_undo.cpp:115-145`
- Issue: On completion, `checkSolution` calls `startNewGame(completed_difficulty)`, which calls `updateUIState()` — per the BLOCKER above, that resets status_message to "". Then `checkSolution` calls `uiState.update` to set the completion message, but `startNewGame` has already published the empty-status UIState.
- Why it matters: UX flicker.
- Fix: Set the completion status_message BEFORE calling startNewGame.

**[MEDIUM] autoSaveIfNeeded fires synchronously on every keystroke**
- File: `src/view_model/game_view_model_board.cpp:94`, `game_view_model_state.cpp:305-309`
- Issue: `enterNumber` ends with `autoSaveIfNeeded()`, which calls `autoSave()` — a full SaveManager serialize+encrypt+compress+write per cell placement.
- Why it matters: I/O on the input path. Hot input could stall UI.
- Fix: Debounce via QTimer::singleShot, or only autosave on a periodic tick / pause / commit boundary.

**[MEDIUM] getHint reports "0/10 used" when actually no game session exists**
- File: `src/view_model/game_view_model_hints.cpp:133-138`, `game_view_model_hints.cpp:508-521`
- Issue: `getHintCount()` returns 0 if `current_game_session_ == 0`. `getHint` then says "No hints remaining (0/10 used)" — misleading.
- Why it matters: Confuses users.
- Fix: Distinguish the "no active session" branch with its own message.

**[LOW] applyMove(PlaceHint) double-counts moves vs recordMove**
- File: `src/view_model/game_view_model_undo.cpp:219-225`
- Issue: `applyMove` for `PlaceHint` calls `state.incrementMoves()`. `recordMove` then also records a move. But the regular `PlaceNumber` path doesn't call `incrementMoves` in applyMove — only the hint path does.
- Why it matters: Asymmetric move-count semantics between hint and normal placement.
- Fix: Either always increment in applyMove, or never.

#### Notes
- VM does not hold any raw View pointer — clean Observable subscriber model.
- Qt types (QString/QObject) correctly absent from the VM surface.
- `setShowConflicts`/`setShowHints` use the get-copy-mutate-set pattern, which is safe; combined with the BLOCKER above their effect is lost on next updateUIState.
- `coaching_context_` and `coachingState` are always cleared together via `resetCoachingState`.
- `endGameSession` does not pause the running GameState timer.

#### Coverage
8 of 8 files reviewed.

---

### Appendix N — Chunk 14: View layer

#### Findings

**[BLOCKER] settingsObservable subscription never unsubscribed on MainWindow destruction**
- File: `src/view/main_window.cpp:646`
- Issue: `settings_manager_->settingsObservable().subscribe([this](...){ ... })` captures `this`, discards the returned unsubscribe lambda. `observer_.unsubscribeAll()` in `~MainWindow` only clears the CompositeObserver-tracked observables; this direct subscribe survives.
- Why it matters: `settings_manager_` is a `shared_ptr` held by DIContainer — outlives MainWindow. Next `setSettings(...)` invocation calls a lambda dereferencing a destroyed `this` → use-after-free on `auto_save_timer_`, `training_mode_action_`, etc.
- Fix: store returned unsubscribe lambda and call it in `~MainWindow` (or route via `observer_`).

**[HIGH] applyLocale drops working translator when new .qm load fails**
- File: `src/view/main_window.cpp:683-700`
- Issue: `removeTranslator(&translator_)` runs unconditionally before `translator_.load()`. On failure, the function returns early without reinstalling the previous translation.
- Why it matters: User switches to an unsupported locale → loses their previously working translation until restart.
- Fix: load into a temp QTranslator first; swap only on success.

**[HIGH] Reset Puzzle menu action permanently disabled — Ctrl+R never fires**
- File: `src/view/main_window.cpp:288-290`
- Issue: `reset_action->setEnabled(false)` immediately after creating the action; no code path ever re-enables it.
- Why it matters: User-facing feature regression — `showResetDialog()`/`GameCommand::ResetGame` is unreachable from the UI.
- Fix: drive enabled state from `view_model_->isGameActive()` in `updateToolBar()`/observer.

**[HIGH] animatePageTransition effect dangles across rebuildPages**
- File: `src/view/training_widget.cpp:858-870`
- Issue: `QGraphicsOpacityEffect` is parented to `target_widget`; `QPropertyAnimation` (DeleteWhenStopped) targets that effect and captures `target_widget` raw. `rebuildPages()` (called on `QEvent::LanguageChange`) deletes all pages → effect deleted with parent; animation still runs against deleted QObject.
- Why it matters: Switching language mid-animation crashes or trips ASan.
- Fix: stop/delete the running animation in `rebuildPages()`, or use `QPointer<QGraphicsOpacityEffect>` and bail in the finished lambda.

**[MEDIUM] clock_timer_ polls ViewModel every 1 s — explicit anti-pattern per project context**
- File: `src/view/main_window.cpp:512-516, 799-828`
- Issue: `QTimer(1000ms)` → `updateStatusBar()` reads `view_model_->gameState.get()`, `getFormattedTime()`, `isTimerRunning()`.
- Why it matters: Wakes the main thread once a second; bypasses the Observable contract.
- Fix: expose a formatted-time/tick Observable from `GameViewModel`; subscribe normally.

**[MEDIUM] Wait-cursor is not RAII — leaks WaitCursor on early return / throw**
- File: `src/view/main_window.cpp:908-910, 946-948`
- Issue: `setOverrideCursor(Qt::WaitCursor)` / `restoreOverrideCursor()` bracket synchronous solver calls.
- Why it matters: Cursor stays Wait forever until restart.
- Fix: small RAII guard struct calling restore in dtor.

**[MEDIUM] difficultyString default branch returns localized "Unknown" — unreachable but masks bugs**
- File: `src/view/main_window.cpp:1416-1418`
- Issue: `default:` returns `loc("Sudoku", "Unknown")`; hides any future enumerator addition silently.
- Why it matters: New difficulty added → no compile warning.
- Fix: drop default; let `-Wswitch` flag missing enumerators.

**[MEDIUM] Hard-coded English "Copyright (C) 2025-2026 Alexander Bendlin" inside translatable format**
- File: `src/view/main_window.cpp:1287-1293`
- Issue: About box format string concatenates translated text with non-translatable copyright line and tech list.
- Why it matters: Translators can't localize it.
- Fix: split into `core::loc("Sudoku", "Copyright (C) %1 %2").arg(years, author)`.

**[MEDIUM] auto_save_timer_ fallback `30000` ms is a magic number**
- File: `src/view/main_window.cpp:520`
- Issue: `auto_save_interval_ms : 30000`. Project rule: "No magic numbers."
- Why it matters: Settings default vs view fallback can drift silently.
- Fix: pull from `core::DEFAULT_AUTO_SAVE_INTERVAL_MS`.

**[MEDIUM] keyPressEvent calls updateButtonPanel() directly, bypassing the Observable path**
- File: `src/view/main_window.cpp:746-757`
- Issue: After `view_model_->cycleInputMode()` / `setInputMode(...)`, the code calls `updateButtonPanel()` directly. The VM's `uiState` Observable already drives the same call.
- Why it matters: Duplicate paths.
- Fix: rely on the Observable; drop the direct call.

**[MEDIUM] View directly reads VM-side input mode to branch in keyPressEvent / Clear Cell handler**
- File: `src/view/main_window.cpp:387-391, 766-779`
- Issue: View switches behavior on `view_model_->getInputMode()` — i.e., the View carries logic about EditGivens-vs-Normal that the VM should own.
- Why it matters: Puts policy in the View.
- Fix: collapse the two paths behind one `GameCommand`/`clearAt()` method on `GameViewModel`.

**[LOW] PuzzleImportDialog trim can split a multi-code-unit character mid-paste**
- File: `src/view/puzzle_import_dialog.cpp:62-71`
- Issue: `cursor.setPosition(kMaxInputChars)` cuts at QChar (UTF-16 code-unit) index; pasting >4096 chars whose 4096th boundary lands on a surrogate pair creates an unpaired surrogate.
- Why it matters: 81-char puzzles are ASCII so practical hit is near-zero.
- Fix: align on a grapheme/codepoint boundary.

**[LOW] paintEvent has no early-out for zero-size widget / repaint during destruction**
- File: `src/view/sudoku_board_widget.cpp:82-136`
- Issue: With `width()==0 || height()==0`, `cellSize()` clamps to `min_size`, board is drawn at negative origin off-canvas.
- Why it matters: Edge-case render churn.
- Fix: `if (width() <= 0 || height() <= 0) return;` at top of paintEvent.

**[LOW] showLoadDialog opens even when the save list is empty**
- File: `src/view/main_window.cpp:1077-1086`
- Issue: Empty save list still produces a 0-row table with OK button.
- Why it matters: UX dead end.
- Fix: early-return with a toast if `saves.empty()`.

**[LOW] retranslateUi rebuilds the entire menu bar — drops user-installed Action shortcuts/state**
- File: `src/view/main_window.cpp:1426-1428`
- Issue: `menuBar()->clear(); setupMenuBar();` wipes & rebuilds every QAction on `LanguageChange`.
- Why it matters: Switching language reveals experimental actions even when the user has them disabled.
- Fix: have `retranslateUi()` re-apply settings-driven visibility after rebuild.

#### Notes
- `_bmad-output/project-context.md` states `locFormat` was removed (commit f0e5567), but `src/core/i18n_helpers.h` still defines and documents it. Did not flag — treating the in-tree convention as canonical.
- All view classes that declare signals carry `Q_OBJECT`; no Qt5 `SIGNAL()`/`SLOT()` macros; no `Q_FOREACH`; no raw `new`/`delete` of parented widgets; no `std::thread`/`async`/`mutex`. Parent-child ownership respected.
- `coaching_panel_->hide()` / `show()` and overlay highlight code in `onCoachingStateChanged` correctly drive off the `coachingState` Observable.
- The `Hint` feature being experimental (per MEMORY.md) was honored — not flagged for incomplete hint UI.

#### Coverage
18 of 18 files reviewed (deep: main_window.cpp/h, sudoku_board_widget.cpp/h, board_painter.cpp/h, training_widget.cpp/h, board_render_data.cpp; skim: training_number_pad.cpp/h, puzzle_import_dialog.cpp, puzzle_technique_dialog.cpp, toast_widget.cpp/h, style_colors.h, locale_utils.cpp).

---

*End of report. For the GitHub issues draft, see [CODE_REVIEW_2026-05-25_ISSUES.md](CODE_REVIEW_2026-05-25_ISSUES.md).*
