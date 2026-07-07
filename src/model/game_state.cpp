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

#include "game_state.h"

#include "core/board_utils.h"
#include "core/constants.h"
#include "core/i_game_validator.h"
#include "core/i_puzzle_generator.h"
#include "core/i_time_provider.h"
#include "core/observable.h"

#include <utility>

namespace sudoku::model {

GameState::GameState(std::shared_ptr<core::ITimeProvider> time_provider)
    : onBoardChanged(false), onMoveCountChanged(0), onGameCompleted(false),
      onDifficultyChanged(core::Difficulty::Medium), time_provider_(std::move(time_provider)) {
}

std::chrono::milliseconds GameState::getElapsedTime() const {
    if (timer_running_) {
        auto now = time_provider_->systemNow();
        auto current_session = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        return elapsed_time_ + current_session;
    }
    return elapsed_time_;
}

void GameState::startTimer() {
    start_time_ = time_provider_->systemNow();
    timer_running_ = true;
    markDirty();
}

void GameState::pauseTimer() {
    if (timer_running_) {
        pause_time_ = time_provider_->systemNow();
        elapsed_time_ += std::chrono::duration_cast<std::chrono::milliseconds>(pause_time_ - start_time_);
        timer_running_ = false;
        markDirty();
    }
}

void GameState::resumeTimer() {
    if (!timer_running_) {
        start_time_ = time_provider_->systemNow();
        timer_running_ = true;
        markDirty();
    }
}

void GameState::resetTimer() {
    elapsed_time_ = std::chrono::milliseconds{0};
    timer_running_ = false;
    markDirty();
}

void GameState::setSolutionBoard(const core::BoardData& solution) {
    solution_board_ = solution;
    markDirty();
}

void GameState::setPuzzleSeed(uint32_t seed) {
    puzzle_seed_ = seed;
    markDirty();
}

void GameState::clearBoard() {
    values_ = core::BoardData{};
    notes_data_ = core::NotesData{};
    givens_ = core::CellFlags{};
    hints_revealed_ = core::CellFlags{};
    conflicts_ = core::CellFlags{};

    // Reset game state
    is_complete_ = false;
    move_count_ = 0;
    mistake_count_ = 0;
    resetTimer();

    // Notify observers
    onBoardChanged.set(true);
    onMoveCountChanged.set(move_count_);
    onGameCompleted.set(is_complete_);
    markDirty();
}

void GameState::setDifficulty(core::Difficulty difficulty) {
    if (difficulty_ != difficulty) {
        difficulty_ = difficulty;
        onDifficultyChanged.set(difficulty_);
        markDirty();
    }
}

void GameState::setComplete(bool complete) {
    if (is_complete_ != complete) {
        is_complete_ = complete;
        onGameCompleted.set(is_complete_);
        markDirty();
    }
}

void GameState::incrementMoves() {
    ++move_count_;
    onMoveCountChanged.set(move_count_);
    markDirty();
}

void GameState::incrementMistakes() {
    ++mistake_count_;
    markDirty();
}

void GameState::loadPuzzle(const core::BoardData& puzzle) {
    clearBoard();

    core::forEachCell([&](size_t row, size_t col) {
        if (puzzle[row][col] != 0) {
            values_[row][col] = puzzle[row][col];
            givens_.set(row, col, true);
        }
    });

    // Notify that board has changed
    onBoardChanged.set(true);
    markDirty();
}

core::BoardData GameState::extractNumbers() const {
    return values_;
}

core::BoardData GameState::extractGivenNumbers() const {
    core::BoardData given;

    core::forEachCell([&](size_t row, size_t col) {
        if (givens_(row, col)) {
            given[row][col] = values_[row][col];
        }
    });

    return given;
}

// Per-cell value access

void GameState::setValue(size_t row, size_t col, int value) {
    // Bounds guard (Story 7.1, defense in depth) — match the sibling addNote/removeNote setters so an
    // out-of-range position from any caller (e.g. a replayed move from a hostile save) is a no-op
    // rather than an out-of-bounds write. The (Position, value) overload delegates here, so it is
    // covered too. Load-time validation is the primary defense; this is belt-and-suspenders.
    if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
        values_[row][col] = value;
        markDirty();
    }
}

void GameState::setValue(const core::Position& pos, int value) {
    setValue(pos.row, pos.col, value);
}

int GameState::getValue(size_t row, size_t col) const {
    return values_[row][col];
}

int GameState::getValue(const core::Position& pos) const {
    return getValue(pos.row, pos.col);
}

// Per-cell flag access

bool GameState::isGiven(size_t row, size_t col) const {
    return givens_(row, col);
}

