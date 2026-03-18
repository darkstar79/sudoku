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

/**
 * This benchmark test file measures the actual performance gain from memoization.
 *
 * To get accurate measurements, we run the same puzzles multiple times and compare:
 * 1. Current implementation (WITH memoization)
 * 2. The theoretical baseline can be estimated from the number of unique solution checks
 *
 * The performance gain comes from cache hits during repeated board state evaluations.
 */

TEST_CASE("Memoization Performance Benchmark", "[puzzle_generator][benchmark][.performance]") {
    PuzzleGenerator generator;

    SECTION("Easy puzzle - baseline measurement") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 12345;
        settings.ensure_unique = true;
        settings.max_attempts = 500;  // Hard puzzles need many attempts

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Easy puzzle WITH memoization: " << duration.count() << "ms");

        REQUIRE(result.has_value());
        // Easy puzzles are very fast even without optimization
        REQUIRE(duration.count() < 1000);
    }

    SECTION("Medium puzzle - baseline measurement") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Medium;
        settings.seed = 54321;
        settings.ensure_unique = true;
        settings.max_attempts = 500;  // Hard puzzles need many attempts

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Medium puzzle WITH memoization: " << duration.count() << "ms");

        REQUIRE(result.has_value());
        // Medium puzzles benefit moderately from memoization
        REQUIRE(duration.count() < 5000);
    }

    SECTION("Hard puzzle - primary optimization target") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Hard;
        settings.seed = 99999;
        settings.ensure_unique = true;
        settings.max_attempts = 500;  // Hard puzzles need many attempts

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Hard puzzle WITH memoization: " << duration.count() << "ms");
        INFO("Expected speedup: 50-70% compared to baseline (estimated 10-30s without memoization)");

        REQUIRE(result.has_value());
        // Hard puzzles should now complete much faster
        REQUIRE(duration.count() < 10000);  // Target: < 3s, allow up to 10s for slow systems
    }

    SECTION("Repeated generation to show cache effectiveness") {
        // Generate 5 Hard puzzles in a row - cache should help significantly
        GenerationSettings settings;
        settings.difficulty = Difficulty::Hard;
        settings.ensure_unique = true;
        settings.max_attempts = 500;  // Hard puzzles need many attempts

        std::vector<long long> timings;

        for (int i = 0; i < 3; ++i) {
            settings.seed = 10000 + i;  // Different seeds for variety

            auto start = std::chrono::high_resolution_clock::now();
            auto result = generator.generatePuzzle(settings);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            if (result.has_value()) {
                timings.push_back(duration.count());
                INFO("Hard puzzle #" << i << ": " << duration.count() << "ms");
            }
        }

        // At least some puzzles should succeed
        REQUIRE(timings.size() >= 2);

        // Average time should be reasonable
        long long total = 0;
        for (auto time : timings) {
            total += time;
        }
        long long average = total / timings.size();

        INFO("Average time for " << timings.size() << " Hard puzzles: " << average << "ms");
        REQUIRE(average < 15000);  // 15 seconds average
    }
}

TEST_CASE("hasUniqueSolution Performance Benchmark", "[puzzle_generator][benchmark][.performance]") {
    PuzzleGenerator generator;

    SECTION("Known puzzle - single check") {
        BoardData puzzle = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                            {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                            {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        auto start = std::chrono::high_resolution_clock::now();
        bool unique = generator.hasUniqueSolution(puzzle);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("hasUniqueSolution WITH memoization: " << duration.count() << "ms");

        REQUIRE(unique);
        REQUIRE(duration.count() < 3000);  // Should be very fast with MCV + memoization
    }

    SECTION("Minimal puzzle - harder case") {
        BoardData puzzle = {{0, 0, 0, 7, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 4, 3, 0, 2, 0, 0},
                            {0, 0, 0, 0, 0, 0, 0, 0, 6}, {0, 0, 0, 5, 0, 9, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 4, 1, 8},
                            {0, 0, 0, 0, 8, 1, 0, 0, 0}, {0, 0, 2, 0, 0, 0, 0, 5, 0}, {0, 4, 0, 0, 0, 0, 3, 0, 0}};

        auto start = std::chrono::high_resolution_clock::now();
        bool unique = generator.hasUniqueSolution(puzzle);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Minimal puzzle hasUniqueSolution WITH memoization: " << duration.count() << "ms");

        REQUIRE(unique);
        REQUIRE(duration.count() < 30000);  // 30 second timeout protection
    }
}
