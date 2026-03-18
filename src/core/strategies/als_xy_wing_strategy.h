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
#include "als_helpers.h"

#include <algorithm>
#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding ALS-XY-Wing patterns.
/// Three non-overlapping ALSs (A, B, C) linked by restricted commons:
///   - A and B share restricted common X (all X-cells in A see all X-cells in B)
///   - B and C share restricted common Y (Y ≠ X)
///   - Value Z exists in both A and C (Z ≠ X, Z ≠ Y)
///   - Eliminate Z from cells that see all Z-cells in both A and C
class ALSXYWingStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        return findALSXYWing(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::ALSXYWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "ALS-XY-Wing";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::ALSXYWing);
    }

private:
    /// Main search: enumerate triples of ALSs and check for the pattern.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — triple ALS enumeration with restricted common checks; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findALSXYWing(const BoardData& board,
                                                                const CandidateGrid& candidates) {
        auto all_als = ALSHelpers::enumerateALSs(board, candidates);
        if (all_als.size() < 3) {
            return std::nullopt;
        }

        // For each pair (A, B), find restricted common X
        for (size_t ai = 0; ai < all_als.size(); ++ai) {
            for (size_t bi = ai + 1; bi < all_als.size(); ++bi) {
                if (ALSHelpers::sharesCells(all_als[ai], all_als[bi])) {
                    continue;
                }

                uint16_t common_ab = all_als[ai].cand_mask & all_als[bi].cand_mask;
                if (common_ab == 0) {
                    continue;
                }

                // Find restricted commons X between A and B
                for (int x = MIN_VALUE; x <= MAX_VALUE; ++x) {
                    if ((common_ab & (1U << x)) == 0) {
                        continue;
                    }

                    auto a_x_cells = ALSHelpers::cellsWithValue(candidates, all_als[ai], x);
                    auto b_x_cells = ALSHelpers::cellsWithValue(candidates, all_als[bi], x);
                    if (a_x_cells.empty() || b_x_cells.empty()) {
                        continue;
                    }
                    if (!ALSHelpers::allSee(a_x_cells, b_x_cells)) {
                        continue;  // Not a restricted common
                    }

                    // X is restricted common between A and B.
                    // Now find C with restricted common Y (≠ X) with B, and shared Z with A.
                    auto result = findThirdALS(board, candidates, all_als, ai, bi, x);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }

        return std::nullopt;
    }

    /// Find a third ALS C that completes the ALS-XY-Wing pattern.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — searches for C with restricted common Y and shared Z; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findThirdALS(const BoardData& board, const CandidateGrid& candidates,
                                                               const std::vector<ALS>& all_als, size_t ai, size_t bi,
                                                               int val_x) {
        const auto& als_a = all_als[ai];
        const auto& als_b = all_als[bi];

        for (size_t ci = 0; ci < all_als.size(); ++ci) {
            if (ci == ai || ci == bi) {
                continue;
            }
            const auto& als_c = all_als[ci];
            if (ALSHelpers::sharesCells(als_b, als_c) || ALSHelpers::sharesCells(als_a, als_c)) {
                continue;
            }

            uint16_t common_bc = als_b.cand_mask & als_c.cand_mask;
            if (common_bc == 0) {
                continue;
            }

            // Find restricted common Y between B and C (Y ≠ X)
            for (int y = MIN_VALUE; y <= MAX_VALUE; ++y) {
                if (y == val_x || (common_bc & (1U << y)) == 0) {
                    continue;
                }

                auto b_y_cells = ALSHelpers::cellsWithValue(candidates, als_b, y);
                auto c_y_cells = ALSHelpers::cellsWithValue(candidates, als_c, y);
                if (b_y_cells.empty() || c_y_cells.empty()) {
                    continue;
                }
                if (!ALSHelpers::allSee(b_y_cells, c_y_cells)) {
                    continue;
                }

                // Y is restricted common between B and C.
                // Find Z in both A and C (Z ≠ X, Z ≠ Y)
                uint16_t common_ac = als_a.cand_mask & als_c.cand_mask;
                uint16_t z_mask = common_ac & ~(1U << val_x) & ~(1U << y);

                for (int z = MIN_VALUE; z <= MAX_VALUE; ++z) {
                    if ((z_mask & (1U << z)) == 0) {
                        continue;
                    }

                    auto result = tryElimination(board, candidates, als_a, als_b, als_c, val_x, y, z);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }

        return std::nullopt;
    }

    /// Check if a position sees all cells in the given list.
    [[nodiscard]] static bool seesAllCells(const Position& pos, const std::vector<Position>& cells) {
        return std::ranges::all_of(cells, [&pos](const Position& c) { return sees(pos, c); });
    }

    /// Try to eliminate Z from cells seeing all Z-cells in both A and C.
    [[nodiscard]] static std::optional<SolveStep> tryElimination(const BoardData& board,
                                                                 const CandidateGrid& candidates, const ALS& als_a,
                                                                 const ALS& als_b, const ALS& als_c, int val_x,
                                                                 int val_y, int val_z) {
        auto a_z_cells = ALSHelpers::cellsWithValue(candidates, als_a, val_z);
        auto c_z_cells = ALSHelpers::cellsWithValue(candidates, als_c, val_z);
        if (a_z_cells.empty() || c_z_cells.empty()) {
            return std::nullopt;
        }

        auto isInALS = [&](size_t row, size_t col) {
            Position p{.row = row, .col = col};
            auto eq = [&p](const Position& c) { return c == p; };
            return std::ranges::any_of(als_a.cells, eq) || std::ranges::any_of(als_b.cells, eq) ||
                   std::ranges::any_of(als_c.cells, eq);
        };

        std::vector<Elimination> eliminations;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL || isInALS(row, col)) {
                    continue;
                }
                if (!candidates.isAllowed(row, col, val_z)) {
                    continue;
                }
                Position p{.row = row, .col = col};
                if (seesAllCells(p, a_z_cells) && seesAllCells(p, c_z_cells)) {
                    eliminations.push_back(Elimination{.position = p, .value = val_z});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        return buildStep(als_a, als_b, als_c, val_x, val_y, val_z, eliminations);
    }

    [[nodiscard]] static SolveStep buildStep(const ALS& als_a, const ALS& als_b, const ALS& als_c, int val_x, int val_y,
                                             int val_z, const std::vector<Elimination>& eliminations) {
        std::vector<Position> positions;
        std::vector<CellRole> roles;

        for (const auto& p : als_a.cells) {
            positions.push_back(p);
            roles.push_back(CellRole::SetA);
        }
        for (const auto& p : als_b.cells) {
            positions.push_back(p);
            roles.push_back(CellRole::SetB);
        }
        for (const auto& p : als_c.cells) {
            positions.push_back(p);
            roles.push_back(CellRole::SetC);
        }

        auto explanation = fmt::format("ALS-XY-Wing: A↔B restricted common {}, B↔C restricted common {} — "
                                       "eliminates {} from cells seeing all {}-cells in A and C",
                                       val_x, val_y, val_z, val_z);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::ALSXYWing,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .points = getTechniquePoints(SolvingTechnique::ALSXYWing),
                         .explanation_data = {.positions = positions,
                                              .values = {val_x, val_y, val_z},
                                              .position_roles = std::move(roles)}};
    }
};

}  // namespace sudoku::core
