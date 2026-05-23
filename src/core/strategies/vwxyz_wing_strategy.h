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
#include "wing_helpers.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding VWXYZ-Wing patterns.
/// Five cells with exactly 5 candidate values total. Exactly one value must be
/// "non-restricted" (not all cells containing it see each other). That value Z
/// can be eliminated from external cells that see all cells containing Z.
/// Extension of WXYZ-Wing from 4 cells/values to 5 cells/values.
class VWXYZWingStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        auto pivot_cells = WingHelpers::findCellsByCandidateCount(board, candidates, 3, 5);
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
        return SolvingTechnique::VWXYZWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "VWXYZ-Wing";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::VWXYZWing);
    }

private:
    // CPD-OFF — wing enumeration with different nesting depths vs wxyz_wing
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — enumerates wing quadruples for VWXYZ pattern; nesting is inherent
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

        if (wings.size() < 4) {
            return std::nullopt;
        }

        // Try all quadruples of wings
        for (size_t i = 0; i < wings.size(); ++i) {
            for (size_t j = i + 1; j < wings.size(); ++j) {
                uint16_t partial2 = pivot_mask | wings[i].second | wings[j].second;
                if (std::popcount(partial2) > 5) {
                    continue;
                }
                for (size_t k = j + 1; k < wings.size(); ++k) {
                    uint16_t partial3 = partial2 | wings[k].second;
                    if (std::popcount(partial3) > 5) {
                        continue;
                    }
                    for (size_t l = k + 1; l < wings.size(); ++l) {
                        uint16_t union_mask = partial3 | wings[l].second;
                        if (std::popcount(union_mask) != 5) {
                            continue;
                        }

                        const auto& w1 = wings[i].first;
                        const auto& w2 = wings[j].first;
                        const auto& w3 = wings[k].first;
                        const auto& w4 = wings[l].first;

                        std::array<Position, 5> pattern = {pivot, w1, w2, w3, w4};
                        std::array<uint16_t, 5> masks = {pivot_mask, wings[i].second, wings[j].second, wings[k].second,
                                                         wings[l].second};

                        auto result =
                            classifyAndEliminate(board, candidates, pattern, masks, union_mask, pivot, w1, w2, w3, w4);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }
        }

        return std::nullopt;
    }

    /// Find the single non-restricted value (Z) and its cells. Returns nullopt if not exactly one.
    struct ZValueResult {
        int z_value;
        std::vector<Position> z_cells;
    };
    [[nodiscard]] static std::optional<ZValueResult> findNonRestrictedValue(const std::array<Position, 5>& pattern,
                                                                            const std::array<uint16_t, 5>& masks,
                                                                            uint16_t union_mask) {
        int z_value = 0;
        int non_restricted_count = 0;
        std::vector<Position> z_cells;

        for (int val = MIN_VALUE; val <= MAX_VALUE; ++val) {
            if ((union_mask & valueToBit(val)) == 0) {
                continue;
            }
            std::vector<Position> cells_with_val;
            for (int ci = 0; ci < 5; ++ci) {
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
            return std::nullopt;
        }
        return ZValueResult{.z_value = z_value, .z_cells = std::move(z_cells)};
    }
    // CPD-ON

    /// Check if a position sees all cells in a list.
    [[nodiscard]] static bool seesAllCells(const Position& pos, const std::vector<Position>& cells) {
        return std::ranges::all_of(cells, [&pos](const Position& c) { return sees(pos, c); });
    }

    /// Classify values as restricted/non-restricted, find Z, and eliminate.
    [[nodiscard]] static std::optional<SolveStep>
    classifyAndEliminate(const BoardData& board, const CandidateGrid& candidates,
                         const std::array<Position, 5>& pattern, const std::array<uint16_t, 5>& masks,
                         uint16_t union_mask, const Position& pivot, const Position& w1, const Position& w2,
                         const Position& w3, const Position& w4) {
        auto z_result = findNonRestrictedValue(pattern, masks, union_mask);
        if (!z_result.has_value()) {
            return std::nullopt;
        }
        int z_value = z_result->z_value;
        const auto& z_cells = z_result->z_cells;

        std::array<Position, 5> wing_cells = {pivot, w1, w2, w3, w4};
        std::vector<Elimination> eliminations;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                Position pos{.row = row, .col = col};
                if (std::ranges::any_of(wing_cells, [&pos](const Position& w) { return w == pos; })) {
                    continue;
                }
                if (!candidates.isAllowed(row, col, z_value)) {
                    continue;
                }
                if (seesAllCells(pos, z_cells)) {
                    eliminations.push_back(Elimination{.position = pos, .value = z_value});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation =
            fmt::format("VWXYZ-Wing: pivot {} with wings {}, {}, {}, {} — eliminates {} from cells seeing all Z-cells",
                        formatPosition(pivot), formatPosition(w1), formatPosition(w2), formatPosition(w3),
                        formatPosition(w4), z_value);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::VWXYZWing,
                         .position = pivot,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::VWXYZWing),
                         .explanation_data = {.positions = {pivot, w1, w2, w3, w4},
                                              .values = {z_value},
                                              .position_roles = {CellRole::Pivot, CellRole::Wing, CellRole::Wing,
                                                                 CellRole::Wing, CellRole::Wing}}};
    }
};

}  // namespace sudoku::core
