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
 * @file test_simd_constraint_state.cpp
 * @brief Unit tests for AVX2-optimized SIMD constraint state.
 */

#include "../helpers/test_utils.h"
#include "core/board.h"
#include "core/constants.h"
#include "core/cpu_features.h"
#include "core/simd_constraint_state.h"

#include <chrono>
#include <random>
#include <set>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace sudoku::core;

// ============================================================================
// Test Case 1: Basic Construction and Initialization
// ============================================================================

TEST_CASE("SIMDConstraintState - Default construction", "[simd][construction]") {
    SIMDConstraintState state;

    SECTION("All cells start with all candidates") {
        for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
            REQUIRE(state.getCandidates(cell) == ALL_CANDIDATES_MASK);
            REQUIRE(state.countCandidates(cell) == 9);
        }
    }

    SECTION("Padding cells are zeroed") {
        for (size_t cell = BOARD_SIZE * BOARD_SIZE; cell < SIMD_PADDED_CELLS; ++cell) {
            REQUIRE(state.getCandidates(cell) == 0);
        }
    }
}

TEST_CASE("SIMDConstraintState - Cell indexing", "[simd][indexing]") {
    SECTION("cellIndex computes correctly") {
        REQUIRE(SIMDConstraintState::cellIndex(0, 0) == 0);
        REQUIRE(SIMDConstraintState::cellIndex(0, 8) == 8);
        REQUIRE(SIMDConstraintState::cellIndex(1, 0) == 9);
        REQUIRE(SIMDConstraintState::cellIndex(8, 8) == 80);
    }

    SECTION("cellRow and cellCol are inverses of cellIndex") {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                size_t cell = SIMDConstraintState::cellIndex(row, col);
                REQUIRE(SIMDConstraintState::cellRow(cell) == row);
                REQUIRE(SIMDConstraintState::cellCol(cell) == col);
            }
        }
    }
}

// ============================================================================
// Test Case 2: Peer Table Verification
// ============================================================================

TEST_CASE("SIMDConstraintState - Peer table correctness", "[simd][peers]") {
    SECTION("Each cell has exactly 20 peers") {
        for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
            const auto& peers = PEER_TABLE[cell];
            // Verify all 20 peer slots are used (no duplicates, no self)
            std::set<uint8_t> unique_peers;
            for (size_t i = 0; i < 20; ++i) {
                REQUIRE(peers.peers[i] != cell);  // No self-reference
                REQUIRE(peers.peers[i] < 81);     // Valid cell index
                unique_peers.insert(peers.peers[i]);
            }
            REQUIRE(unique_peers.size() == 20);  // All unique
        }
    }

    SECTION("Peers include same row, column, and box") {
        // Test cell (4, 4) - center of the board
        size_t cell = SIMDConstraintState::cellIndex(4, 4);
        const auto& peers = PEER_TABLE[cell];

        std::set<uint8_t> peer_set(peers.peers.begin(), peers.peers.end());

        // Should include all cells in row 4 (except self)
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (col != 4) {
                REQUIRE(peer_set.count(static_cast<uint8_t>((4 * BOARD_SIZE) + col)) == 1);
            }
        }

        // Should include all cells in column 4 (except self)
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            if (row != 4) {
                REQUIRE(peer_set.count(static_cast<uint8_t>((row * BOARD_SIZE) + 4)) == 1);
            }
        }

        // Should include box cells (3,3), (3,5), (5,3), (5,5) - box corners excluding row/col overlap
        REQUIRE(peer_set.count(static_cast<uint8_t>(SIMDConstraintState::cellIndex(3, 3))) == 1);
        REQUIRE(peer_set.count(static_cast<uint8_t>(SIMDConstraintState::cellIndex(3, 5))) == 1);
        REQUIRE(peer_set.count(static_cast<uint8_t>(SIMDConstraintState::cellIndex(5, 3))) == 1);
        REQUIRE(peer_set.count(static_cast<uint8_t>(SIMDConstraintState::cellIndex(5, 5))) == 1);
    }
}

// ============================================================================
// Test Case 3: Candidate Operations
// ============================================================================

TEST_CASE("SIMDConstraintState - isAllowed", "[simd][candidates]") {
    SIMDConstraintState state;

    SECTION("All digits allowed initially") {
        for (int digit = 1; digit <= 9; ++digit) {
            REQUIRE(state.isAllowed(0, digit));
            REQUIRE(state.isAllowed(0, 0, digit));
        }
    }

    SECTION("Eliminated digit is not allowed") {
        state.eliminate(0, 5);
        REQUIRE_FALSE(state.isAllowed(0, 5));
        // Other digits still allowed
        for (int digit = 1; digit <= 9; ++digit) {
            if (digit != 5) {
                REQUIRE(state.isAllowed(0, digit));
            }
        }
    }
}

