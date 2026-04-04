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

#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/puzzle_rater.h"
#include "../../src/core/sudoku_solver.h"
#include "../../tests/helpers/memory_monitor.h"
#include "../../tests/helpers/test_utils.h"

#include <chrono>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

// Sanitizers add significant overhead (2-5x), so relax timing thresholds
#if defined(__SANITIZE_ADDRESS__)
inline constexpr int kRatingTimeoutSeconds = 60;
#elif defined(__has_feature)
#    if __has_feature(address_sanitizer)
inline constexpr int kRatingTimeoutSeconds = 60;
#    else
inline constexpr int kRatingTimeoutSeconds = 10;
#    endif
#else
inline constexpr int kRatingTimeoutSeconds = 10;
#endif

using namespace sudoku::core;
using namespace sudoku::test;

// ============================================================================
// Test Case 1: Rate Easy Puzzle with Monitoring
// ============================================================================

TEST_CASE("PuzzleRater - Easy puzzle completes without timeout", "[puzzle_rater][safety]") {
    // Setup
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    MemoryMonitor memory;
    memory.start();

    auto puzzle = getEasyPuzzleWithPatterns();

    // Execute with timing
    auto start = std::chrono::steady_clock::now();
    auto result = rater.ratePuzzle(puzzle);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Assertions
    REQUIRE(result.has_value());                 // Should succeed
    REQUIRE(elapsed < std::chrono::seconds(5));  // Should be fast

    // Memory assertion (only on Linux where monitoring works)
    size_t memory_increase = memory.getMemoryIncrease();
    if (memory_increase > 0) {                         // Monitoring is working
        REQUIRE(memory_increase < 100 * 1024 * 1024);  // < 100MB growth
        INFO("Memory increase: " << (memory_increase / 1024) << " KB");
    }

    INFO("Rating completed in " << elapsed_ms << "ms");
}

// ============================================================================
// Test Case 2: Rate Medium/Hard Puzzles
// ============================================================================

TEST_CASE("PuzzleRater - Medium/Hard puzzles complete safely", "[puzzle_rater][safety]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    SECTION("Medium puzzle") {
        MemoryMonitor memory;
        memory.start();

        auto puzzle = getMediumPuzzle();

        auto start = std::chrono::steady_clock::now();
        auto result = rater.ratePuzzle(puzzle);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        // Assertions
        REQUIRE(result.has_value());                                     // Should succeed
        REQUIRE(elapsed < std::chrono::seconds(kRatingTimeoutSeconds));  // Reasonable timeout for medium

        size_t memory_increase = memory.getMemoryIncrease();
        if (memory_increase > 0) {
            REQUIRE(memory_increase < 200 * 1024 * 1024);  // < 200MB growth
            INFO("Memory increase: " << (memory_increase / 1024) << " KB");
        }

        INFO("Rating completed in " << elapsed_ms << "ms");
    }
}

// ============================================================================
// Test Case 3: Additional Pathological Puzzles
// ============================================================================

TEST_CASE("PuzzleRater - Easter Monster (pathological case)", "[puzzle_rater][pathological][.][manual]") {
    // Easter Monster - one of the hardest known Sudoku puzzles.
    // Tagged [.] to exclude from default test runs (very slow under sanitizers).
    // Run explicitly with: ./unit_tests "[manual]"

    auto validator = std::make_shared<GameValidator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    BoardData easter_monster = {{0, 0, 0, 0, 0, 0, 0, 1, 0}, {4, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 2, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 5, 0, 4, 0, 7}, {0, 0, 8, 0, 0, 0, 3, 0, 0}, {0, 0, 1, 0, 9, 0, 0, 0, 0},
                                {3, 0, 0, 4, 0, 0, 2, 0, 0}, {0, 5, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 8, 0, 6, 0, 0, 0}};

    MemoryMonitor memory;
    memory.start();

    auto start = std::chrono::steady_clock::now();
    auto result = rater.ratePuzzle(easter_monster);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    REQUIRE(elapsed < std::chrono::seconds(kRatingTimeoutSeconds));

    size_t memory_increase = memory.getMemoryIncrease();
    if (memory_increase > 0) {
        REQUIRE(memory_increase < 1024UL * 1024 * 1024);  // < 1GB
        INFO("Memory increase: " << (memory_increase / (1024UL * 1024)) << " MB");
    }

    INFO("Easter Monster rated in " << elapsed_ms << "ms");
    REQUIRE(
        (result.has_value() || result.error() == RatingError::Timeout || result.error() == RatingError::Unsolvable));
}

