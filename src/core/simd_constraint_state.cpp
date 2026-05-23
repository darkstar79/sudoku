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
 * @file simd_constraint_state.cpp
 * @brief AVX2-optimized constraint state implementation.
 */

#include "simd_constraint_state.h"

#include "core/board.h"
#include "core/simd_avx512_ops.h"

#include <bit>  // std::popcount (C++20)

#ifdef _MSC_VER
#    include <intrin.h>
#else
#    include <immintrin.h>
#endif

namespace sudoku::core {

namespace {

/// Precomputed peer elimination masks: for each cell, a 96-element bitmask where
/// peer cells = 0x0000 and non-peer cells = 0xFFFF. Used for bulk SIMD AND elimination.
/// Layout: PEER_ELIM_MASKS[cell][i] = 0xFFFF if cell i is NOT a peer, 0x0000 if it IS a peer.
/// This allows: candidates[i] &= (mask[i] | ~digit_mask) to eliminate digit from peers only.
struct alignas(SIMD_ALIGNMENT) PeerElimMask {
    std::array<uint16_t, SIMD_PADDED_CELLS> mask;
};

[[nodiscard]] constexpr std::array<PeerElimMask, BOARD_SIZE * BOARD_SIZE> generatePeerElimMasks() {
    auto peer_table = generatePeerTable();
    std::array<PeerElimMask, BOARD_SIZE * BOARD_SIZE> masks{};

    for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
        // Start with all 0xFFFF (non-peer = preserve all candidates)
        for (size_t i = 0; i < SIMD_PADDED_CELLS; ++i) {
            masks[cell].mask[i] = 0xFFFF;
        }
        // Set peer cells to 0x0000 (peer = eligible for elimination)
        for (size_t p = 0; p < 20; ++p) {
            masks[cell].mask[peer_table[cell].peers[p]] = 0x0000;
        }
    }
    return masks;
}

/// Compile-time peer elimination mask table (81 × 192 bytes = 15.2 KB)
inline constexpr auto PEER_ELIM_MASKS = generatePeerElimMasks();

/// Lookup table for 4-bit popcount (replicated for both 128-bit lanes)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays) — alignas required for _mm256_load_si256
alignas(SIMD_ALIGNMENT) constexpr uint8_t POPCOUNT_LOOKUP[32] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,  // Lane 0
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4   // Lane 1
};

/**
 * @brief Vectorized population count for 16-bit elements using AVX2.
 *
 * Uses lookup table approach since AVX2 doesn't have native VPOPCNTDQ.
 * Processes 16 elements (256 bits) in parallel.
 *
 * @param v Vector of 16 x uint16_t values
 * @return Vector with popcount of each element in low byte (high byte = 0)
 */
inline __m256i popcnt_epi16(__m256i v) {
    // Load lookup table (already 32-byte aligned)
    const __m256i lookup = _mm256_load_si256(reinterpret_cast<const __m256i*>(POPCOUNT_LOOKUP));

    // Mask for low 4 bits
    const __m256i mask4 = _mm256_set1_epi8(0x0F);

    // Split each byte into nibbles and look up popcount
    __m256i lo = _mm256_shuffle_epi8(lookup, _mm256_and_si256(v, mask4));
    __m256i hi = _mm256_shuffle_epi8(lookup, _mm256_and_si256(_mm256_srli_epi16(v, 4), mask4));

    // Sum nibble counts within each byte
    __m256i sum = _mm256_add_epi8(lo, hi);

    // Sum bytes within each 16-bit element
    // For 9-bit candidates (max popcount = 9), high byte is always 0
    // So we just need: low_byte + (high_byte >> 8)
    __m256i byte_sum = _mm256_add_epi8(sum, _mm256_srli_epi16(sum, 8));

    // Mask to keep only low byte of each 16-bit element
    return _mm256_and_si256(byte_sum, _mm256_set1_epi16(0x00FF));
}

}  // anonymous namespace

void SIMDConstraintState::initFromBoard(const BoardData& board) {
    // Reset all cells to all candidates
    for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
        candidates[i] = ALL_CANDIDATES_MASK;
    }
    // Clear padding
    for (size_t i = BOARD_SIZE * BOARD_SIZE; i < SIMD_PADDED_CELLS; ++i) {
        candidates[i] = 0;
    }

    // Reset region usage tracking
    for (size_t i = 0; i < BOARD_SIZE; ++i) {
        row_used_[i] = 0;
        col_used_[i] = 0;
        box_used_[i] = 0;
    }

    // Place all existing digits (propagates constraints and updates region tracking)
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            int digit = board[row][col];
            if (digit != 0) {
                place(row, col, digit);
            }
        }
    }
}

