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

#include "constraint_state.h"

#include "core/board_utils.h"
#include "i_game_validator.h"

#include <bit>

namespace sudoku::core {

ConstraintState::ConstraintState(const BoardData& board) : row_used_{}, col_used_{}, box_used_{} {
    // Build constraint bitmasks from current board state
    forEachCell([&](size_t row, size_t col) {
        int value = board[row][col];
        if (value != 0) {
            placeValue(row, col, value);
        }
    });
}

ConstraintState::ConstraintState(const Board& board) : row_used_{}, col_used_{}, box_used_{} {
    for (size_t i = 0; i < TOTAL_CELLS; ++i) {
        int value = static_cast<unsigned char>(board.cells[i]);
        if (value != 0) {
            placeValue(i / BOARD_SIZE, i % BOARD_SIZE, value);
        }
    }
}

bool ConstraintState::isAllowed(size_t row, size_t col, int value) const {
    auto mask = valueToBit(value);

    // Value is allowed if it's not used in row, column, or box
    size_t box = getBoxIndex(row, col);
    return (row_used_[row] & mask) == 0 && (col_used_[col] & mask) == 0 && (box_used_[box] & mask) == 0;
}

void ConstraintState::placeValue(size_t row, size_t col, int value) {
    auto mask = valueToBit(value);
    size_t box = getBoxIndex(row, col);

    row_used_[row] |= mask;
    col_used_[col] |= mask;
    box_used_[box] |= mask;
}

void ConstraintState::removeValue(size_t row, size_t col, int value) {
    auto mask = valueToBit(value);
    size_t box = getBoxIndex(row, col);

    row_used_[row] &= ~mask;
    col_used_[col] &= ~mask;
    box_used_[box] &= ~mask;
}

uint16_t ConstraintState::getPossibleValuesMask(size_t row, size_t col) const {
    size_t box = getBoxIndex(row, col);

    // Combine constraints from row, column, and box
    uint16_t used_mask = row_used_[row] | col_used_[col] | box_used_[box];

    // Possible values are those not used (bits 1-9, bit 0 unused)
    // Invert and mask to get only bits 1-9
    uint16_t possible_mask = (~used_mask) & 0x03FE;  // 0b0000001111111110

    return possible_mask;
}

int ConstraintState::countPossibleValues(size_t row, size_t col) const {
    uint16_t possible_mask = getPossibleValuesMask(row, col);

    // Count set bits using C++20 std::popcount
    // This compiles to a single POPCNT instruction on modern CPUs
    return std::popcount(possible_mask);
}

std::optional<std::pair<size_t, int>> ConstraintState::findHiddenSingleInRow(size_t row, const BoardData& board) const {
    // Delegate to template-based implementation
    auto result = findHiddenSingleInRegion<Region::Row>(row, board);
    if (!result.has_value()) {
        return std::nullopt;
    }
    // Extract column from Position
    return std::make_pair(result->first.col, result->second);
}

std::optional<std::pair<size_t, int>> ConstraintState::findHiddenSingleInCol(size_t col, const BoardData& board) const {
    // Delegate to template-based implementation
    auto result = findHiddenSingleInRegion<Region::Col>(col, board);
    if (!result.has_value()) {
        return std::nullopt;
    }
    // Extract row from Position
    return std::make_pair(result->first.row, result->second);
}

std::optional<std::pair<Position, int>> ConstraintState::findHiddenSingleInBox(size_t box,
                                                                               const BoardData& board) const {
    // Delegate to template-based implementation (Box already returns Position)
    return findHiddenSingleInRegion<Region::Box>(box, board);
}

std::optional<std::pair<Position, int>> ConstraintState::findHiddenSingle(const BoardData& board) const {
    // Check all rows first
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        auto result = findHiddenSingleInRow(row, board);
        if (result.has_value()) {
            auto [col, value] = result.value();
            return std::make_pair(Position{.row = row, .col = col}, value);
        }
    }

    // Check all columns
    for (size_t col = 0; col < BOARD_SIZE; ++col) {
        auto result = findHiddenSingleInCol(col, board);
        if (result.has_value()) {
            auto [row, value] = result.value();
            return std::make_pair(Position{.row = row, .col = col}, value);
        }
    }

    // Check all boxes
    for (size_t box = 0; box < BOARD_SIZE; ++box) {
        auto result = findHiddenSingleInBox(box, board);
        if (result.has_value()) {
            return result;
        }
    }

    return std::nullopt;
}

