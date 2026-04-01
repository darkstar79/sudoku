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

#include "core/board_utils.h"
#include "core/candidate_grid.h"
#include "core/constants.h"
#include "core/i_game_validator.h"
#include "core/observable.h"
#include "game_view_model.h"
#include "model/game_state.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

void GameViewModel::enterNumber(const core::Position& pos, int number) {
    if (number < 1 || number > 9 || !isGameActive()) {
        return;
    }

    const auto& current_state = gameState.get();

    // Don't allow editing given numbers
    if (current_state.isGiven(pos)) {
        return;
    }

    // Create move
    core::Move move;
    move.position = pos;
    move.value = number;
    move.move_type = core::MoveType::PlaceNumber;
    move.timestamp = std::chrono::steady_clock::now();

    // Capture previous state for undo
    move.previous_value = current_state.getValue(pos);
    move.previous_notes = current_state.getNotes(pos);
    move.previous_hint_revealed = current_state.isCellHintRevealed(pos);

    // Check against solution board to detect mistakes (not just conflicts)
    bool is_mistake = false;
    if (current_state.hasSolution()) {
        const auto& solution = current_state.getSolutionBoard();
        is_mistake = (solution[pos.row][pos.col] != number);
    } else {
        // Fallback: conflict-only check if no solution available (e.g. loaded save)
        auto validation_result = validator_->validateMove(current_state.extractNumbers(), move);
        if (validation_result) {
            is_mistake = !*validation_result;
        } else {
            is_mistake = true;
        }
    }

    // Apply move
    applyMove(move);
    recordMove(move, is_mistake);

    spdlog::debug("Placed {} at ({}, {}){}", number, pos.row, pos.col, is_mistake ? " [MISTAKE]" : "");

    // Check for completion
    checkGameCompletion();

    updateUIState();
    autoSaveIfNeeded();
}

void GameViewModel::enterNote(const core::Position& pos, int number) {
    if (number < 1 || number > 9 || !isGameActive()) {
        return;
    }

    const auto& current_state = gameState.get();

    // Don't allow notes on given numbers or filled cells
    if (current_state.isGiven(pos) || current_state.getValue(pos) != 0) {
        return;
    }

    // Check if note already exists to determine move type
    bool note_exists = current_state.getNotes(pos).contains(number);

    // Create move for undo/redo support
    core::Move move;
    move.position = pos;
    move.value = number;
    move.move_type = note_exists ? core::MoveType::RemoveNote : core::MoveType::AddNote;
    move.is_note = true;
    move.timestamp = std::chrono::steady_clock::now();

    // Capture previous state for undo
    move.previous_value = current_state.getValue(pos);
    move.previous_notes = current_state.getNotes(pos);
    move.previous_hint_revealed = current_state.isCellHintRevealed(pos);

    // Apply move
    applyMove(move);
    recordMove(move, false);  // Notes are never mistakes

    updateUIState();
    spdlog::debug("Note {} {} at ({}, {})", number, note_exists ? "removed" : "added", pos.row, pos.col);
}

void GameViewModel::clearCell(const core::Position& pos) {
    if (!isGameActive()) {
        return;
    }

    const auto& current_state = gameState.get();

    // Don't allow clearing given numbers
    if (current_state.isGiven(pos)) {
        return;
    }

    // Only create move if there's something to clear
    if (current_state.getValue(pos) == 0 && current_state.getNotes(pos).empty()) {
        return;  // Nothing to clear
    }

    // Create move for undo/redo support
    core::Move move;
    move.position = pos;
    move.value = 0;  // Clear to empty
    move.move_type = core::MoveType::RemoveNumber;
    move.timestamp = std::chrono::steady_clock::now();

    // Capture previous state for undo
    move.previous_value = current_state.getValue(pos);
    move.previous_notes = current_state.getNotes(pos);
    move.previous_hint_revealed = current_state.isCellHintRevealed(pos);

    // Apply move
    applyMove(move);
    recordMove(move, false);  // Clearing is not a mistake

    updateUIState();
    spdlog::debug("Cell cleared at ({}, {})", pos.row, pos.col);
}

