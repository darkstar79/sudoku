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

/// Strategy for finding Hidden Triples
/// A Hidden Triple is three values that can only appear in the same three cells within
/// a region, even though those cells may have other candidates. All other candidates
/// can be eliminated from those three cells.
class HiddenTripleStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board, const CandidateGrid& state) const override {
        // Check all regions (rows, columns, boxes)
        for (auto region_type : {RegionType::Row, RegionType::Col, RegionType::Box}) {
            for (size_t region_idx = 0; region_idx < BOARD_SIZE; ++region_idx) {
                auto result = findHiddenTripleInRegion(board, state, region_type, region_idx);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::HiddenTriple;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Hidden Triple";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::HiddenTriple);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×cell combination search; nesting is inherent to hidden-subset algorithms
    [[nodiscard]] static std::optional<SolveStep> findHiddenTripleInRegion(const BoardData& board,
                                                                           const CandidateGrid& state,
                                                                           RegionType region_type, size_t region_idx) {
        // Get empty cells in region
        auto empty_cells = getEmptyCellsInUnit(board, region_type, region_idx);

        if (empty_cells.size() < 3) {
            return std::nullopt;
        }

        // For each combination of 3 values (1-9), check if they appear in exactly 3 cells
        for (int val1 = MIN_VALUE; val1 <= MAX_VALUE; ++val1) {
            for (int val2 = val1 + 1; val2 <= MAX_VALUE; ++val2) {
                for (int val3 = val2 + 1; val3 <= MAX_VALUE; ++val3) {
                    // Verify each value appears as a candidate in at least one empty cell
                    // (skip values that are already placed in this region)
                    bool val1_exists = false;
                    bool val2_exists = false;
                    bool val3_exists = false;
                    for (const auto& pos : empty_cells) {
                        if (state.isAllowed(pos.row, pos.col, val1)) {
                            val1_exists = true;
                        }
                        if (state.isAllowed(pos.row, pos.col, val2)) {
                            val2_exists = true;
                        }
                        if (state.isAllowed(pos.row, pos.col, val3)) {
                            val3_exists = true;
                        }
                    }
                    if (!val1_exists || !val2_exists || !val3_exists) {
                        continue;
                    }

                    // Find cells that can contain any of these three values
                    std::vector<Position> cells_with_values;

                    for (const auto& pos : empty_cells) {
                        bool has_any = state.isAllowed(pos.row, pos.col, val1) ||
                                       state.isAllowed(pos.row, pos.col, val2) ||
                                       state.isAllowed(pos.row, pos.col, val3);
                        if (has_any) {
                            cells_with_values.push_back(pos);
                        }
                    }

                    // Check if these three values appear in exactly 3 cells
                    if (cells_with_values.size() == 3) {
                        const auto& triple_cells = cells_with_values;

                        // Create eliminations: remove all candidates except val1, val2, val3
                        std::vector<Elimination> eliminations;
                        for (const auto& pos : triple_cells) {
                            auto candidates = getCandidates(state, pos.row, pos.col);
                            for (int value : candidates) {
                                if (value != val1 && value != val2 && value != val3) {
                                    eliminations.push_back(Elimination{.position = pos, .value = value});
                                }
                            }
                        }

                        if (!eliminations.empty()) {
                            auto explanation = fmt::format(
                                "Hidden Triple [{}, {}, {}] at {}, {}, {} in {}"
                                " eliminates other candidates from these cells",
                                val1, val2, val3, formatPosition(triple_cells[0]), formatPosition(triple_cells[1]),
                                formatPosition(triple_cells[2]), formatRegion(region_type, region_idx));

                            return SolveStep{
                                .type = SolveStepType::Elimination,
                                .technique = SolvingTechnique::HiddenTriple,
                                .position = triple_cells[0],
                                .value = 0,
                                .eliminations = eliminations,
                                .explanation = explanation,
                                .rating = getTechniqueRating(SolvingTechnique::HiddenTriple),
                                .explanation_data = {.positions = {triple_cells[0], triple_cells[1], triple_cells[2]},
                                                     .values = {val1, val2, val3},
                                                     .region_type = region_type,
                                                     .region_index = region_idx}};
                        }
                    }
                }
            }
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
