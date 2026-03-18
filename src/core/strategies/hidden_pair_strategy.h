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

#include <algorithm>
#include <optional>
#include <set>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Hidden Pairs
/// A Hidden Pair is two values that can only appear in the same two cells within
/// a region, even though those cells may have other candidates. All other candidates
/// can be eliminated from those two cells.
class HiddenPairStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board, const CandidateGrid& state) const override {
        // Check all regions (rows, columns, boxes)
        for (auto region_type : {RegionType::Row, RegionType::Col, RegionType::Box}) {
            for (size_t region_idx = 0; region_idx < BOARD_SIZE; ++region_idx) {
                auto result = findHiddenPairInRegion(board, state, region_type, region_idx);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::HiddenPair;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Hidden Pair";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::HiddenPair);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×cell combination search; nesting is inherent to hidden-subset algorithms
    [[nodiscard]] static std::optional<SolveStep> findHiddenPairInRegion(const BoardData& board,
                                                                         const CandidateGrid& state,
                                                                         RegionType region_type, size_t region_idx) {
        // Get empty cells in region
        auto empty_cells = getEmptyCellsInUnit(board, region_type, region_idx);

        if (empty_cells.size() < 2) {
            return std::nullopt;
        }

        // For each pair of values (1-9), check if they appear in exactly 2 cells
        for (int val1 = MIN_VALUE; val1 <= MAX_VALUE; ++val1) {
            for (int val2 = val1 + 1; val2 <= MAX_VALUE; ++val2) {
                // Find cells that can contain val1
                std::vector<Position> cells_with_val1;
                for (const auto& pos : empty_cells) {
                    auto candidates = getCandidates(state, pos.row, pos.col);
                    if (std::ranges::contains(candidates, val1)) {
                        cells_with_val1.push_back(pos);
                    }
                }

                // Find cells that can contain val2
                std::vector<Position> cells_with_val2;
                for (const auto& pos : empty_cells) {
                    auto candidates = getCandidates(state, pos.row, pos.col);
                    if (std::ranges::contains(candidates, val2)) {
                        cells_with_val2.push_back(pos);
                    }
                }

                // Check if both values appear in exactly the same 2 cells
                if (cells_with_val1.size() == 2 && cells_with_val2.size() == 2 &&
                    cells_with_val1[0] == cells_with_val2[0] && cells_with_val1[1] == cells_with_val2[1]) {
                    const auto& pos1 = cells_with_val1[0];
                    const auto& pos2 = cells_with_val1[1];

                    // Check if these cells have OTHER candidates (otherwise it's a naked pair)
                    auto cand1 = getCandidates(state, pos1.row, pos1.col);
                    auto cand2 = getCandidates(state, pos2.row, pos2.col);

                    // Create eliminations: remove all candidates except val1 and val2
                    std::vector<Elimination> eliminations;
                    for (int value : cand1) {
                        if (value != val1 && value != val2) {
                            eliminations.push_back(Elimination{.position = pos1, .value = value});
                        }
                    }
                    for (int value : cand2) {
                        if (value != val1 && value != val2) {
                            eliminations.push_back(Elimination{.position = pos2, .value = value});
                        }
                    }

                    if (!eliminations.empty()) {
                        auto explanation = fmt::format(
                            "Hidden Pair [{}, {}] at {}, {} in {} eliminates other candidates from these cells", val1,
                            val2, formatPosition(pos1), formatPosition(pos2), formatRegion(region_type, region_idx));

                        return SolveStep{.type = SolveStepType::Elimination,
                                         .technique = SolvingTechnique::HiddenPair,
                                         .position = pos1,
                                         .value = 0,
                                         .eliminations = eliminations,
                                         .explanation = explanation,
                                         .points = getTechniquePoints(SolvingTechnique::HiddenPair),
                                         .explanation_data = {.positions = {pos1, pos2},
                                                              .values = {val1, val2},
                                                              .region_type = region_type,
                                                              .region_index = region_idx}};
                    }
                }
            }
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
