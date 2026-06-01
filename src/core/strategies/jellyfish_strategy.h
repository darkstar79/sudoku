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

#include "../i_solving_strategy.h"
#include "../solve_step.h"
#include "../solving_technique.h"
#include "../strategy_base.h"
#include "fish_helpers.h"

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Jellyfish patterns (4x4 fish generalization of Swordfish).
/// A Jellyfish occurs when a candidate value appears in 2-4 cells in each of
/// four different rows, and the union of those columns is exactly 4 columns.
class JellyfishStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        auto result = findRowBased(board, candidates);
        if (result.has_value()) {
            return result;
        }
        return findColBased(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::Jellyfish;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Jellyfish";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::Jellyfish);
    }

private:
    // CPD-OFF — fish enumeration pattern shared with finned strategies
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×row1×row2×row3×row4 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findRowBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto rows_cols = FishHelpers::collectCandidatePositions(board, candidates, value, true);

            // Find 4 rows with 2-4 candidate positions each, union of cols == 4
            for (size_t r1 = 0; r1 < BOARD_SIZE; ++r1) {
                if (rows_cols[r1].size() < 2 || rows_cols[r1].size() > 4) {
                    continue;
                }
                for (size_t r2 = r1 + 1; r2 < BOARD_SIZE; ++r2) {
                    if (rows_cols[r2].size() < 2 || rows_cols[r2].size() > 4) {
                        continue;
                    }
                    for (size_t r3 = r2 + 1; r3 < BOARD_SIZE; ++r3) {
                        if (rows_cols[r3].size() < 2 || rows_cols[r3].size() > 4) {
                            continue;
                        }
                        for (size_t r4 = r3 + 1; r4 < BOARD_SIZE; ++r4) {
                            if (rows_cols[r4].size() < 2 || rows_cols[r4].size() > 4) {
                                continue;
                            }

                            auto union_cols =
                                FishHelpers::indexUnion(rows_cols[r1], rows_cols[r2], rows_cols[r3], rows_cols[r4]);
                            if (union_cols.size() != 4) {
                                continue;
                            }

                            std::vector<Elimination> eliminations;
                            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                                if (row == r1 || row == r2 || row == r3 || row == r4) {
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
                                std::vector<Position> positions;
                                for (size_t row : {r1, r2, r3, r4}) {
                                    for (size_t col : union_cols) {
                                        if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                                            positions.push_back(Position{.row = row, .col = col});
                                        }
                                    }
                                }

                                auto explanation =
                                    fmt::format("Jellyfish on value {} in Rows {}, {}, {}, {} and Columns {}, {}, {}, "
                                                "{} eliminates {} from other cells in those columns",
                                                value, r1 + 1, r2 + 1, r3 + 1, r4 + 1, union_cols[0] + 1,
                                                union_cols[1] + 1, union_cols[2] + 1, union_cols[3] + 1, value);

                                return SolveStep{
                                    .type = SolveStepType::Elimination,
                                    .technique = SolvingTechnique::Jellyfish,
                                    .position = positions[0],
                                    .value = 0,
                                    .eliminations = eliminations,
                                    .explanation = explanation,
                                    .rating = getTechniqueRating(SolvingTechnique::Jellyfish),
                                    .explanation_data = {.positions = positions,
                                                         .values = {value, static_cast<int>(r1 + 1),
                                                                    static_cast<int>(r2 + 1), static_cast<int>(r3 + 1),
                                                                    static_cast<int>(r4 + 1),
                                                                    static_cast<int>(union_cols[0] + 1),
                                                                    static_cast<int>(union_cols[1] + 1),
                                                                    static_cast<int>(union_cols[2] + 1),
                                                                    static_cast<int>(union_cols[3] + 1)},
                                                         .pattern_axis = RegionType::Row,
                                                         .elimination_axis = RegionType::Col}};
                            }
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×col1×col2×col3×col4 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findColBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto cols_rows = FishHelpers::collectCandidatePositions(board, candidates, value, false);

            for (size_t c1 = 0; c1 < BOARD_SIZE; ++c1) {
                if (cols_rows[c1].size() < 2 || cols_rows[c1].size() > 4) {
                    continue;
                }
                for (size_t c2 = c1 + 1; c2 < BOARD_SIZE; ++c2) {
                    if (cols_rows[c2].size() < 2 || cols_rows[c2].size() > 4) {
                        continue;
                    }
                    for (size_t c3 = c2 + 1; c3 < BOARD_SIZE; ++c3) {
                        if (cols_rows[c3].size() < 2 || cols_rows[c3].size() > 4) {
                            continue;
                        }
                        for (size_t c4 = c3 + 1; c4 < BOARD_SIZE; ++c4) {
                            if (cols_rows[c4].size() < 2 || cols_rows[c4].size() > 4) {
                                continue;
                            }

                            auto union_rows =
                                FishHelpers::indexUnion(cols_rows[c1], cols_rows[c2], cols_rows[c3], cols_rows[c4]);
                            if (union_rows.size() != 4) {
                                continue;
                            }

                            std::vector<Elimination> eliminations;
                            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                                if (col == c1 || col == c2 || col == c3 || col == c4) {
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
                                std::vector<Position> positions;
                                for (size_t col : {c1, c2, c3, c4}) {
                                    for (size_t row : union_rows) {
                                        if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                                            positions.push_back(Position{.row = row, .col = col});
                                        }
                                    }
                                }

                                auto explanation =
                                    fmt::format("Jellyfish on value {} in Columns {}, {}, {}, {} and Rows {}, {}, {}, "
                                                "{} eliminates {} from other cells in those rows",
                                                value, c1 + 1, c2 + 1, c3 + 1, c4 + 1, union_rows[0] + 1,
                                                union_rows[1] + 1, union_rows[2] + 1, union_rows[3] + 1, value);

                                return SolveStep{
                                    .type = SolveStepType::Elimination,
                                    .technique = SolvingTechnique::Jellyfish,
                                    .position = positions[0],
                                    .value = 0,
                                    .eliminations = eliminations,
                                    .explanation = explanation,
                                    .rating = getTechniqueRating(SolvingTechnique::Jellyfish),
                                    .explanation_data = {.positions = positions,
                                                         .values = {value, static_cast<int>(c1 + 1),
                                                                    static_cast<int>(c2 + 1), static_cast<int>(c3 + 1),
                                                                    static_cast<int>(c4 + 1),
                                                                    static_cast<int>(union_rows[0] + 1),
                                                                    static_cast<int>(union_rows[1] + 1),
                                                                    static_cast<int>(union_rows[2] + 1),
                                                                    static_cast<int>(union_rows[3] + 1)},
                                                         .pattern_axis = RegionType::Col,
                                                         .elimination_axis = RegionType::Row}};
                            }
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
