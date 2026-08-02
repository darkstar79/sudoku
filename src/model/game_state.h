// sudoku - Offline Sudoku Game
// Copyright (C) 2025-2026 Alexander Bendlin (darkstar79)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "../core/i_game_validator.h"
#include "../core/i_puzzle_generator.h"
#include "../core/i_time_provider.h"
#include "../core/observable.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace sudoku::model {

/// Represents a single cell on the Sudoku board
struct Cell {
    int value{0};                  // 0 = empty, 1-9 = filled
    core::CellNotes notes;         // Pencil marks/notes (1-9) as bitmask
    bool is_given{false};          // True if this was part of original puzzle
    bool is_hint_revealed{false};  // True if value was revealed by hint
    bool has_conflict{false};      // True if this cell has a conflict

    bool operator==(const Cell& other) const = default;
};

/// Represents the current state of a Sudoku game
class GameState {
public:
    explicit GameState(
        std::shared_ptr<core::ITimeProvider> time_provider = std::make_shared<core::SystemTimeProvider>());
    ~GameState() = default;
    GameState(const GameState&) = default;
    GameState& operator=(const GameState&) = default;
    GameState(GameState&&) = default;
    GameState& operator=(GameState&&) = default;

    // Board access (read-only snapshot assembled from flat arrays)
    [[nodiscard]] Cell getCell(size_t row, size_t col) const {
        return Cell{
            .value = values_[row][col],
            .notes = notes_data_[row][col],
            .is_given = givens_(row, col),
            .is_hint_revealed = hints_revealed_(row, col),
            .has_conflict = conflicts_(row, col),
        };
    }

    [[nodiscard]] Cell getCell(const core::Position& pos) const {
        return getCell(pos.row, pos.col);
    }

    // Game metadata
    [[nodiscard]] core::Difficulty getDifficulty() const {
        return difficulty_;
    }
    void setDifficulty(core::Difficulty difficulty);

    [[nodiscard]] uint32_t getPuzzleSeed() const {
        return puzzle_seed_;
    }
    void setPuzzleSeed(uint32_t seed);

    [[nodiscard]] bool isComplete() const {
        return is_complete_;
    }
    void setComplete(bool complete);

    /// True once a puzzle is loaded (has at least one given). Unlike hasSolution(), this holds for
    /// games restored from a save and for custom-puzzle edit mode, neither of which sets a
    /// solution_board_ — so it is the right "a game is in progress" signal for save/pause/resume.
    [[nodiscard]] bool hasPuzzle() const {
        return !givens_.empty();
    }

    // Time tracking
    [[nodiscard]] std::chrono::milliseconds getElapsedTime() const;

    /// Re-seats the accumulated play time — the restore path's counterpart to resetTimer().
    /// A running timer is re-anchored to "now" so the in-flight span is discarded rather than
    /// added on top of @p elapsed. Negative input clamps to 0 (sink-side defense against a
    /// corrupt save; range validation of the YAML field itself is the deserializer's job).
    void setElapsedTime(std::chrono::milliseconds elapsed);

    void startTimer();
    void pauseTimer();
    void resumeTimer();
    void resetTimer();
    [[nodiscard]] bool isTimerRunning() const {
        return timer_running_;
    }

    // Move tracking
    [[nodiscard]] int getMoveCount() const {
        return move_count_;
    }
    [[nodiscard]] int getMistakeCount() const {
        return mistake_count_;
    }

    void incrementMoves();
    void incrementMistakes();

    /// Re-seats the mistake counter when resuming a saved game so the game-info dialog reports
    /// the true lifetime count. Negative input clamps to 0.
    void setMistakeCount(int count);

    // Board operations
    void clearBoard();
    void loadPuzzle(const core::BoardData& puzzle);
    [[nodiscard]] core::BoardData extractNumbers() const;
    [[nodiscard]] core::BoardData extractGivenNumbers() const;

    // Per-cell value access
    void setValue(size_t row, size_t col, int value);
    void setValue(const core::Position& pos, int value);
    [[nodiscard]] int getValue(size_t row, size_t col) const;
    [[nodiscard]] int getValue(const core::Position& pos) const;

    // Per-cell flag access
    [[nodiscard]] bool isGiven(size_t row, size_t col) const;
    [[nodiscard]] bool isGiven(const core::Position& pos) const;
    void setGiven(const core::Position& pos, bool given);
    void setConflict(const core::Position& pos, bool conflict);
    void setHintRevealed(const core::Position& pos, bool revealed);

    // Per-cell notes access
    void setNotes(const core::Position& pos, const core::CellNotes& notes);
    [[nodiscard]] const core::CellNotes& getNotes(size_t row, size_t col) const;
    [[nodiscard]] const core::CellNotes& getNotes(const core::Position& pos) const;

