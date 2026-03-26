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

#include "../../src/core/puzzle_generator.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// NOTE: This file tests hasUniqueSolution correctness across various puzzle types.
// Performance benchmarking should be done separately with profiling tools, not unit tests.
// Timing assertions have been removed to prevent CI flakiness on slow systems.

TEST_CASE("hasUniqueSolution - Correctness Verification", "[puzzle_generator]") {
    PuzzleGenerator generator;

    SECTION("Complete board (1 solution)") {
        // A complete solved board should have exactly 1 solution (itself)
        BoardData complete_board = sudoku::testing::kSolvedBoard;

        bool unique = generator.hasUniqueSolution(complete_board);
        REQUIRE(unique);
    }

    SECTION("Empty board (multiple solutions)") {
        // Empty board has many solutions
        BoardData empty_board;

        bool unique = generator.hasUniqueSolution(empty_board);
        REQUIRE_FALSE(unique);  // Empty board has multiple solutions
    }

    SECTION("Minimal puzzle (17 clues)") {
        // One of the hardest known 17-clue puzzles
        // This puzzle has a unique solution
        BoardData minimal_puzzle = {
            {0, 0, 0, 7, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 4, 3, 0, 2, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 6}, {0, 0, 0, 5, 0, 9, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 4, 1, 8},
            {0, 0, 0, 0, 8, 1, 0, 0, 0}, {0, 0, 2, 0, 0, 0, 0, 5, 0}, {0, 4, 0, 0, 0, 0, 3, 0, 0}};

        bool unique = generator.hasUniqueSolution(minimal_puzzle);
        REQUIRE(unique);
    }

    SECTION("Easy puzzle (~40 clues)") {
        BoardData easy_puzzle = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                                 {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                                 {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        bool unique = generator.hasUniqueSolution(easy_puzzle);
        REQUIRE(unique);
    }

    SECTION("Medium puzzle (~30 clues)") {
        BoardData medium_puzzle = {
            {0, 0, 0, 6, 0, 0, 4, 0, 0}, {7, 0, 0, 0, 0, 3, 6, 0, 0}, {0, 0, 0, 0, 9, 1, 0, 8, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 5, 0, 1, 8, 0, 0, 0, 3}, {0, 0, 0, 3, 0, 6, 0, 4, 5},
            {0, 4, 0, 2, 0, 0, 0, 6, 0}, {9, 0, 3, 0, 0, 0, 0, 0, 0}, {0, 2, 0, 0, 0, 0, 1, 0, 0}};

        bool unique = generator.hasUniqueSolution(medium_puzzle);
        REQUIRE(unique);
    }

    SECTION("Hard puzzle (~25 clues)") {
        BoardData hard_puzzle = {{0, 0, 0, 0, 0, 0, 0, 1, 2}, {0, 0, 0, 0, 3, 5, 0, 0, 0}, {0, 0, 0, 6, 0, 0, 0, 7, 0},
                                 {7, 0, 0, 0, 0, 0, 3, 0, 0}, {0, 0, 0, 4, 0, 0, 8, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 0},
                                 {0, 0, 0, 1, 2, 0, 0, 0, 0}, {0, 8, 0, 0, 0, 0, 0, 4, 0}, {0, 5, 0, 0, 0, 0, 6, 0, 0}};

        bool unique = generator.hasUniqueSolution(hard_puzzle);
        REQUIRE(unique);
    }

    SECTION("Nearly empty puzzle (multiple solutions)") {
        // A puzzle with very few clues - guaranteed to have multiple solutions
        BoardData nearly_empty = {
            {5, 3, 0, 0, 0, 0, 0, 0, 0}, {6, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};

        bool unique = generator.hasUniqueSolution(nearly_empty);
        REQUIRE_FALSE(unique);  // Should detect non-unique solution (multiple solutions exist)
    }
}
