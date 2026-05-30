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

#include "../../src/core/candidate_grid.h"

#include <algorithm>
#include <initializer_list>
#include <utility>
#include <vector>

namespace sudoku::testing {

using core::BOARD_SIZE;
using core::BOX_SIZE;
using core::EMPTY_CELL;
using core::MAX_VALUE;
using core::MIN_VALUE;

/// Reduce a cell's candidates to only the specified values.
/// Used by strategy tests to set up specific candidate patterns.
inline void keepOnly(core::CandidateGrid& grid, size_t row, size_t col, const std::vector<int>& keep) {
    for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
        if (std::ranges::find(keep, v) == keep.end() && grid.isAllowed(row, col, v)) {
            grid.eliminateCandidate(row, col, v);
        }
    }
}

/// Eliminate every digit except `digit` from all empty cells.
/// Shrinks a strategy search from 9 digits to 1 (used by the fish-strategy tests).
inline void keepOnlyDigit(core::CandidateGrid& state, const core::BoardData& board, int digit) {
    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            if (board[r][c] == EMPTY_CELL) {
                keepOnly(state, r, c, {digit});
            }
        }
    }
}

/// Eliminate `digit` from every empty cell EXCEPT the listed keep-cells.
/// Combined with keepOnlyDigit this carves an exact single-digit pattern.
inline void keepDigitOnlyAtCells(core::CandidateGrid& state, const core::BoardData& board, int digit,
                                 std::initializer_list<std::pair<size_t, size_t>> keep_cells) {
    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            if (board[r][c] != EMPTY_CELL) {
                continue;
            }
            bool is_keep = std::ranges::any_of(
                keep_cells, [r, c](const auto& cell) { return cell.first == r && cell.second == c; });
            if (!is_keep) {
                state.eliminateCandidate(r, c, digit);
            }
        }
    }
}

/// True if two cells share a row, a column, or a box (i.e. they "see" each other).
[[nodiscard]] inline bool seesEachOther(size_t r1, size_t c1, size_t r2, size_t c2) {
    if (r1 == r2 && c1 == c2) {
        return false;
    }
    bool same_box = (r1 / BOX_SIZE == r2 / BOX_SIZE) && (c1 / BOX_SIZE == c2 / BOX_SIZE);
    return r1 == r2 || c1 == c2 || same_box;
}

/// Independent (strategy-agnostic) soundness check for a 2-base-house fish elimination.
/// Brute-forces every conflict-free assignment of the digit to one cell in each base house and
/// confirms the eliminated cell `(elim_row, elim_col)` sees a placed digit in EVERY such case —
/// i.e. the eliminated cell can never itself hold the digit. Returns false if any assignment
/// leaves the elimination unrefuted, or if no conflict-free assignment exists (vacuous check).
[[nodiscard]] inline bool fishEliminationIsSound(const std::vector<std::pair<size_t, size_t>>& house_a,
                                                 const std::vector<std::pair<size_t, size_t>>& house_b, size_t elim_row,
                                                 size_t elim_col) {
    int valid_assignments = 0;
    for (const auto& [r_a, c_a] : house_a) {
        for (const auto& [r_b, c_b] : house_b) {
            if (seesEachOther(r_a, c_a, r_b, c_b)) {
                continue;  // two trues of the same digit cannot share a house
            }
            ++valid_assignments;
            bool refuted = seesEachOther(elim_row, elim_col, r_a, c_a) || seesEachOther(elim_row, elim_col, r_b, c_b);
            if (!refuted) {
                return false;
            }
        }
    }
    return valid_assignments > 0;
}

/// Create an empty 9x9 board (all zeros)
[[nodiscard]] inline core::BoardData createEmptyBoard() {
    return core::BoardData{};
}

/// A valid complete Sudoku solution for use in tests.
/// Commonly used in "returns nullopt for complete board" negative tests.
// clang-format off
inline const core::BoardData kSolvedBoard = {
    {5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
    {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
    {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
// clang-format on

}  // namespace sudoku::testing
