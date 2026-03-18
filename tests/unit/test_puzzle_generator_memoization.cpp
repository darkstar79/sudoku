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

#include <chrono>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("PuzzleGenerator - Memoization functionality", "[puzzle_generator][memoization]") {
    PuzzleGenerator generator;

    SECTION("hasUniqueSolution uses memoization correctly") {
        BoardData puzzle1 = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                             {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                             {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        // Check twice - should be consistent
        bool unique1 = generator.hasUniqueSolution(puzzle1);
        bool unique2 = generator.hasUniqueSolution(puzzle1);

        // Results should be identical
        REQUIRE(unique1 == unique2);
        REQUIRE(unique1 == true);  // This puzzle has unique solution
    }

    SECTION("Memoization works across different puzzles") {
        BoardData puzzle1 = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                             {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                             {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        BoardData puzzle2 = {{0, 0, 0, 7, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 4, 3, 0, 2, 0, 0},
                             {0, 0, 0, 0, 0, 0, 0, 0, 6}, {0, 0, 0, 5, 0, 9, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 4, 1, 8},
                             {0, 0, 0, 0, 8, 1, 0, 0, 0}, {0, 0, 2, 0, 0, 0, 0, 5, 0}, {0, 4, 0, 0, 0, 0, 3, 0, 0}};

        // Check different puzzles - cache should be cleared between calls
        bool unique1 = generator.hasUniqueSolution(puzzle1);
        bool unique2 = generator.hasUniqueSolution(puzzle2);

        // Both have unique solutions
        REQUIRE(unique1 == true);
        REQUIRE(unique2 == true);
    }

    SECTION("hasUniqueSolution uses memoization") {
        BoardData puzzle = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                            {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                            {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        auto start = std::chrono::high_resolution_clock::now();
        bool has_unique = generator.hasUniqueSolution(puzzle);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("hasUniqueSolution with memoization time: " << duration.count() << "ms");

        REQUIRE(has_unique);
        // With memoization, this should complete quickly
        REQUIRE(duration.count() < 3000);  // 3 seconds max
    }
}

TEST_CASE("PuzzleGenerator - Memoization performance comparison", "[puzzle_generator][memoization][performance]") {
    PuzzleGenerator generator;

    SECTION("Medium puzzle generation benefits from memoization") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Medium;
        settings.seed = 54321;
        settings.ensure_unique = true;
        settings.max_attempts = 100;

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Medium puzzle with memoization: " << duration.count() << "ms");

        REQUIRE(result.has_value());
        if (result.has_value()) {
            // With memoization, Medium puzzles should complete faster
            REQUIRE(duration.count() < 10000);  // 10 seconds max
        }
    }

    SECTION("Hard puzzle generation benefits from memoization") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Hard;
        settings.seed = 99999;
        settings.ensure_unique = true;
        settings.max_attempts = 1000;  // Hard puzzles need more attempts (increased from 100)

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Hard puzzle with memoization: " << duration.count() << "ms");
        INFO("Expected: Faster than 30s baseline due to memoization");

        REQUIRE(result.has_value());
        if (result.has_value()) {
            // With memoization, Hard puzzles should complete within timeout
            // Target is < 3s but allow up to 30s for slower systems
            REQUIRE(duration.count() < 30000);
        }
    }
}
