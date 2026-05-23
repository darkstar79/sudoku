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

#include "../../src/core/puzzle_generator.h"
#include "../helpers/test_utils.h"

#include <chrono>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("Constraint Propagation Performance", "[puzzle_generator][benchmark][.performance]") {
    PuzzleGenerator generator;

    SECTION("Easy puzzle - baseline (minimal gain expected)") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Easy;
        settings.seed = 12345;
        settings.ensure_unique = true;
        settings.max_attempts = 10;

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Easy puzzle WITH constraint propagation: " << duration.count() << "ms");
        INFO("Baseline (memoization only): ~0ms");

        REQUIRE(result.has_value());
        REQUIRE(duration.count() < 100);  // Should remain instant
    }

    SECTION("Medium puzzle - moderate gain expected") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Medium;
        settings.seed = 54321;
        settings.ensure_unique = true;
        settings.max_attempts = 100;

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Medium puzzle WITH constraint propagation: " << duration.count() << "ms");
        INFO("Baseline (memoization only): ~9ms");
        INFO("Expected: 20-30% faster (~6-7ms)");

        REQUIRE(result.has_value());
        REQUIRE(duration.count() < 8000);  // Target < 8ms
    }

    SECTION("Hard puzzle - significant gain expected (PRIMARY TARGET)") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Hard;
        settings.seed = 99999;
        settings.ensure_unique = true;
        settings.max_attempts = 1000;  // Hard puzzles need many attempts (increased from 100)

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Hard puzzle WITH constraint propagation: " << duration.count() << "ms");
        INFO("Baseline (memoization only): 239ms");
        INFO("Expected: 30-50% faster (120-170ms)");

        REQUIRE(result.has_value());
        REQUIRE(duration.count() < 3000);  // Target < 3s (well under roadmap goal)
    }

    SECTION("Expert puzzle - major gain expected") {
        GenerationSettings settings;
        settings.difficulty = Difficulty::Expert;
        settings.seed = 12345;
        settings.ensure_unique = true;
        settings.max_attempts = 1000;

        auto start = std::chrono::high_resolution_clock::now();
        auto result = generator.generatePuzzle(settings);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("Expert puzzle WITH constraint propagation: " << duration.count() << "ms");
        INFO("Baseline (memoization only): 33s (failed)");
        INFO("Expected: 50-70% faster, higher success rate");

        if (result.has_value()) {
            const auto& puzzle = result.value();
            REQUIRE(puzzle.difficulty == Difficulty::Expert);
            REQUIRE(generator.hasUniqueSolution(puzzle.board));
            REQUIRE(duration.count() < 20000);  // Target < 20s (vs 33s baseline)
        } else {
            WARN("Expert puzzle generation failed - constraint propagation may not be enough");
        }
    }
}

TEST_CASE("Naked Singles Detection During Solution Counting", "[puzzle_generator][constraint_propagation]") {
    PuzzleGenerator generator;

    SECTION("hasUniqueSolution with many forced moves") {
        // Create puzzle with many naked singles (heavily constrained)
        BoardData puzzle = sudoku::test::getEasyPuzzleWithPatterns();

        auto start = std::chrono::high_resolution_clock::now();
        bool unique = generator.hasUniqueSolution(puzzle);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        INFO("hasUniqueSolution WITH constraint propagation: " << duration.count() << "ms");
        INFO("Baseline (memoization only): ~0ms (already cached)");

        REQUIRE(unique);
        REQUIRE(duration.count() < 1000);  // Should be very fast
    }
}
