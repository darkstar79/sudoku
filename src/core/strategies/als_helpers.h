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

#include "../candidate_grid.h"
#include "../i_game_validator.h"
#include "../strategy_base.h"

#include <vector>

#include <bit>

namespace sudoku::core {

/// Almost Locked Set: N cells in one house with N+1 candidates.
struct ALS {
    std::vector<Position> cells;
    uint16_t cand_mask;
};

/// Shared utility functions for ALS-based strategies (ALS-XZ, ALS-XY-Wing, Death Blossom).
/// Inherits StrategyBase for sees(), getEmptyCellsIn*(), etc.
class ALSHelpers : protected StrategyBase {
public:
    /// Enumerate all ALSs (2-cell and 3-cell) across all 27 regions.
    [[nodiscard]] static std::vector<ALS> enumerateALSs(const BoardData& board, const CandidateGrid& candidates) {
        std::vector<ALS> result;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            auto empty = getEmptyCellsInRow(board, row);
            enumerateInRegion(candidates, empty, result);
        }
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            auto empty = getEmptyCellsInCol(board, col);
            enumerateInRegion(candidates, empty, result);
        }
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            auto empty = getEmptyCellsInBox(board, box);
            enumerateInRegion(candidates, empty, result);
        }

        return result;
    }

    /// Enumerate ALSs within a single region's empty cells.
    static void enumerateInRegion(const CandidateGrid& candidates, const std::vector<Position>& empty_cells,
                                  std::vector<ALS>& out) {
        size_t n = empty_cells.size();

        // N=2: find 2-cell subsets with 3 candidates
        for (size_t i = 0; i < n; ++i) {
            uint16_t mi = candidates.getPossibleValuesMask(empty_cells[i].row, empty_cells[i].col);
            for (size_t j = i + 1; j < n; ++j) {
                uint16_t mj = candidates.getPossibleValuesMask(empty_cells[j].row, empty_cells[j].col);
                uint16_t union_mask = mi | mj;
                if (std::popcount(union_mask) == 3) {
                    out.push_back(ALS{.cells = {empty_cells[i], empty_cells[j]}, .cand_mask = union_mask});
                }
            }
        }

        // N=3: find 3-cell subsets with 4 candidates
        for (size_t i = 0; i < n; ++i) {
            uint16_t mi = candidates.getPossibleValuesMask(empty_cells[i].row, empty_cells[i].col);
            for (size_t j = i + 1; j < n; ++j) {
                uint16_t mj = candidates.getPossibleValuesMask(empty_cells[j].row, empty_cells[j].col);
                uint16_t mij = mi | mj;
                if (std::popcount(mij) > 4) {
                    continue;
                }
                for (size_t k = j + 1; k < n; ++k) {
                    uint16_t mk = candidates.getPossibleValuesMask(empty_cells[k].row, empty_cells[k].col);
                    uint16_t union_mask = mij | mk;
                    if (std::popcount(union_mask) == 4) {
                        out.push_back(
                            ALS{.cells = {empty_cells[i], empty_cells[j], empty_cells[k]}, .cand_mask = union_mask});
                    }
                }
            }
        }
    }

    /// Check if all cells in group_a see all cells in group_b.
    [[nodiscard]] static bool allSee(const std::vector<Position>& group_a, const std::vector<Position>& group_b) {
        for (const auto& a : group_a) {
            for (const auto& b : group_b) {
                if (!sees(a, b)) {
                    return false;
                }
            }
        }
        return true;
    }

    /// Check if two ALSs share any cell.
    [[nodiscard]] static bool sharesCells(const ALS& a, const ALS& b) {
        for (const auto& ca : a.cells) {
            for (const auto& cb : b.cells) {
                if (ca == cb) {
                    return true;
                }
            }
        }
        return false;
    }

    /// Get cells within an ALS that contain a specific candidate.
    [[nodiscard]] static std::vector<Position> cellsWithValue(const CandidateGrid& candidates, const ALS& als,
                                                              int value) {
        std::vector<Position> result;
        for (const auto& pos : als.cells) {
            if (candidates.isAllowed(pos.row, pos.col, value)) {
                result.push_back(pos);
            }
        }
        return result;
    }
};

}  // namespace sudoku::core
