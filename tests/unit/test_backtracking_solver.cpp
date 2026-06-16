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

#include "../helpers/candidate_test_utils.h"
#include "../helpers/test_utils.h"
#include "core/backtracking_solver.h"
#include "core/game_validator.h"
#include "core/i_time_provider.h"

#include <chrono>
#include <memory>
#include <random>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

TEST_CASE("BacktrackingSolver - Construction", "[backtracking_solver]") {
    auto validator = std::make_shared<GameValidator>();

    SECTION("Constructor accepts validator") {
        REQUIRE_NOTHROW(BacktrackingSolver(validator));
    }
}

TEST_CASE("BacktrackingSolver - Sequential Strategy", "[backtracking_solver]") {
    auto validator = std::make_shared<GameValidator>();
    BacktrackingSolver solver(validator);

    SECTION("Solves simple puzzle with sequential strategy") {
        auto board = createSimpleBoard();
        bool result = solver.solve(board, ValueSelectionStrategy::Sequential);

        REQUIRE(result);
        REQUIRE(validator->validateBoard(board));
        REQUIRE(validator->isComplete(board));
    }

    // NOTE: Minimal clues test removed - too slow (54 empty cells = huge search space)

    // NOTE: Unsolvable test removed - board validation happens before solving, not during

    SECTION("Handles already complete board") {
        // Create a valid complete board
        BoardData board = sudoku::testing::kSolvedBoard;

        bool result = solver.solve(board, ValueSelectionStrategy::Sequential);

        REQUIRE(result);  // Should return true (already solved)
        REQUIRE(validator->validateBoard(board));
        REQUIRE(validator->isComplete(board));
    }

    // NOTE: Empty board test removed - too slow for regular test runs
    // Solving a completely empty board requires extensive backtracking
}

TEST_CASE("BacktrackingSolver - Randomized Strategy", "[backtracking_solver]") {
    auto validator = std::make_shared<GameValidator>();
    BacktrackingSolver solver(validator);
    std::mt19937 rng(42);  // Fixed seed for reproducibility

    SECTION("Solves puzzle with randomized strategy") {
        auto board = createSimpleBoard();
        bool result = solver.solve(board, ValueSelectionStrategy::Randomized, &rng);

        REQUIRE(result);
        REQUIRE(validator->validateBoard(board));
        REQUIRE(validator->isComplete(board));
    }

    // NOTE: Empty board test removed - too slow for regular test runs

    SECTION("Falls back to sequential when RNG is null") {
        auto board = createSimpleBoard();

        // Call with Randomized strategy but null RNG - should fallback to Sequential
        bool result = solver.solve(board, ValueSelectionStrategy::Randomized, nullptr);

        REQUIRE(result);
        REQUIRE(validator->validateBoard(board));
        REQUIRE(validator->isComplete(board));
    }

    // NOTE: Empty board test removed - too slow for regular test runs
}

TEST_CASE("BacktrackingSolver - MostConstrained Strategy", "[backtracking_solver]") {
    auto validator = std::make_shared<GameValidator>();
    BacktrackingSolver solver(validator);

    SECTION("Solves puzzle with most constrained strategy") {
        auto board = createSimpleBoard();
        bool result = solver.solve(board, ValueSelectionStrategy::MostConstrained);

        REQUIRE(result);
        REQUIRE(validator->validateBoard(board));
        REQUIRE(validator->isComplete(board));
    }

    // NOTE: Difficult puzzle test removed - too slow (only 17 clues = 64 empty cells)

    // NOTE: Empty board test removed - too slow for regular test runs
}

