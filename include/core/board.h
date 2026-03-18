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

#include "core/board_data.h"
#include "core/constants.h"

#include <array>
#include <cstdint>
#include <vector>

#include <stddef.h>

namespace sudoku::core {

/**
 * @brief Flat, SIMD-friendly board representation.
 *
 * Stores 81 Sudoku cells as a contiguous, 32-byte-aligned array of int8_t.
 * Values: 0 = empty, 1-9 = filled digit.
 *
 * Key properties:
 * - 96 bytes total (81 cells + 15 padding), fits in 2 cache lines
 * - alignas(32) enables aligned AVX2 loads/stores
 * - operator[] returns int8_t* so board[row][col] syntax works unchanged
 * - Padding cells (indices 81-95) are always zero
 */
struct Board {
    alignas(32) std::array<int8_t, PADDED_CELLS> cells{};

    /// Row accessor — enables board[row][col] syntax
    [[nodiscard]] int8_t* operator[](size_t row) {
        return &cells[row * BOARD_SIZE];
    }

    /// Row accessor (const) — enables board[row][col] syntax
    [[nodiscard]] const int8_t* operator[](size_t row) const {
        return &cells[row * BOARD_SIZE];
    }

    /// Flat data pointer for SIMD operations
    [[nodiscard]] int8_t* data() {
        return cells.data();
    }

    /// Flat data pointer for SIMD operations (const)
    [[nodiscard]] const int8_t* data() const {
        return cells.data();
    }

    /// Defaulted equality comparison
    bool operator==(const Board&) const = default;

    /// Convert from BoardData (int array) to Board (int8_t SIMD-aligned array)
    [[nodiscard]] static Board fromBoardData(const BoardData& src);

    /// Convert from Board (int8_t SIMD-aligned array) to BoardData (int array)
    [[nodiscard]] BoardData toBoardData() const;
};

}  // namespace sudoku::core
