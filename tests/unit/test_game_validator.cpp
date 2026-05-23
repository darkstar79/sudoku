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

#include "../../src/core/game_validator.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("GameValidator - Basic Validation", "[game_validator]") {
    GameValidator validator;

    SECTION("Valid positions") {
        REQUIRE(validator.isValidPosition({.row = 0, .col = 0}));
        REQUIRE(validator.isValidPosition({.row = 8, .col = 8}));
        REQUIRE(validator.isValidPosition({.row = 4, .col = 4}));
    }

    SECTION("Invalid positions") {
        REQUIRE_FALSE(validator.isValidPosition({.row = 9, .col = 0}));
        REQUIRE_FALSE(validator.isValidPosition({.row = 0, .col = 9}));
        REQUIRE_FALSE(validator.isValidPosition({.row = 10, .col = 5}));
        REQUIRE_FALSE(validator.isValidPosition({.row = 5, .col = 10}));
    }

    SECTION("Valid values") {
        REQUIRE(validator.isValidValue(1));
        REQUIRE(validator.isValidValue(9));
        REQUIRE(validator.isValidValue(5));
    }

    SECTION("Invalid values") {
        REQUIRE_FALSE(validator.isValidValue(0));
        REQUIRE_FALSE(validator.isValidValue(10));
        REQUIRE_FALSE(validator.isValidValue(-1));
    }
}

TEST_CASE("GameValidator - Conflict Detection", "[game_validator]") {
    GameValidator validator;

    // Create test board with some conflicts
    BoardData board;

    SECTION("Row conflicts") {
        board[0][0] = 5;
        board[0][4] = 5;  // Same row conflict

        REQUIRE(validator.hasRowConflict(board, 0, 8, 5));        // Would conflict
        REQUIRE_FALSE(validator.hasRowConflict(board, 0, 8, 7));  // No conflict
        REQUIRE_FALSE(validator.hasRowConflict(board, 1, 8, 5));  // Different row
    }

    SECTION("Column conflicts") {
        board[0][0] = 3;
        board[4][0] = 3;  // Same column conflict

        REQUIRE(validator.hasColumnConflict(board, 8, 0, 3));        // Would conflict
        REQUIRE_FALSE(validator.hasColumnConflict(board, 8, 0, 7));  // No conflict
        REQUIRE_FALSE(validator.hasColumnConflict(board, 8, 1, 3));  // Different column
    }

    SECTION("Box conflicts") {
        board[0][0] = 7;
        board[2][2] = 7;  // Same 3x3 box conflict

        REQUIRE(validator.hasBoxConflict(board, 1, 1, 7));        // Would conflict
        REQUIRE_FALSE(validator.hasBoxConflict(board, 1, 1, 4));  // No conflict
        REQUIRE_FALSE(validator.hasBoxConflict(board, 3, 3, 7));  // Different box
    }
}

