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
#include "../../src/core/strategies/hidden_quad_strategy.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

TEST_CASE("HiddenQuadStrategy - Metadata", "[hidden_quad]") {
    HiddenQuadStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::HiddenQuad);
    REQUIRE(strategy.getName() == "Hidden Quad");
    REQUIRE(strategy.getDifficultyPoints() == 150);
}

TEST_CASE("HiddenQuadStrategy - Returns nullopt for complete board", "[hidden_quad]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    HiddenQuadStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("HiddenQuadStrategy - Returns nullopt for near-complete board", "[hidden_quad]") {
    BoardData board = {{0, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    HiddenQuadStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("HiddenQuadStrategy - Finds hidden quad with manual elimination setup", "[hidden_quad]") {
    // Create a board and manually set up a hidden quad via candidate elimination.
    // In row 0, leave 5 empty cells (cols 0-4). We arrange that:
    //   Values {1,2,3,4} appear ONLY in cells (0,0)-(0,3) (the quad cells)
    //   Those cells also have value 5 as a candidate (the "hidden" part)
    //   Cell (0,4) has {5} only (values 1-4 eliminated from it)
    // This makes {1,2,3,4} a hidden quad in cells (0,0)-(0,3).
    // Value 5 should be eliminated from those quad cells.

    auto board = createEmptyBoard();
    // Fill cols 5-8 of row 0
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;

    CandidateGrid state(board);

    // Eliminate values 1,2,3,4 from cell (0,4) so they're confined to (0,0)-(0,3)
    state.eliminateCandidate(0, 4, 1);
    state.eliminateCandidate(0, 4, 2);
    state.eliminateCandidate(0, 4, 3);
    state.eliminateCandidate(0, 4, 4);

    HiddenQuadStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::HiddenQuad);
    REQUIRE(result->points == 150);
    REQUIRE_FALSE(result->eliminations.empty());

    // All eliminations should remove value 5 from the quad cells (0,0)-(0,3)
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.position.row == 0);
        REQUIRE(elim.position.col <= 3);
        REQUIRE(elim.value == 5);
    }
}

TEST_CASE("HiddenQuadStrategy - Explanation contains relevant info", "[hidden_quad]") {
    auto board = createEmptyBoard();
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;

    CandidateGrid state(board);
    state.eliminateCandidate(0, 4, 1);
    state.eliminateCandidate(0, 4, 2);
    state.eliminateCandidate(0, 4, 3);
    state.eliminateCandidate(0, 4, 4);

    HiddenQuadStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Hidden Quad") != std::string::npos);
        REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    }
}
