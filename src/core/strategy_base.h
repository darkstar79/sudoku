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

#include "candidate_grid.h"
#include "i_game_validator.h"
#include "solve_step.h"

#include <algorithm>
#include <string>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Base class providing shared helper functions for solving strategies
/// Reduces code duplication across strategy implementations
class StrategyBase {
protected:
    /// Extracts candidate list from candidate grid bitmask
    /// @param candidates Candidate grid with candidate tracking
    /// @param row Cell row (0-8)
    /// @param col Cell column (0-8)
    /// @return Vector of possible values (1-9)
    [[nodiscard]] static std::vector<int> getCandidates(const CandidateGrid& candidates, size_t row, size_t col) {
        std::vector<int> result;
        uint16_t mask = candidates.getPossibleValuesMask(row, col);

        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            if ((mask & valueToBit(value)) != 0) {
                result.push_back(value);
            }
        }

        return result;
    }

    /// Checks if cell has exactly N candidates
    /// @param candidates Candidate grid
    /// @param row Cell row (0-8)
    /// @param col Cell column (0-8)
    /// @param n Expected candidate count
    /// @return true if cell has exactly n candidates
    [[nodiscard]] static bool hasNCandidates(const CandidateGrid& candidates, size_t row, size_t col, int n) {
        return candidates.countPossibleValues(row, col) == n;
    }

    /// Formats position as "R{row}C{col}" (1-indexed)
    /// @param pos Cell position (0-indexed)
    /// @return Formatted string (e.g., "R3C5")
    [[nodiscard]] static std::string formatPosition(const Position& pos) {
        return fmt::format("R{}C{}", pos.row + 1, pos.col + 1);
    }

    /// Formats region name (Row, Column, or Box)
    /// @param type Region type
    /// @param idx Region index (0-8, 1-indexed in output)
    /// @return Formatted string (e.g., "Row 3", "Column 5", "Box 2")
    [[nodiscard]] static std::string formatRegion(RegionType type, size_t idx) {
        switch (type) {
            case RegionType::Row:
                return fmt::format("Row {}", idx + 1);
            case RegionType::Col:
                return fmt::format("Column {}", idx + 1);
            case RegionType::Box:
                return fmt::format("Box {}", idx + 1);
            default:
                return "Unknown Region";
        }
    }

    /// Gets all empty cells in a row
    /// @param board Current board state
    /// @param row Row index (0-8)
    /// @return Vector of positions of empty cells
    [[nodiscard]] static std::vector<Position> getEmptyCellsInRow(const BoardData& board, size_t row) {
        std::vector<Position> empty_cells;
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == EMPTY_CELL) {
                empty_cells.push_back(Position{.row = row, .col = col});
            }
        }
        return empty_cells;
    }

    /// Gets all empty cells in a column
    /// @param board Current board state
    /// @param col Column index (0-8)
    /// @return Vector of positions of empty cells
    [[nodiscard]] static std::vector<Position> getEmptyCellsInCol(const BoardData& board, size_t col) {
        std::vector<Position> empty_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            if (board[row][col] == EMPTY_CELL) {
                empty_cells.push_back(Position{.row = row, .col = col});
            }
        }
        return empty_cells;
    }

    /// Gets all empty cells in a 3x3 box
    /// @param board Current board state
    /// @param box_idx Box index (0-8, row-major order)
    /// @return Vector of positions of empty cells
    [[nodiscard]] static std::vector<Position> getEmptyCellsInBox(const BoardData& board, size_t box_idx) {
        std::vector<Position> empty_cells;
        size_t box_start_row = (box_idx / BOX_SIZE) * BOX_SIZE;
        size_t box_start_col = (box_idx % BOX_SIZE) * BOX_SIZE;

        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t row = box_start_row + br;
                size_t col = box_start_col + bc;
                if (board[row][col] == EMPTY_CELL) {
                    empty_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }
        return empty_cells;
    }

    // =========================================================================
    // Additional helpers for advanced strategies
    // =========================================================================

    /// Gets box index from row and column
    /// @param row Row index (0-8)
    /// @param col Column index (0-8)
    /// @return Box index (0-8) in row-major order
    [[nodiscard]] static size_t getBoxIndex(size_t row, size_t col) {
        return ((row / BOX_SIZE) * BOX_SIZE) + (col / BOX_SIZE);
    }

    /// Checks if two positions are in the same box
    [[nodiscard]] static bool sameBox(const Position& pos_a, const Position& pos_b) {
        return getBoxIndex(pos_a.row, pos_a.col) == getBoxIndex(pos_b.row, pos_b.col);
    }

    /// Checks if a specific value is a candidate in a cell
    [[nodiscard]] static bool hasCandidate(const CandidateGrid& candidates, size_t row, size_t col, int value) {
        return candidates.isAllowed(row, col, value);
    }

    /// Gets all empty cells in a region where a specific value is a candidate
    [[nodiscard]] static std::vector<Position>
    getCellsWithCandidate(const CandidateGrid& candidates, const std::vector<Position>& region_cells, int value) {
        std::vector<Position> result;
        for (const auto& pos : region_cells) {
            if (candidates.isAllowed(pos.row, pos.col, value)) {
                result.push_back(pos);
            }
        }
        return result;
    }

    /// Checks if one position sees another (same row, column, or box)
    [[nodiscard]] static bool sees(const Position& pos_a, const Position& pos_b) {
        return pos_a.row == pos_b.row || pos_a.col == pos_b.col || sameBox(pos_a, pos_b);
    }

    /// Counts the number of unique boxes spanned by 4 cells
    [[nodiscard]] static size_t countUniqueBoxes(const Position& c1, const Position& c2, const Position& c3,
                                                 const Position& c4) {
        std::vector<size_t> boxes = {getBoxIndex(c1.row, c1.col), getBoxIndex(c2.row, c2.col),
                                     getBoxIndex(c3.row, c3.col), getBoxIndex(c4.row, c4.col)};
        std::ranges::sort(boxes);
        auto last = std::ranges::unique(boxes);
        return static_cast<size_t>(std::ranges::distance(boxes.begin(), last.begin()));
    }

    /// Gets the unit index for a position given a region type
    [[nodiscard]] static size_t getUnitIndex(RegionType type, const Position& pos) {
        switch (type) {
            case RegionType::Row:
                return pos.row;
            case RegionType::Col:
                return pos.col;
            case RegionType::Box:
                return getBoxIndex(pos.row, pos.col);
            default:
                return 0;
        }
    }

    /// Gets all empty cells in a unit (row, column, or box)
    [[nodiscard]] static std::vector<Position> getEmptyCellsInUnit(const BoardData& board, RegionType type,
                                                                   size_t index) {
        switch (type) {
            case RegionType::Row:
                return getEmptyCellsInRow(board, index);
            case RegionType::Col:
                return getEmptyCellsInCol(board, index);
            case RegionType::Box:
                return getEmptyCellsInBox(board, index);
            default:
                return {};
        }
    }
};

}  // namespace sudoku::core
