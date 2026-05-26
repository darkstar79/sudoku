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

#include "core/i18n_helpers.h"
#include "core/i_game_validator.h"
#include "core/i_statistics_manager.h"
#include "core/observable.h"
#include "game_view_model.h"
#include "model/game_state.h"

#include <chrono>
#include <expected>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

void GameViewModel::undo() {
    if (!canUndo()) {
        return;
    }

    resetCoachingIfNotTryIt();

    // Skip over hint moves - they can't be undone
    while (move_history_index_ >= 0) {
        const auto& move = move_history_[move_history_index_];

        if (move.move_type != core::MoveType::PlaceHint) {
            // Regular move - undo it
            revertMove(move);
            move_history_index_--;
            updateConflictHighlighting();
            updateUIState();
            spdlog::debug("Move undone");
            return;
        }

        // Skip hint move
        move_history_index_--;
        spdlog::debug("Skipped hint move during undo");
    }

    // If we get here, all remaining moves were hints
    updateUIState();
}

void GameViewModel::redo() {
    if (!canRedo()) {
        return;
    }

    resetCoachingIfNotTryIt();

    move_history_index_++;
    const auto& move = move_history_[move_history_index_];
    applyMove(move);
    updateConflictHighlighting();

    updateUIState();
    spdlog::debug("Move redone");
}

void GameViewModel::undoToLastValid() {
    // Check if we have a recorded valid state
    if (last_valid_state_index_ < 0) {
        uiState.update(
            [](auto& ui) { ui.status_message = std::string(core::loc("Sudoku", "No valid state in history")); });
        return;
    }

    // Check if current state has errors (conflicts OR wrong values vs solution)
    if (!hasBoardErrors()) {
        uiState.update(
            [](auto& ui) { ui.status_message = std::string(core::loc("Sudoku", "Board is already valid")); });
        return;
    }

    resetCoachingIfNotTryIt();

    // Walk back exactly one move per iteration. Delegating to undo() in a loop is unsafe:
    // a single undo() call decrements move_history_index_ by 1 + (number of contiguous hint
    // moves immediately below current), which can step past last_valid_state_index_.
    while (move_history_index_ > last_valid_state_index_) {
        const auto& move = move_history_[move_history_index_];
        if (move.move_type != core::MoveType::PlaceHint) {
            revertMove(move);
        }
        move_history_index_--;
    }

    updateConflictHighlighting();
    updateUIState();
    uiState.update(
        [](auto& ui) { ui.status_message = std::string(core::loc("Sudoku", "Undone to last valid state")); });
}

bool GameViewModel::canUndo() const {
    return move_history_index_ >= 0;
}

bool GameViewModel::canRedo() const {
    return move_history_index_ < static_cast<int>(move_history_.size()) - 1;
}

void GameViewModel::checkSolution() {
    // Get current board state
    auto board = gameState.get().extractNumbers();

    if (validator_->isComplete(board)) {
        // Store current difficulty and time before ending session
        const auto& state = gameState.get();
        auto completed_difficulty = state.getDifficulty();
        auto completion_time = state.getElapsedTime();

        // Mark game as complete
        gameState.update([](model::GameState& state) { state.setComplete(true); });

        // End game session in statistics
        endGameSession(true);

        // Log completion
        spdlog::info("Puzzle completed! Time: {}ms. Auto-starting new game at same difficulty...",
                     completion_time.count());

        // Auto-start new game at same difficulty
        startNewGame(completed_difficulty);

        // Update UI with completion message
        uiState.update([&completion_time](auto& ui) {
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(completion_time);
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(completion_time - minutes);
            ui.status_message =
                core::locFormat(core::loc("Sudoku", "Puzzle completed in {0}:{1}! New game started."),
                                fmt::format("{:02d}", minutes.count()), fmt::format("{:02d}", seconds.count()));
        });

        spdlog::info("New game started automatically at same difficulty");
    } else {
        // Find and mark conflicts
        auto conflicts = validator_->findConflicts(board);

        gameState.update([&conflicts](model::GameState& state) {
            // Clear previous conflicts
            state.clearConflicts();

            // Mark new conflicts
            for (const auto& pos : conflicts) {
                if (!state.isGiven(pos)) {
                    state.setConflict(pos, true);
                }
            }
        });

        uiState.update([](auto& ui) {
            ui.status_message = std::string(core::loc("Sudoku", "Solution has errors. Keep trying!"));
        });
    }
}

