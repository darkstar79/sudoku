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

#include "core/board_utils.h"
#include "core/candidate_grid.h"
#include "core/constants.h"
#include "core/i_game_validator.h"
#include "core/observable.h"
#include "game_view_model.h"
#include "model/game_state.h"

#include <algorithm>
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

    resetCoachingIfNotTryIt();

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
    applyMoveCapture(move);
    recordMove(move, is_mistake);

    spdlog::debug("Placed {} at ({}, {}){}", number, pos.row, pos.col, is_mistake ? " [MISTAKE]" : "");

    // Notes are now stale — reset toggle so next press re-fills
    uiState.update([](UIState& state) { state.notes_filled = false; });

    // Check for completion
    checkGameCompletion();

    updateUIState();
    autoSaveIfNeeded();
}

void GameViewModel::enterNote(const core::Position& pos, int number) {
    if (number < 1 || number > 9 || !isGameActive()) {
        return;
    }

    resetCoachingIfNotTryIt();

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

    // Capture previous state for undo
    move.previous_value = current_state.getValue(pos);
    move.previous_notes = current_state.getNotes(pos);
    move.previous_hint_revealed = current_state.isCellHintRevealed(pos);

    // Apply move
    applyMoveCapture(move);
    recordMove(move, false);  // Notes are never mistakes

    updateUIState();
    spdlog::debug("Note {} {} at ({}, {})", number, note_exists ? "removed" : "added", pos.row, pos.col);
}

void GameViewModel::clearCell(const core::Position& pos) {
    if (!isGameActive()) {
        return;
    }

    resetCoachingIfNotTryIt();

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

    // Capture previous state for undo
    move.previous_value = current_state.getValue(pos);
    move.previous_notes = current_state.getNotes(pos);
    move.previous_hint_revealed = current_state.isCellHintRevealed(pos);

    // Apply move
    applyMoveCapture(move);
    recordMove(move, false);  // Clearing is not a mistake

    // Notes are now stale — reset toggle so next press re-fills
    uiState.update([](UIState& state) { state.notes_filled = false; });

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
    // Filling/clearing all notes rewrites pencil marks outside the move model, so any pending redo
    // would replay a stored delta against a now-stale note state — drop the redo tail first.
    invalidateRedoTail();
    if (uiState.get().notes_filled) {
        clearAllNotes();
        uiState.update([](UIState& state) { state.notes_filled = false; });
        spdlog::info("Cleared all pencil marks");
    } else {
        recomputeAutoNotes();
        uiState.update([](UIState& state) { state.notes_filled = true; });
        spdlog::info("Filled pencil marks for all empty cells");
    }
}

void GameViewModel::clearAllNotes() {
    gameState.update([](model::GameState& state) {
        core::forEachCell([&](size_t row, size_t col) {
            core::Position pos{.row = row, .col = col};
            state.clearNotes(pos);
        });
    });
}

void GameViewModel::colorCell(const core::Position& pos, uint8_t color_index) {
    gameState.update(
        [&pos, color_index](model::GameState& state) { state.setCellColor(pos.row, pos.col, color_index); });
}

void GameViewModel::clearCellNotes(const core::Position& pos) {
    if (!isGameActive()) {
        return;
    }
    const auto& state = gameState.get();
    // Pencil marks only ever exist on empty, non-given cells.
    if (state.isGiven(pos) || state.getValue(pos) != core::EMPTY_CELL) {
        return;
    }
    // Toggle each present mark off through the existing undoable note path — no parallel
    // note-clearing logic (AC-13). Snapshot first: enterNote mutates the live state.
    const auto notes = state.getNotes(pos);
    for (int value = core::MIN_VALUE; value <= core::MAX_VALUE; ++value) {
        if (notes.contains(value)) {
            enterNote(pos, value);
        }
    }
}

