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

#include "../candidate_grid.h"
#include "../i_game_validator.h"
#include "../strategy_base.h"

#include <vector>

namespace sudoku::core {

/// Shared utility functions for wing-based strategies (XY, XYZ, WXYZ, VWXYZ).
/// Inherits StrategyBase for sees(), BOARD_SIZE, etc.
class WingHelpers : protected StrategyBase {
public:
    /// Check if all cells in a set are mutually visible (share row, col, or box pairwise).
    [[nodiscard]] static bool allMutuallyVisible(const std::vector<Position>& cells) {
        for (size_t ci = 0; ci < cells.size(); ++ci) {
            for (size_t cj = ci + 1; cj < cells.size(); ++cj) {
                if (!sees(cells[ci], cells[cj])) {
                    return false;
                }
            }
        }
        return true;
    }

    /// Find empty cells with candidate count in [min_count, max_count].
    [[nodiscard]] static std::vector<Position>
    findCellsByCandidateCount(const BoardData& board, const CandidateGrid& candidates, int min_count, int max_count) {
        std::vector<Position> result;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                int count = candidates.countPossibleValues(row, col);
                if (count >= min_count && count <= max_count) {
                    result.push_back(Position{.row = row, .col = col});
                }
            }
        }
        return result;
    }
};

}  // namespace sudoku::core
