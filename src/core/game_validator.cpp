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

#include "game_validator.h"

#include "core/board_utils.h"
#include "core/constants.h"
#include "core/i_game_validator.h"

#include <expected>
#include <optional>
#include <utility>

namespace sudoku::core {

namespace {
/// Gets the top-left corner of the 3x3 box containing the position
std::pair<size_t, size_t> getBoxOrigin(size_t row, size_t col) {
    return {(row / BOX_SIZE) * BOX_SIZE, (col / BOX_SIZE) * BOX_SIZE};
}
}  // namespace

std::expected<bool, ValidationError> GameValidator::validateMove(const BoardData& board, const Move& move) const {
    // Validate position
    if (!isValidPosition(move.position)) {
        return std::unexpected(ValidationError::InvalidPosition);
    }

    // For clearing a cell (value = 0), always allow
    if (move.value == 0) {
        return true;
    }

    // Validate value range
    if (!isValidValue(move.value)) {
        return std::unexpected(ValidationError::InvalidValue);
    }

    // Check if game is already complete
    if (isComplete(board)) {
        return std::unexpected(ValidationError::GameComplete);
    }

    // For note operations, validation is different (always allow)
    if (move.is_note || move.move_type == MoveType::AddNote || move.move_type == MoveType::RemoveNote) {
        return true;
    }

    // Check for conflicts when placing a number
    if (move.move_type == MoveType::PlaceNumber) {
        const auto& pos = move.position;

        if (hasRowConflict(board, pos.row, pos.col, move.value)) {
            return std::unexpected(ValidationError::ConflictInRow);
        }

        if (hasColumnConflict(board, pos.row, pos.col, move.value)) {
            return std::unexpected(ValidationError::ConflictInColumn);
        }

        if (hasBoxConflict(board, pos.row, pos.col, move.value)) {
            return std::unexpected(ValidationError::ConflictInBox);
        }
    }

    return true;
}

bool GameValidator::isComplete(const BoardData& board) const {
    // Check if all cells are filled using board utilities
    if (anyCell([&](size_t row, size_t col) { return board[row][col] == 0; })) {
        return false;
    }

    // Check if board is valid
    return validateBoard(board);
}

std::vector<Position> GameValidator::findConflicts(const BoardData& board) const {
    std::vector<Position> conflicts;

    forEachCell([&](size_t row, size_t col) {
        int value = board[row][col];
        if (value != 0 && hasConflict(board, {.row = row, .col = col}, value)) {
            conflicts.push_back({.row = row, .col = col});
        }
    });

    return conflicts;
}

bool GameValidator::validateBoard(const BoardData& board) const {
    return findConflicts(board).empty();
}

std::vector<int> GameValidator::getPossibleValues(const BoardData& board, const Position& position) const {
    std::vector<int> possible_values;

    if (!isValidPosition(position)) {
        return possible_values;
    }

    // If cell is already filled, no values are possible
    if (board[position.row][position.col] != 0) {
        return possible_values;
    }

    for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
        if (!hasConflict(board, position, value)) {
            possible_values.push_back(value);
        }
    }

    return possible_values;
}

std::optional<int> GameValidator::findNakedSingle(const BoardData& board, const Position& position) const {
    // Reuse existing getPossibleValues method
    auto possible = getPossibleValues(board, position);

    if (possible.size() == 1) {
        return possible[0];  // Naked single found
    }

    return std::nullopt;  // Not a naked single (0, 2+ values possible)
}

bool GameValidator::hasConflict(const BoardData& board, const Position& pos, int value) {
    return hasRowConflict(board, pos.row, pos.col, value) || hasColumnConflict(board, pos.row, pos.col, value) ||
           hasBoxConflict(board, pos.row, pos.col, value);
}

bool GameValidator::hasRowConflict(const BoardData& board, size_t row, size_t col, int value) {
    for (size_t check_col = 0; check_col < BOARD_SIZE; ++check_col) {
        if (check_col != col && board[row][check_col] == value) {
            return true;
        }
    }
    return false;
}

bool GameValidator::hasColumnConflict(const BoardData& board, size_t row, size_t col, int value) {
    for (size_t check_row = 0; check_row < BOARD_SIZE; ++check_row) {
        if (check_row != row && board[check_row][col] == value) {
            return true;
        }
    }
    return false;
}

bool GameValidator::hasBoxConflict(const BoardData& board, size_t row, size_t col, int value) {
    auto [box_row, box_col] = getBoxOrigin(row, col);

    for (size_t check_row = box_row; check_row < box_row + BOX_SIZE; ++check_row) {
        for (size_t check_col = box_col; check_col < box_col + BOX_SIZE; ++check_col) {
            if ((check_row != row || check_col != col) && board[check_row][check_col] == value) {
                return true;
            }
        }
    }
    return false;
}

bool GameValidator::isValidPosition(const Position& pos) {
    // size_t is always >= 0, so only check upper bounds
    return pos.row < BOARD_SIZE && pos.col < BOARD_SIZE;
}

bool GameValidator::isValidValue(int value) {
    return value >= MIN_VALUE && value <= MAX_VALUE;
}

}  // namespace sudoku::core