void SIMDConstraintState::initFromBoard(const Board& board) {
    // Reset all cells to all candidates
    for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
        candidates[i] = ALL_CANDIDATES_MASK;
    }
    // Clear padding
    for (size_t i = BOARD_SIZE * BOARD_SIZE; i < SIMD_PADDED_CELLS; ++i) {
        candidates[i] = 0;
    }

    // Reset region usage tracking
    for (size_t i = 0; i < BOARD_SIZE; ++i) {
        row_used_[i] = 0;
        col_used_[i] = 0;
        box_used_[i] = 0;
    }

    // Place all existing digits using flat iteration (more efficient than nested loops)
    for (size_t i = 0; i < TOTAL_CELLS; ++i) {
        int digit = static_cast<unsigned char>(board.cells[i]);
        if (digit != 0) {
            place(i, digit);
        }
    }
}

int SIMDConstraintState::countCandidates(size_t cell) const {
    return std::popcount(candidates[cell]);
}

int SIMDConstraintState::findMRVCell() const {
    // AVX-512 dispatch: 32 cells per iteration (3 vs 6 for AVX2)
    if (simd_level_ == SolverPath::AVX512) {
        return avx512::findMRVCell(candidates);
    }

    int best_cell = -1;
    int best_count = 10;  // Max possible + 1

    // Process 16 cells at a time with AVX2
    for (size_t i = 0; i < SIMD_PADDED_CELLS; i += 16) {
        // Load 16 candidate masks
        __m256i cands = _mm256_load_si256(reinterpret_cast<const __m256i*>(&candidates[i]));

        // Compute popcount for each cell
        __m256i counts = popcnt_epi16(cands);

        // Extract counts to array for comparison
        // (Could be further optimized with horizontal min, but this is already fast)
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays) — alignas required for _mm256_store_si256
        alignas(SIMD_ALIGNMENT) uint16_t count_arr[16];
        _mm256_store_si256(reinterpret_cast<__m256i*>(count_arr), counts);

        // Find minimum among unsolved cells (count > 1)
        for (size_t j = 0; j < 16; ++j) {
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

void SIMDConstraintState::place(size_t cell, int digit) {
    // Set cell's candidates to only the placed digit
    auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
    candidates[cell] = digit_mask;

    // Update region usage tracking
    size_t row = cellRow(cell);
    size_t col = cellCol(cell);
    size_t box = getBoxIndex(row, col);
    row_used_[row] |= digit_mask;
    col_used_[col] |= digit_mask;
    box_used_[box] |= digit_mask;

    // Bulk SIMD elimination: AND candidates with (peer_mask | ~digit_mask)
    // For peer cells (mask=0x0000): candidates &= ~digit_mask (eliminates digit)
    // For non-peer cells (mask=0xFFFF): candidates &= 0xFFFF (unchanged)
    const auto& elim_mask = PEER_ELIM_MASKS[cell].mask;
    const __m256i not_digit = _mm256_set1_epi16(static_cast<short>(~digit_mask));

    for (size_t i = 0; i < SIMD_PADDED_CELLS; i += 16) {
        __m256i cands = _mm256_load_si256(reinterpret_cast<const __m256i*>(&candidates[i]));
        __m256i mask = _mm256_load_si256(reinterpret_cast<const __m256i*>(&elim_mask[i]));
        // (mask | ~digit_mask): 0xFFFF for non-peers, ~digit_mask for peers
        __m256i combined = _mm256_or_si256(mask, not_digit);
        _mm256_store_si256(reinterpret_cast<__m256i*>(&candidates[i]), _mm256_and_si256(cands, combined));
    }
}

int SIMDConstraintState::getSolvedDigit(size_t cell) const {
    uint16_t cands = candidates[cell];
    if (cands == 0 || (cands & (cands - 1)) != 0) {
        // Zero candidates or more than one candidate
        return 0;
    }
    // Single bit set - find which one
    return std::countr_zero(static_cast<unsigned>(cands)) + 1;
}

int SIMDConstraintState::findNakedSingle(const BoardData& board) const {
    // Find cells with exactly 1 candidate that haven't been placed on the board
    for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
        size_t row = cellRow(cell);
        size_t col = cellCol(cell);

        // Check if cell is empty on board but has exactly 1 candidate
        if (board[row][col] == 0) {
            uint16_t cands = candidates[cell];
            // Check if exactly one bit is set: (cands & (cands - 1)) == 0 and cands != 0
            if (cands != 0 && (cands & (cands - 1)) == 0) {
                return static_cast<int>(cell);
            }
        }
    }
    return -1;  // No naked single found
}

int SIMDConstraintState::findNakedSingle(const Board& board) const {
    // AVX-512 dispatch: 32-cell mask comparison
    if (simd_level_ == SolverPath::AVX512) {
        return avx512::findNakedSingle(candidates, board);
    }

    // AVX2: process 16 cells at a time with vectorized power-of-2 check
    const __m256i zero = _mm256_setzero_si256();
    const __m256i ones = _mm256_set1_epi16(1);

    for (size_t i = 0; i < SIMD_PADDED_CELLS; i += 16) {
        __m256i cands = _mm256_load_si256(reinterpret_cast<const __m256i*>(&candidates[i]));

        // Check for power-of-2 (exactly one candidate): (cands & (cands - 1)) == 0
        __m256i cands_minus1 = _mm256_sub_epi16(cands, ones);
        __m256i not_power_of_2 = _mm256_and_si256(cands, cands_minus1);

        // Mask: cells where cands is power-of-2 AND non-zero
        __m256i is_single = _mm256_cmpeq_epi16(not_power_of_2, zero);           // 0xFFFF if power-of-2 or zero
        __m256i is_nonzero = _mm256_cmpeq_epi16(cands, zero);                   // 0xFFFF if zero
        __m256i naked_single_vec = _mm256_andnot_si256(is_nonzero, is_single);  // power-of-2 AND non-zero

        // Convert to lane index via movemask_epi8
        // Each 16-bit lane produces 2 identical bits, so byte index / 2 = lane index
        auto byte_mask = static_cast<uint32_t>(_mm256_movemask_epi8(naked_single_vec));

        while (byte_mask != 0) {
            int byte_idx = std::countr_zero(static_cast<unsigned>(byte_mask));
            size_t cell = i + static_cast<size_t>(byte_idx / 2);
            if (cell < BOARD_SIZE * BOARD_SIZE && board.cells[cell] == 0) {
                return static_cast<int>(cell);
            }
            // Clear both bits of this 16-bit lane (clear 2 lowest set bits)
            byte_mask &= byte_mask - 1;
            byte_mask &= byte_mask - 1;
        }
    }

    return -1;  // No naked single found
}

/// Bit-reduction algorithm for hidden singles detection.
///
/// Instead of checking each digit individually (9 digits × 9 cells = 81 ops per region),
/// this processes all 9 digits simultaneously using bitwise accumulation:
///   at_least_twice |= (at_least_once & cand)  — tracks digits seen in 2+ cells
///   at_least_once  |= cand                    — tracks digits seen in 1+ cells
/// After scanning all cells: exactly_once = at_least_once & ~at_least_twice & ~region_used
/// This reduces the inner loop from O(81) with unpredictable branches to O(9) branchless ops.
// CPD-OFF — dirty-region optimization variant alongside full-scan; hot path
template <typename BoardT>
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — three separate region scans (row/col/box); each is simple; clang-tidy accumulates across loops
std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl(const BoardT& board) const {
    // --- Rows ---
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;

        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            size_t cell = cellIndex(row, col);
            uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }

        uint16_t exactly_once = at_least_once & ~at_least_twice & ~row_used_[row];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                size_t cell = cellIndex(row, col);
                if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                    return {static_cast<int>(cell), digit};
                }
            }
        }
    }

    // --- Columns ---
    for (size_t col = 0; col < BOARD_SIZE; ++col) {
        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            size_t cell = cellIndex(row, col);
            uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }

        uint16_t exactly_once = at_least_once & ~at_least_twice & ~col_used_[col];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                size_t cell = cellIndex(row, col);
                if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                    return {static_cast<int>(cell), digit};
                }
            }
        }
    }

    // --- Boxes ---
    for (size_t box = 0; box < BOARD_SIZE; ++box) {
        size_t box_row = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_col = (box % BOX_SIZE) * BOX_SIZE;

        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;

        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t row = box_row + br;
                size_t col = box_col + bc;
                size_t cell = cellIndex(row, col);
                uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
                at_least_twice |= (at_least_once & cand);
                at_least_once |= cand;
            }
        }

        uint16_t exactly_once = at_least_once & ~at_least_twice & ~box_used_[box];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t br = 0; br < BOX_SIZE; ++br) {
                for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                    size_t row = box_row + br;
                    size_t col = box_col + bc;
                    size_t cell = cellIndex(row, col);
                    if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                        return {static_cast<int>(cell), digit};
                    }
                }
            }
        }
    }

    return {-1, 0};  // No hidden single found
}

