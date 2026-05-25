# Code Review 2026-05-25 — GitHub Issues Draft

**Purpose:** Convert the P0–P2 findings from [CODE_REVIEW_2026-05-25.md](CODE_REVIEW_2026-05-25.md) into a tractable GH issue backlog. Each section below is a ready-to-create issue.

**Process:**
1. Review titles, bodies, and labels below.
2. Run `gh label create` for any labels that don't yet exist (see [Labels](#labels) section).
3. Run `gh issue create` per section (or batch via the script at the bottom).
4. P3/P4 (LOW + cleanup) stays in the MD only — promote opportunistically.

**Aggressive grouping:** related findings are bundled into one issue when they share a fix surface. Each issue references the underlying finding IDs (BL-N / HIGH / MEDIUM) so the MD remains the source of truth.

---

## Labels

Create these once before bulk issue creation:

```sh
# Priority
gh label create priority/p0 --color B60205 --description "Must fix before next release (data loss / crash / unsoundness)"
gh label create priority/p1 --color D93F0B --description "Strategy soundness — fix before training-mode trust"
gh label create priority/p2 --color FBCA04 --description "Determinism + persistence + architecture hardening"

# Area
gh label create area/solver --color 0E8A16 --description "Solver loop, strategies, candidate grid"
gh label create area/simd --color 5319E7 --description "AVX2/AVX-512 SIMD layer + CPU dispatch"
gh label create area/persistence --color 1D76DB --description "Save/load, encryption, statistics serialization"
gh label create area/view --color C5DEF5 --description "Qt6 widgets, MainWindow, painters"
gh label create area/viewmodel --color BFD4F2 --description "GameViewModel, TrainingViewModel"
gh label create area/core --color 0052CC --description "DI, Observable, Model, Infrastructure"
gh label create area/training --color 008672 --description "Training subsystem"

# Type
gh label create type/bug --color D73A4A --description "Concrete defect with reproducer"
gh label create type/refactor --color A2EEEF --description "Structural change without behavior change"
gh label create type/audit --color FEF2C0 --description "Cross-cutting sweep across multiple files"

# Source
gh label create from/code-review-2026-05-25 --color 5319E7 --description "Originated from the 2026-05-25 whole-project code review"
```

---

## P0 — Ship-blockers (8 issues)

### Issue 1: `fix(save): Master-difficulty saves silently rejected (BL-8)`

**Labels:** `priority/p0` `area/persistence` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`save_manager_serialization.cpp:52` defines `MAX_DIFFICULTY = 3` ("Expert"), but `Difficulty::Master = 4` (`DIFFICULTY_COUNT == 5`). Every save of a Master puzzle fails to load with `InvalidSaveData`.

## Impact
- **Permanent data loss.** Master saves can never be reloaded.
- Auto-save round-trip silently broken on Master difficulty.

## Reproduction
1. Start a Master-difficulty game, place a move, let auto-save fire (or manually save).
2. Quit and restart the app.
3. Load the save — fails with InvalidSaveData.

## Fix
- Set `MAX_DIFFICULTY = static_cast<int>(Difficulty::Master)` (or `4`) in `save_manager_serialization.cpp`.
- Add a regression test under `tests/unit/` that round-trips a Master-difficulty save.

## References
- BL-8 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/core/save_manager_serialization.cpp:52-53, 278-283`
```

---

### Issue 2: `fix(save): zlib decompression bomb — cap max output size (BL-9)`

**Labels:** `priority/p0` `area/persistence` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`save_manager_compression.cpp:74-91` (`decompressData`) grows the output buffer up to `BUFFER_SIZE_MULTIPLIER^MAX_DECOMPRESSION_ATTEMPTS = 1024×` the compressed input, with no absolute cap. A 10 MB crafted blob can demand ~10 GB RAM before failing.

## Impact
- DoS via OOM on `importSave` or on a tampered plain-YAML sessions file.
- Single-threaded Qt app → process death.

## Reproduction
Craft a 10 MB zlib blob that decompresses to >1 GB. Trigger via `importSave` or place at the sessions file path.

## Fix
- Add an absolute max output guard (suggested: 64 MB).
- Abort with `CompressionError` when exceeded — do not allocate further.

## References
- BL-9 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/core/save_manager_compression.cpp:74-91`
```

---

### Issue 3: `fix(training): stats serializer drops 12 advanced techniques (BL-10)`

**Labels:** `priority/p0` `area/training` `area/persistence` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`training_statistics_serializer.cpp:74` loops `for (int i = 0; i <= static_cast<int>(SolvingTechnique::GroupedXCycles); ++i)` — only 0..41. Techniques 42–53 (SashimiXWing/Swordfish/Jellyfish, Unit/Region ForcingChain, MutantFish, KrakenFish, ALSChain, JuniorExocet, UniqueLoop, ContinuousNiceLoop, GroupedNiceLoop) are never name-matched on reload.