std::optional<std::pair<Position, int>> ConstraintState::findHiddenSingle(const Board& board) const {
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        auto result = findHiddenSingleInRegion<Region::Row>(row, board);
        if (result.has_value()) {
            return result;
        }
    }
    for (size_t col = 0; col < BOARD_SIZE; ++col) {
        auto result = findHiddenSingleInRegion<Region::Col>(col, board);
        if (result.has_value()) {
            return result;
        }
    }
    for (size_t box = 0; box < BOARD_SIZE; ++box) {
        auto result = findHiddenSingleInRegion<Region::Box>(box, board);
        if (result.has_value()) {
            return result;
        }
    }
    return std::nullopt;
}

// Template-based hidden single search (consolidates Row/Col/Box logic)
// BoardT can be BoardData or Board — both support board[row][col]
template <ConstraintState::Region R, typename BoardT>
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — if constexpr branches are compile-time selection, not runtime branching; clang-tidy over-counts them
std::optional<std::pair<Position, int>> ConstraintState::findHiddenSingleInRegion(size_t region_index,
                                                                                  const BoardT& board) const {
    // For each value 1-9, check if it can only go in one cell within this region
    for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
        // Skip if value already placed in this region
        auto mask = valueToBit(value);

        bool value_used = false;
        if constexpr (R == Region::Row) {
            value_used = (row_used_[region_index] & mask) != 0;
        } else if constexpr (R == Region::Col) {
            value_used = (col_used_[region_index] & mask) != 0;
        } else {  // Region::Box
            value_used = (box_used_[region_index] & mask) != 0;
        }

        if (value_used) {
            continue;
        }

        // Count how many empty cells in this region can hold this value
        Position candidate_pos{.row = BOARD_SIZE, .col = BOARD_SIZE};  // Invalid initial value
        int count = 0;

        if constexpr (R == Region::Row) {
            // Search across columns in this row
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[region_index][col] == 0 && isAllowed(region_index, col, value)) {
                    count++;
                    candidate_pos = Position{.row = region_index, .col = col};
                    if (count > 1) {
                        break;  // Not a hidden single
                    }
                }
            }
        } else if constexpr (R == Region::Col) {
            // Search across rows in this column
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (board[row][region_index] == 0 && isAllowed(row, region_index, value)) {
                    count++;
                    candidate_pos = Position{.row = row, .col = region_index};
                    if (count > 1) {
                        break;  // Not a hidden single
                    }
                }
            }
        } else {  // Region::Box
            // Calculate box top-left corner
            size_t box_row = (region_index / BOX_SIZE) * BOX_SIZE;
            size_t box_col = (region_index % BOX_SIZE) * BOX_SIZE;

            // Search within the 3x3 box
            for (size_t box_r = 0; box_r < BOX_SIZE; ++box_r) {
                for (size_t box_c = 0; box_c < BOX_SIZE; ++box_c) {
                    size_t row = box_row + box_r;
                    size_t col = box_col + box_c;

                    if (board[row][col] == 0 && isAllowed(row, col, value)) {
                        count++;
                        candidate_pos = Position{.row = row, .col = col};
                        if (count > 1) {
                            break;  // Not a hidden single
                        }
                    }
                }
                if (count > 1) {
                    break;
                }
            }
        }

        // If exactly one cell can hold this value → hidden single!
        if (count == 1) {
            return std::make_pair(candidate_pos, value);
        }
    }

    return std::nullopt;
}

// Explicit template instantiations for vector<vector<int>>
template std::optional<std::pair<Position, int>>
ConstraintState::findHiddenSingleInRegion<ConstraintState::Region::Row, BoardData>(size_t, const BoardData&) const;
template std::optional<std::pair<Position, int>>
ConstraintState::findHiddenSingleInRegion<ConstraintState::Region::Col, BoardData>(size_t, const BoardData&) const;
template std::optional<std::pair<Position, int>>
ConstraintState::findHiddenSingleInRegion<ConstraintState::Region::Box, BoardData>(size_t, const BoardData&) const;

// Explicit template instantiations for Board
template std::optional<std::pair<Position, int>>
ConstraintState::findHiddenSingleInRegion<ConstraintState::Region::Row, Board>(size_t, const Board&) const;
template std::optional<std::pair<Position, int>>
ConstraintState::findHiddenSingleInRegion<ConstraintState::Region::Col, Board>(size_t, const Board&) const;
template std::optional<std::pair<Position, int>>
ConstraintState::findHiddenSingleInRegion<ConstraintState::Region::Box, Board>(size_t, const Board&) const;

}  // namespace sudoku::core
