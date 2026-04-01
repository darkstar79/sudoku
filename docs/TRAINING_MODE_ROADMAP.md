# Training Mode Roadmap

Status overview and implementation roadmap for Training Mode.

---

## Current State (March 2026)

### Completed

- [x] **Core types**: `TrainingPhase`, `TrainingInteractionMode`, `AnswerResult`, `TrainingExercise`, `TrainingUIState`, `TrainingBoard`, `TrainingCellState`
- [x] **Exercise generator**: Generates exercises by solving puzzles and extracting board snapshots at target technique steps
- [x] **TrainingViewModel**: Full state machine (5 phases), exercise loading, answer evaluation (placement + elimination), per-technique hint progression
- [x] **TrainingWidget**: 5-page QStackedWidget with technique selection, theory review, interactive exercise board, feedback, lesson complete
- [x] **MainWindow integration**: Menu item, stacked widget page switching, ViewModel wiring, error logging
- [x] **Technique descriptions**: Theory text (title, what_it_is, what_to_look_for) for all 54 techniques
- [x] **Unit tests**: Types, exercise generator (mock), ViewModel state machine
- [x] **Interactive exercise board** (unified SudokuBoardWidget) — cell selection, candidate rendering, CellRole highlighting, player color support
- [x] **Number pad for training** (TrainingNumberPad) — 1-9 buttons with mode-aware tooltips and per-cell enable/disable
- [x] **Color palette for coloring exercises** — Color A/B toggle buttons, shown only in Coloring mode
- [x] **Custom per-technique hint progressions** — 11 technique categories with 3-level progressive hints via `getTrainingHint()`
- [x] **Training statistics persistence** — per-technique stats saved to `~/.local/share/sudoku/training/training_stats.yaml`
- [x] **Technique mastery tracking** — 4-level mastery (Beginner → Intermediate → Proficient → Mastered) with UI badges
- [x] **Mastery badges on technique selection** — buttons show mastery level, lesson complete page shows current mastery
- [x] **Guided learning path** — prerequisite graph (25 technique dependencies), recommended next technique algorithm, prerequisite-locked buttons, ">> " marker on recommended technique
- [x] **Prerequisites on theory page** — shows prerequisite techniques when viewing theory for a technique
- [x] **Visual diff on feedback page** — read-only board with green/red/yellow highlights for correct/wrong/missed answers
- [x] **Show Solution button** — reveals expected answer with technique-specific hint highlights on feedback board
- [x] **Candidate hover highlighting** — hovering over a candidate highlights all cells containing that candidate
- [x] **Smooth page transitions** — fade-in animation (200ms) between training phases

---

## Phase 1: Interactive Exercise Board — COMPLETE

**Goal**: Replace the placeholder `QLabel("[ Training Board ]")` with a functional board widget.

**Status**: Complete.

### 1.1 Training Board (Unified SudokuBoardWidget)

Unified `SudokuBoardWidget` rendering from `BoardRenderData` (converted from `TrainingBoard` via `toBoardRenderData()`).

**Rendering:**
- Given cells: bold, non-interactive
- Placed values: normal weight
- Empty cells: show candidate pencil marks in a 3x3 sub-grid layout
- Highlighted cells: background color based on `CellRole` (pattern, pivot, wing, fin, etc.)
- Player selections: visual indicator (border, background tint)
- Eliminated candidates: strikethrough or red color

**Interaction:**
- Click cell to select it
- Emit `cellSelected(row, col)` signal
- Display selected cell highlight

**Files:**
- `src/view/sudoku_board_widget.h/.cpp` — unified board widget (shared with game mode via `BoardRenderData`)
- `src/view/board_render_data.h` — `RenderCell`/`BoardRenderData` types and conversion functions

### 1.2 TrainingNumberPad

Compact 1-9 button strip below the board.

**Behavior by interaction mode:**
- **Placement**: click number → place value in selected cell, set `player_selected = true`
- **Elimination**: click number → toggle candidate elimination in selected cell (remove from `candidates`, set `player_selected = true`)
- Disable numbers not in the selected cell's candidate list

**Files to create:**
- `src/view/training_number_pad.h`
- `src/view/training_number_pad.cpp`

### 1.3 Color Palette

Two toggle buttons (Color A / Color B) shown only during Coloring exercises.

**Behavior:**
- Select active color from palette
- Click cell on board → set `player_color` to active color
- Visual: Color A = blue, Color B = green

**Implementation**: Can be part of the board widget or a small companion widget in the exercise page layout.

