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
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Naked Pairs
/// A Naked Pair is two cells in the same region (row/col/box) that both have
/// exactly the same two candidates. These two values can be eliminated from
/// all other cells in that region.
class NakedPairStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board, const CandidateGrid& state) const override {
        // Check all regions (rows, columns, boxes)
        for (auto region_type : {RegionType::Row, RegionType::Col, RegionType::Box}) {
            for (size_t region_idx = 0; region_idx < BOARD_SIZE; ++region_idx) {
                auto result = findNakedPairInRegion(board, state, region_type, region_idx);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::NakedPair;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Naked Pair";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::NakedPair);
    }

private:
    [[nodiscard]] static std::optional<SolveStep> findNakedPairInRegion(const BoardData& board,
                                                                        const CandidateGrid& state,
                                                                        RegionType region_type, size_t region_idx) {
        // Get empty cells in region
        auto empty_cells = getEmptyCellsInUnit(board, region_type, region_idx);

        // Find cells with exactly 2 candidates
        std::vector<Position> pair_candidates;
        for (const auto& pos : empty_cells) {
            if (hasNCandidates(state, pos.row, pos.col, 2)) {
                pair_candidates.push_back(pos);
            }
        }

        // Need at least 2 cells to form a pair
        if (pair_candidates.size() < 2) {
            return std::nullopt;
        }

        // Check all pairs of cells
        for (size_t i = 0; i < pair_candidates.size(); ++i) {
            for (size_t j = i + 1; j < pair_candidates.size(); ++j) {
                const auto& pos1 = pair_candidates[i];
                const auto& pos2 = pair_candidates[j];

                auto cand1 = getCandidates(state, pos1.row, pos1.col);
                auto cand2 = getCandidates(state, pos2.row, pos2.col);

                // Check if candidates match (both have same two values)
                if (cand1 == cand2 && cand1.size() == 2) {
                    // Found naked pair! Create eliminations
                    auto eliminations = createEliminationsForPair(board, state, empty_cells, pos1, pos2, cand1);

                    if (!eliminations.empty()) {
                        auto explanation =
                            fmt::format("Naked Pair [{}, {}] at {}, {} in {} eliminates candidates from other cells",
                                        cand1[0], cand1[1], formatPosition(pos1), formatPosition(pos2),
                                        formatRegion(region_type, region_idx));

                        return SolveStep{.type = SolveStepType::Elimination,
                                         .technique = SolvingTechnique::NakedPair,
                                         .position = pos1,  // Primary position (not used for eliminations)
                                         .value = 0,        // Not used for eliminations
                                         .eliminations = eliminations,
                                         .explanation = explanation,
                                         .rating = getTechniqueRating(SolvingTechnique::NakedPair),
                                         .explanation_data = {.positions = {pos1, pos2},
                                                              .values = {cand1[0], cand1[1]},
                                                              .region_type = region_type,
                                                              .region_index = region_idx}};
                    }
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::vector<Elimination>
    createEliminationsForPair(const BoardData& /* board */, const CandidateGrid& state,
                              const std::vector<Position>& region_cells, const Position& pair_pos1,
                              const Position& pair_pos2, const std::vector<int>& pair_values) {
        std::vector<Elimination> eliminations;

        // Check each cell in the region
        for (const auto& pos : region_cells) {
            // Skip the pair cells themselves
            if (pos == pair_pos1 || pos == pair_pos2) {
                continue;
            }

            // Check if this cell has any of the pair values as candidates
            auto candidates = getCandidates(state, pos.row, pos.col);
            for (int value : pair_values) {
                if (std::ranges::contains(candidates, value)) {
                    eliminations.push_back(Elimination{.position = pos, .value = value});
                }
            }
        }

        return eliminations;
    }
};

}  // namespace sudoku::core
