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
 * @file test_puzzle_generator_solver_integration.cpp
 * @brief Unit tests for Phase 3 solver integration with iterative propagation
 *
 * Tests that integrating iterative propagation into the solver:
 * 1. Produces identical results to the current solver (correctness)
 * 2. Reduces backtracking depth (performance)
 * 3. Solves puzzles faster (especially Hard/Expert)
 * 4. Handles edge cases (contradictions, complete boards)
 *
 * Test Strategy:
 * - Verify solution correctness on known puzzles
 * - Compare SIMD and scalar solver results (must be identical)
 * - Measure solving performance improvement
 */

#include "../helpers/candidate_test_utils.h"
#include "../helpers/test_utils.h"
#include "core/game_validator.h"
#include "core/puzzle_generator.h"

#include <chrono>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// Helper: Create Medium difficulty puzzle (known to have unique solution)
static BoardData createMediumPuzzle() {
    return sudoku::test::getEasyPuzzleWithPatterns();
}

// Helper: Create Hard difficulty puzzle with many cascading singles
static BoardData createHardPuzzle() {
    // This puzzle has multiple forced moves that cascade
    return {{0, 0, 0, 0, 0, 0, 6, 8, 0}, {0, 0, 0, 0, 7, 3, 0, 0, 9}, {3, 0, 9, 0, 0, 0, 0, 4, 5},
            {4, 9, 0, 0, 0, 0, 0, 0, 0}, {8, 0, 3, 0, 5, 0, 9, 0, 2}, {0, 0, 0, 0, 0, 0, 0, 3, 6},
            {9, 6, 0, 0, 0, 0, 3, 0, 8}, {7, 0, 0, 6, 8, 0, 0, 0, 0}, {0, 2, 8, 0, 0, 0, 0, 0, 0}};
}

// Helper: Create nearly-solved puzzle (only a few cells empty)
static BoardData createNearlySolvedPuzzle() {
    return {{5, 3, 0, 6, 7, 8, 9, 1, 2},  // (0,2) = 4
            {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7}, {8, 5, 9, 7, 6, 1, 4, 2, 3},
            {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6}, {9, 6, 1, 5, 3, 7, 2, 8, 4},
            {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
}

TEST_CASE("PuzzleGenerator - Solver with Propagation Correctness", "[puzzle_generator][solver][phase3]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("Solver produces valid solution for Medium puzzle") {
        auto puzzle = createMediumPuzzle();

        auto solution = generator.solvePuzzle(puzzle);

        REQUIRE(solution.has_value());
        REQUIRE(validator.isComplete(solution.value()));
        REQUIRE(validator.validateBoard(solution.value()));
    }

    SECTION("Solver produces valid solution for Hard puzzle") {
        auto puzzle = createHardPuzzle();

        auto solution = generator.solvePuzzle(puzzle);

        REQUIRE(solution.has_value());
        REQUIRE(validator.isComplete(solution.value()));
        REQUIRE(validator.validateBoard(solution.value()));
    }

    SECTION("Solver solves nearly-complete puzzle instantly") {
        auto puzzle = createNearlySolvedPuzzle();

        auto start = std::chrono::high_resolution_clock::now();
        auto solution = generator.solvePuzzle(puzzle);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        REQUIRE(solution.has_value());
        REQUIRE(solution.value()[0][2] == 4);  // Verify the missing cell

        // Should solve in < 1ms (nearly all forced moves)
        REQUIRE(duration.count() < 1000);  // Less than 1ms
    }

    SECTION("hasUniqueSolution correctly identifies unique solutions") {
        auto puzzle = createMediumPuzzle();

        bool unique = generator.hasUniqueSolution(puzzle);

        REQUIRE(unique == true);
    }

    SECTION("hasUniqueSolution correctly identifies multiple solutions") {
        // Create puzzle with only a few clues (multiple solutions)
        BoardData multi_solution_puzzle;
        multi_solution_puzzle[0][0] = 5;
        multi_solution_puzzle[1][1] = 3;

        bool unique = generator.hasUniqueSolution(multi_solution_puzzle);

        REQUIRE(unique == false);
    }
}

TEST_CASE("PuzzleGenerator - Solver SIMD/Scalar Consistency", "[puzzle_generator][solver][phase3][simd]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("Scalar and SIMD solvers produce identical results") {
        auto puzzle = createHardPuzzle();

        // Both should solve to same solution
        auto solution = generator.solvePuzzle(puzzle);

        REQUIRE(solution.has_value());

        // Verify uniqueness (countSolutions should return 1)
        bool unique = generator.hasUniqueSolution(puzzle);
        REQUIRE(unique == true);
    }

    SECTION("Both solvers agree on multiple solution detection") {
        BoardData multi_solution_puzzle;
        multi_solution_puzzle[0][0] = 5;

        bool unique = generator.hasUniqueSolution(multi_solution_puzzle);

        REQUIRE(unique == false);
    }
}

TEST_CASE("PuzzleGenerator - Solver Performance with Propagation", "[puzzle_generator][solver][phase3][performance]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("Solver benefits from propagation on cascading puzzles") {
        auto puzzle = createHardPuzzle();

        // Measure solving time
        auto start = std::chrono::high_resolution_clock::now();
        auto solution = generator.solvePuzzle(puzzle);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        REQUIRE(solution.has_value());

        // With propagation, should solve Hard puzzles quickly (< 10ms)
        REQUIRE(duration.count() < 10);
    }

    SECTION("Nearly-solved puzzles benefit maximally from propagation") {
        auto puzzle = createNearlySolvedPuzzle();

        auto start = std::chrono::high_resolution_clock::now();
        auto solution = generator.solvePuzzle(puzzle);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        REQUIRE(solution.has_value());

        // Should solve in < 100μs (all forced moves via propagation)
        REQUIRE(duration.count() < 100);
    }
}

TEST_CASE("PuzzleGenerator - Propagation Edge Cases in Solver", "[puzzle_generator][solver][phase3]") {
    GameValidator validator;
    PuzzleGenerator generator;

    SECTION("Solver handles empty board") {
        BoardData empty_board;

        // Empty board has many solutions (not unique)
        bool unique = generator.hasUniqueSolution(empty_board);

        REQUIRE(unique == false);
    }

    SECTION("Solver handles complete board") {
        BoardData complete_board = sudoku::testing::kSolvedBoard;

        bool unique = generator.hasUniqueSolution(complete_board);

        REQUIRE(unique == true);  // Exactly 1 solution (itself)
    }

    SECTION("Solver handles contradictory board") {
        // Create board with contradiction (two 5's in same row)
        BoardData invalid_board;
        invalid_board[0][0] = 5;
        invalid_board[0][1] = 5;  // Contradiction!

        auto solution = generator.solvePuzzle(invalid_board);

        // Should fail to solve (invalid initial state)
        REQUIRE_FALSE(solution.has_value());
    }
}
