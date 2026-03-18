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

#include <bit>
#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Empty Rectangle patterns.
/// For a given value and box, if the candidate cells form an L-shape (occupy ≥2 rows AND ≥2 cols),
/// a conjugate pair in an external row/column passing through the ER can eliminate the value
/// from a cell at the intersection of the ER's projection and the conjugate pair's endpoint.
class EmptyRectangleStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            for (size_t box = 0; box < BOARD_SIZE; ++box) {
                auto result = checkBox(board, candidates, value, box);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::EmptyRectangle;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Empty Rectangle";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::EmptyRectangle);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — box/conjugate pair enumeration; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> checkBox(const BoardData& board, const CandidateGrid& candidates,
                                                           int value, size_t box) {
        size_t box_start_row = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_start_col = (box % BOX_SIZE) * BOX_SIZE;

        // Find cells in this box with value as candidate
        std::vector<Position> box_cells;
        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t row = box_start_row + br;
                size_t col = box_start_col + bc;
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                    box_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }

        if (box_cells.size() < 2) {
            return std::nullopt;
        }

        // Check they form an L-shape: occupy ≥2 rows AND ≥2 cols
        size_t row_set = 0;
        size_t col_set = 0;
        for (const auto& pos : box_cells) {
            row_set |= (size_t{1} << (pos.row - box_start_row));
            col_set |= (size_t{1} << (pos.col - box_start_col));
        }

        int row_count = std::popcount(static_cast<unsigned>(row_set));
        int col_count = std::popcount(static_cast<unsigned>(col_set));
        if (row_count < 2 || col_count < 2) {
            return std::nullopt;  // All in one line — would be pointing pair, not ER
        }

        // For each (er_row, er_col) within the box where all candidates lie in er_row OR er_col:
        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t er_row = box_start_row + br;
                size_t er_col = box_start_col + bc;

                // Verify all box candidates lie in er_row or er_col
                bool all_in_cross = true;
                for (const auto& pos : box_cells) {
                    if (pos.row != er_row && pos.col != er_col) {
                        all_in_cross = false;
                        break;
                    }
                }
                if (!all_in_cross) {
                    continue;
                }

                // Try conjugate pair in a ROW outside this box, passing through er_col
                auto result = tryRowConjugate(board, candidates, value, box, er_row, er_col);
                if (result.has_value()) {
                    return result;
                }

                // Try conjugate pair in a COLUMN outside this box, passing through er_row
                result = tryColConjugate(board, candidates, value, box, er_row, er_col);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    // CPD-OFF — row/col symmetry
    /// Find conjugate pair in a row outside the ER box that passes through er_col.
    [[nodiscard]] static std::optional<SolveStep> tryRowConjugate(const BoardData& board,
                                                                  const CandidateGrid& candidates, int value,
                                                                  size_t box, size_t er_row, size_t er_col) {
        size_t box_start_row = (box / BOX_SIZE) * BOX_SIZE;

        for (size_t cp_row = 0; cp_row < BOARD_SIZE; ++cp_row) {
            // Row must be outside the ER box
            if (cp_row >= box_start_row && cp_row < box_start_row + BOX_SIZE) {
                continue;
            }

            // Find cells in this row with value as candidate
            std::vector<size_t> cols_with_value;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[cp_row][col] == EMPTY_CELL && candidates.isAllowed(cp_row, col, value)) {
                    cols_with_value.push_back(col);
                }
            }

            // Must be exactly 2 (conjugate pair)
            if (cols_with_value.size() != 2) {
                continue;
            }

            // One endpoint must be at er_col
            size_t target_col = 0;
            bool found = false;
            if (cols_with_value[0] == er_col) {
                target_col = cols_with_value[1];
                found = true;
            } else if (cols_with_value[1] == er_col) {
                target_col = cols_with_value[0];
                found = true;
            }
            if (!found) {
                continue;
            }

            // Target: eliminate value at (er_row, target_col)
            // Must not be in the ER box
            if (getBoxIndex(er_row, target_col) == box) {
                continue;
            }

            if (board[er_row][target_col] != EMPTY_CELL || !candidates.isAllowed(er_row, target_col, value)) {
                continue;
            }

            Position target{.row = er_row, .col = target_col};
            Position cp_endpoint{.row = cp_row, .col = er_col};

            auto explanation = fmt::format(
                "Empty Rectangle on value {}: ER in Box {} with conjugate pair in Row {} — eliminates {} from {}",
                value, box + 1, cp_row + 1, value, formatPosition(target));

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::EmptyRectangle,
                             .position = target,
                             .value = 0,
                             .eliminations = {Elimination{.position = target, .value = value}},
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::EmptyRectangle),
                             .explanation_data = {.positions = {cp_endpoint, target},
                                                  .values = {value, static_cast<int>(box + 1)},
                                                  .region_type = RegionType::Row,
                                                  .region_index = cp_row,
                                                  .position_roles = {CellRole::LinkEndpoint, CellRole::Pattern}}};
        }
        return std::nullopt;
    }

    /// Find conjugate pair in a column outside the ER box that passes through er_row.
    [[nodiscard]] static std::optional<SolveStep> tryColConjugate(const BoardData& board,
                                                                  const CandidateGrid& candidates, int value,
                                                                  size_t box, size_t er_row, size_t er_col) {
        size_t box_start_col = (box % BOX_SIZE) * BOX_SIZE;

        for (size_t cp_col = 0; cp_col < BOARD_SIZE; ++cp_col) {
            if (cp_col >= box_start_col && cp_col < box_start_col + BOX_SIZE) {
                continue;
            }

            std::vector<size_t> rows_with_value;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (board[row][cp_col] == EMPTY_CELL && candidates.isAllowed(row, cp_col, value)) {
                    rows_with_value.push_back(row);
                }
            }

            if (rows_with_value.size() != 2) {
                continue;
            }

            size_t target_row = 0;
            bool found = false;
            if (rows_with_value[0] == er_row) {
                target_row = rows_with_value[1];
                found = true;
            } else if (rows_with_value[1] == er_row) {
                target_row = rows_with_value[0];
                found = true;
            }
            if (!found) {
                continue;
            }

            if (getBoxIndex(target_row, er_col) == box) {
                continue;
            }

            if (board[target_row][er_col] != EMPTY_CELL || !candidates.isAllowed(target_row, er_col, value)) {
                continue;
            }

            Position target{.row = target_row, .col = er_col};
            Position cp_endpoint{.row = er_row, .col = cp_col};

            auto explanation = fmt::format(
                "Empty Rectangle on value {}: ER in Box {} with conjugate pair in Column {} — eliminates {} from {}",
                value, box + 1, cp_col + 1, value, formatPosition(target));

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::EmptyRectangle,
                             .position = target,
                             .value = 0,
                             .eliminations = {Elimination{.position = target, .value = value}},
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::EmptyRectangle),
                             .explanation_data = {.positions = {cp_endpoint, target},
                                                  .values = {value, static_cast<int>(box + 1)},
                                                  .region_type = RegionType::Col,
                                                  .region_index = cp_col,
                                                  .position_roles = {CellRole::LinkEndpoint, CellRole::Pattern}}};
        }
        return std::nullopt;
    }
    // CPD-ON
};

}  // namespace sudoku::core