void GameViewModel::recomputeAutoNotes() {
    gameState.update([this](model::GameState& state) {
        auto board = state.extractNumbers();
        core::forEachCell([&](size_t row, size_t col) {
            core::Position pos{.row = row, .col = col};
            if (state.getValue(row, col) == 0 && !state.isGiven(row, col)) {
                auto possible = validator_->getPossibleValues(board, pos);
                core::CellNotes notes;
                for (int val : possible) {
                    notes.add(val);
                }
                state.setNotes(pos, notes);
            } else {
                state.clearNotes(pos);
            }
        });
    });
}

void GameViewModel::fillNotes() {
    recomputeAutoNotes();
    spdlog::info("Filled pencil marks for all empty cells");
}

void GameViewModel::colorCell(const core::Position& pos, uint8_t color_index) {
    gameState.update(
        [&pos, color_index](model::GameState& state) { state.setCellColor(pos.row, pos.col, color_index); });
}

void GameViewModel::handleNumberInput(const core::Position& pos, int number) {
    if (!isGameActive() || number < 1 || number > 9) {
        return;
    }
    const auto& state = gameState.get();
    if (!state.isTimerRunning()) {
        return;
    }
    const auto& cell = state.getCell(pos);
    if (cell.is_given) {
        return;
    }

    switch (getInputMode()) {
        case InputMode::Normal:
            if (cell.value == 0) {
                enterNumber(pos, number);
            } else if (cell.value == number) {
                clearCell(pos);
            }
            break;
        case InputMode::Notes:
            if (cell.value == 0) {
                enterNote(pos, number);
            }
            break;
        case InputMode::Color:
            if (number <= 6) {
                colorCell(pos, static_cast<uint8_t>(number));
            }
            break;
    }
}

void GameViewModel::clearAllCellColors() {
    gameState.update([](model::GameState& state) { state.clearAllCellColors(); });
}

std::optional<GameViewModel::AnalysisResult> GameViewModel::analyzePosition() const {
    if (!isGameActive()) {
        return std::nullopt;
    }

    const auto& state = gameState.get();
    auto board = state.extractNumbers();
    auto given_board = state.extractGivenNumbers();

    // Build candidate masks from current board
    core::CandidateGrid candidates(board);
    std::vector<uint16_t> masks(core::TOTAL_CELLS);
    core::forEachCell([&](size_t row, size_t col) {
        masks[(row * core::BOARD_SIZE) + col] = candidates.getPossibleValuesMask(row, col);
    });

    auto steps = solver_->findAllApplicableSteps(board);
    if (steps.empty()) {
        return std::nullopt;
    }

    return AnalysisResult{
        .board = board,
        .given_board = given_board,
        .candidate_masks = std::move(masks),
        .applicable_steps = std::move(steps),
    };
}

bool GameViewModel::hasBoardErrors() const {
    const auto& state = gameState.get();
    auto board = state.extractNumbers();

    // Check for structural conflicts (duplicates in row/col/box)
    if (!validator_->findConflicts(board).empty()) {
        return true;
    }

    // Check placed values against solution board
    if (state.hasSolution()) {
        const auto& solution = state.getSolutionBoard();
        bool has_wrong = false;
        core::forEachCell([&](size_t row, size_t col) {
            if (board[row][col] != 0 && !state.isGiven(row, col) && board[row][col] != solution[row][col]) {
                has_wrong = true;
            }
        });
        return has_wrong;
    }

    return false;
}

void GameViewModel::updateConflictHighlighting() {
    auto board = gameState.get().extractNumbers();
    auto conflicts = validator_->findConflicts(board);
    gameState.update([&conflicts](model::GameState& state) { state.updateConflicts(conflicts); });
}

