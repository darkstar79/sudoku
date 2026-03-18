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

/**
 * Tests for ConstraintState - optimized constraint tracking with bitmasks.
 *
 * ConstraintState provides O(1) validation by maintaining bitmasks of
 * which values are used in each row, column, and box.
 */

#include "core/board_utils.h"
#include "core/constraint_state.h"
#include "core/i_game_validator.h"  // For Position definition

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("ConstraintState - Initialization", "[constraint_state]") {
    SECTION("Empty board initializes with no constraints") {
        BoardData empty_board;
        ConstraintState state(empty_board);

        // All values should be allowed everywhere
        for (size_t row = 0; row < 9; ++row) {
            for (size_t col = 0; col < 9; ++col) {
                for (int value = 1; value <= 9; ++value) {
                    REQUIRE(state.isAllowed(row, col, value));
                }
            }
        }
    }

    SECTION("Partially filled board initializes correctly") {
        BoardData board;
        board[0][0] = 5;  // Place 5 at (0,0)
        board[4][4] = 9;  // Place 9 at (4,4)

        ConstraintState state(board);

        // Value 5 should not be allowed in row 0, col 0, or box 0
        REQUIRE_FALSE(state.isAllowed(0, 1, 5));  // Same row
        REQUIRE_FALSE(state.isAllowed(1, 0, 5));  // Same column
        REQUIRE_FALSE(state.isAllowed(1, 1, 5));  // Same box

        // Value 5 should be allowed elsewhere
        REQUIRE(state.isAllowed(4, 4, 5));  // Different row/col/box

        // Value 9 should not be allowed in row 4, col 4, or box 4
        REQUIRE_FALSE(state.isAllowed(4, 5, 9));  // Same row
        REQUIRE_FALSE(state.isAllowed(5, 4, 9));  // Same column
        REQUIRE_FALSE(state.isAllowed(3, 3, 9));  // Same box

        // Value 9 should be allowed elsewhere
        REQUIRE(state.isAllowed(0, 8, 9));  // Different row/col/box
    }
}

TEST_CASE("ConstraintState - Place and Remove", "[constraint_state]") {
    BoardData empty_board;
    ConstraintState state(empty_board);

    SECTION("placeValue updates constraints correctly") {
        // Initially, value 7 is allowed everywhere
        REQUIRE(state.isAllowed(0, 0, 7));
        REQUIRE(state.isAllowed(0, 5, 7));
        REQUIRE(state.isAllowed(5, 0, 7));
        REQUIRE(state.isAllowed(1, 1, 7));

        // Place 7 at (0,0)
        state.placeValue(0, 0, 7);

        // Value 7 should no longer be allowed in row 0, col 0, or box 0
        REQUIRE_FALSE(state.isAllowed(0, 5, 7));  // Same row
        REQUIRE_FALSE(state.isAllowed(5, 0, 7));  // Same column
        REQUIRE_FALSE(state.isAllowed(1, 1, 7));  // Same box

        // Value 7 should still be allowed elsewhere
        REQUIRE(state.isAllowed(5, 5, 7));  // Different row/col/box
    }

    SECTION("removeValue restores constraints correctly") {
        // Place and then remove
        state.placeValue(0, 0, 7);
        REQUIRE_FALSE(state.isAllowed(0, 5, 7));

        state.removeValue(0, 0, 7);

        // Constraints should be restored
        REQUIRE(state.isAllowed(0, 5, 7));
        REQUIRE(state.isAllowed(5, 0, 7));
        REQUIRE(state.isAllowed(1, 1, 7));
    }

    SECTION("Multiple place/remove operations") {
        state.placeValue(0, 0, 1);
        state.placeValue(0, 1, 2);
        state.placeValue(0, 2, 3);

        // Row 0 has 1, 2, 3
        REQUIRE_FALSE(state.isAllowed(0, 8, 1));
        REQUIRE_FALSE(state.isAllowed(0, 8, 2));
        REQUIRE_FALSE(state.isAllowed(0, 8, 3));
        REQUIRE(state.isAllowed(0, 8, 4));

        // Remove middle value
        state.removeValue(0, 1, 2);

        // 2 should be allowed again in row 0
        REQUIRE(state.isAllowed(0, 8, 2));
        // But 1 and 3 still blocked
        REQUIRE_FALSE(state.isAllowed(0, 8, 1));
        REQUIRE_FALSE(state.isAllowed(0, 8, 3));
    }
}

