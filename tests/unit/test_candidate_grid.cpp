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

#include "../../src/core/candidate_grid.h"
#include "../../src/core/constraint_state.h"
#include "../helpers/candidate_test_utils.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

TEST_CASE("CandidateGrid - Construction matches ConstraintState", "[candidate_grid]") {
    auto board = getEasyPuzzleWithPatterns();
    CandidateGrid grid(board);
    ConstraintState state(board);

    SECTION("Candidates match ConstraintState for all cells") {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                REQUIRE(grid.getPossibleValuesMask(row, col) == state.getPossibleValuesMask(row, col));
                REQUIRE(grid.countPossibleValues(row, col) == state.countPossibleValues(row, col));
            }
        }
    }

    SECTION("isAllowed matches ConstraintState for all cells and values") {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
                    REQUIRE(grid.isAllowed(row, col, value) == state.isAllowed(row, col, value));
                }
            }
        }
    }

    SECTION("constraintState() accessor returns underlying state") {
        const auto& inner = grid.constraintState();
        REQUIRE(inner.getPossibleValuesMask(0, 0) == state.getPossibleValuesMask(0, 0));
    }
}

TEST_CASE("CandidateGrid - eliminateCandidate", "[candidate_grid]") {
    auto board = createEmptyBoard();
    // Place value 5 at (0,0) so row 0 has some constraints
    board[0][0] = 5;
    CandidateGrid grid(board);

    SECTION("Eliminating a candidate removes it from the cell") {
        // Cell (1,1) should have value 5 as candidate (no structural block from row/col/box of (1,1))
        // Actually cell (1,1) is in box 0 with value 5 at (0,0), so 5 is NOT a candidate
        // Use cell (1,3) which is outside box 0
        REQUIRE(grid.isAllowed(1, 3, 5));
        int before = grid.countPossibleValues(1, 3);

        grid.eliminateCandidate(1, 3, 5);

        REQUIRE_FALSE(grid.isAllowed(1, 3, 5));
        REQUIRE(grid.countPossibleValues(1, 3) == before - 1);
    }

    SECTION("Eliminating does not affect other cells") {
        REQUIRE(grid.isAllowed(2, 3, 5));
        REQUIRE(grid.isAllowed(1, 4, 5));

        grid.eliminateCandidate(1, 3, 5);

        // Other cells unaffected
        REQUIRE(grid.isAllowed(2, 3, 5));
        REQUIRE(grid.isAllowed(1, 4, 5));
    }

    SECTION("Double elimination is idempotent") {
        grid.eliminateCandidate(1, 3, 5);
        int count_after_first = grid.countPossibleValues(1, 3);

        grid.eliminateCandidate(1, 3, 5);
        REQUIRE(grid.countPossibleValues(1, 3) == count_after_first);
        REQUIRE_FALSE(grid.isAllowed(1, 3, 5));
    }

    SECTION("Multiple candidates can be eliminated from same cell") {
        REQUIRE(grid.isAllowed(1, 3, 3));
        REQUIRE(grid.isAllowed(1, 3, 7));

        grid.eliminateCandidate(1, 3, 3);
        grid.eliminateCandidate(1, 3, 7);

        REQUIRE_FALSE(grid.isAllowed(1, 3, 3));
        REQUIRE_FALSE(grid.isAllowed(1, 3, 7));
    }

    SECTION("Eliminating structurally blocked candidate is safe no-op") {
        // Value 5 is blocked in cell (0,1) by row constraint (5 at (0,0))
        REQUIRE_FALSE(grid.isAllowed(0, 1, 5));

        grid.eliminateCandidate(0, 1, 5);

        REQUIRE_FALSE(grid.isAllowed(0, 1, 5));
    }
}

TEST_CASE("CandidateGrid - placeValue", "[candidate_grid]") {
    auto board = createEmptyBoard();
    CandidateGrid grid(board);

    SECTION("Placing a value updates structural constraints") {
        REQUIRE(grid.isAllowed(0, 1, 5));  // Same row
        REQUIRE(grid.isAllowed(1, 0, 5));  // Same col
        REQUIRE(grid.isAllowed(1, 1, 5));  // Same box

        grid.placeValue(0, 0, 5);

        REQUIRE_FALSE(grid.isAllowed(0, 1, 5));  // Same row blocked
        REQUIRE_FALSE(grid.isAllowed(1, 0, 5));  // Same col blocked
        REQUIRE_FALSE(grid.isAllowed(1, 1, 5));  // Same box blocked
    }

    SECTION("Placing a value clears candidates for that cell") {
        grid.placeValue(0, 0, 5);

        // Filled cell should have no candidates
        REQUIRE(grid.countPossibleValues(0, 0) == 0);
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            REQUIRE_FALSE(grid.isAllowed(0, 0, value));
        }
    }

    SECTION("Placing does not block unrelated cells") {
        REQUIRE(grid.isAllowed(5, 5, 5));  // Different row, col, box

        grid.placeValue(0, 0, 5);

        REQUIRE(grid.isAllowed(5, 5, 5));  // Still allowed
    }
}