// CPD-OFF — row/col/box iteration with different operations (remove vs add+validate)
void GameViewModel::cleanupConflictingNotes(model::GameState& state, const core::Position& pos, int number) {
    // Remove the placed number from notes in all related cells

    // Same row
    for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
        if (col != pos.col && state.getValue(pos.row, col) == 0) {
            state.removeNote({.row = pos.row, .col = col}, number);
        }
    }

    // Same column
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        if (row != pos.row && state.getValue(row, pos.col) == 0) {
            state.removeNote({.row = row, .col = pos.col}, number);
        }
    }

    // Same 3x3 block
    size_t box_start_row = (pos.row / core::BOX_SIZE) * core::BOX_SIZE;
    size_t box_start_col = (pos.col / core::BOX_SIZE) * core::BOX_SIZE;
    for (size_t row = box_start_row; row < box_start_row + core::BOX_SIZE; ++row) {
        for (size_t col = box_start_col; col < box_start_col + core::BOX_SIZE; ++col) {
            if ((row != pos.row || col != pos.col) && state.getValue(row, col) == 0) {
                state.removeNote({.row = row, .col = col}, number);
            }
        }
    }

    spdlog::debug("Cleaned up conflicting notes for number {} at ({}, {})", number, pos.row, pos.col);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — restores notes in row/col/box with conflict checking; nesting is inherent
void GameViewModel::restoreConflictingNotes(model::GameState& state, const core::Position& pos, int number) {
    // Restore the removed number to notes in all related cells (inverse of cleanupConflictingNotes)
    // Only restore if the note would still be valid (no conflicts with other placed numbers)

    // Get current board to check what numbers are already placed
    auto board = state.extractNumbers();

    // Helper: Check if a number can be a valid note at a position
    auto canBeNote = [&](const core::Position& check_pos, int num) -> bool {
        // Check row for conflicts
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            if (board[check_pos.row][col] == num) {
                return false;
            }
        }
        // Check column for conflicts
        for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
            if (board[row][check_pos.col] == num) {
                return false;
            }
        }
        // Check 3x3 box for conflicts
        size_t box_start_row = (check_pos.row / core::BOX_SIZE) * core::BOX_SIZE;
        size_t box_start_col = (check_pos.col / core::BOX_SIZE) * core::BOX_SIZE;
        for (size_t row = box_start_row; row < box_start_row + core::BOX_SIZE; ++row) {
            for (size_t col = box_start_col; col < box_start_col + core::BOX_SIZE; ++col) {
                if (board[row][col] == num) {
                    return false;
                }
            }
        }
        return true;
    };

    // Same row
    for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
        if (col != pos.col && state.getValue(pos.row, col) == 0 && canBeNote({.row = pos.row, .col = col}, number)) {
            state.addNote({.row = pos.row, .col = col}, number);
        }
    }

    // Same column
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        if (row != pos.row && state.getValue(row, pos.col) == 0 && canBeNote({.row = row, .col = pos.col}, number)) {
            state.addNote({.row = row, .col = pos.col}, number);
        }
    }

    // Same 3x3 block
    size_t box_start_row = (pos.row / core::BOX_SIZE) * core::BOX_SIZE;
    size_t box_start_col = (pos.col / core::BOX_SIZE) * core::BOX_SIZE;
    for (size_t row = box_start_row; row < box_start_row + core::BOX_SIZE; ++row) {
        for (size_t col = box_start_col; col < box_start_col + core::BOX_SIZE; ++col) {
            if ((row != pos.row || col != pos.col) && state.getValue(row, col) == 0 &&
                canBeNote({.row = row, .col = col}, number)) {
                state.addNote({.row = row, .col = col}, number);
            }
        }
    }

    spdlog::debug("Restored conflicting notes for number {} at ({}, {})", number, pos.row, pos.col);
}
// CPD-ON

}  // namespace sudoku::viewmodel