TEST_CASE("PuzzleRater - Inkala 2012 (pathological case)", "[puzzle_rater][pathological][.][manual]") {
    // Inkala 2012 - Arto Inkala's "world's hardest" Sudoku puzzle.
    // Tagged [.] to exclude from default test runs (very slow under sanitizers).
    // Run explicitly with: ./unit_tests "[manual]"

    auto validator = std::make_shared<GameValidator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    BoardData inkala_2012 = {{8, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 3, 6, 0, 0, 0, 0, 0}, {0, 7, 0, 0, 9, 0, 2, 0, 0},
                             {0, 5, 0, 0, 0, 7, 0, 0, 0}, {0, 0, 0, 0, 4, 5, 7, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 3, 0},
                             {0, 0, 1, 0, 0, 0, 0, 6, 8}, {0, 0, 8, 5, 0, 0, 0, 1, 0}, {0, 9, 0, 0, 0, 0, 4, 0, 0}};

    MemoryMonitor memory;
    memory.start();

    auto start = std::chrono::steady_clock::now();
    auto result = rater.ratePuzzle(inkala_2012);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    REQUIRE(elapsed < std::chrono::seconds(kRatingTimeoutSeconds));

    size_t memory_increase = memory.getMemoryIncrease();
    if (memory_increase > 0) {
        REQUIRE(memory_increase < 1024UL * 1024 * 1024);  // < 1GB
        INFO("Memory increase: " << (memory_increase / (1024UL * 1024)) << " MB");
    }

    INFO("Inkala 2012 rated in " << elapsed_ms << "ms");
    REQUIRE(
        (result.has_value() || result.error() == RatingError::Timeout || result.error() == RatingError::Unsolvable));
}

// ============================================================================
// Test Case 4: Pathological Puzzle Detection
// ============================================================================

