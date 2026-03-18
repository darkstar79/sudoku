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

#include "../i_solving_strategy.h"
#include "../solve_step.h"
#include "../solving_technique.h"
#include "../strategy_base.h"

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for detecting BUG (Bivalue Universal Grave) + 1.
/// If all empty cells have exactly 2 candidates EXCEPT exactly one cell with 3 candidates,
/// the puzzle is in a BUG+1 state. The trivalue cell's "odd" candidate (the one that
/// appears an odd number of times in its row, column, or box) must be placed.
/// This produces a Placement, not an Elimination.
class BUGStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        Position trivalue_cell{};
        int trivalue_count = 0;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                int count = candidates.countPossibleValues(row, col);
                if (count == 2) {
                    continue;  // expected
                }
                if (count == 3 && trivalue_count == 0) {
                    trivalue_cell = Position{.row = row, .col = col};
                    ++trivalue_count;
                } else {
                    // Not a BUG+1 state: either count != 2 and != 3, or multiple trivalue cells
                    return std::nullopt;
                }
            }
        }

        if (trivalue_count != 1) {
            return std::nullopt;
        }

        // Find the odd candidate: appears an odd number of times in its row, col, or box
        auto cands = getCandidates(candidates, trivalue_cell.row, trivalue_cell.col);
        int odd_value = findOddCandidate(board, candidates, trivalue_cell, cands);
        if (odd_value == 0) {
            return std::nullopt;
        }

        auto explanation =
            fmt::format("BUG: all cells bivalue except {} — value {} must be placed to avoid deadly pattern",
                        formatPosition(trivalue_cell), odd_value);

        return SolveStep{.type = SolveStepType::Placement,
                         .technique = SolvingTechnique::BUG,
                         .position = trivalue_cell,
                         .value = odd_value,
                         .eliminations = {},
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::BUG),
                         .explanation_data = {.positions = {trivalue_cell}, .values = {odd_value}}};
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::BUG;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "BUG";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::BUG);
    }

private:
    /// Find the candidate that appears an odd number of times in the cell's row, col, or box.
    [[nodiscard]] static int findOddCandidate(const BoardData& board, const CandidateGrid& candidates,
                                              const Position& cell, const std::vector<int>& cands) {
        for (int val : cands) {
            // Count occurrences of val in the row
            int row_count = 0;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[cell.row][col] == EMPTY_CELL && candidates.isAllowed(cell.row, col, val)) {
                    ++row_count;
                }
            }
            if (row_count % 2 == 1) {
                return val;
            }

            // Count in column
            int col_count = 0;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (board[row][cell.col] == EMPTY_CELL && candidates.isAllowed(row, cell.col, val)) {
                    ++col_count;
                }
            }
            if (col_count % 2 == 1) {
                return val;
            }

            // Count in box
            size_t box_idx = getBoxIndex(cell.row, cell.col);
            auto box_cells = getEmptyCellsInBox(board, box_idx);
            int box_count = 0;
            for (const auto& pos : box_cells) {
                if (candidates.isAllowed(pos.row, pos.col, val)) {
                    ++box_count;
                }
            }
            if (box_count % 2 == 1) {
                return val;
            }
        }
        return 0;  // Should not happen in a valid BUG+1
    }
};

}  // namespace sudoku::core
