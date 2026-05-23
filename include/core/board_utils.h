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

#include "core/board_data.h"
#include "core/constants.h"

#include <functional>
#include <vector>

namespace sudoku::core {

/**
 * @brief Board iteration utilities to eliminate duplicated nested loop patterns.
 *
 * These template functions provide a DRY-compliant way to iterate over Sudoku
 * board cells, replacing 18+ instances of duplicated nested loops throughout
 * the codebase.
 */

/**
 * @brief Iterate over all cells in the board.
 *
 * @tparam Func Callable type with signature: void(size_t row, size_t col)
 * @param func Function to call for each cell
 *
 * @example
 * forEachCell([&](size_t row, size_t col) {
 *     board[row][col] = 0;
 * });
 */
template <typename Func>
void forEachCell(Func&& func) {
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            func(row, col);
        }
    }
}

/**
 * @brief Iterate over all cells in a 3x3 box.
 *
 * @tparam Func Callable type with signature: void(size_t row, size_t col)
 * @param box_start_row Starting row of the box (must be 0, 3, or 6)
 * @param box_start_col Starting column of the box (must be 0, 3, or 6)
 * @param func Function to call for each cell in the box
 *
 * @example
 * size_t box_row = (pos.row / BOX_SIZE) * BOX_SIZE;
 * size_t box_col = (pos.col / BOX_SIZE) * BOX_SIZE;
 * forEachBoxCell(box_row, box_col, [&](size_t row, size_t col) {
 *     if (board[row][col] == value) found = true;
 * });
 */
template <typename Func>
void forEachBoxCell(size_t box_start_row, size_t box_start_col, Func&& func) {
    for (size_t row = box_start_row; row < box_start_row + BOX_SIZE; ++row) {
        for (size_t col = box_start_col; col < box_start_col + BOX_SIZE; ++col) {
            func(row, col);
        }
    }
}

/**
 * @brief Iterate over cells that satisfy a predicate.
 *
 * @tparam Predicate Callable type with signature: bool(int cell_value)
 * @tparam Func Callable type with signature: void(size_t row, size_t col)
 * @param board The game board to iterate over
 * @param pred Predicate to test each cell value
 * @param func Function to call for cells where predicate returns true
 *
 * @example
 * // Find all empty cells
 * forEachCellIf(board, [](int val) { return val == 0; },
 *     [&](size_t row, size_t col) {
 *         empty_cells.push_back({row, col});
 *     });
 */
template <typename Predicate, typename Func>
void forEachCellIf(const BoardData& board, Predicate&& pred, Func&& func) {
    forEachCell([&](size_t row, size_t col) {
        if (pred(board[row][col])) {
            func(row, col);
        }
    });
}

/**
 * @brief Check if any cell in the board satisfies a condition.
 *
 * @tparam Predicate Callable type with signature: bool(size_t row, size_t col)
 * @param pred Predicate to test for each cell
 * @return true if any cell satisfies the predicate, false otherwise
 *
 * @example
 * bool has_empty = anyCell([&](size_t row, size_t col) {
 *     return board[row][col] == 0;
 * });
 */
template <typename Predicate>
bool anyCell(Predicate&& pred) {
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (pred(row, col)) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Check if all cells in the board satisfy a condition.
 *
 * @tparam Predicate Callable type with signature: bool(size_t row, size_t col)
 * @param pred Predicate to test for each cell
 * @return true if all cells satisfy the predicate, false otherwise
 *
 * @example
 * bool all_filled = allCells([&](size_t row, size_t col) {
 *     return board[row][col] != 0;
 * });
 */
template <typename Predicate>
bool allCells(Predicate&& pred) {
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (!pred(row, col)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace sudoku::core
