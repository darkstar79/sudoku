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
#include "als_helpers.h"

#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding ALS-XZ (Almost Locked Set XZ-Rule) patterns.
/// An ALS is N cells in one house with N+1 candidates.
/// Two ALSs linked by restricted common candidate X can eliminate non-restricted common Z
/// from cells seeing all Z-cells in both ALSs.
class ALSxZStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        auto als_list = ALSHelpers::enumerateALSs(board, candidates);
        if (als_list.size() < 2) {
            return std::nullopt;
        }

        return findALSPairs(board, candidates, als_list);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::ALSxZ;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "ALS-XZ";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::ALSxZ);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — ALS pair enumeration with X/Z restricted-common matching; nesting is inherent to the algorithm
    [[nodiscard]] static std::optional<SolveStep> findALSPairs(const BoardData& board, const CandidateGrid& candidates,
                                                               const std::vector<ALS>& als_list) {
        for (size_t i = 0; i < als_list.size(); ++i) {
            for (size_t j = i + 1; j < als_list.size(); ++j) {
                if (ALSHelpers::sharesCells(als_list[i], als_list[j])) {
                    continue;
                }

                uint16_t common = als_list[i].cand_mask & als_list[j].cand_mask;
                if (std::popcount(common) < 2) {
                    continue;
                }

                // Try each common value as X (restricted common)
                for (int x = MIN_VALUE; x <= MAX_VALUE; ++x) {
                    if (!(common & (1 << x))) {
                        continue;
                    }

                    auto a_x_cells = ALSHelpers::cellsWithValue(candidates, als_list[i], x);
                    auto b_x_cells = ALSHelpers::cellsWithValue(candidates, als_list[j], x);

                    // X must be restricted: all X-cells in A see all X-cells in B
                    if (!ALSHelpers::allSee(a_x_cells, b_x_cells)) {
                        continue;
                    }

                    // Try each other common value as Z
                    for (int z = MIN_VALUE; z <= MAX_VALUE; ++z) {
                        if (z == x || !(common & (1 << z))) {
                            continue;
                        }

                        auto a_z_cells = ALSHelpers::cellsWithValue(candidates, als_list[i], z);
                        auto b_z_cells = ALSHelpers::cellsWithValue(candidates, als_list[j], z);

                        // Eliminate Z from cells outside both ALSs that see all Z-cells in both
                        std::vector<Elimination> eliminations;
                        for (size_t row = 0; row < BOARD_SIZE; ++row) {
                            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                                if (board[row][col] != EMPTY_CELL) {
                                    continue;
                                }
                                if (!candidates.isAllowed(row, col, z)) {
                                    continue;
                                }
                                Position pos{.row = row, .col = col};

                                // Skip cells in either ALS
                                bool in_als = false;
                                for (const auto& c : als_list[i].cells) {
                                    if (c == pos) {
                                        in_als = true;
                                        break;
                                    }
                                }
                                if (!in_als) {
                                    for (const auto& c : als_list[j].cells) {
                                        if (c == pos) {
                                            in_als = true;
                                            break;
                                        }
                                    }
                                }
                                if (in_als) {
                                    continue;
                                }

                                // Must see all Z-cells in both ALSs
                                bool sees_all_a = true;
                                for (const auto& az : a_z_cells) {
                                    if (!sees(pos, az)) {
                                        sees_all_a = false;
                                        break;
                                    }
                                }
                                if (!sees_all_a) {
                                    continue;
                                }

                                bool sees_all_b = true;
                                for (const auto& bz : b_z_cells) {
                                    if (!sees(pos, bz)) {
                                        sees_all_b = false;
                                        break;
                                    }
                                }
                                if (sees_all_b) {
                                    eliminations.push_back(Elimination{.position = pos, .value = z});
                                }
                            }
                        }

                        if (!eliminations.empty()) {
                            std::vector<Position> all_positions;
                            for (const auto& c : als_list[i].cells) {
                                all_positions.push_back(c);
                            }
                            for (const auto& c : als_list[j].cells) {
                                all_positions.push_back(c);
                            }

                            std::string als_a_str;
                            for (size_t ci = 0; ci < als_list[i].cells.size(); ++ci) {
                                if (ci > 0) {
                                    als_a_str += ",";
                                }
                                als_a_str += formatPosition(als_list[i].cells[ci]);
                            }
                            std::string als_b_str;
                            for (size_t ci = 0; ci < als_list[j].cells.size(); ++ci) {
                                if (ci > 0) {
                                    als_b_str += ",";
                                }
                                als_b_str += formatPosition(als_list[j].cells[ci]);
                            }
                            auto explanation = fmt::format("ALS-XZ: ALS {{{}}} and ALS {{{}}} linked by restricted "
                                                           "common {} — eliminates {} from cells seeing both ALSs",
                                                           als_a_str, als_b_str, x, z);

                            std::vector<CellRole> als_roles(als_list[i].cells.size(), CellRole::SetA);
                            als_roles.insert(als_roles.end(), als_list[j].cells.size(), CellRole::SetB);

                            return SolveStep{
                                .type = SolveStepType::Elimination,
                                .technique = SolvingTechnique::ALSxZ,
                                .position = eliminations[0].position,
                                .value = 0,
                                .eliminations = eliminations,
                                .explanation = explanation,
                                .rating = getTechniqueRating(SolvingTechnique::ALSxZ),
                                .explanation_data = {.positions = all_positions,
                                                     .values = {x, z, static_cast<int>(als_list[i].cells.size()),
                                                                static_cast<int>(als_list[j].cells.size())},
                                                     .position_roles = std::move(als_roles)}};
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }
};

}  // namespace sudoku::core
