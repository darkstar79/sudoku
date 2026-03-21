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
#include "fish_helpers.h"

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Sashimi X-Wing patterns.
/// Like a Finned X-Wing, but at least one base row (or column) has only 1 candidate position
/// instead of the minimum 2 required by finned fish. The "missing" position is what makes it sashimi.
/// Eliminations are restricted to cells in the base columns (or rows) that also share the fin's box.
class SashimiXWingStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        auto result = findRowBased(board, candidates);
        if (result.has_value()) {
            return result;
        }
        return findColBased(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::SashimiXWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Sashimi X-Wing";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::SashimiXWing);
    }

private:
    /// Row-based Sashimi X-Wing:
    /// Two rows where at least one has exactly 1 position (the sashimi condition).
    /// Both rows have 1-3 positions, union of columns = 3 (N+1 for order-2 fish).
    /// Uses validateFinnedFish to identify the fin and base columns.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×row1×row2×fin search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findRowBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto rows_cols = FishHelpers::collectCandidatePositions(board, candidates, value, true);

            for (size_t r1 = 0; r1 < BOARD_SIZE; ++r1) {
                if (rows_cols[r1].empty() || rows_cols[r1].size() > 3) {
                    continue;
                }
                for (size_t r2 = r1 + 1; r2 < BOARD_SIZE; ++r2) {
                    if (rows_cols[r2].empty() || rows_cols[r2].size() > 3) {
                        continue;
                    }

                    // At least one row must have exactly 1 position (sashimi condition)
                    bool has_sashimi = (rows_cols[r1].size() == 1 || rows_cols[r2].size() == 1);
                    if (!has_sashimi) {
                        continue;
                    }

                    auto union_cols = FishHelpers::indexUnion(rows_cols[r1], rows_cols[r2]);
                    if (union_cols.size() != 3) {
                        continue;
                    }

                    std::array<size_t, 2> base_rows = {r1, r2};
                    for (size_t fin_idx = 0; fin_idx < 3; ++fin_idx) {
                        auto info = FishHelpers::validateFinnedFish(rows_cols, base_rows, union_cols, fin_idx);
                        if (!info) {
                            continue;
                        }

                        Position fin_pos{.row = info->fin_primary, .col = info->fin_secondary};
                        size_t fin_box = getBoxIndex(fin_pos.row, fin_pos.col);

                        auto eliminations = FishHelpers::buildFinnedEliminations(board, candidates, value, base_rows,
                                                                                 info->base_secondaries, fin_box, true);
                        if (eliminations.empty()) {
                            continue;
                        }

                        auto [positions, roles] =
                            FishHelpers::buildFinnedPositionsAndRoles(rows_cols, base_rows, fin_pos, true);

                        auto explanation =
                            fmt::format("Sashimi X-Wing on value {} in Rows {} and {}, with fin at {} — eliminates {} "
                                        "from cells in fin's box",
                                        value, r1 + 1, r2 + 1, formatPosition(fin_pos), value);

                        return SolveStep{
                            .type = SolveStepType::Elimination,
                            .technique = SolvingTechnique::SashimiXWing,
                            .position = eliminations[0].position,
                            .value = 0,
                            .eliminations = eliminations,
                            .explanation = explanation,
                            .rating = getTechniqueRating(SolvingTechnique::SashimiXWing),
                            .explanation_data = {.positions = positions,
                                                 .values = {value, static_cast<int>(r1 + 1), static_cast<int>(r2 + 1)},
                                                 .region_type = RegionType::Row,
                                                 .position_roles = std::move(roles)}};
                    }
                }
            }
        }
        return std::nullopt;
    }

    /// Column-based Sashimi X-Wing (mirror of row-based).
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×col1×col2×fin search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findColBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto cols_rows = FishHelpers::collectCandidatePositions(board, candidates, value, false);

            for (size_t c1 = 0; c1 < BOARD_SIZE; ++c1) {
                if (cols_rows[c1].empty() || cols_rows[c1].size() > 3) {
                    continue;
                }
                for (size_t c2 = c1 + 1; c2 < BOARD_SIZE; ++c2) {
                    if (cols_rows[c2].empty() || cols_rows[c2].size() > 3) {
                        continue;
                    }

                    bool has_sashimi = (cols_rows[c1].size() == 1 || cols_rows[c2].size() == 1);
                    if (!has_sashimi) {
                        continue;
                    }

                    auto union_rows = FishHelpers::indexUnion(cols_rows[c1], cols_rows[c2]);
                    if (union_rows.size() != 3) {
                        continue;
                    }

                    std::array<size_t, 2> base_cols = {c1, c2};
                    for (size_t fin_idx = 0; fin_idx < 3; ++fin_idx) {
                        auto info = FishHelpers::validateFinnedFish(cols_rows, base_cols, union_rows, fin_idx);
                        if (!info) {
                            continue;
                        }

                        Position fin_pos{.row = info->fin_secondary, .col = info->fin_primary};
                        size_t fin_box = getBoxIndex(fin_pos.row, fin_pos.col);

                        auto eliminations = FishHelpers::buildFinnedEliminations(
                            board, candidates, value, base_cols, info->base_secondaries, fin_box, false);
                        if (eliminations.empty()) {
                            continue;
                        }

                        auto [positions, roles] =
                            FishHelpers::buildFinnedPositionsAndRoles(cols_rows, base_cols, fin_pos, false);

                        auto explanation = fmt::format(
                            "Sashimi X-Wing on value {} in Columns {} and {}, with fin at {} — eliminates {} "
                            "from cells in fin's box",
                            value, c1 + 1, c2 + 1, formatPosition(fin_pos), value);

                        return SolveStep{
                            .type = SolveStepType::Elimination,
                            .technique = SolvingTechnique::SashimiXWing,
                            .position = eliminations[0].position,
                            .value = 0,
                            .eliminations = eliminations,
                            .explanation = explanation,
                            .rating = getTechniqueRating(SolvingTechnique::SashimiXWing),
                            .explanation_data = {.positions = positions,
                                                 .values = {value, static_cast<int>(c1 + 1), static_cast<int>(c2 + 1)},
                                                 .region_type = RegionType::Col,
                                                 .position_roles = std::move(roles)}};
                    }
                }
            }
        }
        return std::nullopt;
    }
};

}  // namespace sudoku::core
