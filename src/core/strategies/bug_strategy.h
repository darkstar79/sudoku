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

/// Strategy for detecting BUG (Bivalue Universal Grave) + N (N = 1, 2, or 3).
/// If all empty cells have exactly 2 candidates EXCEPT N cells with 3 candidates,
/// the puzzle is in a BUG+N state. Each trivalue cell's "odd" candidate (the one that
/// appears an odd number of times in its row, column, or box) must be placed.
/// This produces a Placement, not an Elimination.
class BUGStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    static constexpr int MAX_TRIVALUE_CELLS = 3;

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — BUG+N scans all cells, finds odd candidates per trivalue cell, and checks consistency; nesting is inherent
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        std::vector<Position> trivalue_cells;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                int count = candidates.countPossibleValues(row, col);
                if (count == 2) {
                    continue;  // expected bivalue
                }
                if (count == 3) {
                    trivalue_cells.push_back(Position{.row = row, .col = col});
                    if (static_cast<int>(trivalue_cells.size()) > MAX_TRIVALUE_CELLS) {
                        return std::nullopt;  // too many trivalue cells
                    }
                } else {
                    // Cell with count != 2 and != 3 — not a BUG state
                    return std::nullopt;
                }
            }
        }

        if (trivalue_cells.empty() || static_cast<int>(trivalue_cells.size()) > MAX_TRIVALUE_CELLS) {
            return std::nullopt;
        }

        // For each trivalue cell, find its odd candidate
        struct OddPlacement {
            Position cell;
            int value;
        };
        std::vector<OddPlacement> placements;

        for (const auto& cell : trivalue_cells) {
            auto cands = getCandidates(candidates, cell.row, cell.col);
            int odd_value = findOddCandidate(board, candidates, cell, cands);
            if (odd_value == 0) {
                return std::nullopt;  // can't determine odd candidate
            }
            placements.push_back({cell, odd_value});
        }

        // Verify consistency: no two odd placements conflict
        // (same value in same row, column, or box)
        for (size_t i = 0; i < placements.size(); ++i) {
            for (size_t j = i + 1; j < placements.size(); ++j) {
                if (placements[i].value == placements[j].value) {
                    if (placements[i].cell.row == placements[j].cell.row ||
                        placements[i].cell.col == placements[j].cell.col ||
                        sameBox(placements[i].cell, placements[j].cell)) {
                        return std::nullopt;  // conflicting placements
                    }
                }
            }
        }

        // Return the first placement (solver will re-evaluate after each)
        const auto& first = placements[0];
        int n = static_cast<int>(trivalue_cells.size());

        std::string explanation;
        if (n == 1) {
            explanation =
                fmt::format("BUG: all cells bivalue except {} — value {} must be placed to avoid deadly pattern",
                            formatPosition(first.cell), first.value);
        } else {
            std::string cells_str;
            for (size_t i = 0; i < trivalue_cells.size(); ++i) {
                if (i > 0) {
                    cells_str += ", ";
                }
                cells_str += formatPosition(trivalue_cells[i]);
            }
            explanation = fmt::format(
                "BUG+{}: all cells bivalue except {} — value {} must be placed at {} to avoid deadly pattern", n,
                cells_str, first.value, formatPosition(first.cell));
        }

        return SolveStep{.type = SolveStepType::Placement,
                         .technique = SolvingTechnique::BUG,
                         .position = first.cell,
                         .value = first.value,
                         .eliminations = {},
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::BUG),
                         .explanation_data = {.positions = {first.cell}, .values = {first.value}}};
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
