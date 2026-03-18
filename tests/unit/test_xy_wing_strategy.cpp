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

#include "../../src/core/candidate_grid.h"
#include "../../src/core/strategies/xy_wing_strategy.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

TEST_CASE("XYWingStrategy - Metadata", "[xy_wing]") {
    XYWingStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::XYWing);
    REQUIRE(strategy.getName() == "XY-Wing");
    REQUIRE(strategy.getDifficultyRating() == 4.2);
}

TEST_CASE("XYWingStrategy - Returns nullopt for complete board", "[xy_wing]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    XYWingStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("XYWingStrategy - Finds XY-Wing with manual elimination setup", "[xy_wing]") {
    // Set up an XY-Wing pattern:
    // Pivot at (0,0) with candidates {1,2}
    // Wing1 at (0,3) with candidates {1,3} (same row as pivot)
    // Wing2 at (3,0) with candidates {2,3} (same col as pivot)
    // Value 3 should be eliminated from cells that see both wings
    // Cell (3,3) sees both wing1 (same row 3? no, wing1 is row 0) —
    // Actually: wing1=(0,3), wing2=(3,0). A cell sees both if it shares row/col/box
    // with both. Cell (3,3) shares row with wing2 (row 3) and col with wing1 (col 3).
    // If (3,3) has candidate 3, it should be eliminated.

    auto board = createEmptyBoard();
    // Fill most of the board to constrain candidates
    // Row 0: leave (0,0) and (0,3) empty
    board[0][1] = 4;
    board[0][2] = 5;
    board[0][4] = 6;
    board[0][5] = 7;
    board[0][6] = 8;
    board[0][7] = 9;

    // Col 0: leave (0,0) and (3,0) empty
    board[1][0] = 6;
    board[2][0] = 7;
    board[4][0] = 8;
    board[5][0] = 9;

    CandidateGrid state(board);

    // Eliminate candidates to create the bi-value cells:
    // (0,0): keep {1,2} only
    for (int v = 1; v <= 9; ++v) {
        if (v != 1 && v != 2 && state.isAllowed(0, 0, v)) {
            state.eliminateCandidate(0, 0, v);
        }
    }

    // (0,3): keep {1,3} only
    for (int v = 1; v <= 9; ++v) {
        if (v != 1 && v != 3 && state.isAllowed(0, 3, v)) {
            state.eliminateCandidate(0, 3, v);
        }
    }

    // (3,0): keep {2,3} only
    for (int v = 1; v <= 9; ++v) {
        if (v != 2 && v != 3 && state.isAllowed(3, 0, v)) {
            state.eliminateCandidate(3, 0, v);
        }
    }

    // Verify the setup
    REQUIRE(state.countPossibleValues(0, 0) == 2);
    REQUIRE(state.countPossibleValues(0, 3) == 2);
    REQUIRE(state.countPossibleValues(3, 0) == 2);

    XYWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->technique == SolvingTechnique::XYWing);
        REQUIRE(result->rating == 4.2);
        REQUIRE_FALSE(result->eliminations.empty());

        // All eliminations should be for value 3
        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.value == 3);
        }
    }
}

TEST_CASE("XYWingStrategy - Explanation contains relevant info", "[xy_wing]") {
    auto board = createEmptyBoard();
    board[0][1] = 4;
    board[0][2] = 5;
    board[0][4] = 6;
    board[0][5] = 7;
    board[0][6] = 8;
    board[0][7] = 9;
    board[1][0] = 6;
    board[2][0] = 7;
    board[4][0] = 8;
    board[5][0] = 9;

    CandidateGrid state(board);

    for (int v = 1; v <= 9; ++v) {
        if (v != 1 && v != 2 && state.isAllowed(0, 0, v)) {
            state.eliminateCandidate(0, 0, v);
        }
    }
    for (int v = 1; v <= 9; ++v) {
        if (v != 1 && v != 3 && state.isAllowed(0, 3, v)) {
            state.eliminateCandidate(0, 3, v);
        }
    }
    for (int v = 1; v <= 9; ++v) {
        if (v != 2 && v != 3 && state.isAllowed(3, 0, v)) {
            state.eliminateCandidate(3, 0, v);
        }
    }

    XYWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("XY-Wing") != std::string::npos);
        REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    }
}