TEST_CASE("GameValidator - Move Validation", "[game_validator]") {
    GameValidator validator;
    BoardData board;

    SECTION("Valid moves") {
        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(board, move);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("Invalid position") {
        Move move{.position = {.row = 9, .col = 0},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::InvalidPosition);
    }

    SECTION("Invalid value") {
        Move move{.position = {.row = 0, .col = 0},
                  .value = 10,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::InvalidValue);
    }

    SECTION("Clearing cell (value 0) is always allowed") {
        Move move{.position = {.row = 0, .col = 0},
                  .value = 0,
                  .move_type = MoveType::RemoveNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(board, move);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("Note operations are always allowed") {
        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::AddNote,
                  .is_note = true,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(board, move);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("Conflict in row") {
        board[0][0] = 5;  // Place 5 in row 0
        Move move{.position = {.row = 0, .col = 5},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::ConflictInRow);
    }

    SECTION("Conflict in column") {
        board[0][0] = 7;  // Place 7 in column 0
        Move move{.position = {.row = 5, .col = 0},
                  .value = 7,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::ConflictInColumn);
    }

    SECTION("Conflict in box") {
        board[0][0] = 3;  // Place 3 in top-left box
        Move move{.position = {.row = 2, .col = 2},
                  .value = 3,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::ConflictInBox);
    }

    SECTION("Game already complete") {
        // Create a complete valid board
        BoardData complete_board = {
            {1, 2, 3, 4, 5, 6, 7, 8, 9}, {4, 5, 6, 7, 8, 9, 1, 2, 3}, {7, 8, 9, 1, 2, 3, 4, 5, 6},
            {2, 3, 4, 5, 6, 7, 8, 9, 1}, {5, 6, 7, 8, 9, 1, 2, 3, 4}, {8, 9, 1, 2, 3, 4, 5, 6, 7},
            {3, 4, 5, 6, 7, 8, 9, 1, 2}, {6, 7, 8, 9, 1, 2, 3, 4, 5}, {9, 1, 2, 3, 4, 5, 6, 7, 8}};
        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};
        auto result = validator.validateMove(complete_board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::GameComplete);
    }
}

TEST_CASE("GameValidator - Board Completion", "[game_validator]") {
    GameValidator validator;

    SECTION("Incomplete board") {
        BoardData board;
        board[0][0] = 1;  // Only one cell filled
        REQUIRE_FALSE(validator.isComplete(board));
    }

    SECTION("Complete valid board") {
        // Simple valid complete board (not a real Sudoku solution, but valid for this test)
        BoardData board = {{1, 2, 3, 4, 5, 6, 7, 8, 9}, {4, 5, 6, 7, 8, 9, 1, 2, 3}, {7, 8, 9, 1, 2, 3, 4, 5, 6},
                           {2, 3, 4, 5, 6, 7, 8, 9, 1}, {5, 6, 7, 8, 9, 1, 2, 3, 4}, {8, 9, 1, 2, 3, 4, 5, 6, 7},
                           {3, 4, 5, 6, 7, 8, 9, 1, 2}, {6, 7, 8, 9, 1, 2, 3, 4, 5}, {9, 1, 2, 3, 4, 5, 6, 7, 8}};
        REQUIRE(validator.isComplete(board));
    }
}

TEST_CASE("GameValidator - Possible Values", "[game_validator]") {
    GameValidator validator;
    BoardData board;

    SECTION("Empty board - all values possible") {
        auto possible = validator.getPossibleValues(board, {.row = 0, .col = 0});
        REQUIRE(possible.size() == 9);
        for (int i = 1; i <= 9; ++i) {
            REQUIRE(std::find(possible.begin(), possible.end(), i) != possible.end());
        }
    }

    SECTION("Constrained position") {
        // Fill row 0 with values 1-8, leaving 9 as only possibility
        for (int i = 0; i < 8; ++i) {
            board[0][i] = i + 1;
        }

        auto possible = validator.getPossibleValues(board, {.row = 0, .col = 8});
        REQUIRE(possible.size() == 1);
        REQUIRE(possible[0] == 9);
    }

    SECTION("Filled cell - no values possible") {
        board[0][0] = 5;
        auto possible = validator.getPossibleValues(board, {.row = 0, .col = 0});
        REQUIRE(possible.empty());
    }

    SECTION("Invalid position - returns empty vector") {
        auto possible = validator.getPossibleValues(board, {.row = 9, .col = 0});
        REQUIRE(possible.empty());

        possible = validator.getPossibleValues(board, {.row = 0, .col = 9});
        REQUIRE(possible.empty());

        possible = validator.getPossibleValues(board, {.row = 10, .col = 10});
        REQUIRE(possible.empty());
    }
}

TEST_CASE("GameValidator - Naked Singles Detection", "[game_validator][constraint_propagation]") {
    GameValidator validator;
    BoardData board;

    SECTION("Naked single in constrained position") {
        // Fill row 0 with values 1-8, position (0,8) can only be 9
        for (int i = 0; i < 8; ++i) {
            board[0][i] = i + 1;
        }

        auto single = validator.findNakedSingle(board, {.row = 0, .col = 8});
        REQUIRE(single.has_value());
        REQUIRE(single.value() == 9);
    }

    SECTION("No naked single - empty board has 9 possibilities") {
        auto single = validator.findNakedSingle(board, {.row = 0, .col = 0});
        REQUIRE_FALSE(single.has_value());
    }

    SECTION("No naked single - filled cell") {
        board[0][0] = 5;
        auto single = validator.findNakedSingle(board, {.row = 0, .col = 0});
        REQUIRE_FALSE(single.has_value());
    }

    SECTION("No naked single - multiple possibilities") {
        // Constrain to 2 possibilities
        for (int i = 0; i < 7; ++i) {
            board[0][i] = i + 1;
        }
        // Cell (0,8) can be 8 or 9
        auto single = validator.findNakedSingle(board, {.row = 0, .col = 8});
        REQUIRE_FALSE(single.has_value());
    }

    SECTION("Naked single with complex constraints") {
        // Row constraint: 1-7 placed
        for (int i = 0; i < 7; ++i) {
            board[0][i] = i + 1;
        }
        // Column constraint: 8 already in column
        board[1][8] = 8;
        // Box constraint doesn't eliminate 9
        // Result: Cell (0,8) can only be 9

        auto single = validator.findNakedSingle(board, {.row = 0, .col = 8});
        REQUIRE(single.has_value());
        REQUIRE(single.value() == 9);
    }

    SECTION("Invalid position - returns nullopt") {
        auto single = validator.findNakedSingle(board, {.row = 9, .col = 0});
        REQUIRE_FALSE(single.has_value());

        single = validator.findNakedSingle(board, {.row = 0, .col = 9});
        REQUIRE_FALSE(single.has_value());
    }
}