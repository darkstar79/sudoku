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
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <bit>
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

    /// Resolves the region for which `(row, col)` is the *only* empty cell — i.e. a "Full House"
    /// (SE "Last value in block/row/col", rating 1.0). Region precedence is **box → row → col**, the
    /// deterministic order FullHouseStrategy reports. Returns `nullopt` when every region of the cell
    /// still has another empty (the cell is a genuine naked/hidden single, not a region-last cell).
    /// Single source of truth shared by FullHouseStrategy (for labelling) and the single-strategy guards.
    [[nodiscard]] static std::optional<std::pair<RegionType, size_t>> getRegionLastCell(const BoardData& board,
                                                                                        size_t row, size_t col) {
        if (board[row][col] != EMPTY_CELL) {
            return std::nullopt;  // Only an empty cell can be a region's last empty cell.
        }
        if (getEmptyCellsInBox(board, getBoxIndex(row, col)).size() == 1) {
            return std::make_pair(RegionType::Box, getBoxIndex(row, col));
        }
        if (getEmptyCellsInRow(board, row).size() == 1) {
            return std::make_pair(RegionType::Row, row);
        }
        if (getEmptyCellsInCol(board, col).size() == 1) {
            return std::make_pair(RegionType::Col, col);
        }
        return std::nullopt;
    }

    /// True iff `(row, col)` is the only empty cell in at least one of its regions (box, row, or col),
    /// i.e. it is a Full House. `NakedSingleStrategy`/`HiddenSingleStrategy` *defer* such cells (emit no
    /// step), so the FullHouse 1.0 label holds intrinsically — regardless of strategy registration order
    /// (story 0b.4d). A filled cell is never region-last (returns false).
    [[nodiscard]] static bool isRegionLastCell(const BoardData& board, size_t row, size_t col) {
        return getRegionLastCell(board, row, col).has_value();
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

    /// Gets all nine cell positions of a unit (geometric — independent of board contents).
    [[nodiscard]] static std::vector<Position> getUnitPositions(RegionType type, size_t index) {
        std::vector<Position> cells;
        cells.reserve(BOARD_SIZE);
        switch (type) {
            case RegionType::Row:
                for (size_t c = 0; c < BOARD_SIZE; ++c) {
                    cells.push_back(Position{.row = index, .col = c});
                }
                break;
            case RegionType::Col:
                for (size_t r = 0; r < BOARD_SIZE; ++r) {
                    cells.push_back(Position{.row = r, .col = index});
                }
                break;
            case RegionType::Box: {
                size_t box_start_row = (index / BOX_SIZE) * BOX_SIZE;
                size_t box_start_col = (index % BOX_SIZE) * BOX_SIZE;
                for (size_t br = 0; br < BOX_SIZE; ++br) {
                    for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                        cells.push_back(Position{.row = box_start_row + br, .col = box_start_col + bc});
                    }
                }
                break;
            }
            default:
                break;
        }
        return cells;
    }

    // =========================================================================
    // "Direct" detection (story 0b.4b) — does this step's own eliminations force a placement?
    //
    // SE rates the placement-forcing "Direct" form of Pointing/Claiming/Hidden-subset lower than the
    // elimination-only form. "Forces a placement" means the eliminations create a *single* — either a
    // naked single (a cell drops to one candidate) OR a hidden single (a digit drops to one position in
    // a unit). Both faces are required: Hidden Pair/Triple eliminations leave their host cells with ≥2
    // candidates, so only the hidden face can fire for them; and a Pointing/Claiming elimination can open
    // a hidden single without a naked one. Both detectors are pure arithmetic over the *pre-elimination*
    // grid — no copy, no re-solve. Pass the complete, unapplied elimination set.
    // =========================================================================

    /// True if removing `elims` leaves some affected cell with exactly one remaining candidate.
    [[nodiscard]] static bool createsNakedSingle(const CandidateGrid& candidates,
                                                 const std::vector<Elimination>& elims) {
        for (const auto& target : elims) {
            size_t row = target.position.row;
            size_t col = target.position.col;
            // Count DISTINCT still-allowed values eliminated from this cell. A bitmask dedups, so a
            // duplicated {cell,value} entry cannot inflate the count and falsely skip the == 1 test (P4).
            uint16_t removed_mask = 0;
            for (const auto& other : elims) {
                if (other.position.row == row && other.position.col == col &&
                    candidates.isAllowed(row, col, other.value)) {
                    removed_mask |= valueToBit(other.value);
                }
            }
            if (candidates.countPossibleValues(row, col) - std::popcount(removed_mask) == 1) {
                return true;
            }
        }
        return false;
    }

    /// True if removing `elims` leaves some digit with exactly one remaining position in a unit
    /// (a newly-created hidden single).
    [[nodiscard]] static bool createsHiddenSingle(const CandidateGrid& candidates,
                                                  const std::vector<Elimination>& elims) {
        auto eliminatedByStep = [&elims](size_t row, size_t col, int value) {
            return std::ranges::any_of(elims, [&](const Elimination& e) {
                return e.position.row == row && e.position.col == col && e.value == value;
            });
        };
        for (const auto& target : elims) {
            int digit = target.value;
            for (RegionType type : {RegionType::Row, RegionType::Col, RegionType::Box}) {
                size_t unit_index = getUnitIndex(type, target.position);
                int remaining = 0;
                for (const auto& pos : getUnitPositions(type, unit_index)) {
                    if (candidates.isAllowed(pos.row, pos.col, digit) && !eliminatedByStep(pos.row, pos.col, digit)) {
                        ++remaining;
                    }
                }
                if (remaining == 1) {
                    return true;
                }
            }
        }
        return false;
    }

    /// True if this step's eliminations force a placement (naked OR hidden single) — the SE "Direct"
    /// predicate. Drives the context-sensitive Direct rating (story 0b.4b).
    [[nodiscard]] static bool eliminationsForcePlacement(const CandidateGrid& candidates,
                                                         const std::vector<Elimination>& elims) {
        return createsNakedSingle(candidates, elims) || createsHiddenSingle(candidates, elims);
    }
};

}  // namespace sudoku::core