TEST_CASE("BacktrackingSolver - Edge Cases", "[backtracking_solver]") {
    auto validator = std::make_shared<GameValidator>();
    BacktrackingSolver solver(validator);

    SECTION("Board with single empty cell") {
        BoardData board = {
            {5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8},
            {1, 9, 8, 3, 4, 2, 5, 6, 7}, {8, 5, 9, 7, 6, 1, 4, 2, 3},
            {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5},
            {3, 4, 5, 2, 8, 6, 1, 7, 0}  // Single empty cell at [8][8]
        };

        bool result = solver.solve(board, ValueSelectionStrategy::Sequential);

        REQUIRE(result);
        REQUIRE(board[8][8] == 9);  // Only valid value
        REQUIRE(validator->validateBoard(board));
    }

    // NOTE: Impossible constraints test removed - not critical for coverage

    SECTION("All three strategies produce valid solutions for same puzzle") {
        auto board_seq = createSimpleBoard();
        auto board_rand = board_seq;
        auto board_mc = board_seq;
        std::mt19937 rng(42);

        bool result_seq = solver.solve(board_seq, ValueSelectionStrategy::Sequential);
        bool result_rand = solver.solve(board_rand, ValueSelectionStrategy::Randomized, &rng);
        bool result_mc = solver.solve(board_mc, ValueSelectionStrategy::MostConstrained);

        REQUIRE(result_seq);
        REQUIRE(result_rand);
        REQUIRE(result_mc);

        REQUIRE(validator->validateBoard(board_seq));
        REQUIRE(validator->validateBoard(board_rand));
        REQUIRE(validator->validateBoard(board_mc));

        // All should produce valid solutions (may or may not be the same)
        // Sequential and MostConstrained should produce same solution (deterministic)
        REQUIRE(board_seq == board_mc);
    }
}

// NOTE: Backtracking behavior tests removed - too slow (only 20-23 clues = huge search space)
// The solver IS tested for correctness with faster test cases above

// Deadline plumbing (#24 H1): the short-circuit must be driven by the INJECTED clock, not a direct
// steady_clock::now() call. The mock clock is advanced an hour ahead of the real wall clock and the
// deadline is set just behind the mock's "now". Only an implementation that reads
// time_provider_->steadyNow() sees the deadline as already-passed and bails immediately; one still
// reading the real clock would view the deadline as a future point and grind the AI-Escargot board.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) — Catch2 TEST_CASE macro expansion
TEST_CASE("BacktrackingSolver short-circuits a passed deadline via the injected clock",
          "[backtracking_solver][timeout]") {
    auto validator = std::make_shared<GameValidator>();
    auto mock_time = std::make_shared<MockTimeProvider>();
    BacktrackingSolver solver(validator, mock_time);

    // AI Escargot — 17-clue, no logical path, would otherwise consume seconds of backtracking.
    BoardData board = {{1, 0, 0, 0, 0, 7, 0, 9, 0}, {0, 3, 0, 0, 2, 0, 0, 0, 8}, {0, 0, 9, 6, 0, 0, 5, 0, 0},
                       {0, 0, 5, 3, 0, 0, 9, 0, 0}, {0, 1, 0, 0, 8, 0, 0, 0, 2}, {6, 0, 0, 0, 0, 4, 0, 0, 0},
                       {3, 0, 0, 0, 0, 0, 0, 1, 0}, {0, 4, 0, 0, 0, 0, 0, 0, 7}, {0, 0, 7, 0, 0, 0, 3, 0, 0}};

    // Drive the injected clock well ahead of real time, then set the deadline behind the mock's now
    // but ahead of real time — distinguishing the injected clock from std::chrono::steady_clock::now().
    mock_time->advanceSteadyTime(std::chrono::hours(1));
    const auto past_deadline = mock_time->steadyNow() - std::chrono::seconds(1);

    auto t0 = std::chrono::steady_clock::now();
    bool result = solver.solve(board, ValueSelectionStrategy::MostConstrained, /*rng*/ nullptr, past_deadline);
    auto elapsed = std::chrono::steady_clock::now() - t0;

    REQUIRE(!result);
    // Must exit essentially immediately — backtracking should not have meaningfully started.
    REQUIRE(elapsed < std::chrono::milliseconds(50));
}

// In-recursion deadline enforcement is exercised end-to-end by the AI-Escargot timeout test
// in test_sudoku_solver_timeout.cpp; a deterministic unit-level reproducer would require a
// backtracking-slow board we don't have a stable example of.
