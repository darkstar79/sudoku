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

#include <array>
#include <vector>

namespace sudoku::core {

/// Shared utility functions for coloring-based strategies (Simple Coloring, Multi-Coloring, X-Cycles).
/// Inherits StrategyBase for cellIndex(), sees(), etc.
class ColoringHelpers : protected StrategyBase {
public:
    /// Flat index for a 9x9 grid
    [[nodiscard]] static size_t cellIndex(size_t row, size_t col) {
        return (row * BOARD_SIZE) + col;
    }

    /// Build adjacency list of conjugate pairs for a single value.
    /// Two cells are linked if they are the only two candidates for `value` in a row, column, or box.
    [[nodiscard]] static std::array<std::vector<size_t>, TOTAL_CELLS>
    buildConjugatePairGraph(const std::vector<std::vector<int>>& board, const CandidateGrid& candidates, int value) {
        std::array<std::vector<size_t>, TOTAL_CELLS> adj{};

        auto hasCandidate = [&board, &candidates, value](size_t r, size_t c) {
            return board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, value);
        };

        scanRows(adj, hasCandidate);
        scanColumns(adj, hasCandidate);
        scanBoxes(adj, hasCandidate);

        return adj;
    }

private:
    /// Link two cells as conjugate pair neighbours
    static void linkIfConjugate(std::array<std::vector<size_t>, TOTAL_CELLS>& adj, const std::vector<size_t>& cells) {
        if (cells.size() == 2) {
            adj[cells[0]].push_back(cells[1]);
            adj[cells[1]].push_back(cells[0]);
        }
    }

    /// Scan all rows for conjugate pairs of a candidate value
    template <typename Pred>
    static void scanRows(std::array<std::vector<size_t>, TOTAL_CELLS>& adj, const Pred& hasCandidate) {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            std::vector<size_t> cells;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (hasCandidate(row, col)) {
                    cells.push_back(cellIndex(row, col));
                }
            }
            linkIfConjugate(adj, cells);
        }
    }

    /// Scan all columns for conjugate pairs of a candidate value
    template <typename Pred>
    static void scanColumns(std::array<std::vector<size_t>, TOTAL_CELLS>& adj, const Pred& hasCandidate) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            std::vector<size_t> cells;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (hasCandidate(row, col)) {
                    cells.push_back(cellIndex(row, col));
                }
            }
            linkIfConjugate(adj, cells);
        }
    }

    /// Scan all boxes for conjugate pairs of a candidate value
    template <typename Pred>
    static void scanBoxes(std::array<std::vector<size_t>, TOTAL_CELLS>& adj, const Pred& hasCandidate) {
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            size_t start_row = (box / BOX_SIZE) * BOX_SIZE;
            size_t start_col = (box % BOX_SIZE) * BOX_SIZE;
            std::vector<size_t> cells;
            for (size_t r = start_row; r < start_row + BOX_SIZE; ++r) {
                for (size_t c = start_col; c < start_col + BOX_SIZE; ++c) {
                    if (hasCandidate(r, c)) {
                        cells.push_back(cellIndex(r, c));
                    }
                }
            }
            linkIfConjugate(adj, cells);
        }
    }
};

}  // namespace sudoku::core