TEST_CASE("CandidateGrid - findHiddenSingle", "[candidate_grid]") {
    SECTION("Finds hidden single on board with patterns") {
        auto board = getEasyPuzzleWithPatterns();
        CandidateGrid grid(board);

        auto result = grid.findHiddenSingle(board);

        // Easy puzzle should have hidden singles
        if (result.has_value()) {
            auto [pos, value] = result.value();
            REQUIRE(pos.row < BOARD_SIZE);
            REQUIRE(pos.col < BOARD_SIZE);
            REQUIRE(value >= MIN_VALUE);
            REQUIRE(value <= MAX_VALUE);
            REQUIRE(board[pos.row][pos.col] == 0);             // Must be empty cell
            REQUIRE(grid.isAllowed(pos.row, pos.col, value));  // Must be valid candidate
        }
    }

    SECTION("Matches ConstraintState results when no eliminations applied") {
        auto board = getEasyPuzzleWithPatterns();
        CandidateGrid grid(board);
        ConstraintState state(board);

        auto grid_result = grid.findHiddenSingle(board);
        auto state_result = state.findHiddenSingle(board);

        // Both should find the same first hidden single
        REQUIRE(grid_result.has_value() == state_result.has_value());
        if (grid_result.has_value() && state_result.has_value()) {
            REQUIRE(grid_result->first == state_result->first);
            REQUIRE(grid_result->second == state_result->second);
        }
    }

    SECTION("Finds hidden single revealed by elimination") {
        // Create a board where elimination reveals a hidden single
        auto board = createEmptyBoard();
        // Set up row 0: place values 1-7, leaving cells (0,7) and (0,8) empty
        board[0][0] = 1;
        board[0][1] = 2;
        board[0][2] = 3;
        board[0][3] = 4;
        board[0][4] = 5;
        board[0][5] = 6;
        board[0][6] = 7;
        // Cells (0,7) and (0,8) can have 8 or 9

        CandidateGrid grid(board);

        // Both cells should have candidates {8, 9}
        REQUIRE(grid.isAllowed(0, 7, 8));
        REQUIRE(grid.isAllowed(0, 7, 9));
        REQUIRE(grid.isAllowed(0, 8, 8));
        REQUIRE(grid.isAllowed(0, 8, 9));

        // Eliminate 8 from cell (0,7) — now only 9 is possible there
        // This should make 8 a hidden single in cell (0,8) for row 0
        grid.eliminateCandidate(0, 7, 8);

        auto result = grid.findHiddenSingle(board);
        REQUIRE(result.has_value());

        auto [pos, value] = result.value();
        // The naked single at (0,7) with value 9 would be found first,
        // but findHiddenSingle looks for values with one cell, not cells with one candidate.
        // Value 8 can only go in (0,8) in row 0 → hidden single
        // Value 9 can only go in (0,7) in row 0 → hidden single
        // Which one is found first depends on iteration order (value 8 before 9)
        REQUIRE(((pos.row == 0 && pos.col == 8 && value == 8) || (pos.row == 0 && pos.col == 7 && value == 9)));
    }

    SECTION("Returns nullopt for complete board") {
        // Use a known valid complete Sudoku board
        BoardData board = sudoku::testing::kSolvedBoard;
        CandidateGrid grid(board);

        auto result = grid.findHiddenSingle(board);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("CandidateGrid - getPossibleValuesMask combines constraints", "[candidate_grid]") {
    auto board = createEmptyBoard();
    board[0][0] = 1;  // Structural constraint
    CandidateGrid grid(board);

    // Cell (0,1) can't have 1 (same row) — structural
    uint16_t mask_before = grid.getPossibleValuesMask(0, 1);
    REQUIRE((mask_before & (1 << 1)) == 0);  // Bit 1 not set

    // Cell (0,1) can have 5 — structural allows it
    REQUIRE((mask_before & (1 << 5)) != 0);

    // Now eliminate 5 from (0,1)
    grid.eliminateCandidate(0, 1, 5);
    uint16_t mask_after = grid.getPossibleValuesMask(0, 1);

    // Both structural (1) and elimination (5) now blocked
    REQUIRE((mask_after & (1 << 1)) == 0);
    REQUIRE((mask_after & (1 << 5)) == 0);

    // Other values still present
    REQUIRE((mask_after & (1 << 2)) != 0);
}
