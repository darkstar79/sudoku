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

#include <algorithm>
#include <optional>
#include <set>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Naked Triples
/// A Naked Triple is three cells in the same region (row/col/box) that collectively
/// contain only three candidate values. These three values can be eliminated from
/// all other cells in that region.
/// Note: Each cell can have 2 or 3 candidates, but the union must be exactly 3 values.
class NakedTripleStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board, const CandidateGrid& state) const override {
        // Check all regions (rows, columns, boxes)
        for (auto region_type : {RegionType::Row, RegionType::Col, RegionType::Box}) {
            for (size_t region_idx = 0; region_idx < BOARD_SIZE; ++region_idx) {
                auto result = findNakedTripleInRegion(board, state, region_type, region_idx);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::NakedTriple;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Naked Triple";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::NakedTriple);
    }

private:
    [[nodiscard]] static std::optional<SolveStep> findNakedTripleInRegion(const BoardData& board,
                                                                          const CandidateGrid& state,
                                                                          RegionType region_type, size_t region_idx) {
        // Get empty cells in region
        auto empty_cells = getEmptyCellsInUnit(board, region_type, region_idx);

        // Find cells with 2 or 3 candidates (potential triple members)
        std::vector<Position> triple_candidates;
        for (const auto& pos : empty_cells) {
            int candidate_count = state.countPossibleValues(pos.row, pos.col);
            if (candidate_count == 2 || candidate_count == 3) {
                triple_candidates.push_back(pos);
            }
        }

        // Need at least 3 cells to form a triple
        if (triple_candidates.size() < 3) {
            return std::nullopt;
        }

        // Check all combinations of 3 cells
        for (size_t i = 0; i < triple_candidates.size(); ++i) {
            for (size_t j = i + 1; j < triple_candidates.size(); ++j) {
                for (size_t k = j + 1; k < triple_candidates.size(); ++k) {
                    const auto& pos1 = triple_candidates[i];
                    const auto& pos2 = triple_candidates[j];
                    const auto& pos3 = triple_candidates[k];

                    auto cand1 = getCandidates(state, pos1.row, pos1.col);
                    auto cand2 = getCandidates(state, pos2.row, pos2.col);
                    auto cand3 = getCandidates(state, pos3.row, pos3.col);

                    // Compute union of all candidates
                    std::set<int> union_set;
                    union_set.insert(cand1.begin(), cand1.end());
                    union_set.insert(cand2.begin(), cand2.end());
                    union_set.insert(cand3.begin(), cand3.end());

                    // Check if union has exactly 3 values (naked triple condition)
                    if (union_set.size() == 3) {
                        // Convert set to vector for elimination logic
                        std::vector<int> triple_values(union_set.begin(), union_set.end());

                        // Found naked triple! Create eliminations
                        auto eliminations =
                            createEliminationsForTriple(board, state, empty_cells, pos1, pos2, pos3, triple_values);

                        if (!eliminations.empty()) {
                            auto explanation = fmt::format(
                                "Naked Triple [{}, {}, {}] at {}, {}, {} in {} eliminates candidates from other cells",
                                triple_values[0], triple_values[1], triple_values[2], formatPosition(pos1),
                                formatPosition(pos2), formatPosition(pos3), formatRegion(region_type, region_idx));

                            return SolveStep{
                                .type = SolveStepType::Elimination,
                                .technique = SolvingTechnique::NakedTriple,
                                .position = pos1,  // Primary position (not used for eliminations)
                                .value = 0,        // Not used for eliminations
                                .eliminations = eliminations,
                                .explanation = explanation,
                                .rating = getTechniqueRating(SolvingTechnique::NakedTriple),
                                .explanation_data = {.positions = {pos1, pos2, pos3},
                                                     .values = {triple_values[0], triple_values[1], triple_values[2]},
                                                     .region_type = region_type,
                                                     .region_index = region_idx}};
                        }
                    }
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::vector<Elimination>
    createEliminationsForTriple(const BoardData& /* board */, const CandidateGrid& state,
                                const std::vector<Position>& region_cells, const Position& triple_pos1,
                                const Position& triple_pos2, const Position& triple_pos3,
                                const std::vector<int>& triple_values) {
        std::vector<Elimination> eliminations;

        // Check each cell in the region
        for (const auto& pos : region_cells) {
            // Skip the triple cells themselves
            if (pos == triple_pos1 || pos == triple_pos2 || pos == triple_pos3) {
                continue;
            }

            // Check if this cell has any of the triple values as candidates
            auto candidates = getCandidates(state, pos.row, pos.col);
            for (int value : triple_values) {
                if (std::ranges::contains(candidates, value)) {
                    eliminations.push_back(Elimination{.position = pos, .value = value});
                }
            }
        }

        return eliminations;
    }
};

}  // namespace sudoku::core
