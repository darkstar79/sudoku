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
#include "../helpers/test_utils.h"

#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

TEST_CASE("GameValidator - Edge Cases and Branch Coverage", "[game_validator][branches]") {
    GameValidator validator;

    SECTION("validateMove on complete board returns GameComplete error") {
        // Create a complete board (all cells filled with valid values)
        BoardData solution = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                              {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                              {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};

        // Verify board is complete
        REQUIRE(validator.isComplete(solution));

        // Try to make a move on complete board
        Move move{.position = {.row = 0, .col = 2},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        auto result = validator.validateMove(solution, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::GameComplete);
    }

    SECTION("validateMove with conflicting flags: is_note=true AND move_type=PlaceNumber") {
        BoardData board;

        // Create move with conflicting flags
        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = true,  // Contradictory: is_note=true but move_type=PlaceNumber
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        // Should allow (note operations always allowed - line 32)
        auto result = validator.validateMove(board, move);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("validateMove with is_note=false but move_type=AddNote") {
        BoardData board;

        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::AddNote,
                  .is_note = false,  // Contradictory: move_type=AddNote but is_note=false
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        // Should allow (note operations always allowed - line 32)
        auto result = validator.validateMove(board, move);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("validateMove with move_type=RemoveNote") {
        BoardData board;

        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::RemoveNote,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        // Should allow (note operations always allowed - line 32)
        auto result = validator.validateMove(board, move);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("validateMove with move_type=RemoveNumber") {
        BoardData board;
        board[0][0] = 5;

        Move move{.position = {.row = 0, .col = 0},
                  .value = 0,  // Clear cell
                  .move_type = MoveType::RemoveNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        // Should allow (value = 0 always allowed - line 17)
        auto result = validator.validateMove(board, move);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("Multiple simultaneous conflicts - row AND column") {
        BoardData board;
        board[0][5] = 5;  // Row conflict
        board[5][0] = 5;  // Column conflict

        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        // Should return first conflict detected (row)
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::ConflictInRow);
    }

    SECTION("Multiple simultaneous conflicts - row AND box") {
        BoardData board;
        board[0][5] = 5;  // Row conflict
        board[1][1] = 5;  // Box conflict (same 3x3 box)

        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        // Should return first conflict detected (row)
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::ConflictInRow);
    }

    SECTION("Multiple simultaneous conflicts - column AND box") {
        BoardData board;
        board[5][0] = 5;  // Column conflict
        board[1][1] = 5;  // Box conflict

        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        // Row check passes, column check fails
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::ConflictInColumn);
    }

    SECTION("Multiple simultaneous conflicts - row AND column AND box") {
        BoardData board;
        board[0][5] = 5;  // Row conflict
        board[5][0] = 5;  // Column conflict
        board[1][1] = 5;  // Box conflict

        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
                  .move_type = MoveType::PlaceNumber,
                  .is_note = false,
                  .timestamp = std::chrono::steady_clock::now(),
                  .previous_value = 0,
                  .previous_notes = {},
                  .previous_hint_revealed = false};

        // Should return first conflict detected (row)
        auto result = validator.validateMove(board, move);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ValidationError::ConflictInRow);
    }

    SECTION("Box conflict only (no row/column conflict)") {
        BoardData board;
        board[1][1] = 5;  // Same box as {0, 0}

        Move move{.position = {.row = 0, .col = 0},
                  .value = 5,
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
}

TEST_CASE("GameValidator - getPossibleValues edge cases", "[game_validator][branches]") {
    GameValidator validator;

    SECTION("getPossibleValues on filled cell returns empty vector") {
        BoardData board;
        board[0][0] = 5;  // Cell already filled

        Position pos{.row = 0, .col = 0};
        auto possible = validator.getPossibleValues(board, pos);

        // Line 96-97: Early return for filled cells
        REQUIRE(possible.empty());
    }

    SECTION("getPossibleValues on invalid position returns empty vector") {
        BoardData board;

        Position invalid_pos{.row = 9, .col = 0};  // Out of bounds
        auto possible = validator.getPossibleValues(board, invalid_pos);

        // Line 91-92: Early return for invalid position
        REQUIRE(possible.empty());
    }

    SECTION("getPossibleValues on empty cell with all values possible") {
        BoardData board;

        Position pos{.row = 0, .col = 0};
        auto possible = validator.getPossibleValues(board, pos);

        // All values 1-9 should be possible
        REQUIRE(possible.size() == 9);
        for (int i = 1; i <= 9; ++i) {
            REQUIRE(std::find(possible.begin(), possible.end(), i) != possible.end());
        }
    }

    SECTION("getPossibleValues on empty cell with some conflicts") {
        BoardData board;
        board[0][1] = 1;  // Row conflict for value 1
        board[1][0] = 2;  // Column conflict for value 2
        board[1][1] = 3;  // Box conflict for value 3

        Position pos{.row = 0, .col = 0};
        auto possible = validator.getPossibleValues(board, pos);

        // Values 4-9 should be possible (6 total)
        REQUIRE(possible.size() == 6);
        REQUIRE(std::find(possible.begin(), possible.end(), 1) == possible.end());
        REQUIRE(std::find(possible.begin(), possible.end(), 2) == possible.end());
        REQUIRE(std::find(possible.begin(), possible.end(), 3) == possible.end());
        REQUIRE(std::find(possible.begin(), possible.end(), 4) != possible.end());
    }

    SECTION("getPossibleValues on empty cell with no values possible") {
        BoardData board;

        // Fill row 0 with values 1-8, leaving column 0 empty
        for (int i = 1; i < 9; ++i) {
            board[0][i] = i;  // Fill columns 1-8 with values 1-8
        }

        // Fill column 0 with value 9 at row 1 (creates conflict for value 9)
        board[1][0] = 9;

        // Now all values 1-9 conflict at position {0, 0}:
        // - Values 1-8 conflict in row 0
        // - Value 9 conflicts in column 0
        Position pos{.row = 0, .col = 0};
        auto possible = validator.getPossibleValues(board, pos);

        REQUIRE(possible.empty());
    }
}

TEST_CASE("GameValidator - Boundary position validation", "[game_validator][branches]") {
    GameValidator validator;

    SECTION("Valid corner positions") {
        REQUIRE(validator.isValidPosition({.row = 0, .col = 0}));  // Top-left
        REQUIRE(validator.isValidPosition({.row = 0, .col = 8}));  // Top-right
        REQUIRE(validator.isValidPosition({.row = 8, .col = 0}));  // Bottom-left
        REQUIRE(validator.isValidPosition({.row = 8, .col = 8}));  // Bottom-right
    }

    SECTION("Invalid positions - out of bounds") {
        REQUIRE_FALSE(validator.isValidPosition({.row = 9, .col = 0}));   // Row too large
        REQUIRE_FALSE(validator.isValidPosition({.row = 0, .col = 9}));   // Col too large
        REQUIRE_FALSE(validator.isValidPosition({.row = 9, .col = 9}));   // Both too large
        REQUIRE_FALSE(validator.isValidPosition({.row = 10, .col = 5}));  // Way out of bounds
    }

    SECTION("Edge positions just inside bounds") {
        REQUIRE(validator.isValidPosition({.row = 8, .col = 7}));
        REQUIRE(validator.isValidPosition({.row = 7, .col = 8}));
    }
}

TEST_CASE("GameValidator - Value validation edge cases", "[game_validator][branches]") {
    GameValidator validator;

    SECTION("Valid values 1-9") {
        for (int i = 1; i <= 9; ++i) {
            REQUIRE(validator.isValidValue(i));
        }
    }

    SECTION("Invalid values - boundary cases") {
        REQUIRE_FALSE(validator.isValidValue(0));    // Below min
        REQUIRE_FALSE(validator.isValidValue(10));   // Above max
        REQUIRE_FALSE(validator.isValidValue(-1));   // Negative
        REQUIRE_FALSE(validator.isValidValue(100));  // Way above max
    }
}

TEST_CASE("GameValidator - Conflict detection edge cases", "[game_validator][branches]") {
    GameValidator validator;

    SECTION("hasRowConflict - conflict at first column") {
        BoardData board;
        board[0][0] = 5;

        REQUIRE(validator.hasRowConflict(board, 0, 8, 5));
    }

    SECTION("hasRowConflict - conflict at last column") {
        BoardData board;
        board[0][8] = 5;

        REQUIRE(validator.hasRowConflict(board, 0, 0, 5));
    }

    SECTION("hasRowConflict - no conflict with same cell") {
        BoardData board;
        board[0][0] = 5;

        // Line 119: check_col != col condition prevents self-conflict
        REQUIRE_FALSE(validator.hasRowConflict(board, 0, 0, 5));
    }

    SECTION("hasColumnConflict - conflict at first row") {
        BoardData board;
        board[0][0] = 5;

        REQUIRE(validator.hasColumnConflict(board, 8, 0, 5));
    }

    SECTION("hasColumnConflict - conflict at last row") {
        BoardData board;
        board[8][0] = 5;

        REQUIRE(validator.hasColumnConflict(board, 0, 0, 5));
    }

    SECTION("hasColumnConflict - no conflict with same cell") {
        BoardData board;
        board[0][0] = 5;

        // Line 129: check_row != row condition prevents self-conflict
        REQUIRE_FALSE(validator.hasColumnConflict(board, 0, 0, 5));
    }

    SECTION("hasBoxConflict - conflict at all 9 positions within box") {
        BoardData board;

        // Test box {0,0} to {2,2}
        std::vector<Position> box_positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 0, .col = 2},
                                               {.row = 1, .col = 0}, {.row = 1, .col = 1}, {.row = 1, .col = 2},
                                               {.row = 2, .col = 0}, {.row = 2, .col = 1}, {.row = 2, .col = 2}};

        for (const auto& pos : box_positions) {
            board[pos.row][pos.col] = 5;

            // Check all other positions in box for conflict
            for (const auto& check_pos : box_positions) {
                if (check_pos == pos) {
                    // Line 142: Same position should NOT conflict
                    REQUIRE_FALSE(validator.hasBoxConflict(board, check_pos.row, check_pos.col, 5));
                } else {
                    // Different position should conflict
                    REQUIRE(validator.hasBoxConflict(board, check_pos.row, check_pos.col, 5));
                }
            }

            board[pos.row][pos.col] = 0;  // Clear for next iteration
        }
    }

    SECTION("hasBoxConflict - all 9 boxes tested") {
        BoardData board;

        // Test all 9 boxes
        for (size_t box_row = 0; box_row < 3; ++box_row) {
            for (size_t box_col = 0; box_col < 3; ++box_col) {
                size_t test_row = box_row * 3;
                size_t test_col = box_col * 3;

                // Place value in first cell of box
                board[test_row][test_col] = 5;

                // Check last cell of same box for conflict
                REQUIRE(validator.hasBoxConflict(board, test_row + 2, test_col + 2, 5));

                // Clear
                board[test_row][test_col] = 0;
            }
        }
    }
}

TEST_CASE("GameValidator - findConflicts comprehensive", "[game_validator][branches]") {
    GameValidator validator;

    SECTION("findConflicts on board with no conflicts") {
        auto board = getEasyPuzzleWithPatterns();

        auto conflicts = validator.findConflicts(board);
        REQUIRE(conflicts.empty());
    }

    SECTION("findConflicts on board with multiple conflicts") {
        BoardData board;
        board[0][0] = 5;
        board[0][1] = 5;  // Row conflict
        board[1][0] = 5;  // Column conflict

        auto conflicts = validator.findConflicts(board);

        // All three cells should be in conflict list
        REQUIRE(conflicts.size() == 3);
        REQUIRE(std::find(conflicts.begin(), conflicts.end(), Position{.row = 0, .col = 0}) != conflicts.end());
        REQUIRE(std::find(conflicts.begin(), conflicts.end(), Position{.row = 0, .col = 1}) != conflicts.end());
        REQUIRE(std::find(conflicts.begin(), conflicts.end(), Position{.row = 1, .col = 0}) != conflicts.end());
    }

    SECTION("findConflicts ignores empty cells") {
        BoardData board;
        // All cells empty (value = 0)

        auto conflicts = validator.findConflicts(board);

        // Line 73: value != 0 condition prevents checking empty cells
        REQUIRE(conflicts.empty());
    }

    SECTION("validateBoard returns true when no conflicts") {
        auto board = getEasyPuzzleWithPatterns();

        REQUIRE(validator.validateBoard(board));
    }

    SECTION("validateBoard returns false when conflicts exist") {
        BoardData board;
        board[0][0] = 5;
        board[0][1] = 5;  // Conflict

        REQUIRE_FALSE(validator.validateBoard(board));
    }
}

TEST_CASE("GameValidator - isComplete edge cases", "[game_validator][branches]") {
    GameValidator validator;

    SECTION("isComplete on empty board") {
        BoardData board;

        REQUIRE_FALSE(validator.isComplete(board));
    }

    SECTION("isComplete on partially filled board") {
        auto board = getEasyPuzzleWithPatterns();

        REQUIRE_FALSE(validator.isComplete(board));
    }

    SECTION("isComplete on full board with conflicts") {
        BoardData board = BoardData::filled(5);

        // Line 58: anyCell returns false (no empty cells)
        // Line 63: validateBoard returns false (conflicts exist)
        REQUIRE_FALSE(validator.isComplete(board));
    }

    SECTION("isComplete on valid solution") {
        // Use the same complete solution from earlier test
        BoardData solution = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                              {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                              {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};

        REQUIRE(validator.isComplete(solution));
    }
}