### 1.4 Wire into TrainingWidget

- Replace `QLabel("[ Training Board ]")` in `buildExercisePage()` with `SudokuBoardWidget` + `TrainingNumberPad`
- Connect `SudokuBoardWidget` signals to update `TrainingViewModel::trainingBoard`
- Show/hide color palette based on `TrainingInteractionMode`

**File to modify:**
- `src/view/training_widget.cpp`

---

## Phase 2: Custom Per-Technique Hints — COMPLETE

**Goal**: Replace the generic hint system with technique-specific progressive reveals.

**Status**: Complete. Implemented in `src/core/training_hints.h` with 11 technique categories.

### 2.1 Hint Data Structure

```cpp
struct TrainingHint {
    std::string text;                      // Hint message to display
    std::vector<Position> highlight_cells;  // Cells to highlight on the board
    std::vector<CellRole> highlight_roles;  // Color role per highlighted cell
    std::vector<int> highlight_values;      // Candidate values to emphasize (optional)
};
```

### 2.2 Hint Progression Function

```cpp
TrainingHint getHintForTechnique(SolvingTechnique technique, int level, const SolveStep& expected);
```

Returns technique-appropriate hints at each level (1-3):

| Category | Level 1 | Level 2 | Level 3 |
|----------|---------|---------|---------|
| **Singles** | "Look at cell RxCy" | "Count candidates in this cell" | "The value is N" |
| **Subsets** | "Focus on [row/col/box] N" | Highlight subset cells | Show which candidates to eliminate |
| **Fish** | "Look for value N in these rows/cols" | Show base + cover sets | Show all eliminations |
| **Wings** | "Find the pivot cell" | Show pivot + wing cells | Show eliminations |
| **Chains** | "Start from cell RxCy" | Show chain path | Show eliminations |
| **UR** | "Look for a deadly pattern" | Highlight 4 corner cells | Show which candidate breaks the pattern |
| **ALS** | "Find an Almost Locked Set" | Highlight ALS cells + restricted common | Show eliminations |

### 2.3 Update TrainingViewModel

Modify `requestHint()` to call `getHintForTechnique()` instead of the current generic logic.

**Files to create:**
- `src/core/training_hints.h`

**Files to modify:**
- `src/view_model/training_view_model.cpp` — replace hint logic in `requestHint()`

---

## Phase 3: Training Statistics — COMPLETE

**Goal**: Track player progress across sessions.

**Status**: Complete. Implemented as `ITrainingStatisticsManager` / `TrainingStatisticsManager` with YAML persistence.

### 3.1 Training Statistics Data

Per-technique tracking (`TechniqueStats` struct):

- `total_exercises_attempted` / `total_correct`
- `best_score` (out of 5) / `best_session_hints`
- `proficient_count` / `mastered_count` — track mastery progression
- `average_hints` — running average per exercise
- `last_practiced` — timestamp

### 3.2 Mastery Criteria (`MasteryLevel` enum)

| Level | Requirement |
|-------|------------|
| Beginner | < 3 correct in best session |
| Intermediate | 3-4 correct in best session |
| Proficient | 5/5 correct with ≤ 5 hints total (at least once) |
| Mastered | 5/5 correct with 0 hints, achieved twice |

### 3.3 Persistence

Separate from game statistics. Uses `DirectoryType::TrainingStatistics` → `~/.local/share/sudoku/training/training_stats.yaml`.

### 3.4 UI Integration

- Mastery badges on technique selection buttons (e.g., "[Mastered]")
- Lesson complete page shows current mastery level for the technique
- Stats recorded automatically when lesson completes (via `TrainingViewModel::recordLessonStats()`)

### 3.5 Files Created

- `src/core/i_training_statistics_manager.h` — interface + `TechniqueStats`, `TrainingLessonResult`, `MasteryLevel`
- `src/core/training_statistics_manager.h/.cpp` — YAML-backed implementation
- `src/core/training_statistics_serializer.h/.cpp` — YAML read/write
- `tests/unit/test_training_statistics.cpp` — 6 test cases, 43 assertions

### 3.6 Files Modified

- `include/infrastructure/app_directory_manager.h` — added `DirectoryType::TrainingStatistics`
- `src/infrastructure/app_directory_manager.cpp` — added "training" subdirectory
- `src/view_model/training_view_model.h/.cpp` — stats manager injection, `recordLessonStats()`
- `src/view/training_widget.cpp` — mastery badges, lesson complete mastery display
- `src/main.cpp` — DI registration for `ITrainingStatisticsManager`