TEST_CASE("SIMDConstraintState - countCandidates", "[simd][candidates]") {
    SIMDConstraintState state;

    SECTION("Full candidates = 9") {
        REQUIRE(state.countCandidates(0) == 9);
    }

    SECTION("Count decreases with elimination") {
        state.eliminate(0, 1);
        REQUIRE(state.countCandidates(0) == 8);

        state.eliminate(0, 2);
        state.eliminate(0, 3);
        REQUIRE(state.countCandidates(0) == 6);
    }

    SECTION("Empty candidates = 0") {
        for (int digit = 1; digit <= 9; ++digit) {
            state.eliminate(0, digit);
        }
        REQUIRE(state.countCandidates(0) == 0);
        REQUIRE(state.isContradiction(0));
    }
}

// ============================================================================
// Test Case 4: Place and Constraint Propagation
// ============================================================================

TEST_CASE("SIMDConstraintState - place propagates constraints", "[simd][propagation]") {
    SIMDConstraintState state;

    SECTION("Placed cell has only placed digit") {
        state.place(0, 5);                             // Place digit 5 in cell (0,0)
        REQUIRE(state.getCandidates(0) == (1U << 4));  // Bit 4 = digit 5
        REQUIRE(state.countCandidates(0) == 1);
        REQUIRE(state.isSolved(0));
        REQUIRE(state.getSolvedDigit(0) == 5);
    }

    SECTION("Peers lose placed digit") {
        state.place(0, 5);  // Place digit 5 in cell (0,0)

        // All peers should no longer have digit 5 as candidate
        const auto& peers = PEER_TABLE[0];
        for (size_t i = 0; i < 20; ++i) {
            size_t peer_cell = peers.peers[i];
            REQUIRE_FALSE(state.isAllowed(peer_cell, 5));
        }
    }

    SECTION("Non-peers unaffected") {
        state.place(0, 5);  // Place digit 5 in cell (0,0)

        // Cell (8,8) is not a peer of (0,0) - should still have all candidates
        size_t far_cell = SIMDConstraintState::cellIndex(8, 8);
        REQUIRE(state.isAllowed(far_cell, 5));
    }
}

// ============================================================================
// Test Case 5: MRV Heuristic
// ============================================================================

TEST_CASE("SIMDConstraintState - findMRVCell", "[simd][mrv]") {
    SIMDConstraintState state;

    SECTION("Returns -1 when all cells solved") {
        // Simulate all cells solved (single candidate each)
        for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
            state.candidates[cell] = 1;  // Single candidate
        }
        REQUIRE(state.findMRVCell() == -1);
    }

    SECTION("Finds cell with minimum candidates") {
        // Set most cells to 5 candidates
        for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
            state.candidates[cell] = 0b000011111;  // 5 candidates
        }

        // Set one cell to 2 candidates
        state.candidates[40] = 0b000000011;  // 2 candidates

        REQUIRE(state.findMRVCell() == 40);
    }

    SECTION("Skips solved cells") {
        for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
            state.candidates[cell] = 0b000000001;  // Solved (1 candidate)
        }

        // Only one unsolved cell
        state.candidates[50] = 0b000000111;  // 3 candidates

        REQUIRE(state.findMRVCell() == 50);
    }

    SECTION("Returns first minimum if tie") {
        for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
            state.candidates[cell] = 0b111111111;  // 9 candidates
        }

        // Two cells with 2 candidates
        state.candidates[10] = 0b000000011;  // 2 candidates
        state.candidates[70] = 0b000000011;  // 2 candidates

        // Should return first one found (10)
        REQUIRE(state.findMRVCell() == 10);
    }
}

// ============================================================================
// Test Case 6: Board Initialization
// ============================================================================

TEST_CASE("SIMDConstraintState - initFromBoard", "[simd][init]") {
    BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

    SIMDConstraintState state;
    state.initFromBoard(board);

    SECTION("Filled cells are solved with correct digit") {
        REQUIRE(state.isSolved(SIMDConstraintState::cellIndex(0, 0)));
        REQUIRE(state.getSolvedDigit(SIMDConstraintState::cellIndex(0, 0)) == 5);

        REQUIRE(state.isSolved(SIMDConstraintState::cellIndex(0, 1)));
        REQUIRE(state.getSolvedDigit(SIMDConstraintState::cellIndex(0, 1)) == 3);
    }

    SECTION("Empty cells have reduced candidates") {
        // Cell (0,2) is empty but constrained by 5, 3 in row, etc.
        size_t cell = SIMDConstraintState::cellIndex(0, 2);
        REQUIRE_FALSE(state.isSolved(cell));
        REQUIRE_FALSE(state.isAllowed(cell, 5));  // 5 in row
        REQUIRE_FALSE(state.isAllowed(cell, 3));  // 3 in row
        REQUIRE_FALSE(state.isAllowed(cell, 8));  // 8 in column
    }
}

// ============================================================================
// Test Case 7: Performance Benchmarks (Release build only)
// ============================================================================