bool GameState::isGiven(const core::Position& pos) const {
    return isGiven(pos.row, pos.col);
}

void GameState::setGiven(const core::Position& pos, bool given) {
    givens_.set(pos.row, pos.col, given);
    markDirty();
}

void GameState::setConflict(const core::Position& pos, bool conflict) {
    conflicts_.set(pos.row, pos.col, conflict);
    markDirty();
}

void GameState::setHintRevealed(const core::Position& pos, bool revealed) {
    hints_revealed_.set(pos.row, pos.col, revealed);
    markDirty();
}

// Per-cell notes access

void GameState::setNotes(const core::Position& pos, const core::CellNotes& notes) {
    // Bounds guard (Story 7.1, defense in depth) — see setValue above.
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        notes_data_[pos.row][pos.col] = notes;
        markDirty();
    }
}

const core::CellNotes& GameState::getNotes(size_t row, size_t col) const {
    return notes_data_[row][col];
}

const core::CellNotes& GameState::getNotes(const core::Position& pos) const {
    return getNotes(pos.row, pos.col);
}

void GameState::updateConflicts(const std::vector<core::Position>& conflicts) {
    // Clear existing conflicts
    clearConflicts();

    // Set new conflicts
    for (const auto& pos : conflicts) {
        if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
            conflicts_.set(pos.row, pos.col, true);
        }
    }
    markDirty();
}

void GameState::clearConflicts() {
    conflicts_.reset();
    markDirty();
}

std::vector<core::Position> GameState::getConflictPositions() const {
    std::vector<core::Position> conflicts;

    core::forEachCell([&](size_t row, size_t col) {
        if (conflicts_(row, col)) {
            conflicts.push_back({.row = row, .col = col});
        }
    });

    return conflicts;
}

void GameState::addNote(const core::Position& pos, int value) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE && value >= core::MIN_VALUE &&
        value <= core::MAX_VALUE) {
        auto& notes = notes_data_[pos.row][pos.col];
        if (!notes.contains(value)) {
            notes.add(value);
            markDirty();
        }
    }
}

void GameState::removeNote(const core::Position& pos, int value) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        auto& notes = notes_data_[pos.row][pos.col];
        if (notes.contains(value)) {
            notes.remove(value);
            markDirty();
        }
    }
}

void GameState::clearNotes(const core::Position& pos) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        auto& notes = notes_data_[pos.row][pos.col];
        if (!notes.empty()) {
            notes.clear();
            markDirty();
        }
    }
}

void GameState::toggleNote(const core::Position& pos, int value) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE && value >= core::MIN_VALUE &&
        value <= core::MAX_VALUE) {
        auto& notes = notes_data_[pos.row][pos.col];
        if (notes.contains(value)) {
            notes.remove(value);
        } else {
            notes.add(value);
        }
        markDirty();
    }
}

void GameState::markCellAsHintRevealed(const core::Position& pos) {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        hints_revealed_.set(pos.row, pos.col, true);
        markDirty();
    }
}

bool GameState::isCellHintRevealed(const core::Position& pos) const {
    if (pos.row < core::BOARD_SIZE && pos.col < core::BOARD_SIZE) {
        return hints_revealed_(pos.row, pos.col);
    }
    return false;
}

bool GameState::operator==(const GameState& other) const {
    // For Observable pattern, compare key state that UI cares about.
    // NOTE: timer_running_ MUST be included - it determines isGameActive() which gates all input.
    // cell_colors_ MUST be included even though colors are ephemeral (not saved/undone): they are
    // rendered, so Observable<GameState>::update() must detect a color-only change and notify —
    // otherwise the board does not repaint until the next unrelated change.
    return values_ == other.values_ && notes_data_ == other.notes_data_ && givens_ == other.givens_ &&
           hints_revealed_ == other.hints_revealed_ && conflicts_ == other.conflicts_ &&
           cell_colors_ == other.cell_colors_ && difficulty_ == other.difficulty_ &&
           is_complete_ == other.is_complete_ && move_count_ == other.move_count_ &&
           mistake_count_ == other.mistake_count_ && timer_running_ == other.timer_running_;
}

void GameState::setCellColor(size_t row, size_t col, uint8_t color) {
    if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
        cell_colors_[row][col] = color;
    }
}

uint8_t GameState::getCellColor(size_t row, size_t col) const {
    if (row < core::BOARD_SIZE && col < core::BOARD_SIZE) {
        return cell_colors_[row][col];
    }
    return 0;
}

void GameState::clearAllCellColors() {
    for (auto& row : cell_colors_) {
        row.fill(0);
    }
}

}  // namespace sudoku::model