## Impact
- Stats for **12 of 54 techniques silently vanish on reload**.
- Mastery progress for the most advanced techniques is permanently lost across launches.

## Fix
- Iterate over `kAllTechniques` (from `training_learning_path.h`) instead of the hardcoded enum bound.
- Or use a static `name→enum` map covering all 54.
- Add a serialization round-trip unit test that records mastery for one technique per category and reloads.

## References
- BL-10 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/core/training_statistics_serializer.cpp:74`
```

---

### Issue 4: `fix(view): settingsObservable subscription leaks past ~MainWindow (UAF) (BL-15)`

**Labels:** `priority/p0` `area/view` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`main_window.cpp:646` calls `settings_manager_->settingsObservable().subscribe([this](...){...})` and discards the returned unsubscribe lambda. `observer_.unsubscribeAll()` in `~MainWindow` only covers CompositeObserver-tracked subscriptions; this direct subscribe survives.

## Impact
`settings_manager_` is a `shared_ptr` held by DIContainer and outlives MainWindow. The next `setSettings(...)` invocation calls a lambda that dereferences a destroyed `this` → use-after-free on `auto_save_timer_`, `training_mode_action_`, etc.

## Fix
- Store the returned unsubscribe lambda as a member, call it in `~MainWindow`.
- Or route the subscription via `observer_` (the existing `CompositeObserver`).
- Audit all other direct `subscribe()` call sites in `src/view/` for the same pattern.

## References
- BL-15 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/view/main_window.cpp:646`
```

---

### Issue 5: `fix(viewmodel): updateUIState clobbers preferences + undoToLastValid overshoots (BL-13, BL-14)`

**Labels:** `priority/p0` `area/viewmodel` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Two coupled defects in the ViewModel state pipeline:

### BL-13 — `updateUIState` clobbers user preferences
`game_view_model_state.cpp:202` builds a fresh `UIState ui;` and only forwards `input_mode` and `notes_filled` from the previous state. `show_conflicts`, `show_hints`, `status_message`, `is_paused` are reset to defaults every call.

Result: `setShowConflicts(false)` is wiped by the next keystroke. Completion status messages from `checkSolution` disappear on the next move.

### BL-14 — `undoToLastValid` overshoots `last_valid_state_index_`
`game_view_model_undo.cpp:99` loop walks past `PlaceHint` moves silently. Combined with `recordMove` never updating `last_valid_state_index_` after redo branching, the tracked index can reference a deleted move.

Result: Board reverts further than the "last valid state" tracker promises; redo rebuilds from a state inconsistent with `last_valid_state_index_`.

## Fix
### BL-13
Preserve all sticky fields from `prev` in `updateUIState()`, or use `uiState.update()` to mutate only the fields that changed.

### BL-14
- Compute the target index by walking `move_history_` forward (skipping hints) before the loop.
- When truncating redo history in `recordMove`, clamp `last_valid_state_index_ = std::min(last_valid_state_index_, move_history_index_)`.

## References
- BL-13 and BL-14 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/view_model/game_view_model_state.cpp:202`
- `src/view_model/game_view_model_undo.cpp:99, 170-181`
```

---

### Issue 6: `fix(core): Observable re-entrancy & iteration safety (BL-11, BL-12)`

**Labels:** `priority/p0` `area/core` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Two related Observable lifetime hazards:

### BL-11 — Reentrant `set()` during notification
`observable.h:160-166` copies `callbacks_` for safe iteration, but if a callback reentrantly calls `set()`, subscribers receive notifications with `value_` already mutated to a newer value. Combined with view teardown during notification, captured `[this]` lambdas may run against destroyed Views.

### BL-12 — Observer-interface iteration invalidation
`observable.h:147-156` iterates `observers_` directly (no copy). If `observer->onNotify(...)` triggers `unsubscribe()` (which uses `std::erase_if` over the same vector), `it` is invalidated → UB. The callback path is copy-protected; the interface path is not.

## Impact
- Hard-to-diagnose UI/state corruption from out-of-order notifications.
- Crash or silent skip when an observer unsubscribes during its own callback (common Qt destructor pattern).
- UAF on view destruction during notification.

## Fix
- Snapshot `observers_` (copy of weak_ptrs) before iterating, mirroring the callback path's copy semantics.
- Queue reentrant sets (`pending_set_` flag), drain after the outer notification completes.
- Document subscriber-lifetime contract in `observable.h`: subscribers must outlive the Observable for the duration of any subscription.

