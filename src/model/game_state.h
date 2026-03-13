// sudoku-cpp - Offline Sudoku Game
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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace sudoku::model {

/// Represents a single cell on the Sudoku board
struct Cell {
    int value{0};                  // 0 = empty, 1-9 = filled
    std::vector<int> notes;        // Pencil marks/notes (1-9)
    bool is_given{false};          // True if this was part of original puzzle
    bool is_hint_revealed{false};  // True if value was revealed by hint
    bool has_conflict{false};      // True if this cell has a conflict
    bool is_highlighted{false};    // True if cell is highlighted in UI

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

    // Board access
    [[nodiscard]] const std::vector<std::vector<Cell>>& getBoard() const {
        return board_;
    }
    [[nodiscard]] std::vector<std::vector<Cell>>& getBoard() {
        return board_;
    }

    [[nodiscard]] Cell& getCell(size_t row, size_t col) {
        return board_[row][col];
    }
    [[nodiscard]] const Cell& getCell(size_t row, size_t col) const {
        return board_[row][col];
    }

    [[nodiscard]] Cell& getCell(const core::Position& pos) {
        return board_[pos.row][pos.col];
    }
    [[nodiscard]] const Cell& getCell(const core::Position& pos) const {
        return board_[pos.row][pos.col];
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

    // Time tracking
    [[nodiscard]] std::chrono::milliseconds getElapsedTime() const;
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

    // Selection state
    [[nodiscard]] std::optional<core::Position> getSelectedPosition() const {
        return selected_position_;
    }
    void setSelectedPosition(const core::Position& pos);
    void clearSelection();
    [[nodiscard]] bool hasSelection() const {
        return selected_position_.has_value();
    }

    // Board operations
    void clearBoard();
    void loadPuzzle(const std::vector<std::vector<int>>& puzzle);
    [[nodiscard]] std::vector<std::vector<int>> extractNumbers() const;
    [[nodiscard]] std::vector<std::vector<int>> extractGivenNumbers() const;

    // Conflict tracking
    void updateConflicts(const std::vector<core::Position>& conflicts);
    void clearConflicts();
    [[nodiscard]] std::vector<core::Position> getConflictPositions() const;

    // Notes operations
    void addNote(const core::Position& pos, int value);
    void removeNote(const core::Position& pos, int value);
    void clearNotes(const core::Position& pos);
    void toggleNote(const core::Position& pos, int value);

    // Highlighting
    void highlightNumber(int number);
    void clearHighlights();
    void highlightRelated(const core::Position& pos);  // Same row/col/box

    // Hint tracking
    void markCellAsHintRevealed(const core::Position& pos);
    [[nodiscard]] bool isCellHintRevealed(const core::Position& pos) const;

    // Equality comparison for Observable pattern
    bool operator==(const GameState& other) const;
    bool operator!=(const GameState& other) const {
        return !(*this == other);
    }

    // Solution access
    void setSolutionBoard(const std::vector<std::vector<int>>& solution);
    [[nodiscard]] const std::vector<std::vector<int>>& getSolutionBoard() const {
        return solution_board_;
    }
    [[nodiscard]] bool hasSolution() const {
        return !solution_board_.empty();
    }

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

    // Board data
    std::vector<std::vector<Cell>> board_;
    std::vector<std::vector<int>> solution_board_;  // Complete solution for hints

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

    // UI state
    std::optional<core::Position> selected_position_;

    // Auto-save optimization
    bool is_dirty_{false};  // Tracks if state changed since last save

    void initializeBoard();
};

}  // namespace sudoku::model