    // Conflict tracking
    void updateConflicts(const std::vector<core::Position>& conflicts);
    void clearConflicts();
    [[nodiscard]] std::vector<core::Position> getConflictPositions() const;

    // Notes operations
    void addNote(const core::Position& pos, int value);
    void removeNote(const core::Position& pos, int value);
    void clearNotes(const core::Position& pos);
    void toggleNote(const core::Position& pos, int value);

    // Hint tracking
    void markCellAsHintRevealed(const core::Position& pos);
    [[nodiscard]] bool isCellHintRevealed(const core::Position& pos) const;

    // Equality comparison for Observable pattern
    bool operator==(const GameState& other) const;
    bool operator!=(const GameState& other) const {
        return !(*this == other);
    }

    // Solution access
    void setSolutionBoard(const core::BoardData& solution);

    /// The solution board, or nullopt when this game has none. Games restored from a save and
    /// custom-puzzle (edit-mode) games never carry one — the save format has no solution field and
    /// commitEditedPuzzle does not run the solver — so a solution-less GameState is a supported
    /// shape, not an error.
    ///
    /// Prefer this accessor at any call site that cannot establish hasSolution() on the two lines
    /// above it. Story 8.16 / D1: resetGame() read getSolutionBoard() with no gate and threw
    /// std::bad_optional_access out of a Qt slot the moment a player pressed Reset on a resumed
    /// game — the default launch path. The reference accessor's precondition used to live only in a
    /// comment, which is exactly the kind of contract one of three callers can quietly break.
    [[nodiscard]] std::optional<core::BoardData> trySolutionBoard() const {
        return solution_board_;
    }

    /// @pre hasSolution(). Throws std::bad_optional_access otherwise — use trySolutionBoard() when
    /// the caller cannot guarantee the precondition locally. scripts/check_solution_access.sh pins
    /// that every src/ caller of this overload sits behind a hasSolution() gate.
    ///
    /// Written as an explicit check rather than `solution_board_.value()` so that
    /// bugprone-unchecked-optional-access can verify the access instead of being suppressed. The
    /// NOLINT this replaces asserted a caller contract nothing enforced — and one of the three
    /// callers had quietly broken it for as long as the comment existed (story 8.16 / D1).
    /// Behaviour is unchanged: an absent solution still throws std::bad_optional_access.
    [[nodiscard]] const core::BoardData& getSolutionBoard() const {
        if (!solution_board_.has_value()) {
            throw std::bad_optional_access{};
        }
        return *solution_board_;
    }
    [[nodiscard]] bool hasSolution() const {
        return solution_board_.has_value();
    }

    // Analysis cell colors (ephemeral — not saved, not part of undo)
    void setCellColor(size_t row, size_t col, uint8_t color);
    [[nodiscard]] uint8_t getCellColor(size_t row, size_t col) const;
    void clearAllCellColors();

    // Dirty flag for auto-save optimization
    [[nodiscard]] bool isDirty() const {
        return is_dirty_;
    }
    void markDirty() {
        is_dirty_ = true;
    }
    void clearDirty() {
        is_dirty_ = false;
    }

    // Observable state notifications
    core::Observable<bool> onBoardChanged;
    core::Observable<int> onMoveCountChanged;
    core::Observable<bool> onGameCompleted;
    core::Observable<core::Difficulty> onDifficultyChanged;

private:
    // Dependencies (must be initialized first)
    std::shared_ptr<core::ITimeProvider> time_provider_;

    // Board data (flat arrays — no heap allocations)
    core::BoardData values_;
    core::NotesData notes_data_;
    core::CellFlags givens_;
    core::CellFlags hints_revealed_;
    core::CellFlags conflicts_;
    std::optional<core::BoardData> solution_board_;  // Complete solution for hints

    // Game metadata
    core::Difficulty difficulty_{core::Difficulty::Medium};
    uint32_t puzzle_seed_{0};
    bool is_complete_{false};

    // Timing
    std::chrono::system_clock::time_point start_time_;
    std::chrono::system_clock::time_point pause_time_;
    std::chrono::milliseconds elapsed_time_{0};
    bool timer_running_{false};

    // Move tracking
    int move_count_{0};
    int mistake_count_{0};

    // Analysis cell colors (ephemeral — not saved/loaded, not part of undo)
    std::array<std::array<uint8_t, core::BOARD_SIZE>, core::BOARD_SIZE> cell_colors_{};

    // Auto-save optimization
    bool is_dirty_{false};  // Tracks if state changed since last save
};

}  // namespace sudoku::model