void GameViewModel::handleNumberInput(const core::Position& pos, int number, std::optional<InputMode> override_layer) {
    if (!isGameActive() || number < core::MIN_VALUE || number > core::MAX_VALUE) {
        return;
    }
    const auto& state = gameState.get();
    if (!state.isTimerRunning()) {
        return;
    }
    const auto& cell = state.getCell(pos);

    // EditGivens is the one mode where "given" cells are still editable — the user is
    // building the puzzle, not solving it. Handle it before the is_given guard.
    if (getInputMode() == InputMode::EditGivens) {
        // While laying down givens the *override* is meaningless, not the keystroke: a held
        // modifier is stripped and the key acts as its plain self (Ctrl+8/Alt+8 → set given 8),
        // matching the already-plain Ctrl+Delete given-clear. Only final givens exist here, so
        // there is no value/pencil/color layer to force (AC-9; UX ruling 2026-06-04).
        // Tapping a digit on a cell already showing that digit clears it (toggle behaviour
        // parallel to Normal mode's cell.value == number → clear).
        const int new_value = (cell.value == number) ? 0 : number;
        setEditModeGiven(pos, new_value);
        return;
    }

    if (cell.is_given) {
        return;
    }

    // No override → act on the active mode (regression-safe, AC-1). An override forces its
    // layer (Ctrl→Normal/value, Shift→Notes/pencil, Alt→Color) in any mode.
    const InputMode layer = override_layer.value_or(getInputMode());

    switch (layer) {
        case InputMode::Normal:
            // Value layer: place into an empty cell, or clear when re-pressing the same value.
            // Reused verbatim by the Ctrl override (AC-2, AC-13).
            if (cell.value == 0) {
                enterNumber(pos, number);
            } else if (cell.value == number) {
                clearCell(pos);
            }
            break;
        case InputMode::Notes:
            // enterNote already toggles and honors the empty-cell guard (AC-3, AC-13).
            if (cell.value == 0) {
                enterNote(pos, number);
            }
            break;
        case InputMode::Color:
            if (number <= kColorPaletteSize) {
                if (override_layer.has_value()) {
                    // Alt override toggles off when re-applying the same color, and replaces
                    // otherwise (AC-4). Plain Color mode keeps its set-only behavior (AC-1).
                    const auto current = state.getCellColor(pos.row, pos.col);
                    const auto target =
                        (current == static_cast<uint8_t>(number)) ? uint8_t{0} : static_cast<uint8_t>(number);
                    colorCell(pos, target);
                } else {
                    colorCell(pos, static_cast<uint8_t>(number));
                }
            }
            break;
        case InputMode::EditGivens:
            // Unreachable: active EditGivens is handled above, and an override is never EditGivens.
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

namespace {

/// Invoke `fn(row, col)` for every empty peer of `pos` in its row, column and 3x3 box (the cell
/// itself excluded). A cell shared by two units is visited more than once; callers must be
/// idempotent (the note helpers below dedupe via a contains()/already-present check).
template <typename Fn>
void forEachEmptyPeer(const model::GameState& state, const core::Position& pos, const Fn& fn) {
    for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
        if (col != pos.col && state.getValue(pos.row, col) == 0) {
            fn(pos.row, col);
        }
    }
    for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
        if (row != pos.row && state.getValue(row, pos.col) == 0) {
            fn(row, pos.col);
        }
    }
    const size_t box_start_row = (pos.row / core::BOX_SIZE) * core::BOX_SIZE;
    const size_t box_start_col = (pos.col / core::BOX_SIZE) * core::BOX_SIZE;
    for (size_t row = box_start_row; row < box_start_row + core::BOX_SIZE; ++row) {
        for (size_t col = box_start_col; col < box_start_col + core::BOX_SIZE; ++col) {
            if ((row != pos.row || col != pos.col) && state.getValue(row, col) == 0) {
                fn(row, col);
            }
        }
    }
}

}  // namespace

std::vector<core::Position> GameViewModel::cleanupConflictingNotes(model::GameState& state, const core::Position& pos,
                                                                   int number) {
    // Strip the placed number from every empty peer that ACTUALLY holds it, recording exactly
    // those peers so revert can re-add the same set — capture/replay, not re-derivation (#76).
    std::vector<core::Position> removed;
    forEachEmptyPeer(state, pos, [&](size_t row, size_t col) {
        const core::Position peer{.row = row, .col = col};
        if (state.getNotes(peer).contains(number)) {
            state.removeNote(peer, number);
            removed.push_back(peer);
        }
    });
    spdlog::debug("Cleaned up {} conflicting notes for number {} at ({}, {})", removed.size(), number, pos.row,
                  pos.col);
    return removed;
}

std::vector<core::Position> GameViewModel::restorePeerNotesForClearedValue(model::GameState& state,
                                                                           const core::Position& pos, int value) {
    // Clearing a placed value re-opens it as a candidate. Re-add it to every empty peer where it
    // is now legal (not already placed in that peer's row/col/box) and absent, returning those
    // peers so undoing the clear re-strips exactly them.
    const auto board = state.extractNumbers();
    auto legalAt = [&](const core::Position& peer) -> bool {
        for (size_t col = 0; col < core::BOARD_SIZE; ++col) {
            if (board[peer.row][col] == value) {
                return false;
            }
        }
        for (size_t row = 0; row < core::BOARD_SIZE; ++row) {
            if (board[row][peer.col] == value) {
                return false;
            }
        }
        const size_t box_start_row = (peer.row / core::BOX_SIZE) * core::BOX_SIZE;
        const size_t box_start_col = (peer.col / core::BOX_SIZE) * core::BOX_SIZE;
        for (size_t row = box_start_row; row < box_start_row + core::BOX_SIZE; ++row) {
            for (size_t col = box_start_col; col < box_start_col + core::BOX_SIZE; ++col) {
                if (board[row][col] == value) {
                    return false;
                }
            }
        }
        return true;
    };

    std::vector<core::Position> added;
    forEachEmptyPeer(state, pos, [&](size_t row, size_t col) {
        const core::Position peer{.row = row, .col = col};
        if (!state.getNotes(peer).contains(value) && legalAt(peer)) {
            state.addNote(peer, value);
            added.push_back(peer);
        }
    });
    spdlog::debug("Restored {} peer notes for cleared value {} at ({}, {})", added.size(), value, pos.row, pos.col);
    return added;
}

}  // namespace sudoku::viewmodel
