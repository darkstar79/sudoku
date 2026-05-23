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

#include <cstddef>
#include <cstdint>

namespace sudoku::core {

/**
 * @brief Core Sudoku game constants.
 *
 * This file provides a single source of truth for all game dimension constants
 * used throughout the codebase.
 */

/// Size of the Sudoku board (9x9 grid)
inline constexpr size_t BOARD_SIZE = 9;

/// Size of each 3x3 box within the board
inline constexpr size_t BOX_SIZE = 3;

/// Minimum valid cell value (1-9 for Sudoku)
inline constexpr int MIN_VALUE = 1;

/// Maximum valid cell value (1-9 for Sudoku)
inline constexpr int MAX_VALUE = 9;

/// Empty cell value (0 represents an unfilled cell)
inline constexpr int EMPTY_CELL = 0;

/// Total number of cells in the board (9 * 9 = 81)
inline constexpr size_t TOTAL_CELLS = BOARD_SIZE * BOARD_SIZE;

/// Padded cell count for 32-byte AVX2 alignment (81 cells + 15 padding = 96)
inline constexpr size_t PADDED_CELLS = 96;

/// Number of difficulty levels (Easy, Medium, Hard, Expert, Master)
inline constexpr size_t DIFFICULTY_COUNT = 5;

/// Create bitmask for a single Sudoku value (1-9).
/// Bit N is set for value N; bit 0 is unused.
[[nodiscard]] constexpr uint16_t valueToBit(int value) {
    return static_cast<uint16_t>(1 << value);
}

}  // namespace sudoku::core
