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
 * @file simd_constraint_state.h
 * @brief AVX2-optimized constraint state for Sudoku solving.
 *
 * This implementation uses Structure of Arrays (SoA) layout and AVX2 SIMD
 * intrinsics to accelerate constraint propagation and MRV (Minimum Remaining
 * Values) heuristic computation.
 *
 * Key optimizations:
 * - 96 contiguous uint16_t candidates (81 cells + 15 padding for 32-byte alignment)
 * - Vectorized popcount using lookup table (no native VPOPCNTDQ on AVX2)
 * - SIMD MRV cell finding (process 16 cells per iteration)
 * - Precomputed peer indices for fast constraint propagation
 *
 * Target: AMD Ryzen 5 3400G (Zen+) with AVX2 support.
 */

#include "core/board.h"
#include "core/constants.h"
#include "core/cpu_features.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <utility>
#include <vector>

// Note: <immintrin.h> is intentionally NOT included here.
// The header API uses only uint16_t/int types (no __m256i/__m512i).
// SIMD intrinsics are confined to simd_constraint_state.cpp and simd_avx512_ops.cpp.

namespace sudoku::core {

/// AVX-512 requires 64-byte alignment for aligned load/store operations
/// (also satisfies AVX2's 32-byte requirement)
inline constexpr size_t SIMD_ALIGNMENT = 64;

/// Number of cells in padded array (81 cells + 15 padding = 96 = 6 * 16)
inline constexpr size_t SIMD_PADDED_CELLS = 96;

/// All candidates bitmask (bits 0-8 for digits 1-9)
inline constexpr uint16_t ALL_CANDIDATES_MASK = 0x1FF;

/**
 * @brief Precomputed peer indices for a single cell.
 *
 * Each cell has exactly 20 peers:
 * - 8 cells in the same row (excluding self)
 * - 8 cells in the same column (excluding self)
 * - 4 cells in the same box (excluding row/col overlaps)
 */
struct PeerIndices {
    std::array<uint8_t, 20> peers;  ///< Indices of peer cells (0-80)
};

/**
 * @brief Generate peer indices lookup table at compile time.
 * @return Array of PeerIndices for all 81 cells
 */
[[nodiscard]] constexpr std::array<PeerIndices, BOARD_SIZE * BOARD_SIZE> generatePeerTable() {
    std::array<PeerIndices, BOARD_SIZE * BOARD_SIZE> table{};

    for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
        size_t row = cell / BOARD_SIZE;
        size_t col = cell % BOARD_SIZE;
        size_t box_row = (row / BOX_SIZE) * BOX_SIZE;
        size_t box_col = (col / BOX_SIZE) * BOX_SIZE;

        size_t peer_idx = 0;

        // Add row peers (8 cells)
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            if (c != col) {
                table[cell].peers[peer_idx++] = static_cast<uint8_t>((row * BOARD_SIZE) + c);
            }
        }

        // Add column peers (8 cells)
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            if (r != row) {
                table[cell].peers[peer_idx++] = static_cast<uint8_t>((r * BOARD_SIZE) + col);
            }
        }

        // Add box peers (4 cells, excluding row/col overlaps)
        for (size_t r = box_row; r < box_row + BOX_SIZE; ++r) {
            for (size_t c = box_col; c < box_col + BOX_SIZE; ++c) {
                if (r != row && c != col) {
                    table[cell].peers[peer_idx++] = static_cast<uint8_t>((r * BOARD_SIZE) + c);
                }
            }
        }
    }

    return table;
}

/// Compile-time peer lookup table
inline constexpr auto PEER_TABLE = generatePeerTable();

/**
 * @brief AVX2-optimized constraint state for Sudoku solving.
 *
 * Uses Structure of Arrays (SoA) layout with 32-byte alignment for efficient
 * SIMD operations. Each cell's candidates are stored as a 16-bit bitmask
 * where bit N (0-8) indicates digit (N+1) is a valid candidate.
 *
 * Memory layout: 96 contiguous uint16_t values (192 bytes)
 * - Cells 0-80: Valid Sudoku cells
 * - Cells 81-95: Padding (set to 0 to exclude from MRV search)
 */
