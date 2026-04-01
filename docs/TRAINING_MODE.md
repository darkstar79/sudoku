# Training Mode

Training Mode is a dedicated learning environment where players practice individual Sudoku solving techniques through guided exercises. It follows the same MVVM architecture as the main game, with its own ViewModel, data types, and UI widgets.

---

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Lesson Flow](#lesson-flow)
- [Interaction Modes](#interaction-modes)
- [Exercise Generation](#exercise-generation)
- [Answer Evaluation](#answer-evaluation)
- [Progressive Hint System](#progressive-hint-system)
- [Technique Groups](#technique-groups)
- [Key Files](#key-files)

---

## Overview

Training Mode teaches all 42 logical solving techniques (everything except Backtracking). For each technique, the player:

1. Reads a theory description explaining what the technique is and what to look for
2. Completes 5 exercises where the technique must be applied
3. Receives immediate feedback with explanations after each attempt
4. Gets a score summary at the end of the lesson

The system generates exercises dynamically by solving random puzzles and extracting board snapshots at the exact point where the target technique is needed.

---

## Architecture

Training Mode follows the project's MVVM pattern:

```
┌─────────────────────┐     ┌──────────────────────┐     ┌─────────────────────┐
│   Model (Core)      │     │   ViewModel          │     │   View (Qt6)        │
│                     │     │                      │     │                     │
│ TrainingExercise-   │────▶│ TrainingViewModel    │────▶│ TrainingWidget      │
│   Generator         │     │   (state machine)    │     │   (5 pages)         │
│                     │     │                      │     │                     │
│ TechniqueDescrip-   │     │ Observable<          │     │ SudokuBoardWidget   │
│   tions             │     │   TrainingUIState>   │     │   (unified)         │
│                     │     │ Observable<          │     │                     │
│ TrainingTypes       │     │   TrainingBoard>     │     │                     │
└─────────────────────┘     └──────────────────────┘     └─────────────────────┘
```

### Dependency Injection

`TrainingViewModel` depends on two interfaces:

- **`ITrainingExerciseGenerator`** — generates exercises for a given technique
- **`ILocalizationManager`** — provides localized UI strings

Both are injected via constructor, enabling mock-based unit testing.

---

## Lesson Flow

Training Mode operates as a 5-phase state machine managed by `TrainingViewModel`:

```
┌─────────────────────┐
│ TechniqueSelection  │◀──────────────────────────────┐
│ (pick a technique)  │                                │
└────────┬────────────┘                                │
         │ selectTechnique()                           │
         ▼                                             │
┌─────────────────────┐                                │
│ TheoryReview        │                                │
│ (read description)  │                                │
└────────┬────────────┘                                │
         │ startExercises()                            │
         ▼                                             │
┌─────────────────────┐                                │
│ Exercise            │◀──┐                            │
│ (interact w/ board) │   │ retryExercise()            │
└────────┬────────────┘   │                            │
         │ submitAnswer() │                            │
         ▼                │                            │
┌─────────────────────┐   │                            │
│ Feedback            │───┘                            │
│ (correct/incorrect) │                                │
└────────┬────────────┘                                │
         │ nextExercise()                              │
         ▼ (after last exercise)                       │
┌─────────────────────┐                                │
│ LessonComplete      │───── returnToSelection() ─────┘
│ (score summary)     │
└─────────────────────┘
```

### Phase Details

| Phase | UI Page | User Actions |
|-------|---------|-------------|
| TechniqueSelection | Scrollable list of 54 techniques grouped into 9 categories | Click a technique button |
| TheoryReview | Title, SE difficulty rating, "What It Is", "What to Look For" | Start Exercises / Back |
| Exercise | Interactive board + number pad + hint/skip/quit buttons | Interact with board, Submit, Hint, Skip |
| Feedback | Result (Correct/Partial/Incorrect), explanation, score | Next Exercise / Retry / Quit |
| LessonComplete | Final score, verdict message (Excellent/Good/Keep Practicing) | Try Again / Pick Technique / Return to Game |

---

## Interaction Modes

Each exercise uses one of three interaction modes, determined by the technique:

### Placement

Used for: **Naked Single**, **Hidden Single**

The player identifies a cell and places a value. Select a cell, then press a number to place it.

### Elimination

Used for: most techniques (Naked Pair, X-Wing, Swordfish, etc.)

The player identifies which candidates should be eliminated. Select a cell, then press a number to mark that candidate for elimination (similar to the game's note-entry flow).

### Coloring

Used for: **Simple Coloring**, **Multi-Coloring**

The player assigns colors to cells/candidates to identify chain relationships. Select a color from a palette (Color A / Color B), then click cells to paint them.

For coloring techniques, the first 2 exercises are "guided" (`is_guided_coloring = true`), meaning chain nodes are pre-colored to help the player learn the pattern before doing it independently.

---

## Exercise Generation

`TrainingExerciseGenerator` creates exercises by:

1. **Generate a puzzle** at appropriate difficulty (based on SE rating)
2. **Solve it completely** to get the full `solve_path` (vector of `SolveStep`)
3. **Scan the path** for steps using the target technique
4. **Walk forward** from the initial board, applying all steps before the target to snapshot the exact board state and candidate masks
5. **Package** as a `TrainingExercise` with board, candidates, expected step, and interaction mode

### Difficulty Mapping

| Technique Points | Puzzle Difficulty | Retry Budget | Examples |
|-----------------|-------------------|-------------|----------|
| ≤150 | Medium | 10 | Naked Single, Hidden Pair, Pointing Pair |
| 151-280 | Hard | 20 | X-Wing, XY-Wing, Swordfish |
| 281-400 | Expert | 50 | Jellyfish, Unique Rectangle, Empty Rectangle |
| 401+ | Master | 100 | XY-Chain, ALS-XZ, Forcing Chain, Nice Loop |

### Board Snapshot

The `walkForward()` method replays the solve path up to the target step:

- Placements are applied to the board via `CandidateGrid::placeValue()`
- Eliminations are applied via `CandidateGrid::eliminateCandidate()`
- Candidate bitmasks are captured for all 81 cells (filled cells get mask 0)

This ensures the exercise board is in the exact state where the target technique applies.

---

## Answer Evaluation

### Placement Evaluation

Checks if the player placed the correct value in the correct cell. Binary result: Correct or Incorrect.

### Elimination Evaluation

Compares the player's eliminated candidates against the expected eliminations using set comparison:

| Player Eliminations vs Expected | Result |
|-------------------------------|--------|
| Exact match | Correct |
| Some correct, none wrong | PartiallyCorrect |
| Some correct, some wrong | PartiallyCorrect |
| None correct | Incorrect |

Player eliminations are detected by comparing the player's board candidates against the original exercise candidate masks — any removed candidate is treated as an elimination.

---

## Progressive Hint System

Players can request up to 3 hints per exercise (tracked in `hints_used` for scoring). Hints are technique-specific, implemented in `src/core/training_hints.h` via `getTrainingHint()`.

All 54 techniques are classified into 11 categories, each with tailored 3-level hint progressions:

| Category | Hint 1 | Hint 2 | Hint 3 |
|----------|--------|--------|--------|
| Singles | Point to target cell | Show constraining region | Reveal the value |
| Subsets | Identify the containing unit | Highlight subset cells with roles | Show elimination targets |
| Intersections | Name the confined candidate | Highlight intersection cells | Show eliminations outside intersection |
| Fish | Name the fish candidate | Show base + cover sets with roles | Show cover set eliminations |
| Wings | Identify the pivot cell | Show pivot + wing cells with roles | Show shared candidate eliminations |
| Single-Digit | Name the conjugate pair digit | Show chain cells with colors | Show elimination targets |
| Coloring | Name the coloring digit | Show chain with alternating colors | Show cells seeing both colors |
| Unique Rectangles | Describe deadly pattern shape | Highlight rectangle corners (Roof/Floor) | Show pattern-breaking elimination |
| Chains | Point to chain start cell | Show full chain path with colors | Show eliminations (or forced placement) |
| Set Logic | Describe the ALS structure | Highlight ALS cells + restricted common | Show elimination targets |
| Special (BUG) | Identify the trivalue cell | Point to key cell | Show full explanation |

Each hint level highlights relevant cells on the board using `CellRole`-based colors and displays explanatory text in the exercise UI.

---

## Technique Groups

The technique selection page organizes all 54 techniques into 9 groups:

| Group | Techniques |
|-------|-----------|
| Foundations | Naked Single, Hidden Single |
| Subset Basics | Naked Pair, Naked Triple, Hidden Pair, Hidden Triple |
| Intersections & Quads | Pointing Pair, Box Line Reduction, Naked Quad, Hidden Quad |
| Basic Fish & Wings | X-Wing, XY-Wing, Swordfish, Skyscraper, Two-String Kite, XYZ-Wing |
| Links & Rectangles | Unique Rectangle, W-Wing, Simple Coloring, Finned X-Wing, Remote Pairs, BUG |
| Advanced Fish & Wings | Jellyfish, Finned Swordfish, Empty Rectangle, WXYZ-Wing, Multi-Coloring |
| Advanced Fish (Finned) | Finned Jellyfish |
| Chains & Set Logic | XY-Chain, ALS-XZ, Sue de Coq |
| Inference Engines | Forcing Chain, Nice Loop |

Each button displays the technique name and Sudoku Explainer (SE) difficulty rating (e.g., "X-Wing (SE 3.2)"). Ratings follow the [SE scale](https://github.com/SudokuMonster/SukakuExplainer/wiki/Difficulty-ratings-in-Sudoku-Explainer-v1.2.1) by Nicolas Juillerat, the only generally accepted difficulty standard in the Sudoku community. A puzzle's difficulty equals the hardest technique required to solve it.

---

## Key Files

### Core (Model)

| File | Purpose |
|------|---------|
| `src/core/training_types.h` | `TrainingPhase`, `TrainingInteractionMode`, `AnswerResult`, `TrainingCellState`, `TrainingBoard`, `TrainingExercise`, `TrainingUIState` |
| `src/core/i_training_exercise_generator.h` | Interface for exercise generation |
| `src/core/training_exercise_generator.h/.cpp` | Implementation: puzzle generation → solve → walk forward → snapshot |
| `src/core/technique_descriptions.h` | `TechniqueDescription` with title, what_it_is, what_to_look_for for all 54 techniques |
| `src/core/training_hints.h` | `TrainingHint`, `TechniqueCategory`, `getTrainingHint()` — per-technique 3-level progressive hints |

### ViewModel

| File | Purpose |
|------|---------|
| `src/view_model/training_view_model.h/.cpp` | State machine, exercise loading, answer evaluation, hint progression |

### View (Qt6)

| File | Purpose |
|------|---------|
| `src/view/training_widget.h/.cpp` | 5-page `QStackedWidget` container with technique selection, theory, exercise, feedback, and lesson complete pages |
| `src/view/sudoku_board_widget.h/.cpp` | Unified board widget (shared with game mode) with cell selection, candidate rendering, CellRole highlighting, player coloring via `BoardRenderData` |
| `src/view/board_render_data.h` | `RenderCell`/`BoardRenderData` types and conversion functions from `GameState` and `TrainingBoard` |
| `src/view/training_number_pad.h/.cpp` | 1-9 button strip with mode-aware tooltips and per-cell enable/disable |
| `src/view/main_window.h/.cpp` | Integration: menu item "Training Mode", stacked widget page switching, ViewModel wiring |

### Tests

| File | Purpose |
|------|---------|
| `tests/unit/test_training_types.cpp` | Type construction, equality, defaults |
| `tests/unit/test_training_exercise_generator.cpp` | Exercise generation with mock dependencies |
| `tests/unit/test_training_view_model.cpp` | State machine transitions, answer evaluation, hint progression |
