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

/// Strategy for finding Hidden Quads
/// A Hidden Quad is four values that can only appear in the same four cells within
/// a region, even though those cells may have other candidates. All other candidates
/// can be eliminated from those four cells.
class HiddenQuadStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (auto region_type : {RegionType::Row, RegionType::Col, RegionType::Box}) {
            for (size_t region_idx = 0; region_idx < BOARD_SIZE; ++region_idx) {
                auto result = findHiddenQuadInRegion(board, candidates, region_type, region_idx);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::HiddenQuad;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Hidden Quad";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::HiddenQuad);
    }

private:
    [[nodiscard]] static std::optional<SolveStep> findHiddenQuadInRegion(const BoardData& board,
                                                                         const CandidateGrid& candidates,
                                                                         RegionType region_type, size_t region_idx) {
        auto empty_cells = getEmptyCellsInUnit(board, region_type, region_idx);

        if (empty_cells.size() < 4) {
            return std::nullopt;
        }

        // For each combination of 4 values, check if they appear in exactly 4 cells
        for (int v1 = MIN_VALUE; v1 <= MAX_VALUE; ++v1) {
            for (int v2 = v1 + 1; v2 <= MAX_VALUE; ++v2) {
                for (int v3 = v2 + 1; v3 <= MAX_VALUE; ++v3) {
                    for (int v4 = v3 + 1; v4 <= MAX_VALUE; ++v4) {
                        auto result = checkHiddenQuad(candidates, empty_cells, v1, v2, v3, v4, region_type, region_idx);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep> checkHiddenQuad(const CandidateGrid& candidates,
                                                                  const std::vector<Position>& empty_cells, int val1,
                                                                  int val2, int val3, int val4, RegionType region_type,
                                                                  size_t region_idx) {
        // Verify each value exists as a candidate in at least one empty cell
        bool v1_exists = false;
        bool v2_exists = false;
        bool v3_exists = false;
        bool v4_exists = false;
        for (const auto& pos : empty_cells) {
            if (candidates.isAllowed(pos.row, pos.col, val1)) {
                v1_exists = true;
            }
            if (candidates.isAllowed(pos.row, pos.col, val2)) {
                v2_exists = true;
            }
            if (candidates.isAllowed(pos.row, pos.col, val3)) {
                v3_exists = true;
            }
            if (candidates.isAllowed(pos.row, pos.col, val4)) {
                v4_exists = true;
            }
        }
        if (!v1_exists || !v2_exists || !v3_exists || !v4_exists) {
            return std::nullopt;
        }

        // Find cells that can contain any of these four values
        std::vector<Position> cells_with_values;
        for (const auto& pos : empty_cells) {
            bool has_any = candidates.isAllowed(pos.row, pos.col, val1) ||
                           candidates.isAllowed(pos.row, pos.col, val2) ||
                           candidates.isAllowed(pos.row, pos.col, val3) || candidates.isAllowed(pos.row, pos.col, val4);
            if (has_any) {
                cells_with_values.push_back(pos);
            }
        }

        if (cells_with_values.size() != 4) {
            return std::nullopt;
        }

        // Create eliminations: remove all candidates except the quad values
        std::vector<Elimination> eliminations;
        for (const auto& pos : cells_with_values) {
            auto cell_candidates = getCandidates(candidates, pos.row, pos.col);
            for (int value : cell_candidates) {
                if (value != val1 && value != val2 && value != val3 && value != val4) {
                    eliminations.push_back(Elimination{.position = pos, .value = value});
                }
            }
        }

        if (!eliminations.empty()) {
            auto explanation = fmt::format(
                "Hidden Quad [{}, {}, {}, {}] at {}, {}, {}, {} in {} eliminates other candidates from these cells",
                val1, val2, val3, val4, formatPosition(cells_with_values[0]), formatPosition(cells_with_values[1]),
                formatPosition(cells_with_values[2]), formatPosition(cells_with_values[3]),
                formatRegion(region_type, region_idx));

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::HiddenQuad,
                             .position = cells_with_values[0],
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .points = getTechniquePoints(SolvingTechnique::HiddenQuad),
                             .explanation_data = {.positions = {cells_with_values[0], cells_with_values[1],
                                                                cells_with_values[2], cells_with_values[3]},
                                                  .values = {val1, val2, val3, val4},
                                                  .region_type = region_type,
                                                  .region_index = region_idx}};
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