## References
- BL-11, BL-12 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/core/observable.h:147-166`
- Related HIGH: `observable.h:186` (`CompositeObserver::observe` clears ALL subscribers) — see Issue #20.
```

---

### Issue 7: `fix(simd): AVX-512 BITALG must gate _mm512_popcnt_epi16 (BL-1)`

**Labels:** `priority/p0` `area/simd` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`cpu_features.h:58-61` `hasAvx512()` only checks F + BW. `simd_avx512_ops.cpp:45` issues `_mm512_popcnt_epi16`, which requires AVX-512 **BITALG** (Zen 5+ / Ice Lake-SP+). `CpuFeatures` has no BITALG field; CPUID(7,0).ECX bit 12 is never read.

## Impact
On Skylake-X / Cascade Lake / Zen 4 (AVX-512F+BW but no BITALG), the AVX-512 path is selected and `#UD`-faults — **SIGILL crash on real hardware.**

## Fix
- Add `has_avx512_bitalg = (ecx & (1U<<12)) != 0` reading CPUID(7,0).ECX in `detect()`.
- Require it in `hasAvx512()`.
- Test on a Zen 4 (or VM with `-cpu Skylake-X-v4`) before closing.

## References
- BL-1 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/core/cpu_features.h:58-61`, `src/core/simd_avx512_ops.cpp:45`
```

---

### Issue 8: `fix(simd): alignas(64) + _mm512_loadu_si512 conflict (BL-2 — Aligned Loader)`

**Labels:** `priority/p0` `area/simd` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`SIMDConstraintState` is declared `alignas(64)`. `simd_avx512_ops.cpp:42, 82` call `_mm512_loadu_si512` on a member array of that struct. This is the documented **"Aligned Loader" pattern** from MEMORY.md — GCC has been observed swapping `loadu` for `vmovdqa64` (aligned load) when it infers alignment.

## Impact
Intermittent SIGSEGV in Release builds; never in Debug. Same class as the AVX-512 alignment bug previously fixed in MEMORY.md.

## Fix
Pick **one** approach (current code has the worst of both):

**Option A** — drop the alignment hint and rely on the unaligned load:
- Remove `alignas(64)` from `SIMDConstraintState` and from the member array.
- Keep `_mm512_loadu_si512`.

**Option B** — commit to aligned and assert it:
- Switch to `_mm512_load_si512` (aligned).
- Assert alignment at every entry point that constructs or copies a `SIMDConstraintState`.

Add a unit test that compiles in Release with `-O2` and confirms no SIGSEGV with all-empty / all-full / boundary boards.

## References
- BL-2 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/core/simd_avx512_ops.cpp:42, 82`
- MEMORY.md bestiary: "Aligned Loader"
```

---

## P1 — Strategy soundness (5 issues)

### Issue 9: `fix(solver): Junior Exocet under-implemented — disable until cover-house validation lands (BL-4, BL-5, BL-6)`

**Labels:** `priority/p1` `area/solver` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Junior Exocet strategy has three independent BLOCKER-class soundness gaps and one MEDIUM:

- **BL-4** — Missing S-cell / cover-house validation. Current code only checks "T1, T2 outside base band, different boxes" before emitting eliminations. The full JE definition requires S-cells in target columns/band acting as cover-houses for each base candidate. Produces unsound eliminations on most inputs.
- **BL-5** — T1 and T2 may share the same band. Only `different boxes` is enforced; the JE definition requires the two targets occupy the two non-base BANDS (one each).
- **BL-6** — Every base candidate must appear in at least one target — check missing.
- **MED** — Targets accepted with only 1 base candidate (`junior_exocet_strategy.h:146-149, 165-168`).

Plus **LOW** — only row-based JE patterns searched; column-based half missed.

## Impact
- Unsound eliminations on most non-trivial inputs that pattern-match a pseudo-JE.
- Strategy is registered (rating 9.4) and fires from the solver loop; misattributed difficulty ratings flow into the puzzle generator and rater.

## Fix
**Recommended: disable Junior Exocet at registration** in `sudoku_solver.cpp::initializeStrategies()` until the full pattern check is implemented. Add a TODO referencing this issue.

Then, separately, implement the full Champagne-style JE definition:
1. Verify T1/T2 are in different non-base BANDS (BL-5).
2. Verify `(t1_mask | t2_mask) ⊇ base_mask` (BL-6).
3. Verify S-cells exist as cover-houses for every base candidate (BL-4).
4. Tighten target-cell base-candidate requirements (MED).
5. Mirror the search for column-based bases (LOW).
6. Add regression-test puzzles against a known JE-solver oracle in `tests/unit/test_strategy_correctness.cpp`.