TEST_CASE("SIMDConstraintState - MRV performance", "[simd][benchmark][!mayfail]") {
    // This test may fail in Debug builds due to unoptimized intrinsics
    // Run with Release build for accurate performance measurement

    SIMDConstraintState state;

    // Setup: randomize candidates
    std::mt19937 rng(42);
    for (size_t cell = 0; cell < BOARD_SIZE * BOARD_SIZE; ++cell) {
        state.candidates[cell] = static_cast<uint16_t>(rng() & ALL_CANDIDATES_MASK);
        if (state.candidates[cell] == 0) {
            state.candidates[cell] = 1;  // Avoid contradictions
        }
    }

    // Benchmark: 10000 MRV calls
    constexpr int ITERATIONS = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    int result = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        result = state.findMRVCell();
    }
    // Use result to prevent optimization
    REQUIRE(result >= -1);

    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    double us_per_call = static_cast<double>(elapsed_us) / ITERATIONS;

    INFO("MRV performance: " << us_per_call << " us/call");
    INFO("Total time for " << ITERATIONS << " calls: " << elapsed_us << " us");

    // In Release build, expect < 1us per call with SIMD
    // In Debug build, this may be much slower
#ifdef NDEBUG
    REQUIRE(us_per_call < 5.0);  // Should be < 1us in Release, allow margin
#endif
}

// ============================================================================
// Test Case 8: Correctness Comparison with Scalar
// ============================================================================

TEST_CASE("SIMDConstraintState - Correctness vs scalar popcount", "[simd][correctness]") {
    SIMDConstraintState state;

    SECTION("countCandidates matches std::popcount") {
        // Test all possible 9-bit patterns
        for (uint16_t mask = 0; mask <= ALL_CANDIDATES_MASK; ++mask) {
            state.candidates[0] = mask;
            int simd_count = state.countCandidates(0);
            int scalar_count = std::popcount(mask);
            REQUIRE(simd_count == scalar_count);
        }
    }
}

// ============================================================================
// Test Case 9: AVX-512 aligned-loader regression (#18 / BL-2)
// ============================================================================
//
// SIMDConstraintState is alignas(64) and `candidates` is its first member, so
// the AVX-512 ops were switched from `_mm512_loadu_si512` to the aligned
// `_mm512_load_si512` paired with an `assertSimdAligned()` runtime check. The
// previous unaligned-load form was a latent SIGSEGV per the "Aligned Loader"
// bestiary entry: GCC could substitute `vmovdqa64` (aligned) once it inferred
// alignment from the wrapper struct, which segfaults on any caller that ever
// passed an under-aligned buffer.
//
// This test exercises the AVX-512 entry points on boundary inputs (all-empty,
// all-full, mid-array minimum) to anchor the path in CI. If alignment ever
// weakens, the assert fires in Debug or aligned-load crashes in Release.
TEST_CASE("SIMDConstraintState - AVX-512 path on aligned buffer", "[simd][avx512]") {
    if (!isSolverPathAvailable(SolverPath::AVX512)) {
        SKIP("AVX-512 not available on this CPU");
    }

    SIMDConstraintState state;
    state.setSimdLevel(SolverPath::AVX512);

    // Sanity: the wrapper struct really did honor alignas(64). If this ever
    // fails we have bigger problems than the load instruction.
    REQUIRE((reinterpret_cast<uintptr_t>(state.candidates.data()) % 64U) == 0);

    SECTION("findMRVCell on all-empty (every cell has 9 candidates)") {
        // Default ctor leaves every real cell at ALL_CANDIDATES_MASK.
        int cell = state.findMRVCell();
        REQUIRE(cell >= 0);
        REQUIRE(cell < static_cast<int>(BOARD_SIZE * BOARD_SIZE));
        REQUIRE(state.countCandidates(static_cast<size_t>(cell)) > 1);
    }

    SECTION("findMRVCell on all-solved board returns -1") {
        for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
            state.candidates[i] = 1;  // single candidate = solved
        }
        REQUIRE(state.findMRVCell() == -1);
    }

    SECTION("findMRVCell picks a bivalue at the last cell (96-aligned tail)") {
        for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
            state.candidates[i] = ALL_CANDIDATES_MASK;
        }
        state.candidates[(BOARD_SIZE * BOARD_SIZE) - 1] = 0b000000011;  // 2 cands
        REQUIRE(state.findMRVCell() == static_cast<int>((BOARD_SIZE * BOARD_SIZE) - 1));
    }

    SECTION("findNakedSingle locates a single-candidate empty cell") {
        Board board{};  // all zeros
        for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
            state.candidates[i] = ALL_CANDIDATES_MASK;
        }
        state.candidates[42] = 0b000000100;  // single candidate
        REQUIRE(state.findNakedSingle(board) == 42);
    }

    SECTION("findNakedSingle on solved board returns -1") {
        Board board{};
        for (size_t i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
            board.cells[i] = 1;
            state.candidates[i] = 0b000000001;
        }
        REQUIRE(state.findNakedSingle(board) == -1);
    }
}
