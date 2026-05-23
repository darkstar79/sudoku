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

/**
 * @file simd_avx512_ops.cpp
 * @brief AVX-512 optimized operations for Sudoku constraint state.
 *
 * Compiled with -mavx512f -mavx512bw -mavx512bitalg flags.
 * These functions are called via runtime dispatch from SIMDConstraintState
 * methods when AVX-512 is available on the CPU.
 */

#include "simd_avx512_ops.h"

#include "core/constants.h"

#include <bit>
#include <immintrin.h>

namespace sudoku::core::avx512 {

int findMRVCell(const std::array<uint16_t, SIMD_PADDED_CELLS_512>& candidates) {
    int best_cell = -1;
    int best_count = 10;  // Max possible + 1

    // Process 32 cells at a time with AVX-512 (3 iterations for 96 cells)
    for (size_t i = 0; i < SIMD_PADDED_CELLS_512; i += 32) {
        // Load 32 candidate masks (512 bits)
        __m512i cands = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(&candidates[i]));

        // Native per-word popcount via AVX-512 BITALG
        __m512i counts = _mm512_popcnt_epi16(cands);

        // Extract counts for comparison
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays) — alignas required for _mm512_store_si512
        alignas(64) uint16_t count_arr[32];
        _mm512_store_si512(reinterpret_cast<__m512i*>(count_arr), counts);

        for (size_t j = 0; j < 32; ++j) {
            size_t cell = i + j;
            if (cell >= BOARD_SIZE * BOARD_SIZE) {
                break;  // Don't process padding cells
            }

            int count = static_cast<int>(count_arr[j]);

            // Skip solved cells (count <= 1) and find minimum
            if (count > 1 && count < best_count) {
                best_count = count;
                best_cell = static_cast<int>(cell);

                // Early exit if we found a cell with exactly 2 candidates
                if (best_count == 2) {
                    return best_cell;
                }
            }
        }
    }

    return best_cell;
}

int findNakedSingle(const std::array<uint16_t, SIMD_PADDED_CELLS_512>& candidates, const Board& board) {
    const __m512i zero = _mm512_setzero_si512();
    const __m512i ones = _mm512_set1_epi16(1);

    // Process 32 cells at a time
    for (size_t i = 0; i < SIMD_PADDED_CELLS_512; i += 32) {
        __m512i cands = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(&candidates[i]));

        // Check for power-of-2 (exactly one candidate): (cands & (cands - 1)) == 0
        __m512i cands_minus1 = _mm512_sub_epi16(cands, ones);
        __m512i not_power_of_2 = _mm512_and_si512(cands, cands_minus1);

        // Mask: cells where cands is power-of-2 AND non-zero
        __mmask32 single_mask = _mm512_cmpeq_epi16_mask(not_power_of_2, zero);
        __mmask32 nonzero_mask = _mm512_cmpneq_epi16_mask(cands, zero);
        __mmask32 naked_single_mask = single_mask & nonzero_mask;

        // Check board emptiness for matching cells
        while (naked_single_mask != 0) {
            int j = std::countr_zero(static_cast<unsigned>(naked_single_mask));
            size_t cell = i + static_cast<size_t>(j);
            if (cell < BOARD_SIZE * BOARD_SIZE && board.cells[cell] == 0) {
                return static_cast<int>(cell);
            }
            naked_single_mask &= naked_single_mask - 1;  // Clear lowest set bit
        }
    }

    return -1;
}

namespace {

/// Precomputed cell indices for each box position (box_index, internal_position) → cell_index.
/// Box b has top-left at row=(b/3)*3, col=(b%3)*3.
/// Position s maps to row offset s/3, col offset s%3 within the box.
[[nodiscard]] constexpr std::array<std::array<uint8_t, BOARD_SIZE>, BOARD_SIZE> generateBoxCellTable() {
    std::array<std::array<uint8_t, BOARD_SIZE>, BOARD_SIZE> table{};
    for (size_t b = 0; b < BOARD_SIZE; ++b) {
        size_t box_row_start = (b / BOX_SIZE) * BOX_SIZE;
        size_t box_col_start = (b % BOX_SIZE) * BOX_SIZE;
        for (size_t s = 0; s < BOARD_SIZE; ++s) {
            size_t row = box_row_start + (s / BOX_SIZE);
            size_t col = box_col_start + (s % BOX_SIZE);
            table[b][s] = static_cast<uint8_t>((row * BOARD_SIZE) + col);
        }
    }
    return table;
}

constexpr auto BOX_CELL_TABLE = generateBoxCellTable();

/// Scan a batch of 9 regions using scalar bit-reduction with branchless candidate masking.
/// Returns (cell_index, digit) on first hidden single found, or (-1, 0) if none.
///
/// Template parameter `GetCell` is a callable: (region_index, position) → cell_index.
/// Template parameter `GetUsed` is a callable: (region_index) → used_bitmask.
template <typename GetCell, typename GetUsed>
[[nodiscard]] std::pair<int, int> scanRegionBatch(const std::array<uint16_t, SIMD_PADDED_CELLS_512>& candidates,
                                                  const Board& board, GetCell get_cell, GetUsed get_used) {
    for (size_t region = 0; region < BOARD_SIZE; ++region) {
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;

        for (size_t pos = 0; pos < BOARD_SIZE; ++pos) {
            uint8_t cell_idx = get_cell(region, pos);
            // Branchless: mask is 0xFFFF when empty, 0x0000 when filled
            auto empty_mask = static_cast<uint16_t>(-(board.cells[cell_idx] == 0));
            uint16_t cand = candidates[cell_idx] & empty_mask;
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }

        uint16_t exactly_once = at_least_once & ~at_least_twice & ~get_used(region);
        if (exactly_once != 0) {
            int digit = std::countr_zero(static_cast<unsigned>(exactly_once)) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t pos = 0; pos < BOARD_SIZE; ++pos) {
                uint8_t cell_idx = get_cell(region, pos);
                if (board.cells[cell_idx] == 0 && (candidates[cell_idx] & digit_mask) != 0) {
                    return {static_cast<int>(cell_idx), digit};
                }
            }
        }
    }
    return {-1, 0};
}

}  // anonymous namespace

std::pair<int, int> findHiddenSingle(const std::array<uint16_t, SIMD_PADDED_CELLS_512>& candidates, const Board& board,
                                     const std::array<uint16_t, 9>& row_used, const std::array<uint16_t, 9>& col_used,
                                     const std::array<uint16_t, 9>& box_used) {
    // Rows: row r, position s → cell r*9+s (contiguous access)
    auto result = scanRegionBatch(
        candidates, board,
        [](size_t region, size_t pos) -> uint8_t { return static_cast<uint8_t>((region * BOARD_SIZE) + pos); },
        [&row_used](size_t region) -> uint16_t { return row_used[region]; });
    if (result.first >= 0) {
        return result;
    }

    // Columns: col c, position s → cell s*9+c (stride-9 access)
    result = scanRegionBatch(
        candidates, board,
        [](size_t region, size_t pos) -> uint8_t { return static_cast<uint8_t>((pos * BOARD_SIZE) + region); },
        [&col_used](size_t region) -> uint16_t { return col_used[region]; });
    if (result.first >= 0) {
        return result;
    }

    // Boxes: precomputed cell indices
    return scanRegionBatch(
        candidates, board, [](size_t region, size_t pos) -> uint8_t { return BOX_CELL_TABLE[region][pos]; },
        [&box_used](size_t region) -> uint16_t { return box_used[region]; });
}

}  // namespace sudoku::core::avx512