void GameViewModel::recordMove(const core::Move& move, bool is_mistake) {
    // Add to history (truncate any redo moves)
    if (move_history_index_ + 1 < static_cast<int>(move_history_.size())) {
        move_history_.erase(move_history_.begin() + (move_history_index_ + 1), move_history_.end());
        // Truncation may have dropped the move that last_valid_state_index_ points at, leaving
        // the index dangling past the new array end. Clamp so undoToLastValid never references
        // a deleted move.
        last_valid_state_index_ = std::min(last_valid_state_index_, move_history_index_);
    }
    move_history_.push_back(move);
    move_history_index_ = static_cast<int>(move_history_.size()) - 1;

    // Track last valid state (no conflicts AND no wrong values vs solution)
    if (!hasBoardErrors()) {
        last_valid_state_index_ = move_history_index_;
    }

    // Update conflict highlighting and mistake count
    auto board = gameState.get().extractNumbers();
    auto conflicts = validator_->findConflicts(board);
    gameState.update([&conflicts, is_mistake](model::GameState& state) {
        state.updateConflicts(conflicts);
        if (is_mistake) {
            state.incrementMistakes();
        }
    });

    // Record in statistics
    if (current_game_session_ > 0) {
        auto record_result = stats_manager_->recordMove(current_game_session_, is_mistake);
        if (!record_result) {
            spdlog::warn("Failed to record move: {}", statisticsErrorToString(record_result.error()));
        }
    }
}

void GameViewModel::applyMove(const core::Move& move) {
    gameState.update([&move](model::GameState& state) {
        switch (move.move_type) {
            case core::MoveType::PlaceNumber:
                state.setValue(move.position, move.value);
                state.clearNotes(move.position);
                cleanupConflictingNotes(state, move.position, move.value);
                break;
            case core::MoveType::RemoveNumber:
                state.setValue(move.position, 0);
                break;
            case core::MoveType::AddNote:
                state.addNote(move.position, move.value);
                break;
            case core::MoveType::RemoveNote:
                state.removeNote(move.position, move.value);
                break;
            case core::MoveType::PlaceHint:
                state.setValue(move.position, move.value);
                state.setHintRevealed(move.position, true);
                state.clearNotes(move.position);
                cleanupConflictingNotes(state, move.position, move.value);
                state.incrementMoves();  // Count hint as a move
                break;
            default:
                break;
        }
    });
}

void GameViewModel::revertMove(const core::Move& move) {
    gameState.update([&move](model::GameState& state) {
        switch (move.move_type) {
            case core::MoveType::PlaceNumber:
                state.setValue(move.position, move.previous_value);
                state.setHintRevealed(move.position, move.previous_hint_revealed);
                state.setNotes(move.position, move.previous_notes);
                if (move.value != 0) {
                    restoreConflictingNotes(state, move.position, move.value);
                }
                break;
            case core::MoveType::RemoveNumber:
                state.setValue(move.position, move.previous_value);
                state.setHintRevealed(move.position, move.previous_hint_revealed);
                state.setNotes(move.position, move.previous_notes);
                if (move.previous_value != 0) {
                    cleanupConflictingNotes(state, move.position, move.previous_value);
                }
                break;
            case core::MoveType::AddNote:
            case core::MoveType::RemoveNote:
                state.setNotes(move.position, move.previous_notes);
                break;
            case core::MoveType::PlaceHint:
                state.setValue(move.position, move.previous_value);
                state.setHintRevealed(move.position, move.previous_hint_revealed);
                state.setNotes(move.position, move.previous_notes);
                if (move.value != 0) {
                    restoreConflictingNotes(state, move.position, move.value);
                }
                spdlog::warn("Attempted to revert hint move - hints should not be undoable!");
                break;
            default:
                break;
        }
    });
}

}  // namespace sudoku::viewmodel