#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4324)  // structure was padded due to __declspec(align())
#endif
struct alignas(SIMD_ALIGNMENT) SIMDConstraintState {
    /// Candidate bitmasks for all cells (SoA layout)
    /// Bit N (0-8) is set if digit (N+1) is a valid candidate
    std::array<uint16_t, SIMD_PADDED_CELLS> candidates{};

    /// Region usage tracking for hidden singles detection
    /// Bit N (0-8) is set if digit (N+1) is placed in that region
    std::array<uint16_t, BOARD_SIZE> row_used_{};  ///< Values used in each row
    std::array<uint16_t, BOARD_SIZE> col_used_{};  ///< Values used in each column
    std::array<uint16_t, BOARD_SIZE> box_used_{};  ///< Values used in each box

    /// Which SIMD level to use for vectorized operations (AVX2 or AVX-512)
    SolverPath simd_level_{SolverPath::AVX2};

    /// Set the SIMD level for vectorized operations
    void setSimdLevel(SolverPath level) {
        simd_level_ = level;
    }

    /**
     * @brief Default constructor - initializes all cells with all candidates.
     */
    SIMDConstraintState() {
        // Initialize real cells with all candidates
        for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
            candidates[i] = ALL_CANDIDATES_MASK;
        }
        // Padding cells set to 0 (excluded from MRV search)
        for (size_t i = BOARD_SIZE * BOARD_SIZE; i < SIMD_PADDED_CELLS; ++i) {
            candidates[i] = 0;
        }
    }

    /**
     * @brief Initialize from existing board state.
     * @param board 9x9 Sudoku board (0 = empty, 1-9 = filled)
     */
    void initFromBoard(const BoardData& board);

    /// Initialize from Board (flat array, more efficient)
    void initFromBoard(const Board& board);

    /**
     * @brief Get cell index from row and column.
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @return Cell index (0-80)
     */
    [[nodiscard]] static constexpr size_t cellIndex(size_t row, size_t col) {
        return (row * BOARD_SIZE) + col;
    }

    /**
     * @brief Get row from cell index.
     * @param cell Cell index (0-80)
     * @return Row index (0-8)
     */
    [[nodiscard]] static constexpr size_t cellRow(size_t cell) {
        return cell / BOARD_SIZE;
    }

    /**
     * @brief Get column from cell index.
     * @param cell Cell index (0-80)
     * @return Column index (0-8)
     */
    [[nodiscard]] static constexpr size_t cellCol(size_t cell) {
        return cell % BOARD_SIZE;
    }

    /**
     * @brief Check if a digit is a valid candidate for a cell.
     * @param cell Cell index (0-80)
     * @param digit Digit to check (1-9)
     * @return true if digit is a valid candidate
     */
    [[nodiscard]] bool isAllowed(size_t cell, int digit) const {
        return (candidates[cell] & (1U << (digit - 1))) != 0;
    }

    /**
     * @brief Check if a digit is a valid candidate for a cell (row/col version).
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @param digit Digit to check (1-9)
     * @return true if digit is a valid candidate
     */
    [[nodiscard]] bool isAllowed(size_t row, size_t col, int digit) const {
        return isAllowed(cellIndex(row, col), digit);
    }

    /**
     * @brief Get candidate bitmask for a cell.
     * @param cell Cell index (0-80)
     * @return Bitmask where bit N is set if digit (N+1) is a candidate
     */
    [[nodiscard]] uint16_t getCandidates(size_t cell) const {
        return candidates[cell];
    }

    /**
     * @brief Count number of candidates for a cell.
     * @param cell Cell index (0-80)
     * @return Number of valid candidates (0-9)
     */
    [[nodiscard]] int countCandidates(size_t cell) const;

    /**
     * @brief Find cell with minimum remaining values (MRV heuristic).
     *
     * Uses AVX2 SIMD to process 16 cells at a time. Finds the cell with
     * the fewest candidates (>1) for optimal backtracking.
     *
     * @return Cell index with minimum candidates, or -1 if all cells are solved
     */
    [[nodiscard]] int findMRVCell() const;

    /**
     * @brief Place a digit in a cell and propagate constraints.
     *
     * Sets the cell's candidates to only the placed digit, then eliminates
     * that digit from all 20 peer cells.
     *
     * @param cell Cell index (0-80)
     * @param digit Digit to place (1-9)
     */
    void place(size_t cell, int digit);

    /**
     * @brief Place a digit in a cell (row/col version).
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @param digit Digit to place (1-9)
     */
    void place(size_t row, size_t col, int digit) {
        place(cellIndex(row, col), digit);
    }

    /**
     * @brief Eliminate a digit from a cell's candidates.
     * @param cell Cell index (0-80)
     * @param digit Digit to eliminate (1-9)
     */
    void eliminate(size_t cell, int digit) {
        candidates[cell] &= ~(1U << (digit - 1));
    }

    /**
     * @brief Check if a cell has no remaining candidates (contradiction).
     * @param cell Cell index (0-80)
     * @return true if the cell has no valid candidates
     */
    [[nodiscard]] bool isContradiction(size_t cell) const {
        return candidates[cell] == 0;
    }

    /**
     * @brief Check if a cell is solved (exactly one candidate).
     * @param cell Cell index (0-80)
     * @return true if the cell has exactly one candidate
     */
    [[nodiscard]] bool isSolved(size_t cell) const {
        return countCandidates(cell) == 1;
    }

    /**
     * @brief Get the solved digit for a cell (assumes cell is solved).
     * @param cell Cell index (0-80)
     * @return The solved digit (1-9), or 0 if not solved
     */
    [[nodiscard]] int getSolvedDigit(size_t cell) const;

    /**
     * @brief Find a naked single - cell with exactly 1 candidate that isn't placed on board.
     *
     * A naked single is a cell where constraint propagation has eliminated all but one
     * candidate. This forced value must be placed before continuing the search.
     *
     * @param board Current board state (0 = empty)
     * @return Cell index with naked single, or -1 if none found
     */
    [[nodiscard]] int findNakedSingle(const BoardData& board) const;

    /// Find naked single (Board overload, uses flat indexing)
    [[nodiscard]] int findNakedSingle(const Board& board) const;

    /**
     * @brief Find any hidden single on the board.
     *
     * A hidden single is a value that can only appear in one cell within a row/column/box,
     * even though that cell may have other possible values.
     *
     * Scans rows first (0-8), then columns (0-8), then boxes (0-8).
     *
     * @param board Current board state (0 = empty)
     * @return Pair of (cell_index, digit) if hidden single found, or (-1, 0) if none
     */
    [[nodiscard]] std::pair<int, int> findHiddenSingle(const BoardData& board) const;

    /// Find hidden single (Board overload)
    [[nodiscard]] std::pair<int, int> findHiddenSingle(const Board& board) const;

    /**
     * @brief Find hidden single, scanning only dirty regions.
     *
     * After placing a digit at (row, col, box), only those 3 regions can gain
     * new hidden singles. This overload skips clean regions for a 9x reduction
     * in work per recursion level.
     *
     * @param board Current board state (0 = empty)
     * @param dirty_regions Bitmask: bits 0-8 = rows, 9-17 = cols, 18-26 = boxes
     * @return Pair of (cell_index, digit) if hidden single found, or (-1, 0) if none
     */
    [[nodiscard]] std::pair<int, int> findHiddenSingle(const Board& board, uint32_t dirty_regions) const;

    /**
     * @brief Calculate box index from row and column.
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @return Box index (0-8) in row-major order
     */
    [[nodiscard]] static constexpr size_t getBoxIndex(size_t row, size_t col) {
        return ((row / BOX_SIZE) * BOX_SIZE) + (col / BOX_SIZE);
    }

    /// Bitmask with all 27 regions dirty (9 rows + 9 cols + 9 boxes)
    static constexpr uint32_t ALL_REGIONS_DIRTY = 0x7FFFFFF;

    /// Template implementation for findHiddenSingle (shared by vector and Board overloads)
    template <typename BoardT>
    [[nodiscard]] std::pair<int, int> findHiddenSingleImpl(const BoardT& board) const;

    /// Template implementation for findHiddenSingle with dirty regions mask
    template <typename BoardT>
    [[nodiscard]] std::pair<int, int> findHiddenSingleImpl(const BoardT& board, uint32_t dirty_regions) const;
};
#ifdef _MSC_VER
#    pragma warning(pop)
#endif

}  // namespace sudoku::core