TEST_CASE("ConstraintState - getPossibleValuesMask", "[constraint_state]") {
    BoardData board;
    // Fill row 0 with values 1-8
    for (int i = 0; i < 8; ++i) {
        board[0][i] = i + 1;
    }

    ConstraintState state(board);

    SECTION("Returns correct mask for constrained cell") {
        // Cell (0, 8) can only have value 9
        uint16_t mask = state.getPossibleValuesMask(0, 8);
        REQUIRE(mask == (1 << 9));  // Only bit 9 set
    }

    SECTION("Returns correct mask for unconstrained cell") {
        // Cell (8, 8) has no constraints from our setup
        uint16_t mask = state.getPossibleValuesMask(8, 8);
        // All values 1-9 should be possible
        REQUIRE(mask == 0b1111111110);  // Bits 1-9 set
    }

    SECTION("Empty cell in partially constrained position") {
        // Row 0 has 1-8, column 8 is empty, box 2 is empty
        // Cell (0, 8) can only be 9
        uint16_t mask = state.getPossibleValuesMask(0, 8);

        // Count set bits
        int count = 0;
        for (int i = 1; i <= 9; ++i) {
            if (mask & (1 << i)) {
                count++;
            }
        }
        REQUIRE(count == 1);
    }
}

TEST_CASE("ConstraintState - countPossibleValues", "[constraint_state]") {
    BoardData board;

    SECTION("Empty board has 9 possible values everywhere") {
        ConstraintState state(board);
        REQUIRE(state.countPossibleValues(0, 0) == 9);
        REQUIRE(state.countPossibleValues(4, 4) == 9);
        REQUIRE(state.countPossibleValues(8, 8) == 9);
    }

    SECTION("Constrained cell has fewer possible values") {
        // Fill row 0 with 1-8
        for (int i = 0; i < 8; ++i) {
            board[0][i] = i + 1;
        }
        ConstraintState state(board);

        // Cell (0, 8) can only be 9
        REQUIRE(state.countPossibleValues(0, 8) == 1);
    }

    SECTION("Heavily constrained cell") {
        // Create a situation with only 2 possible values
        board[0][0] = 1;
        board[0][1] = 2;
        board[0][2] = 3;
        board[0][3] = 4;
        board[0][4] = 5;
        board[0][5] = 6;
        board[0][6] = 7;
        // Cell (0, 8) can be 8 or 9

        ConstraintState state(board);
        REQUIRE(state.countPossibleValues(0, 8) == 2);
    }
}