---

## Phase 4: Guided Learning Path — COMPLETE

**Goal**: Recommend techniques to practice based on player skill.

**Status**: Complete. Implemented as pure functions in `src/core/training_learning_path.h`.

### 4.1 Recommended Next Technique

Algorithm (implemented in `getRecommendedTechnique()`):
1. Find the lowest-difficulty technique not yet Mastered
2. Skip techniques whose prerequisites are not met
3. Among same-difficulty techniques, prefer least recently practiced
4. Returns `std::nullopt` when all 54 techniques are Mastered

UI: Recommended technique shown with ">> " prefix on technique selection buttons.

### 4.2 Prerequisite Graph

25 prerequisite relationships across 8 technique families:

- **Subsets**: Naked Pair → Triple → Quad, Hidden Pair → Triple → Quad
- **Fish**: X-Wing → Swordfish → Jellyfish (+ finned variants, Franken Fish)
- **Wings**: XY-Wing → XYZ → WXYZ → VWXYZ, XY-Wing → W-Wing
- **Coloring**: Simple → Multi → 3D Medusa
- **Chains**: XY-Wing → XY-Chain → Forcing Chain → Nice Loop, Simple Coloring → X-Cycles → Grouped X-Cycles
- **Unique Rectangles**: UR → Hidden UR, UR → Avoidable Rectangle
- **ALS family**: ALS-XZ → ALS-XY-Wing, Death Blossom, Sue de Coq
- **Intersections**: Hidden Single → Pointing Pair, Box/Line Reduction

Techniques with unmet prerequisites are disabled (greyed out) on the selection page. Prerequisites shown on the theory page.

Minimum prerequisite level: Intermediate (best score ≥ 3).

### 4.3 Daily Challenge

Not implemented — deferred to future enhancement.

### 4.4 Files Created

- `src/core/training_learning_path.h` — prerequisite graph, recommendation algorithm, `kAllTechniques` array
- `tests/unit/test_training_learning_path.cpp` — 12 test cases, 242 assertions

### 4.5 Files Modified

- `src/view/training_widget.cpp` — prerequisite locking, recommended badge, prerequisites on theory page

---

## Phase 5: Visual Enhancements — COMPLETE

**Goal**: Polish the training UI for a better learning experience.

**Status**: Complete. Implemented 4 of 6 items; board zoom and dark mode deferred.

### 5.1 Visual Diff on Feedback Page

After submitting an answer, a read-only `SudokuBoardWidget` on the feedback page shows the player's board with diff highlighting:

- **Green** (`CorrectAnswer`): correctly identified cells/eliminations
- **Red** (`WrongAnswer`): incorrectly marked cells
- **Yellow** (`MissedAnswer`): expected answers the player missed

Both placement and elimination modes supported via `buildDiffBoard()`.

### 5.2 Show Solution Button

"Show Solution" button on the feedback page applies level-3 hint highlights to the feedback board, revealing the full expected answer with technique-specific coloring (pivot, wings, chains, etc.).

### 5.3 Candidate Hover Highlighting

Mouse tracking on the board widget highlights all cells containing the hovered candidate value with a subtle blue tint. The hovered candidate itself is rendered bold. Disabled in read-only mode.

### 5.4 Smooth Page Transitions

`QGraphicsOpacityEffect` + `QPropertyAnimation` fade-in (200ms, 0→1 opacity) when switching between training pages. Effect is cleaned up after animation completes.

### 5.5 Deferred

- Board zoom for mobile/small screens
- Dark mode support for training pages

### 5.6 Files Modified

- `src/core/solve_step.h` — added `CorrectAnswer`, `WrongAnswer`, `MissedAnswer` to `CellRole` enum
- `src/view/sudoku_board_widget.h/.cpp` — unified board widget with read-only mode, CellRole rendering, `BoardRenderData` input
- `src/view_model/training_view_model.h/.cpp` — `feedbackBoard` Observable, `buildDiffBoard()`, `revealSolution()`
- `src/view/training_widget.h/.cpp` — feedback board widget, Show Solution button, page transitions

---

## Dependencies

| Phase | Depends On | Status |
|-------|-----------|--------|
| Phase 1 (Board) | None | **Complete** |
| Phase 2 (Hints) | Phase 1 | **Complete** |
| Phase 3 (Stats) | Phase 1 | **Complete** |
| Phase 4 (Path) | Phase 3 | **Complete** |
| Phase 5 (Visual) | Phase 1 | **Complete** |
