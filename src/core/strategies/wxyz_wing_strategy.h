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
#include "wing_helpers.h"

#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding WXYZ-Wing patterns.
/// Four cells with exactly 4 candidate values total. Exactly one value must be
/// "non-restricted" (not all cells containing it see each other). That value Z
/// can be eliminated from external cells that see all cells containing Z.
class WXYZWingStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        auto pivot_cells = WingHelpers::findCellsByCandidateCount(board, candidates, 3, 4);
        auto bivalue_cells = WingHelpers::findCellsByCandidateCount(board, candidates, 2, 2);

        for (const auto& pivot : pivot_cells) {
            auto result = tryPivot(board, candidates, pivot, bivalue_cells);
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::WXYZWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "WXYZ-Wing";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::WXYZWing);
    }

private:
    // CPD-OFF — wing enumeration with different nesting depths vs vwxyz_wing
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — enumerates wing triples for WXYZ pattern; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryPivot(const BoardData& board, const CandidateGrid& candidates,
                                                           const Position& pivot,
                                                           const std::vector<Position>& bivalue_cells) {
        uint16_t pivot_mask = candidates.getPossibleValuesMask(pivot.row, pivot.col);

        // Collect wings that see the pivot and share at least one candidate
        std::vector<std::pair<Position, uint16_t>> wings;
        for (const auto& cell : bivalue_cells) {
            if (!sees(pivot, cell)) {
                continue;
            }
            uint16_t wing_mask = candidates.getPossibleValuesMask(cell.row, cell.col);
            if ((wing_mask & pivot_mask) == 0) {
                continue;
            }
            wings.emplace_back(cell, wing_mask);
        }

        // Try all triples of wings
        for (size_t i = 0; i < wings.size(); ++i) {
            for (size_t j = i + 1; j < wings.size(); ++j) {
                for (size_t k = j + 1; k < wings.size(); ++k) {
                    uint16_t union_mask = pivot_mask | wings[i].second | wings[j].second | wings[k].second;
                    if (std::popcount(union_mask) != 4) {
                        continue;
                    }

                    const auto& w1 = wings[i].first;
                    const auto& w2 = wings[j].first;
                    const auto& w3 = wings[k].first;

                    std::array<Position, 4> pattern = {pivot, w1, w2, w3};
                    std::array<uint16_t, 4> masks = {pivot_mask, wings[i].second, wings[j].second, wings[k].second};

                    // Classify each value as restricted or non-restricted.
                    // Restricted: all cells containing the value mutually see each other.
                    // Must have exactly one non-restricted value (Z) for a valid WXYZ-Wing.
                    int z_value = 0;
                    int non_restricted_count = 0;
                    std::vector<Position> z_cells;

                    for (int val = MIN_VALUE; val <= MAX_VALUE; ++val) {
                        if ((union_mask & valueToBit(val)) == 0) {
                            continue;
                        }
                        std::vector<Position> cells_with_val;
                        for (int ci = 0; ci < 4; ++ci) {
                            if ((masks[ci] & valueToBit(val)) != 0) {
                                cells_with_val.push_back(pattern[ci]);
                            }
                        }
                        if (!WingHelpers::allMutuallyVisible(cells_with_val)) {
                            ++non_restricted_count;
                            z_value = val;
                            z_cells = std::move(cells_with_val);
                        }
                    }

                    if (non_restricted_count != 1) {
                        continue;
                    }

                    // Eliminate Z from external cells that see ALL cells containing Z
                    std::vector<Elimination> eliminations;
                    for (size_t row = 0; row < BOARD_SIZE; ++row) {
                        for (size_t col = 0; col < BOARD_SIZE; ++col) {
                            if (board[row][col] != EMPTY_CELL) {
                                continue;
                            }
                            Position pos{.row = row, .col = col};
                            if (pos == pivot || pos == w1 || pos == w2 || pos == w3) {
                                continue;
                            }
                            if (!candidates.isAllowed(row, col, z_value)) {
                                continue;
                            }
                            bool sees_all_z = true;
                            for (const auto& zc : z_cells) {
                                if (!sees(pos, zc)) {
                                    sees_all_z = false;
                                    break;
                                }
                            }
                            if (sees_all_z) {
                                eliminations.push_back(Elimination{.position = pos, .value = z_value});
                            }
                        }
                    }

                    if (!eliminations.empty()) {
                        auto explanation = fmt::format(
                            "WXYZ-Wing: pivot {} with wings {}, {}, {} — eliminates {} from cells seeing all Z-cells",
                            formatPosition(pivot), formatPosition(w1), formatPosition(w2), formatPosition(w3), z_value);

                        return SolveStep{.type = SolveStepType::Elimination,
                                         .technique = SolvingTechnique::WXYZWing,
                                         .position = pivot,
                                         .value = 0,
                                         .eliminations = eliminations,
                                         .explanation = explanation,
                                         .rating = getTechniqueRating(SolvingTechnique::WXYZWing),
                                         .explanation_data = {.positions = {pivot, w1, w2, w3},
                                                              .values = {z_value},
                                                              .position_roles = {CellRole::Pivot, CellRole::Wing,
                                                                                 CellRole::Wing, CellRole::Wing}}};
                    }
                }
            }
        }

        return std::nullopt;
    }
    // CPD-ON
};

}  // namespace sudoku::core
