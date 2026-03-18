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

#include <algorithm>
#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Sue de Coq (Two-Sector Disjoint Subset) patterns.
/// On a 2-cell box-line intersection with 3+ candidates, partitions the candidates
/// into a line-set and a box-set such that each is "covered" by an ALS in the
/// corresponding remainder region. Then eliminates line-set values from line remainder
/// and box-set values from box remainder.
class SueDeCoqStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Check each box/line intersection
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            // Try row intersections
            for (size_t band_row = 0; band_row < BOX_SIZE; ++band_row) {
                size_t row = ((box / BOX_SIZE) * BOX_SIZE) + band_row;
                auto result = tryIntersection(board, candidates, box, row, true);
                if (result.has_value()) {
                    return result;
                }
            }
            // Try column intersections
            for (size_t band_col = 0; band_col < BOX_SIZE; ++band_col) {
                size_t col = ((box % BOX_SIZE) * BOX_SIZE) + band_col;
                auto result = tryIntersection(board, candidates, box, col, false);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::SueDeCoq;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Sue de Coq";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::SueDeCoq);
    }

private:
    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — partitions intersection candidates into ALS pairs; nesting is inherent to Sue de Coq
    tryIntersection(const BoardData& board, const CandidateGrid& candidates, size_t box, size_t line_idx, bool is_row) {
        // Find empty cells in the intersection
        std::vector<Position> inter_cells;
        size_t box_start_row = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_start_col = (box % BOX_SIZE) * BOX_SIZE;

        if (is_row) {
            for (size_t col = box_start_col; col < box_start_col + BOX_SIZE; ++col) {
                if (board[line_idx][col] == EMPTY_CELL) {
                    inter_cells.push_back(Position{.row = line_idx, .col = col});
                }
            }
        } else {
            for (size_t row = box_start_row; row < box_start_row + BOX_SIZE; ++row) {
                if (board[row][line_idx] == EMPTY_CELL) {
                    inter_cells.push_back(Position{.row = row, .col = line_idx});
                }
            }
        }

        if (inter_cells.size() != 2) {
            return std::nullopt;
        }

        uint16_t inter_mask = candidates.getPossibleValuesMask(inter_cells[0].row, inter_cells[0].col) |
                              candidates.getPossibleValuesMask(inter_cells[1].row, inter_cells[1].col);
        int inter_count = std::popcount(inter_mask);
        if (inter_count < 3) {
            return std::nullopt;
        }

        // Get line remainder and box remainder
        std::vector<Position> line_remainder;
        std::vector<Position> box_remainder;

        if (is_row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (col >= box_start_col && col < box_start_col + BOX_SIZE) {
                    continue;
                }
                if (board[line_idx][col] == EMPTY_CELL) {
                    line_remainder.push_back(Position{.row = line_idx, .col = col});
                }
            }
            for (size_t row = box_start_row; row < box_start_row + BOX_SIZE; ++row) {
                if (row == line_idx) {
                    continue;
                }
                for (size_t col = box_start_col; col < box_start_col + BOX_SIZE; ++col) {
                    if (board[row][col] == EMPTY_CELL) {
                        box_remainder.push_back(Position{.row = row, .col = col});
                    }
                }
            }
        } else {
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (row >= box_start_row && row < box_start_row + BOX_SIZE) {
                    continue;
                }
                if (board[row][line_idx] == EMPTY_CELL) {
                    line_remainder.push_back(Position{.row = row, .col = line_idx});
                }
            }
            for (size_t row = box_start_row; row < box_start_row + BOX_SIZE; ++row) {
                for (size_t col = box_start_col; col < box_start_col + BOX_SIZE; ++col) {
                    if (col == line_idx) {
                        continue;
                    }
                    if (board[row][col] == EMPTY_CELL) {
                        box_remainder.push_back(Position{.row = row, .col = col});
                    }
                }
            }
        }

        // Try all non-empty proper subsets of inter_mask as line_set
        int num_values = std::popcount(inter_mask);

        // Extract values
        std::vector<int> values;
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((inter_mask & valueToBit(v)) != 0) {
                values.push_back(v);
            }
        }

        // Enumerate subsets using bitmask over values array
        int total_subsets = (1 << num_values) - 1;  // Skip empty set
        for (int s = 1; s < total_subsets; ++s) {
            uint16_t line_set = 0;
            uint16_t box_set = 0;
            std::vector<int> line_values;
            std::vector<int> box_values;

            for (int bit = 0; bit < num_values; ++bit) {
                if (s & (1 << bit)) {
                    line_set |= valueToBit(values[bit]);
                    line_values.push_back(values[bit]);
                } else {
                    box_set |= valueToBit(values[bit]);
                    box_values.push_back(values[bit]);
                }
            }

            if (line_set == 0 || box_set == 0) {
                continue;
            }

            // line_set must be covered by an ALS in line_remainder
            // Extra candidates of the ALS must not overlap with inter_mask
            auto line_extra = findCoveringALS(candidates, line_remainder, line_set, inter_mask);
            if (!line_extra.has_value()) {
                continue;
            }
            // box_set must be covered by an ALS in box_remainder
            auto box_extra = findCoveringALS(candidates, box_remainder, box_set, inter_mask);
            if (!box_extra.has_value()) {
                continue;
            }
            // Extra candidates of the two ALSes must be mutually disjoint
            if ((line_extra->extra_mask & box_extra->extra_mask) != 0) {
                continue;
            }

            // Valid Sue de Coq: eliminate line_set from line_remainder (excluding line ALS cells),
            // and box_set from box_remainder (excluding box ALS cells)
            auto is_als_cell = [](const std::vector<Position>& als_cells, const Position& pos) {
                return std::ranges::any_of(als_cells, [&pos](const auto& ac) { return ac == pos; });
            };

            std::vector<Elimination> eliminations;
            for (const auto& pos : line_remainder) {
                if (is_als_cell(line_extra->als_cells, pos)) {
                    continue;
                }
                for (int v : line_values) {
                    if (candidates.isAllowed(pos.row, pos.col, v)) {
                        eliminations.push_back(Elimination{.position = pos, .value = v});
                    }
                }
            }
            for (const auto& pos : box_remainder) {
                if (is_als_cell(box_extra->als_cells, pos)) {
                    continue;
                }
                for (int v : box_values) {
                    if (candidates.isAllowed(pos.row, pos.col, v)) {
                        eliminations.push_back(Elimination{.position = pos, .value = v});
                    }
                }
            }

            if (eliminations.empty()) {
                continue;
            }

            auto explanation = fmt::format(
                "Sue de Coq: intersection of {}{} and Box {} — eliminates candidates from rest of line and box",
                is_row ? "Row " : "Column ", line_idx + 1, box + 1);

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::SueDeCoq,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::SueDeCoq),
                             .explanation_data = {.positions = inter_cells,
                                                  .values = {},
                                                  .region_type = is_row ? RegionType::Row : RegionType::Col,
                                                  .region_index = line_idx,
                                                  .secondary_region_type = RegionType::Box,
                                                  .secondary_region_index = box}};
        }

        return std::nullopt;
    }

    struct ALSResult {
        uint16_t extra_mask;              ///< Candidates beyond target set
        std::vector<Position> als_cells;  ///< Cells forming the ALS
    };

    /// Find N cells (N=1..3) in remainder whose combined candidates form a valid ALS
    /// covering target_mask. Returns the extra candidates and ALS cell positions on success.
    /// Constraint: extra candidates must NOT overlap with inter_mask.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — N=1..3 ALS search over remainder cells; nesting is inherent
    [[nodiscard]] static std::optional<ALSResult> findCoveringALS(const CandidateGrid& candidates,
                                                                  const std::vector<Position>& remainder,
                                                                  uint16_t target_mask, uint16_t inter_mask) {
        size_t cell_count = remainder.size();
        int target_count = std::popcount(target_mask);

        // N=1: single cell (ALS has 2 candidates) including all target values
        if (target_count <= 1) {
            for (size_t i = 0; i < cell_count; ++i) {
                uint16_t mask = candidates.getPossibleValuesMask(remainder[i].row, remainder[i].col);
                if ((mask & target_mask) != target_mask || std::popcount(mask) != 2) {
                    continue;
                }
                uint16_t extra = mask & ~target_mask;
                if ((extra & inter_mask) == 0) {
                    return ALSResult{.extra_mask = extra, .als_cells = {remainder[i]}};
                }
            }
        }

        // N=2: two cells (ALS has 3 candidates) including all target values
        if (target_count <= 2) {
            for (size_t i = 0; i < cell_count; ++i) {
                uint16_t mi = candidates.getPossibleValuesMask(remainder[i].row, remainder[i].col);
                for (size_t j = i + 1; j < cell_count; ++j) {
                    uint16_t mj = candidates.getPossibleValuesMask(remainder[j].row, remainder[j].col);
                    uint16_t union_mask = mi | mj;
                    if ((union_mask & target_mask) != target_mask || std::popcount(union_mask) != 3) {
                        continue;
                    }
                    uint16_t extra = union_mask & ~target_mask;
                    if ((extra & inter_mask) == 0) {
                        return ALSResult{.extra_mask = extra, .als_cells = {remainder[i], remainder[j]}};
                    }
                }
            }
        }

        // N=3: three cells (ALS has 4 candidates) including all target values
        if (target_count <= 3) {
            for (size_t i = 0; i < cell_count; ++i) {
                uint16_t mi = candidates.getPossibleValuesMask(remainder[i].row, remainder[i].col);
                for (size_t j = i + 1; j < cell_count; ++j) {
                    uint16_t mj = candidates.getPossibleValuesMask(remainder[j].row, remainder[j].col);
                    uint16_t mij = mi | mj;
                    if (std::popcount(mij) > 4) {
                        continue;
                    }
                    for (size_t k = j + 1; k < cell_count; ++k) {
                        uint16_t mk = candidates.getPossibleValuesMask(remainder[k].row, remainder[k].col);
                        uint16_t union_mask = mij | mk;
                        if ((union_mask & target_mask) != target_mask || std::popcount(union_mask) != 4) {
                            continue;
                        }
                        uint16_t extra = union_mask & ~target_mask;
                        if ((extra & inter_mask) == 0) {
                            return ALSResult{.extra_mask = extra,
                                             .als_cells = {remainder[i], remainder[j], remainder[k]}};
                        }
                    }
                }
            }
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