## References
- BL-4, BL-5, BL-6 in [CODE_REVIEW_2026-05-25.md §4](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/core/strategies/junior_exocet_strategy.h:81-192`
```

---

### Issue 10: `fix(solver): Kraken Fish axis confusion + misattribution (BL-3, HIGH)`

**Labels:** `priority/p1` `area/solver` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Kraken Fish has three coupled issues:

- **BL-3** — Axis confused. `kraken_fish_strategy.h:243-249` skip-check uses `std::ranges::contains(base_primaries, secondary)`, but `secondary` ranges over the perpendicular axis. For row-fish, this should skip rows in `base_primaries`; instead it skips columns whose index numerically coincides with a base-row index. Produces both invalid eliminations and missed ones.
- **HIGH** — Misattributes finned-fish eliminations as Kraken results. When `kraken_elims` is empty but `free_elims` (finned eliminations) are present, the step is still labelled "Kraken Fish." Breaks difficulty rating + technique-tagging assumptions in the correctness harness.
- **HIGH** — `explanation_data` uses the base-line `RegionType`, but Kraken eliminations sit on the cover lines. UI highlight draws the wrong axis.

## Fix
### BL-3
Test `getRow(row)` / `getCol(col)` against `base_primaries` after computing `row`/`col` for the `by_row`/`by_col` case, not the loop axis.

### HIGH (misattribution)
- Report only `kraken_elims` in a Kraken step, OR
- Split into two SolveSteps (free finned-eliminations as a separate sub-step), OR
- Require `kraken_elims` to drive `position` and `roles`, never just the combined `all_elims`.

### HIGH (region axis)
Use the cover-axis `RegionType` (inverse of `by_row`) in `explanation_data`, or omit `region_type` entirely for Kraken steps.

## References
- BL-3, HIGH × 2 in [CODE_REVIEW_2026-05-25.md §4 and §5](CODE_REVIEW_2026-05-25.md#4-blocker-findings)
- `src/core/strategies/kraken_fish_strategy.h:191-193, 218-228, 243-249`
```

---

### Issue 11: `fix(solver): Franken/Mutant fish cover-overlap rejection missing (HIGH × 2)`

**Labels:** `priority/p1` `area/solver` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Two related fish-degeneracy bugs:

### Franken Fish — cover unit can equal a base unit
`franken_fish_strategy.h:178-203, 282-316`. Bestiary's "Overlapping Basemen" fix correctly rejects base-cell overlap (lines 228-234), but the symmetric danger — a chosen cover unit being equal to one of the chosen bases — is silently allowed. A row chosen as both base and cover trivially "covers itself", producing zero genuine eliminations or spurious ones.

### Mutant Fish — cover-cell overlap among chosen covers silently allowed
`mutant_fish_strategy.h:329-352`. Base overlap is rejected (242-248). Cover-cell overlap among the chosen covers is not. For Mutant fish, overlapping covers reduce the effective cover-cell count and can produce a valid-looking but uneliminating pattern.

Also related MEDIUMs:
- **MED** — Franken Fish cover units allowed with only 1 candidate cell (`franken_fish_strategy.h:166, 183`).
- **MED** — Mutant Fish `cell_cover_masks` bit width within 1 of overflow at `cover_indices.size() == 27` (`mutant_fish_strategy.h:306-311, 354-356`).

## Fix
### Franken Fish
Mirror the Mutant Fish base/cover overlap rejection (`mutant_fish_strategy.h:287-296`). Reject cover combinations that include a unit equal to any chosen base.

### Mutant Fish
After building `chosen_covers`, verify total cover-cell count ≥ total base-cell count, or reject when any two chosen covers share a cell.

### Bit-width
Add `static_assert(cover_indices.size() <= 32)` or switch the cover-mask type to `uint64_t`.

### Cover with single candidate cell
Filter `cover_units` to those that cover at least one base cell before enumeration (or require ≥2 candidate cells).

Add new sentinel puzzles to `test_strategy_correctness.cpp` covering these degenerate cases.

## References
- HIGH × 2 + MED × 2 in [CODE_REVIEW_2026-05-25.md §5 and §6](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/core/strategies/franken_fish_strategy.h`, `mutant_fish_strategy.h`
```

---

### Issue 12: `fix(solver): 3D Medusa applies contradiction to non-bipartite components (HIGH)`

**Labels:** `priority/p1` `area/solver` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`three_d_medusa_strategy.h:214-223`: when BFS detects an odd cycle (`is_bipartite=false`), elimination rules are skipped — but `checkContradiction` is still invoked on the same `color_a_nodes`/`color_b_nodes` populated from the (inconsistent) 2-coloring.

## Impact
The "false color" determination depends on BFS path order. An odd-cycle component implies the puzzle has no solution along that chain (shouldn't happen on a sound puzzle), but if it occurs transiently mid-solve, the contradiction rule can eliminate the wrong color → invalid eliminations.

## Fix
- Gate `checkContradiction` behind `if (is_bipartite)` as well as the elimination rules.
- Or treat non-bipartite as a hard contradiction (no eliminations, surface as `SolverError::Unsolvable`).

## References
- HIGH in [CODE_REVIEW_2026-05-25.md §5](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/core/strategies/three_d_medusa_strategy.h:214-223`
```

---

### Issue 13: `fix(solver): Simple Coloring + ForcingChain + loop strategy coverage gaps (HIGH × 5)`

**Labels:** `priority/p1` `area/solver` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Five related coverage / soundness gaps in chain & coloring strategies that under- or over-eliminate:

### HIGH — Simple Coloring skips 2-cell chains
`simple_coloring_strategy.h:124`: `if (size < 4) skip` misses Rule 2 (Exclusion) eliminations on a 2-cell conjugate pair. False negative — strict under-elimination.

### HIGH — Cell-FC `findCommonElimination` not invoked on contradiction branch
`forcing_chain_strategy.h:128`: when one of two branches contradicts, the surviving branch's eliminations on OTHER cells are never harvested. Missed deductions.

### HIGH — GroupedXCycles silently drops Type 2 deductions
`grouped_x_cycles_strategy.h:326-336`: closure handles Type 1 and Type 3 only; strong-strong discontinuity (Type 2) is not built even when `chain[0]` is a single-cell node. Valid placements missed.

### HIGH — GroupedNiceLoop endpoint group may overlap chain interior cells
`grouped_nice_loop_strategy.h:347-381`: `nodesConflict(start_node, end_node)` only checks start vs end overlap. `visited[]` is keyed on node IDs, not cells, so an endpoint group sharing a cell with an interior node passes. Can produce wrong Type 2 eliminations.

### HIGH — Continuous Nice Loop same-cell different-digits rule over-eliminates on bivalue cells
`continuous_nice_loop_strategy.h:400-413`: when `pos_a == pos_b && digit_a != digit_b`, ALL OTHER candidates of that cell are eliminated, but never confirms the loop actually forces this. On bivalue cells where the strong intra-cell link is the chain link, this double-counts the inference.

## Fix
### Simple Coloring
Lower the threshold to `< 2` (or drop the size guard — `adj[start].empty()` already filters); skip only Rule 1 for size-2 components.

### Cell-FC
After confirming the surviving branch is consistent, try elimination-finding using that single branch's mask differences vs `initial_masks`.

### GroupedXCycles Type 2
Add `else if (first_link_strong) { if (nodes[chain[0]].isSingle()) { auto r = buildType2Step(...); ... } }` branch at line 326.

### GroupedNiceLoop
After picking endpoint, verify no cell of `end_node` appears in any `chain[i]`'s cell set.

### Continuous Nice Loop
Mark each chain link with the actual edge used; for cell-internal weak edges, require cell had count ≥ 3 (not bivalue, where it's already a strong link).

Add new sentinel puzzles to `test_strategy_correctness.cpp` covering each case.

## References
- HIGH × 5 in [CODE_REVIEW_2026-05-25.md §5](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/core/strategies/simple_coloring_strategy.h:124`
- `src/core/strategies/forcing_chain_strategy.h:128`
- `src/core/strategies/grouped_x_cycles_strategy.h:326-336`
- `src/core/strategies/grouped_nice_loop_strategy.h:347-381`
- `src/core/strategies/continuous_nice_loop_strategy.h:400-413`
```

---

## P2 — Determinism + persistence + architecture (7 issues)

### Issue 14: `audit(determinism): ITimeProvider violations across solver/rater/model`

**Labels:** `priority/p2` `area/core` `area/solver` `type/audit` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Project rule (`project-context.md`): "ITimeProvider is the only legal time source." Several call sites bypass it directly with wall-clock APIs, breaking determinism in tests and re-opening the AI Escargot livelock class.

## Findings to address
- **HIGH** `backtracking_solver.cpp:51, 117` — `std::chrono::steady_clock::now()` called directly; deadline computed via `time_provider_->steadyNow()` is meaningless.
- **HIGH** `puzzle_rater.cpp:36` — `solver_->solvePuzzle(board)` called with no budget overload, regressing the AI Escargot livelock fix.
- **HIGH** `puzzle_generator.cpp:79, 121, 161-168` — generator's actual seed (from `random_device`) is not stored in `result.seed`; generated puzzles unreproducible.
- **HIGH** `game_state.h:48` — `GameState` default-constructs `SystemTimeProvider`; tests get wall-clock leakage with no compile-time signal.
- **MED** `puzzle_generator.cpp:79, 82, 121` — generator RNG shared across attempts; `result.seed` is the user seed, not the per-attempt one.
- **MED** `training_exercise_generator.cpp:39-125` — no seed contract on `IGenerateExercises`.
- **MED** `statistics_serializer.cpp:411` — `std::localtime` (non-reentrant) instead of `localtime_r`.
- **MED** `save_manager_serialization.cpp:123-124, 378-380` — move history uses `steady_clock` time_points; meaningless after process restart.

## Fix
This is one audit + sweep:
1. **Grep** the codebase for `steady_clock`, `system_clock::now`, `random_device`, `std::localtime`, `std::this_thread::sleep_for` — should turn up the above plus anything new.
2. **Refactor** each to use `ITimeProvider` (inject where not present) or the existing budget overloads.
3. **Add** a CI grep gate that fails the build on these patterns outside `src/infrastructure/` (the only legal location for the bridge to real time).
4. **Document** the contract in `docs/ITIMEPROVIDER_PATTERN.md`.

## References
- HIGH × 4 + MED × 4 in [CODE_REVIEW_2026-05-25.md §3.2, §5, §6.A](CODE_REVIEW_2026-05-25.md#32-determinism--itimeprovider-contract-has-multiple-leaks)
```

---

### Issue 15: `fix(persistence): save write must be atomic (HIGH)`

**Labels:** `priority/p2` `area/persistence` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`save_manager_serialization.cpp:180-185` writes directly to the final `file_path` with `std::ofstream`. A crash or power loss mid-write leaves a truncated/corrupt save (or auto-save) that subsequent loads will reject. `StatisticsManager::atomicWriteFile` (`statistics_manager.cpp:744`) already does this correctly via tmp+rename — mirror that.

## Impact
Single power loss during auto-save → user loses entire game state. Auto-save is the recovery path, so this defeats its purpose.

## Fix
Write to `path + ".tmp"`, `fsync`, then `std::filesystem::rename` to the target. Use `atomicWriteFile` directly if it can be promoted to a shared utility.

Add a unit test that simulates a partial write (open + truncate + don't sync) and verifies the original file is still loadable.

Also affects: **MED** `statistics_serializer.cpp:184-235` (`serializeGameStatsToYaml` rewrites entire sessions file non-atomically with `append=true`). Apply the same fix there.

## References
- HIGH + MED in [CODE_REVIEW_2026-05-25.md §5 (chunk 10) and §6.C](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/core/save_manager_serialization.cpp:180-185`
- `src/core/statistics_serializer.cpp:184-235`
```

---

### Issue 16: `fix(persistence): corrupt sessions file must never be overwritten (HIGH)`

**Labels:** `priority/p2` `area/persistence` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`statistics_manager.cpp:238-250`: when `EncryptionManager::decrypt` fails in `getAllSessions`, the code logs a warning and treats sessions as empty. The subsequent `flushSessions` then overwrites the file via `atomicWriteFile`, **destroying** the original corrupt-but-possibly-recoverable bytes.

## Impact
A single transient decryption failure (e.g., machine-id changed temporarily, hostname changed, partial filesystem read) wipes the entire statistics history.

## Fix
- Return `StatisticsError::FileAccessError` from `getAllSessions` on decrypt failure.
- Skip `flushSessions` when the in-memory state didn't load cleanly.
- Surface the failure to the user (via a non-blocking notification) so they can investigate.
- Optionally: rename the unreadable file to `*.corrupt-<timestamp>` for forensics, never silently delete.

## References
- HIGH in [CODE_REVIEW_2026-05-25.md §5 (chunk 10)](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/core/statistics_manager.cpp:238-250`
```

---

### Issue 17: `fix(stats): recalculateAggregateStats double-counts + import math wrong (HIGH + MED)`

**Labels:** `priority/p2` `area/persistence` `area/core` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Two related statistics integrity bugs:

### HIGH — `recalculateAggregateStats` double-counts pending sessions
`statistics_manager.cpp:613-647, 691-697`. The recalc calls `getAllSessions()`, which appends `pending_sessions_` to disk-loaded sessions. But pending sessions have already been `updateAggregateStats()`'d at `endGame` time.

Stats drift upward over time; `best_win_streak` / `total_games` inflate silently.

### MED — Average-time recomputation on import uses wrong divisor
`statistics_manager.cpp:389-393`. The formula already updates `games_completed[i]` on line 381, then divides by the new total. Weighted average should be computed from pre-update counts.

Result: average-time stat is corrupted on every import; can never be repaired.

Plus **MED** — `recordMove`/`recordHint` counters are plain `int`. Long-lived players can wrap at 2^31 (`statistics_manager.cpp:124-128, 140-142`).

## Fix
### `recalculateAggregateStats`
Either:
- Track which sessions have already been folded into `cached_stats_`, skip them on recalc.
- Or recompute purely from disk and `pending_sessions_.clear()` before the recalc loop.

### Average-time import
Compute weighted average from pre-update counts BEFORE incrementing `games_completed`:
`avg = (local_avg * local_done + imp_avg * imp_done) / (local_done + imp_done)`.

### Counter widths
Widen cumulative counters to `int64_t` / `uint64_t`. Compute running averages via Welford's algorithm to avoid overflow:
`mean += (sample - mean) / n`.

Add unit tests round-tripping an import twice (idempotency) and verifying `total_games == disk_count + pending_count`.

## References
- HIGH + MED × 2 in [CODE_REVIEW_2026-05-25.md §5 (chunk 10) and §6.C](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/core/statistics_manager.cpp:613-647, 389-393, 124-128, 140-142`
```

---

### Issue 18: `fix(persistence): EncryptionManager constructor should not throw (HIGH)`

**Labels:** `priority/p2` `area/persistence` `area/core` `type/refactor` `from/code-review-2026-05-25`

**Body:**
```
## Summary
`encryption_manager.cpp:43-50` constructor throws `std::runtime_error` when `sodium_init() < 0`. `SaveManager` and `StatisticsManager` both construct `EncryptionManager` unconditionally (`save_manager.cpp:51`, `statistics_manager.cpp:45`).

Per project rules: `std::expected<T, E>` for expected failures, exceptions only for genuinely exceptional conditions (OOM/corruption). libsodium init failure (missing /dev/urandom in sandbox, etc.) is recoverable.

## Impact
Uncaught exception in `SaveManager` / `StatisticsManager` constructors → app crash on startup. Inconsistent with project policy. The user sees no diagnostic.

## Fix
- Make the constructor private (or `protected`).
- Add a static factory: `static std::expected<std::unique_ptr<EncryptionManager>, EncryptionError> create();`.
- Or guard `sodium_init()` once at app startup in `main.cpp` and propagate the failure to a "save/load disabled" mode.

While here:
- **HIGH** `encryption_manager.cpp:210` — `const_cast<char*>(system_id.data())` violates the project's "no const_cast" rule and is pointless in C++17 (`std::string::data()` has a non-const overload). Drop the cast or zero a `std::vector<char>` copy.

## References
- HIGH × 2 in [CODE_REVIEW_2026-05-25.md §5 (chunk 10)](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/core/encryption_manager.cpp:43-50, 210`
```

---

### Issue 19: `fix(core): DIContainer silent nullptr + singleton race (HIGH × 2)`

**Labels:** `priority/p2` `area/core` `type/bug` `from/code-review-2026-05-25`

**Body:**
```
## Summary
Two related DIContainer hardening bugs:

### HIGH — `resolve<T>()` silently returns `nullptr` on unregistered
`di_container.h:88`. Returns `nullptr` with no diagnostic when a type was never registered. `main.cpp` dereferences (e.g., `save_manager->hasAutoSave()` line 189) without a null check — wiring omissions become far-away segfaults.

### HIGH — Singleton lazy-init race caches duplicate instance on factory re-entry
`di_container.h:82-86`. If a factory recursively `resolve<T>()`s itself (cycle), or if two resolves interleave via factory dispatch on a different type that transitively resolves T, the factory runs twice and the second store overwrites the first. Clients holding the first pointer now have a divergent shared instance.

## Impact
- Configuration errors become runtime crashes far from the omission site.
- Singleton guarantee broken — `ISudokuSolver` cached state could be lost.

## Fix
### Silent nullptr
- `spdlog::critical` + `std::abort` (loud failure), OR
- Switch return type to `std::expected<std::shared_ptr<T>, ResolveError>` for soft handling at the caller.
- Document the contract in `di_container.h`.

### Re-entry race
- Detect re-entry by setting an "in-progress" marker for the type before invoking the factory; assert / log if the same type is re-entered.
- On re-entry, return the partial instance (cycle requires a `shared_ptr` already in `instances_`), or refuse with a clear diagnostic.

Add unit tests for both: (a) `resolve<Unregistered>()` produces the expected failure mode; (b) a deliberately-cyclic factory is detected.

## References
- HIGH × 2 in [CODE_REVIEW_2026-05-25.md §5 (chunk 12)](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/core/di_container.h:82-93`
```

---

### Issue 20: `fix(viewmodel): GameCommand enum is aspirational — complete or trim (MED × 2)`

**Labels:** `priority/p2` `area/viewmodel` `area/view` `type/refactor` `from/code-review-2026-05-25`

**Body:**
```
## Summary
The `GameCommand` enum in `game_view_model.h` is documented as the canonical UI→VM verb list. Reality:

- **MED** `game_view_model_state.cpp:45-78` — `executeCommand`'s switch handles none of `NewGame`, `SaveGame`, `LoadGame`, `PauseGame`, `ResumeGame`, `ValidateBoard`, `ClearNotes`. They fall to the default `spdlog::warn` branch.
- **MED** `game_view_model_state.cpp:80-97` — `canExecuteCommand` returns `true` for every unhandled command. `SaveGame` returns true on a paused/complete game; `ResumeGame` returns true with no game.
- View bypasses the pipeline anyway: `keyPressEvent`, MainWindow buttons, and other handlers call `view_model_->startNewGame()` / `saveCurrentGame()` / `enterNumber()` directly.

Plus **HIGH** `observable.h:186` — `CompositeObserver::observe` calls `observable.clearSubscriptions()`, nuking **every** subscriber. Load-bearing single-subscriber assumption that nothing enforces; any second subscriber (training mode, dialog) silently breaks the first.

## Impact
- View can't trust `canExecuteCommand` to gate buttons — UI shows enabled actions that no-op or toast errors.
- Project-context.md is violated: "Don't bypass the command pipeline."
- Adding any second Observable subscriber breaks the first without compile/test failure.

## Decide on a direction
Either:
- **A. Complete the pipeline.** Implement all `GameCommand` verbs in `executeCommand`; tighten `canExecuteCommand` to real preconditions; refactor View to dispatch exclusively via the enum.
- **B. Trim the enum.** Remove verbs the architecture doesn't actually centralize. Document the supported surface.

Recommend (A) — single-author project, the pattern was deliberate.

### CompositeObserver fix (independent of A/B)
- Capture the unsubscribe token returned by `subscribe(CallbackFn)` and call it on dtor.
- Drop the `clearSubscriptions()` call.
- Audit `src/view/main_window.cpp` for places that assume single-subscriber semantics.

## References
- MED × 2 + HIGH in [CODE_REVIEW_2026-05-25.md §5, §6.G](CODE_REVIEW_2026-05-25.md#5-high-findings)
- `src/view_model/game_view_model_state.cpp:45-97`
- `src/core/observable.h:186`
```

---

## Issue-creation script (run after labels exist)

Save the following to `/tmp/create-issues.sh`, review, then `bash /tmp/create-issues.sh`:

```sh
#!/usr/bin/env bash
# Bulk-create the 20 issues from CODE_REVIEW_2026-05-25_ISSUES.md.
# Run with `bash /tmp/create-issues.sh` from the repo root.
# Each issue body is read from a here-doc; paste from the MD when prompted (or expand inline).

set -euo pipefail

# Sanity
gh auth status >/dev/null || { echo "gh not authenticated"; exit 1; }
[ -f docs/CODE_REVIEW_2026-05-25.md ] || { echo "Run from sudoku-cpp repo root"; exit 1; }

# === P0 ===
gh issue create \
  --title "fix(save): Master-difficulty saves silently rejected (BL-8)" \
  --label "priority/p0,area/persistence,type/bug,from/code-review-2026-05-25" \
  --body-file <(sed -n '/^### Issue 1:/,/^---$/p' docs/CODE_REVIEW_2026-05-25_ISSUES.md \
                | sed -n '/^## Summary/,$p' | sed '/^## References/,/^---$/!d;//d' || true)
# ... (repeat for each issue; recommend doing them individually with --body-file pointing
# at a per-issue tmp file extracted from this MD, to keep the body formatting clean).

# Simpler: open each in the editor instead
# gh issue create --label "..." --title "..." --web
```

(In practice: just `gh issue create` interactively per section and paste the body. The script above is a sketch — bash extraction from the MD is fiddly; trading 5 minutes of paste-and-click for cleaner formatting is the better choice for a single-author repo.)

---

## Not-promoted-to-issues (P3 / P4)

These stay in [CODE_REVIEW_2026-05-25.md §7](CODE_REVIEW_2026-05-25.md#7-low-findings) and will be picked up opportunistically:

- All 41 LOW findings.
- The MEDIUMs not bundled into the issues above (~30 items spanning strategy coverage gaps, UI hygiene, defense-in-depth, perf, SIMD micro-issues, and minor i18n).

Convert any of these to an issue at the time of fix if it grows scope.

---

*End of issue draft. See [CODE_REVIEW_2026-05-25.md](CODE_REVIEW_2026-05-25.md) for full context.*
