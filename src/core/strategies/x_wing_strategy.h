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
#include "fish_helpers.h"

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding X-Wing patterns
/// An X-Wing occurs when a candidate value appears in exactly 2 cells in each of
/// two different rows, and those cells are in the same two columns. The candidate
/// can then be eliminated from all other cells in those two columns.
/// (Also checks column-based X-Wings: 2 columns with same 2 rows.)
class XWingStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Check row-based X-Wings
        auto row_result = findRowBasedXWing(board, candidates);
        if (row_result.has_value()) {
            return row_result;
        }
        // Check column-based X-Wings
        return findColBasedXWing(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::XWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "X-Wing";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::XWing);
    }

private:
    // CPD-OFF — row/col symmetry
    /// Row-based X-Wing: candidate in exactly 2 cols in each of 2 rows (same cols)
    /// → eliminate from those cols in all other rows
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×row1×row2 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findRowBasedXWing(const BoardData& board,
                                                                    const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            // For each row, find columns where this value is a candidate
            auto rows_cols = FishHelpers::collectCandidatePositions(board, candidates, value, true);

            // Find pairs of rows where value appears in exactly 2 positions in same columns
            for (size_t row1 = 0; row1 < BOARD_SIZE; ++row1) {
                if (rows_cols[row1].size() != 2) {
                    continue;
                }
                for (size_t row2 = row1 + 1; row2 < BOARD_SIZE; ++row2) {
                    if (rows_cols[row2].size() != 2) {
                        continue;
                    }
                    if (rows_cols[row1][0] == rows_cols[row2][0] && rows_cols[row1][1] == rows_cols[row2][1]) {
                        size_t col1 = rows_cols[row1][0];
                        size_t col2 = rows_cols[row1][1];

                        // Eliminate value from these columns in all other rows
                        std::vector<Elimination> eliminations;
                        for (size_t row = 0; row < BOARD_SIZE; ++row) {
                            if (row == row1 || row == row2) {
                                continue;
                            }
                            if (board[row][col1] == EMPTY_CELL && candidates.isAllowed(row, col1, value)) {
                                eliminations.push_back(
                                    Elimination{.position = Position{.row = row, .col = col1}, .value = value});
                            }
                            if (board[row][col2] == EMPTY_CELL && candidates.isAllowed(row, col2, value)) {
                                eliminations.push_back(
                                    Elimination{.position = Position{.row = row, .col = col2}, .value = value});
                            }
                        }

                        if (!eliminations.empty()) {
                            auto explanation = fmt::format("X-Wing on value {} in Rows {} and {}, Columns {} and {} "
                                                           "eliminates {} from other cells in those columns",
                                                           value, row1 + 1, row2 + 1, col1 + 1, col2 + 1, value);

                            return SolveStep{.type = SolveStepType::Elimination,
                                             .technique = SolvingTechnique::XWing,
                                             .position = Position{.row = row1, .col = col1},
                                             .value = 0,
                                             .eliminations = eliminations,
                                             .explanation = explanation,
                                             .rating = getTechniqueRating(SolvingTechnique::XWing),
                                             .explanation_data = {.positions = {Position{.row = row1, .col = col1},
                                                                                Position{.row = row1, .col = col2},
                                                                                Position{.row = row2, .col = col1},
                                                                                Position{.row = row2, .col = col2}},
                                                                  .values = {value},
                                                                  .region_type = RegionType::Row,
                                                                  .region_index = row1,
                                                                  .secondary_region_type = RegionType::Row,
                                                                  .secondary_region_index = row2}};
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    /// Column-based X-Wing: candidate in exactly 2 rows in each of 2 cols (same rows)
    /// → eliminate from those rows in all other cols
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×col1×col2 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findColBasedXWing(const BoardData& board,
                                                                    const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            // For each column, find rows where this value is a candidate
            auto cols_rows = FishHelpers::collectCandidatePositions(board, candidates, value, false);

            // Find pairs of columns where value appears in exactly 2 positions in same rows
            for (size_t col1 = 0; col1 < BOARD_SIZE; ++col1) {
                if (cols_rows[col1].size() != 2) {
                    continue;
                }
                for (size_t col2 = col1 + 1; col2 < BOARD_SIZE; ++col2) {
                    if (cols_rows[col2].size() != 2) {
                        continue;
                    }
                    if (cols_rows[col1][0] == cols_rows[col2][0] && cols_rows[col1][1] == cols_rows[col2][1]) {
                        size_t row1 = cols_rows[col1][0];
                        size_t row2 = cols_rows[col1][1];

                        // Eliminate value from these rows in all other columns
                        std::vector<Elimination> eliminations;
                        for (size_t col = 0; col < BOARD_SIZE; ++col) {
                            if (col == col1 || col == col2) {
                                continue;
                            }
                            if (board[row1][col] == EMPTY_CELL && candidates.isAllowed(row1, col, value)) {
                                eliminations.push_back(
                                    Elimination{.position = Position{.row = row1, .col = col}, .value = value});
                            }
                            if (board[row2][col] == EMPTY_CELL && candidates.isAllowed(row2, col, value)) {
                                eliminations.push_back(
                                    Elimination{.position = Position{.row = row2, .col = col}, .value = value});
                            }
                        }

                        if (!eliminations.empty()) {
                            auto explanation = fmt::format("X-Wing on value {} in Columns {} and {}, Rows {} and {} "
                                                           "eliminates {} from other cells in those rows",
                                                           value, col1 + 1, col2 + 1, row1 + 1, row2 + 1, value);

                            return SolveStep{.type = SolveStepType::Elimination,
                                             .technique = SolvingTechnique::XWing,
                                             .position = Position{.row = row1, .col = col1},
                                             .value = 0,
                                             .eliminations = eliminations,
                                             .explanation = explanation,
                                             .rating = getTechniqueRating(SolvingTechnique::XWing),
                                             .explanation_data = {.positions = {Position{.row = row1, .col = col1},
                                                                                Position{.row = row1, .col = col2},
                                                                                Position{.row = row2, .col = col1},
                                                                                Position{.row = row2, .col = col2}},
                                                                  .values = {value},
                                                                  .region_type = RegionType::Col,
                                                                  .region_index = col1,
                                                                  .secondary_region_type = RegionType::Col,
                                                                  .secondary_region_index = col2}};
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }
    // CPD-ON
};

}  // namespace sudoku::core
