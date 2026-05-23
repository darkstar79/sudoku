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

/// Strategy for finding Finned X-Wing patterns.
/// Like an X-Wing, but one of the two rows (or columns) has an extra candidate cell (the "fin").
/// Eliminations are restricted to cells in the base columns (or rows) that also share the fin's box.
class FinnedXWingStrategy : public ISolvingStrategy, protected StrategyBase {
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
        return SolvingTechnique::FinnedXWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Finned X-Wing";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::FinnedXWing);
    }

private:
    /// Row-based Finned X-Wing:
    /// Row A has value in exactly 2 cols (base). Row B has value in exactly 3 cols,
    /// 2 of which match Row A's cols (the 3rd is the fin).
    /// Also checks reversed: Row A has 3 (finned), Row B has 2 (base).
    [[nodiscard]] static std::optional<SolveStep> findRowBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto rows_cols = FishHelpers::collectCandidatePositions(board, candidates, value, true);

            for (size_t row1 = 0; row1 < BOARD_SIZE; ++row1) {
                for (size_t row2 = row1 + 1; row2 < BOARD_SIZE; ++row2) {
                    // Try row1=base(2), row2=finned(3)
                    auto result = tryRowPair(board, candidates, value, rows_cols, row1, row2, /*base_row=*/row1,
                                             /*fin_row=*/row2);
                    if (result.has_value()) {
                        return result;
                    }
                    // Try row2=base(2), row1=finned(3)
                    result = tryRowPair(board, candidates, value, rows_cols, row1, row2, /*base_row=*/row2,
                                        /*fin_row=*/row1);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }
        return std::nullopt;
    }

    /// Try a specific row pair where base_row has 2 positions and fin_row has 3.
    [[nodiscard]] static std::optional<SolveStep> tryRowPair(const BoardData& board, const CandidateGrid& candidates,
                                                             int value,
                                                             const std::vector<std::vector<size_t>>& rows_cols,
                                                             size_t row1, size_t row2, size_t base_row,
                                                             size_t fin_row) {
        if (rows_cols[base_row].size() != 2 || rows_cols[fin_row].size() != 3) {
            return std::nullopt;
        }

        size_t base_col1 = rows_cols[base_row][0];
        size_t base_col2 = rows_cols[base_row][1];

        // Find which of fin_row's 3 cols are the 2 base cols and which is the fin
        size_t fin_col = 0;
        int base_match_count = 0;
        for (size_t col : rows_cols[fin_row]) {
            if (col == base_col1 || col == base_col2) {
                ++base_match_count;
            } else {
                fin_col = col;
            }
        }

        if (base_match_count != 2) {
            return std::nullopt;
        }

        Position fin_pos{.row = fin_row, .col = fin_col};
        size_t fin_box = getBoxIndex(fin_row, fin_col);

        // Eliminate value from cells in base_col1 and base_col2, not in row1/row2,
        // but only if they're in the fin's box
        std::vector<Elimination> eliminations;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            if (row == row1 || row == row2) {
                continue;
            }
            for (size_t col : {base_col1, base_col2}) {
                if (getBoxIndex(row, col) == fin_box && board[row][col] == EMPTY_CELL &&
                    candidates.isAllowed(row, col, value)) {
                    eliminations.push_back(Elimination{.position = Position{.row = row, .col = col}, .value = value});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation =
            fmt::format("Finned X-Wing on value {} in Rows {} and {}, Columns {} and {} with fin at {} — eliminates {} "
                        "from cells in fin's box",
                        value, row1 + 1, row2 + 1, base_col1 + 1, base_col2 + 1, formatPosition(fin_pos), value);

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::FinnedXWing,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::FinnedXWing),
            .explanation_data = {.positions = {Position{.row = row1, .col = base_col1},
                                               Position{.row = row1, .col = base_col2},
                                               Position{.row = row2, .col = base_col1},
                                               Position{.row = row2, .col = base_col2}, fin_pos},
                                 .values = {value, static_cast<int>(row1 + 1), static_cast<int>(row2 + 1),
                                            static_cast<int>(base_col1 + 1), static_cast<int>(base_col2 + 1)},
                                 .region_type = RegionType::Row,
                                 .position_roles = {CellRole::Pattern, CellRole::Pattern, CellRole::Pattern,
                                                    CellRole::Pattern, CellRole::Fin}}};
    }

    /// Column-based Finned X-Wing (mirror of row-based).
    [[nodiscard]] static std::optional<SolveStep> findColBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto cols_rows = FishHelpers::collectCandidatePositions(board, candidates, value, false);

            for (size_t col1 = 0; col1 < BOARD_SIZE; ++col1) {
                for (size_t col2 = col1 + 1; col2 < BOARD_SIZE; ++col2) {
                    auto result = tryColPair(board, candidates, value, cols_rows, col1, col2, col1, col2);
                    if (result.has_value()) {
                        return result;
                    }
                    result = tryColPair(board, candidates, value, cols_rows, col1, col2, col2, col1);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep> tryColPair(const BoardData& board, const CandidateGrid& candidates,
                                                             int value,
                                                             const std::vector<std::vector<size_t>>& cols_rows,
                                                             size_t col1, size_t col2, size_t base_col,
                                                             size_t fin_col) {
        if (cols_rows[base_col].size() != 2 || cols_rows[fin_col].size() != 3) {
            return std::nullopt;
        }

        size_t base_row1 = cols_rows[base_col][0];
        size_t base_row2 = cols_rows[base_col][1];

        size_t fin_row = 0;
        int base_match_count = 0;
        for (size_t row : cols_rows[fin_col]) {
            if (row == base_row1 || row == base_row2) {
                ++base_match_count;
            } else {
                fin_row = row;
            }
        }

        if (base_match_count != 2) {
            return std::nullopt;
        }

        Position fin_pos{.row = fin_row, .col = fin_col};
        size_t fin_box = getBoxIndex(fin_row, fin_col);

        std::vector<Elimination> eliminations;
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (col == col1 || col == col2) {
                continue;
            }
            for (size_t row : {base_row1, base_row2}) {
                if (getBoxIndex(row, col) == fin_box && board[row][col] == EMPTY_CELL &&
                    candidates.isAllowed(row, col, value)) {
                    eliminations.push_back(Elimination{.position = Position{.row = row, .col = col}, .value = value});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation =
            fmt::format("Finned X-Wing on value {} in Columns {} and {}, Rows {} and {} with fin at {} — eliminates {} "
                        "from cells in fin's box",
                        value, col1 + 1, col2 + 1, base_row1 + 1, base_row2 + 1, formatPosition(fin_pos), value);

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::FinnedXWing,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::FinnedXWing),
            .explanation_data = {.positions = {Position{.row = base_row1, .col = col1},
                                               Position{.row = base_row1, .col = col2},
                                               Position{.row = base_row2, .col = col1},
                                               Position{.row = base_row2, .col = col2}, fin_pos},
                                 .values = {value, static_cast<int>(col1 + 1), static_cast<int>(col2 + 1),
                                            static_cast<int>(base_row1 + 1), static_cast<int>(base_row2 + 1)},
                                 .region_type = RegionType::Col,
                                 .position_roles = {CellRole::Pattern, CellRole::Pattern, CellRole::Pattern,
                                                    CellRole::Pattern, CellRole::Fin}}};
    }
};

}  // namespace sudoku::core