TEST_CASE("PuzzleRater - Minimal clue puzzle (pathological case)", "[puzzle_rater][reproduction][pathological]") {
    // Test with a very sparse puzzle (only 17 clues - minimum for unique solution)
    // This could trigger deep recursion in countSolutionsHelper during rating

    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    SECTION("Minimal valid Sudoku (17 clues)") {
        // Famous "Golden Nugget" - one of the hardest known 17-clue puzzles
        BoardData minimal_puzzle = {
            {0, 0, 0, 0, 0, 0, 0, 1, 2}, {0, 0, 0, 0, 0, 0, 0, 0, 3}, {0, 0, 2, 3, 0, 0, 4, 0, 0},
            {0, 0, 1, 8, 0, 0, 0, 0, 5}, {0, 6, 0, 0, 7, 0, 8, 0, 0}, {0, 0, 0, 0, 0, 9, 0, 0, 0},
            {0, 0, 8, 5, 0, 0, 0, 0, 0}, {9, 0, 0, 0, 4, 0, 5, 0, 0}, {4, 7, 0, 0, 0, 6, 0, 0, 0}};

        MemoryMonitor memory;
        memory.start();

        auto start = std::chrono::steady_clock::now();
        auto result = rater.ratePuzzle(minimal_puzzle);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        // This test will FAIL if bug exists (timeout or memory explosion)
        REQUIRE(elapsed < std::chrono::seconds(kRatingTimeoutSeconds));  // Generous timeout

        size_t memory_increase = memory.getMemoryIncrease();
        if (memory_increase > 0) {
            REQUIRE(memory_increase < 1024UL * 1024 * 1024);  // < 1GB
            INFO("Memory increase: " << (memory_increase / (1024UL * 1024)) << " MB");
        }

        INFO("Rating completed in " << elapsed_ms << "ms");

        // Rating should either succeed or return timeout/unsolvable error
        REQUIRE((result.has_value() || result.error() == RatingError::Timeout ||
                 result.error() == RatingError::Unsolvable));
    }

    SECTION("AI Escargot - World's hardest Sudoku") {
        // "AI Escargot" - claimed to be one of the world's hardest Sudokus
        // This puzzle requires very advanced techniques and could trigger deep solver recursion
        BoardData ai_escargot = {{1, 0, 0, 0, 0, 7, 0, 9, 0}, {0, 3, 0, 0, 2, 0, 0, 0, 8}, {0, 0, 9, 6, 0, 0, 5, 0, 0},
                                 {0, 0, 5, 3, 0, 0, 9, 0, 0}, {0, 1, 0, 0, 8, 0, 0, 0, 2}, {6, 0, 0, 0, 0, 4, 0, 0, 0},
                                 {3, 0, 0, 0, 0, 0, 0, 1, 0}, {0, 4, 0, 0, 0, 0, 0, 0, 7}, {0, 0, 7, 0, 0, 0, 3, 0, 0}};

        MemoryMonitor memory;
        memory.start();

        auto start = std::chrono::steady_clock::now();
        auto result = rater.ratePuzzle(ai_escargot);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        REQUIRE(elapsed < std::chrono::seconds(kRatingTimeoutSeconds));

        size_t memory_increase = memory.getMemoryIncrease();
        if (memory_increase > 0) {
            REQUIRE(memory_increase < 1024UL * 1024 * 1024);  // < 1GB
            INFO("Memory increase: " << (memory_increase / (1024UL * 1024)) << " MB");
        }

        INFO("AI Escargot rated in " << elapsed_ms << "ms");
        REQUIRE((result.has_value() || result.error() == RatingError::Timeout ||
                 result.error() == RatingError::Unsolvable));
    }

    SECTION("Platinum Blonde - Extremely difficult puzzle") {
        // "Platinum Blonde" - another notoriously difficult puzzle
        BoardData platinum_blonde = {
            {0, 0, 0, 0, 0, 0, 0, 1, 2}, {0, 0, 0, 0, 3, 5, 0, 0, 0}, {0, 0, 0, 6, 0, 0, 0, 7, 0},
            {7, 0, 0, 0, 0, 0, 3, 0, 0}, {0, 0, 0, 4, 0, 0, 8, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 1, 2, 0, 0, 0, 0}, {0, 8, 0, 0, 0, 0, 0, 4, 0}, {0, 5, 0, 0, 0, 0, 6, 0, 0}};

        MemoryMonitor memory;
        memory.start();

        auto start = std::chrono::steady_clock::now();
        auto result = rater.ratePuzzle(platinum_blonde);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        REQUIRE(elapsed < std::chrono::seconds(kRatingTimeoutSeconds));

        size_t memory_increase = memory.getMemoryIncrease();
        if (memory_increase > 0) {
            REQUIRE(memory_increase < 1024UL * 1024 * 1024);  // < 1GB
            INFO("Memory increase: " << (memory_increase / (1024UL * 1024)) << " MB");
        }

        INFO("Platinum Blonde rated in " << elapsed_ms << "ms");
        REQUIRE((result.has_value() || result.error() == RatingError::Timeout));
    }
}

// ============================================================================
// Test Case 5: Unique Solution Verification (Direct countSolutions test)
// ============================================================================

