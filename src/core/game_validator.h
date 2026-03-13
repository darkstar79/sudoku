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

#include "i_game_validator.h"

#include <cstddef>
#include <vector>

namespace sudoku::core {

/// Concrete implementation of Sudoku game validation
class GameValidator : public IGameValidator {
public:
    GameValidator() = default;
    ~GameValidator() override = default;

    // Rule-of-5: Explicitly defaulted copy and move operations
    GameValidator(const GameValidator&) = default;
    GameValidator& operator=(const GameValidator&) = default;
    GameValidator(GameValidator&&) = default;
    GameValidator& operator=(GameValidator&&) = default;

    /// Validates if a move is legal according to Sudoku rules
    [[nodiscard]] std::expected<bool, ValidationError> validateMove(const std::vector<std::vector<int>>& board,
                                                                    const Move& move) const override;

    /// Checks if the current board state is complete and valid
    [[nodiscard]] bool isComplete(const std::vector<std::vector<int>>& board) const override;

    /// Finds all conflicts on the current board
    [[nodiscard]] std::vector<Position> findConflicts(const std::vector<std::vector<int>>& board) const override;

    /// Validates that the entire board state is consistent
    [[nodiscard]] bool validateBoard(const std::vector<std::vector<int>>& board) const override;

    /// Gets all possible valid values for a given position
    [[nodiscard]] std::vector<int> getPossibleValues(const std::vector<std::vector<int>>& board,
                                                     const Position& position) const override;

    /// Finds a naked single (cell with only one legal value)
    [[nodiscard]] std::optional<int> findNakedSingle(const std::vector<std::vector<int>>& board,
                                                     const Position& position) const override;

    /// Checks if placing a value at position would create a conflict
    [[nodiscard]] static bool hasConflict(const std::vector<std::vector<int>>& board, const Position& pos, int value);

    /// Checks for conflicts in the same row
    [[nodiscard]] static bool hasRowConflict(const std::vector<std::vector<int>>& board, size_t row, size_t col,
                                             int value);

    /// Checks for conflicts in the same column
    [[nodiscard]] static bool hasColumnConflict(const std::vector<std::vector<int>>& board, size_t row, size_t col,
                                                int value);

    /// Checks for conflicts in the same 3x3 box
    [[nodiscard]] static bool hasBoxConflict(const std::vector<std::vector<int>>& board, size_t row, size_t col,
                                             int value);

    /// Validates position is within board bounds
    [[nodiscard]] static bool isValidPosition(const Position& pos);

    /// Validates value is in range 1-9
    [[nodiscard]] static bool isValidValue(int value);
};

}  // namespace sudoku::core