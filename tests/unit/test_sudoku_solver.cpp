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

#include "../../src/core/constraint_state.h"
#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/sudoku_solver.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// Test helper: Create an easy puzzle with only naked singles
BoardData createEasyPuzzle() {
    // Near-complete board with one naked single
    BoardData board = {{0, 3, 4, 6, 7, 8, 9, 1, 2},  // R1C1 must be 5
                       {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7}, {8, 5, 9, 7, 6, 1, 4, 2, 3},
                       {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6}, {9, 6, 1, 5, 3, 7, 2, 8, 4},
                       {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    return board;
}

BoardData createCompleteBoard() {
    BoardData board = sudoku::testing::kSolvedBoard;
    return board;
}

TEST_CASE("SudokuSolver - Constructor and Dependencies", "[sudoku_solver]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();

    SECTION("Constructs with dependencies") {
        REQUIRE_NOTHROW(SudokuSolver(validator));
    }
}

TEST_CASE("SudokuSolver - findNextStep", "[sudoku_solver]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    SudokuSolver solver(validator);

    SECTION("Finds Full House in near-complete puzzle") {
        // R1C1 is the only empty cell in its row, column, and box — a Full House (SE 1.0), found
        // ahead of the Naked Single fallback (story 0b.4b; previously surfaced as NakedSingle 2.3).
        auto board = createEasyPuzzle();

        auto result = solver.findNextStep(board);

        REQUIRE(result.has_value());
        REQUIRE(result->type == SolveStepType::Placement);
        REQUIRE(result->technique == SolvingTechnique::FullHouse);
        REQUIRE(result->position.row == 0);
        REQUIRE(result->position.col == 0);
        REQUIRE(result->value == 5);
        REQUIRE(result->rating == 1.0);
    }

    SECTION("Returns error for complete board") {
        auto board = createCompleteBoard();

        auto result = solver.findNextStep(board);

        REQUIRE_FALSE(result.has_value());
        // Complete board has no next step
    }
}

TEST_CASE("SudokuSolver - solvePuzzle", "[sudoku_solver]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    SudokuSolver solver(validator);

    SECTION("Solves easy puzzle with only naked singles") {
        auto board = createEasyPuzzle();

        auto result = solver.solvePuzzle(board);

        REQUIRE(result.has_value());
        REQUIRE_FALSE(result->solve_path.empty());
        REQUIRE(result->solution[0][0] == 5);  // First cell filled

        // Verify solution is complete and valid
        for (size_t r = 0; r < 9; ++r) {
            for (size_t c = 0; c < 9; ++c) {
                REQUIRE(result->solution[r][c] != 0);
            }
        }
    }

    SECTION("Returns solution for already complete board") {
        auto board = createCompleteBoard();

        auto result = solver.solvePuzzle(board);

        REQUIRE(result.has_value());
        REQUIRE(result->solve_path.empty());  // No steps needed
        REQUIRE(result->solution == board);
    }
}

TEST_CASE("SudokuSolver - applyStep", "[sudoku_solver]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    SudokuSolver solver(validator);

    SECTION("Applies placement step") {
        auto board = createEasyPuzzle();
        SolveStep step{.type = SolveStepType::Placement,
                       .technique = SolvingTechnique::NakedSingle,
                       .position = Position{.row = 0, .col = 0},
                       .value = 5,
                       .eliminations = {},
                       .explanation = "Test",
                       .rating = 10,
                       .explanation_data = {}};

        bool success = solver.applyStep(board, step);

        REQUIRE(success);
        REQUIRE(board[0][0] == 5);
    }

    SECTION("Handles elimination step (no-op for now)") {
        auto board = createEasyPuzzle();
        SolveStep step{.type = SolveStepType::Elimination,
                       .technique = SolvingTechnique::NakedPair,
                       .position = Position{.row = 0, .col = 0},
                       .value = 0,
                       .eliminations = {Elimination{.position = Position{.row = 0, .col = 1}, .value = 3}},
                       .explanation = "Test",
                       .rating = 50,
                       .explanation_data = {}};

        bool success = solver.applyStep(board, step);

        // For now, eliminations might be no-op (board only tracks placements)
        REQUIRE(success);
    }
}

TEST_CASE("SudokuSolver - Strategy Chain Order", "[sudoku_solver]") {
    auto validator = std::make_shared<GameValidator>();
    SudokuSolver solver(validator);

    SECTION("Tries easier techniques first") {
        // Deterministic, generator-free board: box 0 (the top-left 3x3) is fully
        // emptied from the canonical solved board; every other cell is a given.
        // No region has exactly one empty cell, so this is NOT a Full House — the
        // solver must surface a Naked/Hidden Single from box 0 ahead of any harder
        // technique, exercising the easiest-first chain ordering deterministically.
        // clang-format off
        BoardData board = {
            {0, 0, 0, 6, 7, 8, 9, 1, 2}, {0, 0, 0, 1, 9, 5, 3, 4, 8}, {0, 0, 0, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
        // clang-format on
        REQUIRE(validator->validateBoard(board));

        auto step = solver.findNextStep(board);

        REQUIRE(step.has_value());
        REQUIRE(step->type == SolveStepType::Placement);
        // The chain surfaces the Naked Single at R1C1 (=5) — an easiest-tier single
        // (Full House / Naked / Hidden Single) ahead of any harder technique. If a
        // new easiest-tier (Class A) technique is ever added that applies here, this
        // pin will fail deterministically; update it (and the tier list above) then.
        REQUIRE(step->technique == SolvingTechnique::NakedSingle);
        REQUIRE(step->position.row == 0);
        REQUIRE(step->position.col == 0);
        REQUIRE(step->value == 5);
    }
}

// ============================================================================
// solvePuzzle with a board that is valid but has no solution (line 144)
// Row 0 has values 1-8, so cell(0,0) must be 9. But column 0 and box(0)
// already contain 9 (from row 1), so cell(0,0) has no valid value.
// This board passes validateBoard (no direct conflicts) but is unsolvable.
// ============================================================================

TEST_CASE("SudokuSolver - solvePuzzle with unsolvable board returns error", "[sudoku_solver]") {
    auto validator = std::make_shared<GameValidator>();
    SudokuSolver solver(validator);

    // Nearly-complete board with a contradiction: cell(0,0) must be 9
    // (row 0 has 1-8), but col 0 and box(0) already have 9, so no valid
    // value exists. Only 1 empty cell → backtracking fails immediately.
    // clang-format off
    BoardData unsolvable = {
        {0, 1, 2, 3, 4, 5, 6, 7, 8},  // row 0: (0,0) empty, needs 9
        {9, 4, 3, 8, 5, 7, 2, 6, 1},  // row 1: 9 in col 0 blocks (0,0)
        {5, 8, 7, 2, 6, 1, 9, 3, 4},  // rows 1-8: valid complete rows
        {8, 2, 1, 7, 3, 6, 4, 5, 9},
        {3, 7, 4, 5, 9, 8, 1, 2, 6},
        {6, 5, 9, 1, 2, 4, 3, 8, 7},
        {2, 3, 8, 4, 1, 9, 7, 6, 5},  // (NB: not a valid Sudoku — some
        {7, 9, 5, 6, 8, 3, 4, 1, 2},  // cols/boxes may conflict, but the
        {4, 6, 1, 9, 7, 2, 8, 5, 3},  // point is (0,0) has no candidate)
    };
    // clang-format on

    // solvePuzzle should fail: no strategy can place (0,0), backtracker also fails
    // This covers line 144: return std::unexpected(SolverError::Unsolvable)
    auto result = solver.solvePuzzle(unsolvable);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == SolverError::Unsolvable);
}

}  // anonymous namespace