TEST_CASE("PuzzleGenerator - hasUniqueSolution on pathological puzzles",
          "[puzzle_generator][pathological][deep_recursion]") {
    // The original bug might be in hasUniqueSolution (countSolutionsHelper)
    // not in rating. Test the unique solution check directly.

    auto generator = std::make_shared<PuzzleGenerator>();

    SECTION("Sparse puzzle with few clues") {
        // Very sparse puzzle (18 clues) - could trigger deep recursion
        BoardData sparse_puzzle = {
            {0, 0, 0, 0, 0, 0, 0, 1, 2}, {0, 0, 0, 0, 0, 0, 0, 0, 3}, {0, 0, 2, 3, 0, 0, 4, 0, 0},
            {0, 0, 1, 8, 0, 0, 0, 0, 5}, {0, 6, 0, 0, 7, 0, 8, 0, 0}, {0, 0, 0, 0, 0, 9, 0, 0, 0},
            {0, 0, 8, 5, 0, 0, 0, 0, 0}, {9, 0, 0, 0, 4, 0, 5, 0, 0}, {4, 7, 0, 0, 0, 6, 0, 0, 0}};

        MemoryMonitor memory;
        memory.start();

        auto start = std::chrono::steady_clock::now();
        // This calls countSolutions which uses countSolutionsHelper (the suspected culprit)
        bool has_unique = generator->hasUniqueSolution(sparse_puzzle);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        // Safety checks
        REQUIRE(elapsed < std::chrono::seconds(kRatingTimeoutSeconds));  // Should not hang

        size_t memory_increase = memory.getMemoryIncrease();
        if (memory_increase > 0) {
            REQUIRE(memory_increase < 1024UL * 1024 * 1024);  // < 1GB
            INFO("Memory increase: " << (memory_increase / (1024UL * 1024)) << " MB");
        }

        INFO("hasUniqueSolution completed in " << elapsed_ms
                                               << "ms, result: " << (has_unique ? "unique" : "not unique"));
    }
}

// ============================================================================
// Test Case 6: Expert Puzzle Generation (MANUAL TEST - VERY SLOW)
// ============================================================================

TEST_CASE("PuzzleRater - Expert puzzles (manual test)", "[puzzle_rater][expert][.][manual]") {
    // Expert puzzle generation is extremely slow (3-5 minutes per puzzle)
    // and probabilistic (may fail after 1000 attempts).
    // This test is tagged [.] to exclude it from default test runs.
    // Run explicitly with: ./unit_tests "[expert]"

    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    PuzzleRater rater(solver);

    INFO("Testing Expert difficulty (this will take 10-30 minutes)");

    int successful = 0;
    int failed = 0;

    for (int i = 0; i < 3; ++i) {
        INFO("Attempting Expert puzzle " << (i + 1) << "/3...");

        MemoryMonitor memory;
        memory.start();

        auto puzzle_result = generator->generatePuzzle({.difficulty = Difficulty::Expert});

        if (!puzzle_result.has_value()) {
            failed++;
            INFO("Expert puzzle " << (i + 1) << " generation failed (expected occasionally)");
            continue;
        }

        successful++;

        auto start = std::chrono::steady_clock::now();
        auto rating_result = rater.ratePuzzle(puzzle_result->board);
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        REQUIRE(elapsed < std::chrono::seconds(300));  // 5 minute timeout

        size_t memory_increase = memory.getMemoryIncrease();
        if (memory_increase > 0 && memory_increase > 500UL * 1024 * 1024) {
            FAIL("Memory usage exceeded threshold: " << (memory_increase / (1024UL * 1024)) << "MB");
        }

        INFO("Expert puzzle " << (i + 1) << " rated in " << elapsed_ms << "ms, memory: " << (memory_increase / 1024)
                              << "KB");

        REQUIRE((rating_result.has_value() || rating_result.error() == RatingError::Timeout ||
                 rating_result.error() == RatingError::Unsolvable));
    }

    INFO("Expert puzzle test summary: " << successful << " successful, " << failed << " failed");
    // At least one should succeed
    REQUIRE(successful > 0);
}
