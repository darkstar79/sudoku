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

/// Strategy for finding Pointing Pairs/Triples
/// When a candidate value within a box is confined to a single row or column,
/// that value can be eliminated from other cells in that row/column outside the box.
class PointingPairStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Check each box
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            auto empty_cells = getEmptyCellsInBox(board, box);
            if (empty_cells.empty()) {
                continue;
            }

            // For each candidate value, check if it's confined to one row or column
            for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
                auto cells = getCellsWithCandidate(candidates, empty_cells, value);
                if (cells.size() < 2) {
                    continue;  // Need at least 2 cells for a pointing pair
                }

                auto result = checkPointingInRow(candidates, board, cells, value, box);
                if (result.has_value()) {
                    return result;
                }

                result = checkPointingInCol(candidates, board, cells, value, box);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::PointingPair;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Pointing Pair";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::PointingPair);
    }

private:
    /// Check if all cells with this value in the box lie in a single row
    [[nodiscard]] static std::optional<SolveStep> checkPointingInRow(const CandidateGrid& candidates,
                                                                     const BoardData& board,
                                                                     const std::vector<Position>& cells_in_box,
                                                                     int value, size_t box) {
        // Check if all cells are in the same row
        size_t row = cells_in_box[0].row;
        for (size_t idx = 1; idx < cells_in_box.size(); ++idx) {
            if (cells_in_box[idx].row != row) {
                return std::nullopt;  // Cells span multiple rows
            }
        }

        // All cells in box with this value are in one row
        // Eliminate value from other cells in this row outside the box
        auto row_cells = getEmptyCellsInRow(board, row);
        std::vector<Elimination> eliminations;

        for (const auto& pos : row_cells) {
            if (getBoxIndex(pos.row, pos.col) != box && candidates.isAllowed(pos.row, pos.col, value)) {
                eliminations.push_back(Elimination{.position = pos, .value = value});
            }
        }

        if (!eliminations.empty()) {
            auto explanation =
                fmt::format("Pointing Pair: {} in Box {} confined to Row {} eliminates {} from other cells in Row {}",
                            value, box + 1, row + 1, value, row + 1);

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::PointingPair,
                             .position = cells_in_box[0],
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .points = getTechniquePoints(SolvingTechnique::PointingPair),
                             .explanation_data = {.positions = {cells_in_box.begin(), cells_in_box.end()},
                                                  .values = {value},
                                                  .region_type = RegionType::Box,
                                                  .region_index = box,
                                                  .secondary_region_type = RegionType::Row,
                                                  .secondary_region_index = row}};
        }

        return std::nullopt;
    }

    /// Check if all cells with this value in the box lie in a single column
    [[nodiscard]] static std::optional<SolveStep> checkPointingInCol(const CandidateGrid& candidates,
                                                                     const BoardData& board,
                                                                     const std::vector<Position>& cells_in_box,
                                                                     int value, size_t box) {
        size_t col = cells_in_box[0].col;
        for (size_t idx = 1; idx < cells_in_box.size(); ++idx) {
            if (cells_in_box[idx].col != col) {
                return std::nullopt;
            }
        }

        auto col_cells = getEmptyCellsInCol(board, col);
        std::vector<Elimination> eliminations;

        for (const auto& pos : col_cells) {
            if (getBoxIndex(pos.row, pos.col) != box && candidates.isAllowed(pos.row, pos.col, value)) {
                eliminations.push_back(Elimination{.position = pos, .value = value});
            }
        }

        if (!eliminations.empty()) {
            auto explanation = fmt::format(
                "Pointing Pair: {} in Box {} confined to Column {} eliminates {} from other cells in Column {}", value,
                box + 1, col + 1, value, col + 1);

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::PointingPair,
                             .position = cells_in_box[0],
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .points = getTechniquePoints(SolvingTechnique::PointingPair),
                             .explanation_data = {.positions = {cells_in_box.begin(), cells_in_box.end()},
                                                  .values = {value},
                                                  .region_type = RegionType::Box,
                                                  .region_index = box,
                                                  .secondary_region_type = RegionType::Col,
                                                  .secondary_region_index = col}};
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
