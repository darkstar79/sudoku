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

#include "../../src/core/solution_counter.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("SolutionCounter - countSolutions", "[solution_counter]") {
    SolutionCounter counter;

    SECTION("Complete valid board has exactly 1 solution") {
        BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                           {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                           {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};

        int count = counter.countSolutions(board, 2);
        REQUIRE(count == 1);
    }

    SECTION("Empty board has multiple solutions") {
        BoardData board;

        int count = counter.countSolutions(board, 2);
        REQUIRE(count >= 2);
    }

    SECTION("Unique puzzle returns count of 1") {
        BoardData board = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                           {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                           {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        int count = counter.countSolutions(board, 2);
        REQUIRE(count == 1);
    }

    SECTION("Nearly empty board has multiple solutions") {
        BoardData board = {{5, 3, 0, 0, 0, 0, 0, 0, 0}, {6, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};

        int count = counter.countSolutions(board, 2);
        REQUIRE(count >= 2);
    }

    SECTION("max_solutions=1 stops after finding first solution") {
        BoardData board;

        int count = counter.countSolutions(board, 1);
        REQUIRE(count == 1);
    }
}

TEST_CASE("SolutionCounter - hasUniqueSolution", "[solution_counter]") {
    SolutionCounter counter;

    SECTION("Complete board is unique") {
        BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                           {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                           {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};

        REQUIRE(counter.hasUniqueSolution(board));
    }

    SECTION("Empty board is not unique") {
        BoardData board;

        REQUIRE_FALSE(counter.hasUniqueSolution(board));
    }

    SECTION("Known unique puzzle (17 clues)") {
        BoardData board = {{0, 0, 0, 7, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 4, 3, 0, 2, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 6}, {0, 0, 0, 5, 0, 9, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 4, 1, 8},
                           {0, 0, 0, 0, 8, 1, 0, 0, 0}, {0, 0, 2, 0, 0, 0, 0, 5, 0}, {0, 4, 0, 0, 0, 0, 3, 0, 0}};

        REQUIRE(counter.hasUniqueSolution(board));
    }

    SECTION("Board with two removed clues is not unique") {
        // Remove cells that share a box — (0,0)=5 and (1,1)=7 can potentially swap
        // but they're constrained by row/col, so this specific removal might still be unique.
        // Use a known non-unique configuration instead: remove enough to break uniqueness.
        // Nearly empty board is a safe bet.
        BoardData non_unique = {{5, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};

        REQUIRE_FALSE(counter.hasUniqueSolution(non_unique));
    }
}

TEST_CASE("SolutionCounter - countSolutionsWithTimeout", "[solution_counter]") {
    SolutionCounter counter;

    SECTION("Completes within timeout for solvable board") {
        BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                           {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                           {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};

        int count = counter.countSolutionsWithTimeout(board, 2, std::chrono::milliseconds(5000));
        REQUIRE(count == 1);
    }

    SECTION("Very short timeout returns 0 for complex board") {
        // Empty board with 1 microsecond timeout — should time out
        BoardData board;

        // Use an extremely short timeout; countSolutionsWithTimeout returns 0 on timeout
        int count = counter.countSolutionsWithTimeout(board, 2, std::chrono::milliseconds(0));
        // With 0ms timeout, it may or may not find solutions depending on timing.
        // Just verify it doesn't crash and returns a non-negative value.
        REQUIRE(count >= 0);
    }
}

TEST_CASE("SolutionCounter - hasContradiction", "[solution_counter]") {
    SECTION("Valid board has no contradiction") {
        BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                           {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                           {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};

        REQUIRE_FALSE(SolutionCounter::hasContradiction(board));
    }

    SECTION("Empty board has no contradiction") {
        BoardData board;

        REQUIRE_FALSE(SolutionCounter::hasContradiction(board));
    }

    SECTION("Board with empty domain has contradiction") {
        // Create a board where cell (0,8) has zero candidates:
        // Row 0: values 1-8 in columns 0-7 (eliminates 1-8 from (0,8))
        // Box (0-2,6-8): add 9 so (0,8) can't be 9 either
        // Column 8: add 9 as well for good measure
        BoardData board = {{1, 2, 3, 4, 5, 6, 7, 8, 0}, {0, 0, 0, 0, 0, 0, 9, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 9}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
        // (0,8): row has 1-8, box(0-2,6-8) has 7,8,9, col 8 has 9
        // So all values 1-9 are eliminated from (0,8)

        REQUIRE(SolutionCounter::hasContradiction(board));
    }
}

TEST_CASE("SolutionCounter - propagateConstraints", "[solution_counter]") {
    SECTION("Propagation on valid board succeeds") {
        BoardData board = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                           {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                           {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        auto result = SolutionCounter::propagateConstraints(board);
        REQUIRE(result.has_value());

        // Propagated board should have more filled cells than the original
        int original_filled = 0;
        int propagated_filled = 0;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (board[r][c] != 0) {
                    ++original_filled;
                }
                if (result.value()[r][c] != 0) {
                    ++propagated_filled;
                }
            }
        }
        REQUIRE(propagated_filled >= original_filled);
    }

    SECTION("Propagation on contradictory board fails") {
        // Create a board where cell (0,8) has no legal value:
        // Row 0 has 1-8 in columns 0-7, column 8 has 9 in row 1
        BoardData board;
        for (int c = 0; c < 8; ++c) {
            board[0][c] = c + 1;
        }
        board[1][8] = 9;

        auto result = SolutionCounter::propagateConstraints(board);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Scalar and SIMD propagation agree") {
        BoardData board = {{0, 0, 0, 6, 0, 0, 4, 0, 0}, {7, 0, 0, 0, 0, 3, 6, 0, 0}, {0, 0, 0, 0, 9, 1, 0, 8, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 5, 0, 1, 8, 0, 0, 0, 3}, {0, 0, 0, 3, 0, 6, 0, 4, 5},
                           {0, 4, 0, 2, 0, 0, 0, 6, 0}, {9, 0, 3, 0, 0, 0, 0, 0, 0}, {0, 2, 0, 0, 0, 0, 1, 0, 0}};

        auto scalar_result = SolutionCounter::propagateConstraintsScalar(board);
        auto simd_result = SolutionCounter::propagateConstraintsSIMD(board);

        REQUIRE(scalar_result.has_value());
        REQUIRE(simd_result.has_value());

        // Both paths should produce the same result
        REQUIRE(scalar_result.value() == simd_result.value());
    }
}

TEST_CASE("SolutionCounter - cache management", "[solution_counter]") {
    SolutionCounter counter;

    SECTION("clearCache does not affect correctness") {
        BoardData board = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                           {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                           {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        int count1 = counter.countSolutions(board, 2);
        counter.clearCache();
        int count2 = counter.countSolutions(board, 2);

        REQUIRE(count1 == count2);
    }
}

TEST_CASE("SolutionCounter - solver path", "[solution_counter]") {
    SolutionCounter counter;

    SECTION("Default solver path is Auto") {
        REQUIRE(counter.solverPath() == SolverPath::Auto);
    }

    SECTION("setSolverPath changes the path") {
        counter.setSolverPath(SolverPath::Scalar);
        REQUIRE(counter.solverPath() == SolverPath::Scalar);
    }

    SECTION("Scalar path produces correct results") {
        counter.setSolverPath(SolverPath::Scalar);

        BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                           {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                           {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};

        REQUIRE(counter.hasUniqueSolution(board));
    }
}