TEST_CASE("ConstraintState - Box Index Calculation", "[constraint_state]") {
    BoardData board;
    ConstraintState state(board);

    SECTION("Box indices are correct") {
        // Place values in each box and verify constraint propagation
        // Box 0: rows 0-2, cols 0-2
        state.placeValue(0, 0, 1);
        REQUIRE_FALSE(state.isAllowed(1, 1, 1));  // Same box
        REQUIRE_FALSE(state.isAllowed(2, 2, 1));  // Same box
        REQUIRE_FALSE(state.isAllowed(0, 3, 1));  // Same row (different box)
        REQUIRE_FALSE(state.isAllowed(3, 0, 1));  // Same column (different box)
        REQUIRE(state.isAllowed(3, 3, 1));        // Different row, column, box

        // Box 4: rows 3-5, cols 3-5 (center box)
        state.placeValue(4, 4, 5);
        REQUIRE_FALSE(state.isAllowed(3, 3, 5));  // Same box
        REQUIRE_FALSE(state.isAllowed(5, 5, 5));  // Same box
        REQUIRE_FALSE(state.isAllowed(2, 4, 5));  // Same column (different box)
        REQUIRE_FALSE(state.isAllowed(4, 6, 5));  // Same row (different box)
        REQUIRE(state.isAllowed(2, 2, 5));        // Different row, column, box

        // Box 8: rows 6-8, cols 6-8 (bottom-right box)
        state.placeValue(8, 8, 9);
        REQUIRE_FALSE(state.isAllowed(6, 6, 9));  // Same box
        REQUIRE_FALSE(state.isAllowed(7, 7, 9));  // Same box
        REQUIRE_FALSE(state.isAllowed(5, 8, 9));  // Same column (different box)
        REQUIRE_FALSE(state.isAllowed(8, 5, 9));  // Same row (different box)
        REQUIRE(state.isAllowed(0, 3, 9));        // Different row, column, box
    }
}

TEST_CASE("ConstraintState - Edge Cases", "[constraint_state]") {
    SECTION("Value 0 handling") {
        BoardData board;
        board[0][0] = 0;  // Explicitly zero
        ConstraintState state(board);

        // Value 0 should not affect constraints
        for (int value = 1; value <= 9; ++value) {
            REQUIRE(state.isAllowed(0, 0, value));
        }
    }

    SECTION("Full row constraint") {
        BoardData board;
        // Fill entire row 0
        for (int col = 0; col < 9; ++col) {
            board[0][col] = col + 1;
        }
        ConstraintState state(board);

        // No values allowed in row 0 (all cells filled)
        // But values should be allowed in other rows based on column constraints
        for (int value = 1; value <= 9; ++value) {
            // Value should be blocked in its column
            REQUIRE_FALSE(state.isAllowed(5, value - 1, value));
        }
    }
}

TEST_CASE("ConstraintState - Performance Characteristics", "[constraint_state][.performance]") {
    BoardData board;
    ConstraintState state(board);

    SECTION("Many place/remove operations") {
        // Simulate backtracking behavior
        for (int iteration = 0; iteration < 1000; ++iteration) {
            for (int value = 1; value <= 9; ++value) {
                state.placeValue(0, 0, value);
                bool allowed = state.isAllowed(0, 1, value);
                REQUIRE_FALSE(allowed);
                state.removeValue(0, 0, value);
            }
        }
        // Should complete without issues
        REQUIRE(state.isAllowed(0, 0, 5));
    }
}

