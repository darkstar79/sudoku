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
#include "fish_helpers.h"

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Finned Swordfish patterns.
/// Like a Swordfish, but one of the 3 rows (or columns) has an extra candidate (the "fin").
/// The union of columns is 4 instead of 3. Eliminations are restricted to the 3 base columns
/// in cells that also share the fin's box.
class FinnedSwordfishStrategy : public ISolvingStrategy, protected StrategyBase {
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
        return SolvingTechnique::FinnedSwordfish;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Finned Swordfish";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::FinnedSwordfish);
    }

private:
    // CPD-OFF — fish enumeration pattern shared with finned_jellyfish and jellyfish
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×row1×row2×row3 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findRowBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto rows_cols = FishHelpers::collectCandidatePositions(board, candidates, value, true);

            for (size_t r1 = 0; r1 < BOARD_SIZE; ++r1) {
                if (rows_cols[r1].size() < 2 || rows_cols[r1].size() > 4) {
                    continue;
                }
                for (size_t r2 = r1 + 1; r2 < BOARD_SIZE; ++r2) {
                    if (rows_cols[r2].size() < 2 || rows_cols[r2].size() > 4) {
                        continue;
                    }
                    for (size_t r3 = r2 + 1; r3 < BOARD_SIZE; ++r3) {
                        if (rows_cols[r3].size() < 2 || rows_cols[r3].size() > 4) {
                            continue;
                        }

                        auto union_cols = FishHelpers::indexUnion(rows_cols[r1], rows_cols[r2], rows_cols[r3]);
                        if (union_cols.size() != 4) {
                            continue;
                        }

                        // Try each combination of 3 base cols from the 4-col union
                        auto result =
                            tryRowFinnedSwordfish(board, candidates, value, rows_cols, r1, r2, r3, union_cols);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep>
    tryRowFinnedSwordfish(const BoardData& board, const CandidateGrid& candidates, int value,
                          const std::vector<std::vector<size_t>>& rows_cols, size_t r1, size_t r2, size_t r3,
                          const std::vector<size_t>& union_cols) {
        std::array<size_t, 3> rows = {r1, r2, r3};

        for (size_t fi = 0; fi < 4; ++fi) {
            auto info = FishHelpers::validateFinnedFish(rows_cols, rows, union_cols, fi);
            if (!info) {
                continue;
            }

            Position fin_pos{.row = info->fin_primary, .col = info->fin_secondary};
            size_t fin_box = getBoxIndex(fin_pos.row, fin_pos.col);

            auto eliminations = FishHelpers::buildFinnedEliminations(board, candidates, value, rows,
                                                                     info->base_secondaries, fin_box, true);
            if (eliminations.empty()) {
                continue;
            }

            auto [positions, roles] = FishHelpers::buildFinnedPositionsAndRoles(rows_cols, rows, fin_pos, true);

            auto explanation = fmt::format("Finned Swordfish on value {} in Rows {}, {}, {} with fin at {} — "
                                           "eliminates {} from cells in fin's box",
                                           value, r1 + 1, r2 + 1, r3 + 1, formatPosition(fin_pos), value);

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::FinnedSwordfish,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::FinnedSwordfish),
                             .explanation_data = {.positions = positions,
                                                  .values = {value, static_cast<int>(r1 + 1), static_cast<int>(r2 + 1),
                                                             static_cast<int>(r3 + 1)},
                                                  .region_type = RegionType::Row,
                                                  .position_roles = std::move(roles)}};
        }
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×col1×col2×col3 search; nesting is inherent to fish algorithms
    [[nodiscard]] static std::optional<SolveStep> findColBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto cols_rows = FishHelpers::collectCandidatePositions(board, candidates, value, false);

            for (size_t c1 = 0; c1 < BOARD_SIZE; ++c1) {
                if (cols_rows[c1].size() < 2 || cols_rows[c1].size() > 4) {
                    continue;
                }
                for (size_t c2 = c1 + 1; c2 < BOARD_SIZE; ++c2) {
                    if (cols_rows[c2].size() < 2 || cols_rows[c2].size() > 4) {
                        continue;
                    }
                    for (size_t c3 = c2 + 1; c3 < BOARD_SIZE; ++c3) {
                        if (cols_rows[c3].size() < 2 || cols_rows[c3].size() > 4) {
                            continue;
                        }

                        auto union_rows = FishHelpers::indexUnion(cols_rows[c1], cols_rows[c2], cols_rows[c3]);
                        if (union_rows.size() != 4) {
                            continue;
                        }

                        auto result =
                            tryColFinnedSwordfish(board, candidates, value, cols_rows, c1, c2, c3, union_rows);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep>
    tryColFinnedSwordfish(const BoardData& board, const CandidateGrid& candidates, int value,
                          const std::vector<std::vector<size_t>>& cols_rows, size_t c1, size_t c2, size_t c3,
                          const std::vector<size_t>& union_rows) {
        std::array<size_t, 3> cols = {c1, c2, c3};

        for (size_t fi = 0; fi < 4; ++fi) {
            auto info = FishHelpers::validateFinnedFish(cols_rows, cols, union_rows, fi);
            if (!info) {
                continue;
            }

            Position fin_pos{.row = info->fin_secondary, .col = info->fin_primary};
            size_t fin_box = getBoxIndex(fin_pos.row, fin_pos.col);

            auto eliminations = FishHelpers::buildFinnedEliminations(board, candidates, value, cols,
                                                                     info->base_secondaries, fin_box, false);
            if (eliminations.empty()) {
                continue;
            }

            auto [positions, roles] = FishHelpers::buildFinnedPositionsAndRoles(cols_rows, cols, fin_pos, false);

            auto explanation = fmt::format("Finned Swordfish on value {} in Columns {}, {}, {} with fin at {} — "
                                           "eliminates {} from cells in fin's box",
                                           value, c1 + 1, c2 + 1, c3 + 1, formatPosition(fin_pos), value);

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::FinnedSwordfish,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::FinnedSwordfish),
                             .explanation_data = {.positions = positions,
                                                  .values = {value, static_cast<int>(c1 + 1), static_cast<int>(c2 + 1),
                                                             static_cast<int>(c3 + 1)},
                                                  .region_type = RegionType::Col,
                                                  .position_roles = std::move(roles)}};
        }
        return std::nullopt;
    }
    // CPD-ON
};

}  // namespace sudoku::core
