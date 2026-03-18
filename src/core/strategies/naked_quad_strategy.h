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
#include <set>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Naked Quads
/// A Naked Quad is four cells in the same region that collectively contain only
/// four candidate values. These four values can be eliminated from all other cells
/// in that region. Each cell can have 2, 3, or 4 candidates.
class NakedQuadStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (auto region_type : {RegionType::Row, RegionType::Col, RegionType::Box}) {
            for (size_t region_idx = 0; region_idx < BOARD_SIZE; ++region_idx) {
                auto result = findNakedQuadInRegion(board, candidates, region_type, region_idx);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::NakedQuad;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Naked Quad";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::NakedQuad);
    }

private:
    [[nodiscard]] static std::optional<SolveStep> findNakedQuadInRegion(const BoardData& board,
                                                                        const CandidateGrid& candidates,
                                                                        RegionType region_type, size_t region_idx) {
        auto empty_cells = getEmptyCellsInUnit(board, region_type, region_idx);

        // Find cells with 2-4 candidates (potential quad members)
        std::vector<Position> quad_candidates;
        for (const auto& pos : empty_cells) {
            int count = candidates.countPossibleValues(pos.row, pos.col);
            if (count >= 2 && count <= 4) {
                quad_candidates.push_back(pos);
            }
        }

        if (quad_candidates.size() < 4) {
            return std::nullopt;
        }

        // Check all combinations of 4 cells
        for (size_t idx_a = 0; idx_a < quad_candidates.size(); ++idx_a) {
            for (size_t idx_b = idx_a + 1; idx_b < quad_candidates.size(); ++idx_b) {
                for (size_t idx_c = idx_b + 1; idx_c < quad_candidates.size(); ++idx_c) {
                    for (size_t idx_d = idx_c + 1; idx_d < quad_candidates.size(); ++idx_d) {
                        auto result =
                            checkQuad(candidates, empty_cells, quad_candidates[idx_a], quad_candidates[idx_b],
                                      quad_candidates[idx_c], quad_candidates[idx_d], region_type, region_idx);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep> checkQuad(const CandidateGrid& candidates,
                                                            const std::vector<Position>& region_cells,
                                                            const Position& pos1, const Position& pos2,
                                                            const Position& pos3, const Position& pos4,
                                                            RegionType region_type, size_t region_idx) {
        auto cand1 = getCandidates(candidates, pos1.row, pos1.col);
        auto cand2 = getCandidates(candidates, pos2.row, pos2.col);
        auto cand3 = getCandidates(candidates, pos3.row, pos3.col);
        auto cand4 = getCandidates(candidates, pos4.row, pos4.col);

        std::set<int> union_set;
        union_set.insert(cand1.begin(), cand1.end());
        union_set.insert(cand2.begin(), cand2.end());
        union_set.insert(cand3.begin(), cand3.end());
        union_set.insert(cand4.begin(), cand4.end());

        if (union_set.size() != 4) {
            return std::nullopt;
        }

        std::vector<int> quad_values(union_set.begin(), union_set.end());

        // Create eliminations from other cells in the region
        std::vector<Elimination> eliminations;
        for (const auto& pos : region_cells) {
            if (pos == pos1 || pos == pos2 || pos == pos3 || pos == pos4) {
                continue;
            }
            for (int value : quad_values) {
                if (candidates.isAllowed(pos.row, pos.col, value)) {
                    eliminations.push_back(Elimination{.position = pos, .value = value});
                }
            }
        }

        if (!eliminations.empty()) {
            auto explanation = fmt::format(
                "Naked Quad [{}, {}, {}, {}] at {}, {}, {}, {} in {} eliminates candidates from other cells",
                quad_values[0], quad_values[1], quad_values[2], quad_values[3], formatPosition(pos1),
                formatPosition(pos2), formatPosition(pos3), formatPosition(pos4),
                formatRegion(region_type, region_idx));

            return SolveStep{
                .type = SolveStepType::Elimination,
                .technique = SolvingTechnique::NakedQuad,
                .position = pos1,
                .value = 0,
                .eliminations = eliminations,
                .explanation = explanation,
                .points = getTechniquePoints(SolvingTechnique::NakedQuad),
                .explanation_data = {.positions = {pos1, pos2, pos3, pos4},
                                     .values = {quad_values[0], quad_values[1], quad_values[2], quad_values[3]},
                                     .region_type = region_type,
                                     .region_index = region_idx}};
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