TEST_CASE("ConstraintState - Hidden Singles in Row", "[constraint_state][hidden_singles]") {
    SECTION("Hidden single in row - simple case") {
        /*
         * Row 0: [1, 2, 3, _, 5, 6, 7, 8, 9]
         * Position (0,3) is empty, value 4 can only go there
         * This is a hidden single for value 4 in row 0
         */
        BoardData board = {{1, 2, 3, 0, 5, 6, 7, 8, 9}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
        ConstraintState state(board);

        auto result = state.findHiddenSingleInRow(0, board);
        REQUIRE(result.has_value());
        auto [col, value] = result.value();
        REQUIRE(col == 3);
        REQUIRE(value == 4);
    }

    SECTION("Hidden single vs naked single") {
        /*
         * Row 1: [1, 2, _, _, 5, 6, 7, 8, 9]
         * Two empty cells at positions 2 and 3
         * Values 3 and 4 missing
         * If value 3 is blocked at position 3 (by column/box constraint),
         * then value 3 can only go at position 2 → hidden single
         */
        BoardData board = {{0, 0, 0, 3, 0, 0, 0, 0, 0}, {1, 2, 0, 0, 5, 6, 7, 8, 9}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
        ConstraintState state(board);

        auto result = state.findHiddenSingleInRow(1, board);
        REQUIRE(result.has_value());
        auto [col, value] = result.value();
        REQUIRE(col == 2);  // Value 3 can only go here
        REQUIRE(value == 3);
    }

    SECTION("No hidden single when value has multiple placements") {
        /*
         * Row 2: [1, 2, _, _, 5, 6, 7, 8, 9]
         * Values 3 and 4 can both go in either empty cell
         * No hidden single exists
         */
        BoardData board = {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 0, 0, 5, 6, 7, 8, 9},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
        ConstraintState state(board);

        auto result = state.findHiddenSingleInRow(2, board);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("No hidden single when row is complete") {
        BoardData board = {{0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {1, 2, 3, 4, 5, 6, 7, 8, 9}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
        ConstraintState state(board);

        auto result = state.findHiddenSingleInRow(3, board);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("ConstraintState - Hidden Singles in Column", "[constraint_state][hidden_singles]") {
    SECTION("Hidden single in column - simple case") {
        BoardData board;
        // Fill column 0 with values 1-8, leaving row 3 empty
        for (int row = 0; row < 9; ++row) {
            if (row != 3) {
                board[row][0] = (row < 3) ? (row + 1) : (row + 2);
            }
        }
        ConstraintState state(board);

        auto result = state.findHiddenSingleInCol(0, board);
        REQUIRE(result.has_value());
        auto [row, value] = result.value();
        REQUIRE(row == 3);
        REQUIRE(value == 4);  // Missing value is 4
    }

    SECTION("Hidden single with row constraint") {
        BoardData board;
        // Fill column 4 with values except 2 and 9 at rows 3 and 4
        board[0][4] = 1;
        board[1][4] = 3;
        board[2][4] = 5;
        // board[3][4] = empty (row 3, col 4)
        // board[4][4] = empty (row 4, col 4)
        board[5][4] = 6;
        board[6][4] = 7;
        board[7][4] = 8;
        board[8][4] = 4;

        // Block value 2 from row 3 by placing it elsewhere in row 3
        board[3][8] = 2;
        // Now value 2 can ONLY go at position (4,4) in column 4 (hidden single!)

        ConstraintState state(board);
        auto result = state.findHiddenSingleInCol(4, board);
        REQUIRE(result.has_value());
        auto [row, value] = result.value();
        REQUIRE(value == 2);
        REQUIRE(row == 4);
    }

    SECTION("No hidden single in empty column") {
        BoardData board;
        ConstraintState state(board);

        auto result = state.findHiddenSingleInCol(5, board);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("ConstraintState - Hidden Singles in Box", "[constraint_state][hidden_singles]") {
    SECTION("Hidden single in box - simple case") {
        BoardData board;
        // Fill box 0 (rows 0-2, cols 0-2) except position (1,1)
        board[0][0] = 1;
        board[0][1] = 2;
        board[0][2] = 3;
        board[1][0] = 4;
        // board[1][1] = empty (hidden single will be here)
        board[1][2] = 6;
        board[2][0] = 7;
        board[2][1] = 8;
        board[2][2] = 9;

        ConstraintState state(board);
        auto result = state.findHiddenSingleInBox(0, board);
        REQUIRE(result.has_value());
        auto [pos, value] = result.value();
        REQUIRE(pos.row == 1);
        REQUIRE(pos.col == 1);
        REQUIRE(value == 5);
    }

    SECTION("Hidden single with row/column constraints") {
        BoardData board;
        // Fill box 4 (rows 3-5, cols 3-5) with 8 values, leaving two cells empty
        board[3][3] = 1;
        board[3][4] = 2;
        board[3][5] = 3;
        board[4][3] = 4;
        // board[4][4] = empty
        board[4][5] = 6;
        board[5][3] = 7;
        // board[5][4] = empty
        board[5][5] = 9;

        // Block value 5 from position (4,4) by placing it in same row
        board[4][8] = 5;

        ConstraintState state(board);
        auto result = state.findHiddenSingleInBox(4, board);
        REQUIRE(result.has_value());
        auto [pos, value] = result.value();
        // Value 5 can only go at (5,4) due to row constraint
        REQUIRE(pos.row == 5);
        REQUIRE(pos.col == 4);
        REQUIRE(value == 5);
    }

    SECTION("No hidden single when multiple placements possible") {
        BoardData board;
        // Fill box 8 (rows 6-8, cols 6-8) partially
        board[6][6] = 1;
        board[6][7] = 2;
        // Leave multiple cells empty with no unique placement for any value
        ConstraintState state(board);

        auto result = state.findHiddenSingleInBox(8, board);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("ConstraintState - findHiddenSingle (All Regions)", "[constraint_state][hidden_singles]") {
    SECTION("Finds hidden single in row first") {
        BoardData board = {{1, 2, 3, 0, 5, 6, 7, 8, 9}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
        ConstraintState state(board);

        auto result = state.findHiddenSingle(board);
        REQUIRE(result.has_value());
        auto [pos, value] = result.value();
        REQUIRE(pos.row == 0);
        REQUIRE(pos.col == 3);
        REQUIRE(value == 4);
    }

    SECTION("Finds hidden single in column when no row hidden singles") {
        BoardData board;
        // No row hidden singles, but column 0 has one
        for (int row = 0; row < 9; ++row) {
            if (row != 5) {
                board[row][0] = (row < 5) ? (row + 1) : (row + 2);
            }
        }
        ConstraintState state(board);

        auto result = state.findHiddenSingle(board);
        REQUIRE(result.has_value());
        auto [pos, value] = result.value();
        REQUIRE(pos.row == 5);
        REQUIRE(pos.col == 0);
        REQUIRE(value == 6);
    }

    SECTION("Finds hidden single in box when no row/column hidden singles") {
        BoardData board;
        // Box 0 hidden single
        board[0][0] = 1;
        board[0][1] = 2;
        board[0][2] = 3;
        board[1][0] = 4;
        board[1][2] = 6;
        board[2][0] = 7;
        board[2][1] = 8;
        board[2][2] = 9;
        // Position (1,1) needs value 5

        ConstraintState state(board);
        auto result = state.findHiddenSingle(board);
        REQUIRE(result.has_value());
        auto [pos, value] = result.value();
        REQUIRE(pos.row == 1);
        REQUIRE(pos.col == 1);
        REQUIRE(value == 5);
    }

    SECTION("Returns nullopt when no hidden singles exist") {
        BoardData board;
        ConstraintState state(board);

        auto result = state.findHiddenSingle(board);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Complex case with multiple constraints") {
        /*
         * Create a partially solved board where hidden single detection is useful
         * This simulates a real Sudoku solving scenario
         */
        BoardData board = {{5, 3, 0, 0, 7, 0, 0, 0, 0}, {6, 0, 0, 1, 9, 5, 0, 0, 0}, {0, 9, 8, 0, 0, 0, 0, 6, 0},
                           {8, 0, 0, 0, 6, 0, 0, 0, 3}, {4, 0, 0, 8, 0, 3, 0, 0, 1}, {7, 0, 0, 0, 2, 0, 0, 0, 6},
                           {0, 6, 0, 0, 0, 0, 2, 8, 0}, {0, 0, 0, 4, 1, 9, 0, 0, 5}, {0, 0, 0, 0, 8, 0, 0, 7, 9}};

        ConstraintState state(board);
        auto result = state.findHiddenSingle(board);
        // This board should have at least one hidden single
        // (actual position depends on implementation details and scan order)
        // We just verify that the method runs without errors
        // and returns a valid position/value if found
        if (result.has_value()) {
            auto [pos, value] = result.value();
            REQUIRE(pos.row < 9);
            REQUIRE(pos.col < 9);
            REQUIRE(value >= 1);
            REQUIRE(value <= 9);
            REQUIRE(board[pos.row][pos.col] == 0);              // Cell must be empty
            REQUIRE(state.isAllowed(pos.row, pos.col, value));  // Value must be allowed
        }
    }
}