std::pair<int, int> SIMDConstraintState::findHiddenSingle(const BoardData& board) const {
    return findHiddenSingleImpl(board);
}

std::pair<int, int> SIMDConstraintState::findHiddenSingle(const Board& board) const {
    if (simd_level_ == SolverPath::AVX512) {
        return avx512::findHiddenSingle(candidates, board, row_used_, col_used_, box_used_);
    }
    return findHiddenSingleImpl(board);
}

std::pair<int, int> SIMDConstraintState::findHiddenSingle(const Board& board, uint32_t dirty_regions) const {
    return findHiddenSingleImpl(board, dirty_regions);
}

/// Incremental hidden single detection: only scans dirty regions.
/// dirty_regions bitmask: bits 0-8 = rows, 9-17 = cols, 18-26 = boxes.
/// After placing digit at (row, col, box), only those 3 regions need re-scanning.
template <typename BoardT>
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — dirty-region version: same row/col/box scan structure; clang-tidy accumulates across loops
std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl(const BoardT& board, uint32_t dirty_regions) const {
    // --- Rows (bits 0-8) ---
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        if (!(dirty_regions & (1U << row))) {
            continue;
        }

        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;

        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            size_t cell = cellIndex(row, col);
            uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }

        uint16_t exactly_once = at_least_once & ~at_least_twice & ~row_used_[row];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                size_t cell = cellIndex(row, col);
                if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                    return {static_cast<int>(cell), digit};
                }
            }
        }
    }

    // --- Columns (bits 9-17) ---
    for (size_t col = 0; col < BOARD_SIZE; ++col) {
        if (!(dirty_regions & (1U << (9 + col)))) {
            continue;
        }

        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            size_t cell = cellIndex(row, col);
            uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
            at_least_twice |= (at_least_once & cand);
            at_least_once |= cand;
        }

        uint16_t exactly_once = at_least_once & ~at_least_twice & ~col_used_[col];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                size_t cell = cellIndex(row, col);
                if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                    return {static_cast<int>(cell), digit};
                }
            }
        }
    }

    // --- Boxes (bits 18-26) ---
    for (size_t box = 0; box < BOARD_SIZE; ++box) {
        if (!(dirty_regions & (1U << (18 + box)))) {
            continue;
        }

        size_t box_row = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_col = (box % BOX_SIZE) * BOX_SIZE;

        uint16_t at_least_once = 0;
        uint16_t at_least_twice = 0;

        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t row = box_row + br;
                size_t col = box_col + bc;
                size_t cell = cellIndex(row, col);
                uint16_t cand = (board[row][col] == 0) ? candidates[cell] : uint16_t(0);
                at_least_twice |= (at_least_once & cand);
                at_least_once |= cand;
            }
        }

        uint16_t exactly_once = at_least_once & ~at_least_twice & ~box_used_[box];
        if (exactly_once != 0) {
            int digit = std::countr_zero(exactly_once) + 1;
            auto digit_mask = static_cast<uint16_t>(1U << (digit - 1));
            for (size_t br = 0; br < BOX_SIZE; ++br) {
                for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                    size_t row = box_row + br;
                    size_t col = box_col + bc;
                    size_t cell = cellIndex(row, col);
                    if (board[row][col] == 0 && (candidates[cell] & digit_mask) != 0) {
                        return {static_cast<int>(cell), digit};
                    }
                }
            }
        }
    }

    return {-1, 0};
}

// Explicit template instantiations
template std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl<BoardData>(const BoardData&) const;
template std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl<Board>(const Board&) const;
template std::pair<int, int> SIMDConstraintState::findHiddenSingleImpl<Board>(const Board&, uint32_t) const;
// CPD-ON

}  // namespace sudoku::core
