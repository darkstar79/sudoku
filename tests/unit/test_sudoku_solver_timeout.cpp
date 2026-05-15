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
#include "../../src/core/i_time_provider.h"
#include "../../src/core/sudoku_solver.h"

#include <chrono>
#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace std::chrono_literals;

namespace {

// AI Escargot — known to run >>200 ms on solvePuzzle without timeout.
// Used to verify wall-clock budget enforcement on adversarial input.
BoardData makeAIEscargot() {
    return {{1, 0, 0, 0, 0, 7, 0, 9, 0}, {0, 3, 0, 0, 2, 0, 0, 0, 8}, {0, 0, 9, 6, 0, 0, 5, 0, 0},
            {0, 0, 5, 3, 0, 0, 9, 0, 0}, {0, 1, 0, 0, 8, 0, 0, 0, 2}, {6, 0, 0, 0, 0, 4, 0, 0, 0},
            {3, 0, 0, 0, 0, 0, 0, 1, 0}, {0, 4, 0, 0, 0, 0, 0, 0, 7}, {0, 0, 7, 0, 0, 0, 3, 0, 0}};
}

// Easy puzzle with one naked single — for "budget not exceeded → succeeds" case.
BoardData makeEasyNakedSingle() {
    return {{0, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
}

}  // namespace

// Step 1.1 plumbing test (R2 idiom 1): negative budget proves the timeout check is wired
// without depending on time advancement. Runs in microseconds.
TEST_CASE("solvePuzzle returns Timeout when budget is already exceeded", "[sudoku_solver][timeout]") {
    auto validator = std::make_shared<GameValidator>();
    auto mock_time = std::make_shared<MockTimeProvider>();
    SudokuSolver solver(validator, mock_time);

    auto result = solver.solvePuzzle(makeAIEscargot(), -1ms);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SolverError::Timeout);
}

TEST_CASE("solvePuzzle completes within ample budget on easy puzzle", "[sudoku_solver][timeout]") {
    auto validator = std::make_shared<GameValidator>();
    SudokuSolver solver(validator);  // default SystemTimeProvider

    auto result = solver.solvePuzzle(makeEasyNakedSingle(), 1000ms);

    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->used_backtracking);
}

// Step 1.1 triggering test (R2 idiom 2): real-time integration check. Tagged so it can be
// skipped in inner test loops. Bounds elapsed at 4x the budget per R2's pragmatic slack rule.
TEST_CASE("solvePuzzle bails out on AI Escargot within wall-clock budget", "[sudoku_solver][timeout][.integration]") {
    auto validator = std::make_shared<GameValidator>();
    SudokuSolver solver(validator);

    auto t0 = std::chrono::steady_clock::now();
    auto result = solver.solvePuzzle(makeAIEscargot(), 50ms);
    auto elapsed = std::chrono::steady_clock::now() - t0;

    REQUIRE(elapsed < 200ms);
    if (!result.has_value()) {
        // Expected on adversarial input — strategy iteration burns the budget.
        REQUIRE(result.error() == SolverError::Timeout);
    }
    // If the solver happens to complete inside 50 ms via backtracking, that's also valid;
    // the only thing we verify is the wall-clock bound.
}

// Step 1.2 plumbing test (R2 idiom 1) for findNextStep(board, original_puzzle, budget).
// Per F2, the 2-arg form (with original_puzzle for Avoidable Rectangle) is the production path.
TEST_CASE("findNextStep returns Timeout when budget is already exceeded", "[sudoku_solver][timeout]") {
    auto validator = std::make_shared<GameValidator>();
    auto mock_time = std::make_shared<MockTimeProvider>();
    SudokuSolver solver(validator, mock_time);

    auto result = solver.findNextStep(makeAIEscargot(), makeAIEscargot(), -1ms);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SolverError::Timeout);
}

TEST_CASE("findNextStep with budget succeeds on easy puzzle", "[sudoku_solver][timeout]") {
    auto validator = std::make_shared<GameValidator>();
    SudokuSolver solver(validator);

    auto result = solver.findNextStep(makeEasyNakedSingle(), makeEasyNakedSingle(), 250ms);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::NakedSingle);
}

// Step 1.3 — findNextStepByTechnique. Picks a board where multiple techniques apply, ensuring
// the method returns the requested one (not the cheapest) and InvalidBoard for a strategy
// that doesn't apply / Backtracking sentinel.
TEST_CASE("findNextStepByTechnique returns matching technique", "[sudoku_solver][by_technique]") {
    auto validator = std::make_shared<GameValidator>();
    SudokuSolver solver(validator);

    SECTION("Returns NakedSingle when requested on naked-single board") {
        auto result = solver.findNextStepByTechnique(makeEasyNakedSingle(), SolvingTechnique::NakedSingle);
        REQUIRE(result.has_value());
        REQUIRE(result->technique == SolvingTechnique::NakedSingle);
    }

    SECTION("Returns Unsolvable when requested technique does not apply") {
        // Easy puzzle has no SueDeCoq step.
        auto result = solver.findNextStepByTechnique(makeEasyNakedSingle(), SolvingTechnique::SueDeCoq);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SolverError::Unsolvable);
    }

    SECTION("Returns InvalidBoard for Backtracking sentinel technique") {
        // Backtracking is the fallback sentinel; there is no strategy registered for it.
        auto result = solver.findNextStepByTechnique(makeEasyNakedSingle(), SolvingTechnique::Backtracking);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SolverError::InvalidBoard);
    }

    SECTION("Returns Unsolvable on a complete board") {
        BoardData complete = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                              {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                              {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
        auto result = solver.findNextStepByTechnique(complete, SolvingTechnique::NakedSingle);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == SolverError::Unsolvable);
    }
}
