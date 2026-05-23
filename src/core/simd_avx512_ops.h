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

/**
 * @file simd_avx512_ops.h
 * @brief AVX-512 optimized operations for Sudoku constraint state.
 *
 * Free functions that operate on SIMDConstraintState data using AVX-512
 * intrinsics. Compiled separately with -mavx512f -mavx512bw -mavx512bitalg
 * flags and called via runtime dispatch when AVX-512 is available.
 *
 * Key advantages over AVX2:
 * - 32 uint16_t per register (vs 16): findMRVCell in 3 iterations (vs 6)
 * - Native _mm512_popcnt_epi16 via AVX-512 BITALG (no lookup table needed)
 * - Mask registers (__mmask32) for branchless comparisons
 */

#include "core/board.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace sudoku::core {

inline constexpr size_t SIMD_PADDED_CELLS_512 = 96;  // Same layout as AVX2

namespace avx512 {

/// Find the cell with the minimum remaining values (MRV heuristic).
/// Processes 32 cells per iteration (3 iterations for 96 cells).
/// @param candidates Padded candidate array (96 uint16_t, 32-byte aligned)
/// @return Cell index with minimum candidates > 1, or -1 if all solved
[[nodiscard]] int findMRVCell(const std::array<uint16_t, SIMD_PADDED_CELLS_512>& candidates);

/// Find a naked single (cell with exactly 1 candidate that is empty on board).
/// Processes 32 cells per iteration using mask registers.
/// @param candidates Padded candidate array (96 uint16_t, 32-byte aligned)
/// @param board Current board state
/// @return Cell index with naked single, or -1 if none found
[[nodiscard]] int findNakedSingle(const std::array<uint16_t, SIMD_PADDED_CELLS_512>& candidates, const Board& board);

/// Find a hidden single by processing all 27 regions (9 rows + 9 cols + 9 boxes) in parallel.
/// Uses AVX-512 to perform the bit-reduction across all regions simultaneously in 9 SIMD steps.
/// @param candidates Padded candidate array (96 uint16_t, 64-byte aligned)
/// @param board Current board state (0 = empty)
/// @param row_used Values placed in each row (bitmask, bits 0-8 for digits 1-9)
/// @param col_used Values placed in each column
/// @param box_used Values placed in each box
/// @return Pair of (cell_index, digit) if hidden single found, or (-1, 0) if none
[[nodiscard]] std::pair<int, int> findHiddenSingle(const std::array<uint16_t, SIMD_PADDED_CELLS_512>& candidates,
                                                   const Board& board, const std::array<uint16_t, 9>& row_used,
                                                   const std::array<uint16_t, 9>& col_used,
                                                   const std::array<uint16_t, 9>& box_used);

}  // namespace avx512
}  // namespace sudoku::core
