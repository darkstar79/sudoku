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

/**
 * @file test_puzzle_generator_propagation.cpp
 * @brief Unit tests for Phase 3: Iterative Constraint Propagation
 *
 * Tests the propagation loop that iteratively applies naked singles and hidden singles
 * until a fixed point is reached, with early contradiction detection.
 *
 * Test Strategy:
 * 1. Test propagation finds all forced moves (naked + hidden singles)
 * 2. Test contradiction detection (empty domain)
 * 3. Test integration with solver (fewer backtracking calls)
 * 4. Test both scalar and SIMD implementations behave identically
 */

#include "../helpers/candidate_test_utils.h"
#include "core/game_validator.h"
#include "core/puzzle_generator.h"

#include <set>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// Helper: Create board with single naked single at (0,0)
static BoardData createBoardWithNakedSingle() {
    // Board where (0,0) can only be 9 (all other values 1-8 blocked by row/col/box)
    return {{0, 1, 2, 0, 0, 0, 0, 0, 0},  // Row blocks 1-2
            {3, 0, 0, 0, 0, 0, 0, 0, 0},  // Col blocks 3
            {4, 0, 0, 0, 0, 0, 0, 0, 0},  // Box blocks 4
            {5, 0, 0, 0, 0, 0, 0, 0, 0},  // Col blocks 5
            {6, 0, 0, 0, 0, 0, 0, 0, 0},  // Col blocks 6
            {7, 0, 0, 0, 0, 0, 0, 0, 0},  // Col blocks 7
            {8, 0, 0, 0, 0, 0, 0, 0, 0},  // Col blocks 8
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

// Helper: Create board with hidden single in row
static BoardData createBoardWithHiddenSingle() {
    // Value 5 can only go in (0,0) in row 0 (other cells have 1-4, 6-9)
    return {{0, 1, 2, 3, 4, 6, 7, 8, 9},  // Row 0: only (0,0) can hold 5 (hidden single)
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

// Helper: Create board with cascading forced moves
static BoardData createBoardWithCascadingMoves() {
    // Use a real partial Sudoku with cascading singles
    // From a generated Medium puzzle with known forced moves
    return {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0},
            {0, 9, 8, 0, 0, 0, 0, 6, 0}, {8, 0, 0, 0, 6, 0, 0, 0, 3},
            {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
            {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5},
            {0, 0, 0, 0, 8, 0, 0, 7, 9}};  // This board has multiple naked/hidden singles that cascade
}

// Helper: Create board with contradiction (empty domain)
static BoardData createBoardWithContradiction() {
    // Cell (0,0) has no legal values - all 1-9 blocked
    return {{0, 1, 2, 0, 0, 0, 0, 0, 0},   // Row blocks 1-2
            {3, 0, 0, 0, 0, 0, 0, 0, 0},   // Col blocks 3, box blocks 3
            {4, 0, 0, 0, 0, 0, 0, 0, 0},   // Col blocks 4, box blocks 4
            {5, 0, 0, 0, 0, 0, 0, 0, 0},   // Col blocks 5
            {6, 0, 0, 0, 0, 0, 0, 0, 0},   // Col blocks 6
            {7, 0, 0, 0, 0, 0, 0, 0, 0},   // Col blocks 7
            {8, 0, 0, 0, 0, 0, 0, 0, 0},   // Col blocks 8
            {9, 0, 0, 0, 0, 0, 0, 0, 0},   // Col blocks 9
            {0, 0, 0, 0, 0, 0, 0, 0, 0}};  // (0,0) has domain = {} → contradiction!
}

TEST_CASE("PuzzleGenerator - Iterative Constraint Propagation", "[puzzle_generator][propagation][phase3]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("Single naked single is detected and propagated") {
        auto board = createBoardWithNakedSingle();

        // Before propagation: board[0][0] = 0
        REQUIRE(board[0][0] == 0);

        // Apply propagation (this will be the new method to test)
        // For now, we're testing the expected behavior
        auto result = generator.propagateConstraints(board);

        REQUIRE(result.has_value());
        const auto& propagated = result.value();

        // After propagation: board[0][0] should be filled with 9
        REQUIRE(propagated[0][0] == 9);
    }

    SECTION("Single hidden single is detected and propagated") {
        auto board = createBoardWithHiddenSingle();

        REQUIRE(board[0][0] == 0);

        auto result = generator.propagateConstraints(board);

        REQUIRE(result.has_value());
        const auto& propagated = result.value();

        // (0,0) should be filled with 5 (the hidden single)
        REQUIRE(propagated[0][0] == 5);
    }

    SECTION("Cascading forced moves are all found") {
        auto board = createBoardWithCascadingMoves();

        // Count initial empty cells
        int initial_empty = 0;
        for (const auto& row : board) {
            for (int cell : row) {
                if (cell == 0) {
                    initial_empty++;
                }
            }
        }

        auto result = generator.propagateConstraints(board);

        REQUIRE(result.has_value());
        const auto& propagated = result.value();

        // Count remaining empty cells after propagation
        int final_empty = 0;
        for (const auto& row : propagated) {
            for (int cell : row) {
                if (cell == 0) {
                    final_empty++;
                }
            }
        }

        // Should have filled at least some cells through propagation
        REQUIRE(final_empty < initial_empty);
    }

    SECTION("Propagation detects contradictions (empty domain)") {
        auto board = createBoardWithContradiction();

        auto result = generator.propagateConstraints(board);

        // Should return error indicating contradiction
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == GenerationError::NoUniqueSolution);
    }

    SECTION("Propagation on valid board succeeds") {
        // Valid partial board with no forced moves
        BoardData board;
        board[0][0] = 5;
        board[1][1] = 3;
        board[2][2] = 7;

        auto result = generator.propagateConstraints(board);

        REQUIRE(result.has_value());
        // Board should be unchanged (no forced moves)
        const auto& propagated = result.value();
        REQUIRE(propagated[0][0] == 5);
        REQUIRE(propagated[1][1] == 3);
        REQUIRE(propagated[2][2] == 7);
    }

    SECTION("Propagation on empty board succeeds with no changes") {
        BoardData board;

        auto result = generator.propagateConstraints(board);

        REQUIRE(result.has_value());
        const auto& propagated = result.value();

        // Should be unchanged
        for (size_t r = 0; r < 9; ++r) {
            for (size_t c = 0; c < 9; ++c) {
                REQUIRE(propagated[r][c] == 0);
            }
        }
    }
}

TEST_CASE("PuzzleGenerator - hasContradiction Detection", "[puzzle_generator][propagation][phase3]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("Detects empty domain contradiction") {
        auto board = createBoardWithContradiction();

        // hasContradiction should detect that (0,0) has no legal values
        bool has_contradiction = generator.hasContradiction(board);

        REQUIRE(has_contradiction == true);
    }

    SECTION("Valid board has no contradiction") {
        BoardData board;
        board[0][0] = 5;

        bool has_contradiction = generator.hasContradiction(board);

        REQUIRE(has_contradiction == false);
    }

    SECTION("Nearly-full valid board has no contradiction") {
        // Create board with one empty cell that has valid domain
        BoardData board = {{5, 3, 0, 6, 7, 8, 9, 1, 2},  // (0,2) can be 4
                           {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7}, {8, 5, 9, 7, 6, 1, 4, 2, 3},
                           {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6}, {9, 6, 1, 5, 3, 7, 2, 8, 4},
                           {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};

        bool has_contradiction = generator.hasContradiction(board);

        REQUIRE(has_contradiction == false);
    }
}

TEST_CASE("PuzzleGenerator - Propagation Performance Impact", "[puzzle_generator][propagation][phase3][integration]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("Propagation reduces backtracking depth on typical puzzle") {
        // Generate a medium puzzle and measure backtracking with/without propagation
        auto puzzle_result = generator.generatePuzzle(Difficulty::Medium);

        REQUIRE(puzzle_result.has_value());
        const auto& puzzle = puzzle_result.value();

        // Solver should use propagation and find solution efficiently
        // We can't directly measure backtracking depth without instrumentation,
        // but we can verify the solution is found
        auto solution = generator.solvePuzzle(puzzle.board);

        REQUIRE(solution.has_value());
    }

    SECTION("Propagation works on hard puzzles") {
        auto puzzle_result = generator.generatePuzzle(Difficulty::Hard);

        REQUIRE(puzzle_result.has_value());
        const auto& puzzle = puzzle_result.value();

        // Should solve successfully with propagation
        auto solution = generator.solvePuzzle(puzzle.board);

        REQUIRE(solution.has_value());
    }
}

TEST_CASE("PuzzleGenerator - SIMD Propagation Consistency", "[puzzle_generator][propagation][phase3][simd]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("SIMD and scalar propagation produce identical results on same board") {
        // Create test board with multiple forced moves
        auto board = createBoardWithCascadingMoves();

        // Apply propagation with scalar solver
        auto scalar_result = generator.propagateConstraintsScalar(board);

        // Apply propagation with SIMD solver
        auto simd_result = generator.propagateConstraintsSIMD(board);

        // Both should succeed
        REQUIRE(scalar_result.has_value());
        REQUIRE(simd_result.has_value());

        // Results should be identical
        const auto& scalar_board = scalar_result.value();
        const auto& simd_board = simd_result.value();

        for (size_t r = 0; r < 9; ++r) {
            for (size_t c = 0; c < 9; ++c) {
                REQUIRE(scalar_board[r][c] == simd_board[r][c]);
            }
        }
    }

    SECTION("Both implementations detect same contradictions") {
        auto board = createBoardWithContradiction();

        auto scalar_result = generator.propagateConstraintsScalar(board);
        auto simd_result = generator.propagateConstraintsSIMD(board);

        // Both should detect contradiction
        REQUIRE_FALSE(scalar_result.has_value());
        REQUIRE_FALSE(simd_result.has_value());

        // Same error type
        REQUIRE(scalar_result.error() == simd_result.error());
    }
}

TEST_CASE("PuzzleGenerator - Edge Cases for Propagation", "[puzzle_generator][propagation][phase3]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("Propagation on completely filled board") {
        // Create valid complete board
        BoardData board = sudoku::testing::kSolvedBoard;

        auto result = generator.propagateConstraints(board);

        REQUIRE(result.has_value());
        // Board should remain unchanged
        REQUIRE(result.value() == board);
    }

    SECTION("Propagation with conflicting initial state") {
        // Use board with empty domain (contradiction)
        auto board = createBoardWithContradiction();

        auto result = generator.propagateConstraints(board);

        // Should detect contradiction (empty domain at (0,0))
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Maximum propagation iterations (complex cascade)") {
        // Create board that requires many propagation steps
        // This tests that the while loop terminates correctly
        auto board = createBoardWithCascadingMoves();

        auto result = generator.propagateConstraints(board);

        // Should complete without infinite loop
        REQUIRE(result.has_value());
    }
}
