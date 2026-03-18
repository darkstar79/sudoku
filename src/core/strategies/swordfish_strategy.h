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

/// Strategy for finding Swordfish patterns
/// A Swordfish occurs when a candidate value appears in 2-3 cells in each of
/// three different rows, and the union of those columns is exactly 3 columns.
/// The candidate can then be eliminated from all other cells in those three columns.
/// (Also checks column-based Swordfish: 3 columns with same 3 rows.)
class SwordfishStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        auto row_result = findRowBasedSwordfish(board, candidates);
        if (row_result.has_value()) {
            return row_result;
        }
        return findColBasedSwordfish(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::Swordfish;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Swordfish";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::Swordfish);
    }

private:
    /// Row-based Swordfish: candidate in 2-3 cols in each of 3 rows, union of cols = 3
    /// → eliminate from those cols in all other rows
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×row1×row2×row3 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findRowBasedSwordfish(const BoardData& board,
                                                                        const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            // For each row, find columns where this value is a candidate
            auto rows_cols = FishHelpers::collectCandidatePositions(board, candidates, value, true);

            // Find triples of rows where value appears in 2-3 columns
            // and the union of all columns is exactly 3
            for (size_t r1 = 0; r1 < BOARD_SIZE; ++r1) {
                if (rows_cols[r1].size() < 2 || rows_cols[r1].size() > 3) {
                    continue;
                }
                for (size_t r2 = r1 + 1; r2 < BOARD_SIZE; ++r2) {
                    if (rows_cols[r2].size() < 2 || rows_cols[r2].size() > 3) {
                        continue;
                    }
                    for (size_t r3 = r2 + 1; r3 < BOARD_SIZE; ++r3) {
                        if (rows_cols[r3].size() < 2 || rows_cols[r3].size() > 3) {
                            continue;
                        }

                        auto union_cols = FishHelpers::indexUnion(rows_cols[r1], rows_cols[r2], rows_cols[r3]);
                        if (union_cols.size() != 3) {
                            continue;
                        }

                        // Eliminate value from these 3 columns in all other rows
                        std::vector<Elimination> eliminations;
                        for (size_t row = 0; row < BOARD_SIZE; ++row) {
                            if (row == r1 || row == r2 || row == r3) {
                                continue;
                            }
                            for (size_t col : union_cols) {
                                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                                    eliminations.push_back(
                                        Elimination{.position = Position{.row = row, .col = col}, .value = value});
                                }
                            }
                        }

                        if (!eliminations.empty()) {
                            // Collect pattern positions (intersections of the 3 rows and 3 cols)
                            std::vector<Position> positions;
                            for (size_t row : {r1, r2, r3}) {
                                for (size_t col : union_cols) {
                                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                                        positions.push_back(Position{.row = row, .col = col});
                                    }
                                }
                            }

                            auto explanation = fmt::format("Swordfish on value {} in Rows {}, {}, {} and Columns {}, "
                                                           "{}, {} eliminates {} from other cells in those columns",
                                                           value, r1 + 1, r2 + 1, r3 + 1, union_cols[0] + 1,
                                                           union_cols[1] + 1, union_cols[2] + 1, value);

                            return SolveStep{
                                .type = SolveStepType::Elimination,
                                .technique = SolvingTechnique::Swordfish,
                                .position = positions[0],
                                .value = 0,
                                .eliminations = eliminations,
                                .explanation = explanation,
                                .rating = getTechniqueRating(SolvingTechnique::Swordfish),
                                .explanation_data = {.positions = positions,
                                                     .values = {value, static_cast<int>(r1 + 1),
                                                                static_cast<int>(r2 + 1), static_cast<int>(r3 + 1),
                                                                static_cast<int>(union_cols[0] + 1),
                                                                static_cast<int>(union_cols[1] + 1),
                                                                static_cast<int>(union_cols[2] + 1)},
                                                     .region_type = RegionType::Row,
                                                     .region_index = r1}};
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    /// Column-based Swordfish: candidate in 2-3 rows in each of 3 cols, union of rows = 3
    /// → eliminate from those rows in all other cols
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×col1×col2×col3 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findColBasedSwordfish(const BoardData& board,
                                                                        const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            // For each column, find rows where this value is a candidate
            auto cols_rows = FishHelpers::collectCandidatePositions(board, candidates, value, false);

            // Find triples of columns where value appears in 2-3 rows
            // and the union of all rows is exactly 3
            for (size_t c1 = 0; c1 < BOARD_SIZE; ++c1) {
                if (cols_rows[c1].size() < 2 || cols_rows[c1].size() > 3) {
                    continue;
                }
                for (size_t c2 = c1 + 1; c2 < BOARD_SIZE; ++c2) {
                    if (cols_rows[c2].size() < 2 || cols_rows[c2].size() > 3) {
                        continue;
                    }
                    for (size_t c3 = c2 + 1; c3 < BOARD_SIZE; ++c3) {
                        if (cols_rows[c3].size() < 2 || cols_rows[c3].size() > 3) {
                            continue;
                        }

                        auto union_rows = FishHelpers::indexUnion(cols_rows[c1], cols_rows[c2], cols_rows[c3]);
                        if (union_rows.size() != 3) {
                            continue;
                        }

                        // Eliminate value from these 3 rows in all other columns
                        std::vector<Elimination> eliminations;
                        for (size_t col = 0; col < BOARD_SIZE; ++col) {
                            if (col == c1 || col == c2 || col == c3) {
                                continue;
                            }
                            for (size_t row : union_rows) {
                                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                                    eliminations.push_back(
                                        Elimination{.position = Position{.row = row, .col = col}, .value = value});
                                }
                            }
                        }

                        if (!eliminations.empty()) {
                            // Collect pattern positions
                            std::vector<Position> positions;
                            for (size_t col : {c1, c2, c3}) {
                                for (size_t row : union_rows) {
                                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                                        positions.push_back(Position{.row = row, .col = col});
                                    }
                                }
                            }

                            auto explanation = fmt::format("Swordfish on value {} in Columns {}, {}, {} and Rows {}, "
                                                           "{}, {} eliminates {} from other cells in those rows",
                                                           value, c1 + 1, c2 + 1, c3 + 1, union_rows[0] + 1,
                                                           union_rows[1] + 1, union_rows[2] + 1, value);

                            return SolveStep{
                                .type = SolveStepType::Elimination,
                                .technique = SolvingTechnique::Swordfish,
                                .position = positions[0],
                                .value = 0,
                                .eliminations = eliminations,
                                .explanation = explanation,
                                .rating = getTechniqueRating(SolvingTechnique::Swordfish),
                                .explanation_data = {.positions = positions,
                                                     .values = {value, static_cast<int>(c1 + 1),
                                                                static_cast<int>(c2 + 1), static_cast<int>(c3 + 1),
                                                                static_cast<int>(union_rows[0] + 1),
                                                                static_cast<int>(union_rows[1] + 1),
                                                                static_cast<int>(union_rows[2] + 1)},
                                                     .region_type = RegionType::Col,
                                                     .region_index = c1}};
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }
};

}  // namespace sudoku::core
