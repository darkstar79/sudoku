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

#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace sudoku::core {

/// Strategy for finding Junior Exocet patterns.
///
/// A Junior Exocet involves:
/// - 2 base cells (B1, B2) in the same mini-row (same box + same row)
/// - 2 target cells (T1, T2) in the same columns as the bases but in different rows/boxes
/// - The base candidates constrain the targets: non-base candidates can be eliminated from targets
///
/// This is one of the most powerful logical techniques for extremely hard puzzles.
class JuniorExocetStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // For each box (0-8)
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            auto result = searchBox(board, candidates, box);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::JuniorExocet;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Junior Exocet";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::JuniorExocet);
    }

private:
    /// Minimum number of combined base candidates for a valid pattern
    static constexpr int kMinBaseCandidates = 3;
    /// Maximum number of combined base candidates for a valid pattern
    static constexpr int kMaxBaseCandidates = 5;

    /// Search a single box for Junior Exocet patterns
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — nested search over base pairs and target cells; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> searchBox(const BoardData& board, const CandidateGrid& candidates,
                                                            size_t box) {
        size_t box_start_row = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_start_col = (box % BOX_SIZE) * BOX_SIZE;

        // For each row within this box (3 rows per box)
        for (size_t br = 0; br < BOX_SIZE; ++br) {
            size_t row = box_start_row + br;

            // Find empty cells in this row within this box
            std::vector<Position> row_cells;
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t col = box_start_col + bc;
                if (board[row][col] == EMPTY_CELL) {
                    row_cells.push_back(Position{.row = row, .col = col});
                }
            }

            // Need at least 2 empty cells to form a base pair
            if (row_cells.size() < 2) {
                continue;
            }

            // Try all pairs of empty cells as base pair
            for (size_t i = 0; i < row_cells.size(); ++i) {
                for (size_t j = i + 1; j < row_cells.size(); ++j) {
                    auto result = tryBasePair(board, candidates, row_cells[i], row_cells[j], box);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }
        return std::nullopt;
    }

    /// Try a specific base pair and search for valid target cells
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — target cell search with cross-line analysis
    [[nodiscard]] static std::optional<SolveStep> tryBasePair(const BoardData& board, const CandidateGrid& candidates,
                                                              const Position& base1, const Position& base2,
                                                              size_t base_box) {
        // Compute base_cands = union of candidates in B1 and B2
        uint16_t base_mask = candidates.getPossibleValuesMask(base1.row, base1.col) |
                             candidates.getPossibleValuesMask(base2.row, base2.col);

        int base_count = std::popcount(base_mask);
        if (base_count < kMinBaseCandidates || base_count > kMaxBaseCandidates) {
            return std::nullopt;
        }

        // Find target cells:
        // T1 in same column as B1, different box from base_box, in rows outside base band
        // T2 in same column as B2, different box from base_box
        // T1 and T2 must be in different boxes from each other AND from base_box
        size_t base_band_start = (base1.row / BOX_SIZE) * BOX_SIZE;

        // Collect target candidates for column of B1
        std::vector<Position> t1_candidates;
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            // Skip rows in the base band
            if (r >= base_band_start && r < base_band_start + BOX_SIZE) {
                continue;
            }
            if (board[r][base1.col] != EMPTY_CELL) {
                continue;
            }
            size_t target_box = getBoxIndex(r, base1.col);
            if (target_box == base_box) {
                continue;
            }
            // T1 must have at least one base candidate
            uint16_t t_mask = candidates.getPossibleValuesMask(r, base1.col);
            if ((t_mask & base_mask) == 0) {
                continue;
            }
            t1_candidates.push_back(Position{.row = r, .col = base1.col});
        }

        // Collect target candidates for column of B2
        std::vector<Position> t2_candidates;
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            if (r >= base_band_start && r < base_band_start + BOX_SIZE) {
                continue;
            }
            if (board[r][base2.col] != EMPTY_CELL) {
                continue;
            }
            size_t target_box = getBoxIndex(r, base2.col);
            if (target_box == base_box) {
                continue;
            }
            uint16_t t_mask = candidates.getPossibleValuesMask(r, base2.col);
            if ((t_mask & base_mask) == 0) {
                continue;
            }
            t2_candidates.push_back(Position{.row = r, .col = base2.col});
        }

        // Try all (T1, T2) combinations
        for (const auto& t1 : t1_candidates) {
            size_t t1_box = getBoxIndex(t1.row, t1.col);
            for (const auto& t2 : t2_candidates) {
                size_t t2_box = getBoxIndex(t2.row, t2.col);

                // T1 and T2 must be in different boxes from each other
                if (t1_box == t2_box) {
                    continue;
                }

                // Check for eliminations: non-base candidates in T1 and T2
                auto result = checkEliminations(candidates, base1, base2, t1, t2, base_mask);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    /// Check if there are eliminable non-base candidates in the target cells
    [[nodiscard]] static std::optional<SolveStep> checkEliminations(const CandidateGrid& candidates,
                                                                    const Position& base1, const Position& base2,
                                                                    const Position& target1, const Position& target2,
                                                                    uint16_t base_mask) {
        std::vector<Elimination> eliminations;

        // Eliminate non-base candidates from T1
        uint16_t t1_mask = candidates.getPossibleValuesMask(target1.row, target1.col);
        uint16_t t1_non_base = t1_mask & ~base_mask;
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            if ((t1_non_base & valueToBit(value)) != 0) {
                eliminations.push_back(Elimination{.position = target1, .value = value});
            }
        }

        // Eliminate non-base candidates from T2
        uint16_t t2_mask = candidates.getPossibleValuesMask(target2.row, target2.col);
        uint16_t t2_non_base = t2_mask & ~base_mask;
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            if ((t2_non_base & valueToBit(value)) != 0) {
                eliminations.push_back(Elimination{.position = target2, .value = value});
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        // Build base candidate list for explanation
        std::vector<int> base_values;
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((base_mask & valueToBit(v)) != 0) {
                base_values.push_back(v);
            }
        }

        // Build explanation
        auto explanation = fmt::format("Junior Exocet: base cells {} and {} with candidates {{{}}} "
                                       "— targets {} and {} can only contain base candidates "
                                       "— eliminates non-base candidates",
                                       formatPosition(base1), formatPosition(base2), fmt::join(base_values, ","),
                                       formatPosition(target1), formatPosition(target2));

        ExplanationData data;
        data.positions = {base1, base2, target1, target2};
        data.position_roles = {CellRole::Pattern, CellRole::Pattern, CellRole::Fin, CellRole::Fin};
        for (int v : base_values) {
            data.values.push_back(v);
        }

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::JuniorExocet,
                         .position = base1,
                         .value = 0,
                         .eliminations = std::move(eliminations),
                         .explanation = std::move(explanation),
                         .rating = getTechniqueRating(SolvingTechnique::JuniorExocet),
                         .explanation_data = std::move(data)};
    }
};

}  // namespace sudoku::core
