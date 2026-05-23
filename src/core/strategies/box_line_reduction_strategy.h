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

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Box/Line Reductions
/// When a candidate value within a row or column is confined to a single box,
/// that value can be eliminated from other cells in that box outside the row/column.
class BoxLineReductionStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Check rows
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            auto empty_cells = getEmptyCellsInRow(board, row);
            for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
                auto result = checkRowConfinedToBox(candidates, board, empty_cells, row, value);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        // Check columns
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            auto empty_cells = getEmptyCellsInCol(board, col);
            for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
                auto result = checkColConfinedToBox(candidates, board, empty_cells, col, value);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::BoxLineReduction;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Box/Line Reduction";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::BoxLineReduction);
    }

private:
    // CPD-OFF — row/col symmetry
    /// Check if a value in a row is confined to a single box
    [[nodiscard]] static std::optional<SolveStep> checkRowConfinedToBox(const CandidateGrid& candidates,
                                                                        const BoardData& board,
                                                                        const std::vector<Position>& row_cells,
                                                                        size_t row, int value) {
        auto cells_with_value = getCellsWithCandidate(candidates, row_cells, value);
        if (cells_with_value.size() < 2) {
            return std::nullopt;
        }

        // Check if all cells are in the same box
        size_t target_box = getBoxIndex(cells_with_value[0].row, cells_with_value[0].col);
        for (size_t idx = 1; idx < cells_with_value.size(); ++idx) {
            if (getBoxIndex(cells_with_value[idx].row, cells_with_value[idx].col) != target_box) {
                return std::nullopt;  // Cells span multiple boxes
            }
        }

        // All cells with this value in the row are in one box
        // Eliminate value from other cells in the box outside this row
        auto box_cells = getEmptyCellsInBox(board, target_box);
        std::vector<Elimination> eliminations;

        for (const auto& pos : box_cells) {
            if (pos.row != row && candidates.isAllowed(pos.row, pos.col, value)) {
                eliminations.push_back(Elimination{.position = pos, .value = value});
            }
        }

        if (!eliminations.empty()) {
            auto explanation = fmt::format(
                "Box/Line Reduction: {} in Row {} confined to Box {} eliminates {} from other cells in Box {}", value,
                row + 1, target_box + 1, value, target_box + 1);

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::BoxLineReduction,
                             .position = cells_with_value[0],
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::BoxLineReduction),
                             .explanation_data = {.positions = {cells_with_value.begin(), cells_with_value.end()},
                                                  .values = {value},
                                                  .region_type = RegionType::Row,
                                                  .region_index = row,
                                                  .secondary_region_type = RegionType::Box,
                                                  .secondary_region_index = target_box}};
        }

        return std::nullopt;
    }

    /// Check if a value in a column is confined to a single box
    [[nodiscard]] static std::optional<SolveStep> checkColConfinedToBox(const CandidateGrid& candidates,
                                                                        const BoardData& board,
                                                                        const std::vector<Position>& col_cells,
                                                                        size_t col, int value) {
        auto cells_with_value = getCellsWithCandidate(candidates, col_cells, value);
        if (cells_with_value.size() < 2) {
            return std::nullopt;
        }

        size_t target_box = getBoxIndex(cells_with_value[0].row, cells_with_value[0].col);
        for (size_t idx = 1; idx < cells_with_value.size(); ++idx) {
            if (getBoxIndex(cells_with_value[idx].row, cells_with_value[idx].col) != target_box) {
                return std::nullopt;
            }
        }

        auto box_cells = getEmptyCellsInBox(board, target_box);
        std::vector<Elimination> eliminations;

        for (const auto& pos : box_cells) {
            if (pos.col != col && candidates.isAllowed(pos.row, pos.col, value)) {
                eliminations.push_back(Elimination{.position = pos, .value = value});
            }
        }

        if (!eliminations.empty()) {
            auto explanation = fmt::format(
                "Box/Line Reduction: {} in Column {} confined to Box {} eliminates {} from other cells in Box {}",
                value, col + 1, target_box + 1, value, target_box + 1);

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::BoxLineReduction,
                             .position = cells_with_value[0],
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::BoxLineReduction),
                             .explanation_data = {.positions = {cells_with_value.begin(), cells_with_value.end()},
                                                  .values = {value},
                                                  .region_type = RegionType::Col,
                                                  .region_index = col,
                                                  .secondary_region_type = RegionType::Box,
                                                  .secondary_region_index = target_box}};
        }

        return std::nullopt;
    }
    // CPD-ON
};

}  // namespace sudoku::core
