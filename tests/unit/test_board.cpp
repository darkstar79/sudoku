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
 * Tests for Board - flat, SIMD-friendly board representation.
 *
 * Board provides a contiguous, 32-byte-aligned array of int8_t cells
 * with operator[] enabling familiar board[row][col] syntax.
 */

#include "core/board.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("Board - Construction", "[board]") {
    SECTION("Default construction zero-initializes all cells") {
        Board board;
        for (size_t i = 0; i < PADDED_CELLS; ++i) {
            REQUIRE(board.cells[i] == 0);
        }
    }

    SECTION("Board is 32-byte aligned") {
        Board board;
        auto addr = reinterpret_cast<std::uintptr_t>(board.data());
        REQUIRE((addr % 32) == 0);
    }
}

TEST_CASE("Board - operator[] access", "[board]") {
    Board board;

    SECTION("Write and read via board[row][col]") {
        board[0][0] = 5;
        board[4][3] = 7;
        board[8][8] = 9;

        REQUIRE(board[0][0] == 5);
        REQUIRE(board[4][3] == 7);
        REQUIRE(board[8][8] == 9);
    }

    SECTION("operator[] maps to correct flat index") {
        board[2][5] = 3;
        REQUIRE(board.cells[(2 * BOARD_SIZE) + 5] == 3);
    }

    SECTION("All 81 cells are independently addressable") {
        int8_t value = 1;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                board[row][col] = value;
                value = static_cast<int8_t>((value % 9) + 1);
            }
        }

        value = 1;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                REQUIRE(board[row][col] == value);
                value = static_cast<int8_t>((value % 9) + 1);
            }
        }
    }
}

TEST_CASE("Board - fromBoardData and toBoardData", "[board]") {
    SECTION("Round-trip preserves all values") {
        BoardData original;
        original[0][0] = 5;
        original[3][7] = 2;
        original[8][8] = 9;
        original[4][4] = 1;

        Board board = Board::fromBoardData(original);
        auto result = board.toBoardData();

        REQUIRE(result == original);
    }

    SECTION("fromBoardData sets correct flat indices") {
        BoardData vec;
        vec[1][2] = 7;

        Board board = Board::fromBoardData(vec);
        REQUIRE(board.cells[(1 * BOARD_SIZE) + 2] == 7);
    }

    SECTION("Padding cells remain zero after fromBoardData") {
        BoardData vec = BoardData::filled(5);
        Board board = Board::fromBoardData(vec);

        for (size_t i = TOTAL_CELLS; i < PADDED_CELLS; ++i) {
            REQUIRE(board.cells[i] == 0);
        }
    }

    SECTION("Full board round-trip") {
        BoardData full_board = {
            {5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9},
        };

        Board board = Board::fromBoardData(full_board);
        auto result = board.toBoardData();

        REQUIRE(result == full_board);
    }
}

TEST_CASE("Board - Equality", "[board]") {
    SECTION("Default boards are equal") {
        Board a;
        Board b;
        REQUIRE(a == b);
    }

    SECTION("Boards with same values are equal") {
        Board a;
        Board b;
        a[3][5] = 7;
        b[3][5] = 7;
        REQUIRE(a == b);
    }

    SECTION("Boards with different values are not equal") {
        Board a;
        Board b;
        a[3][5] = 7;
        b[3][5] = 8;
        REQUIRE_FALSE(a == b);
    }
}

TEST_CASE("Board - data() pointer", "[board]") {
    Board board;
    board[0][0] = 1;

    SECTION("data() points to first cell") {
        REQUIRE(board.data()[0] == 1);
    }

    SECTION("const data() works") {
        const Board& cboard = board;
        REQUIRE(cboard.data()[0] == 1);
    }
}
