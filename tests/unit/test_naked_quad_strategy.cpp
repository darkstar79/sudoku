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
#include "../../src/core/strategies/naked_quad_strategy.h"
#include "../helpers/candidate_test_utils.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

TEST_CASE("NakedQuadStrategy - Metadata", "[naked_quad]") {
    NakedQuadStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::NakedQuad);
    REQUIRE(strategy.getName() == "Naked Quad");
    REQUIRE(strategy.getDifficultyRating() == 5.0);
}

TEST_CASE("NakedQuadStrategy - Returns nullopt for complete board", "[naked_quad]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    NakedQuadStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("NakedQuadStrategy - Returns nullopt for near-complete board", "[naked_quad]") {
    // Board with only 1 empty cell — no room for naked quads
    BoardData board = {{0, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    NakedQuadStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("NakedQuadStrategy - Finds naked quad with manual elimination setup", "[naked_quad]") {
    // Create a board and manually set up a naked quad via candidate elimination.
    // In row 0, we leave 5 empty cells (cols 0-4). We eliminate candidates so that:
    //   (0,0) has {1,2}
    //   (0,1) has {2,3}
    //   (0,2) has {3,4}
    //   (0,3) has {1,4}
    //   (0,4) has {1,2,3,4,5} (extra cell with quad values + more)
    // Cells (0,0)-(0,3) form a naked quad {1,2,3,4}.
    // Value 1,2,3,4 should be eliminated from (0,4).

    auto board = createEmptyBoard();
    // Fill cols 5-8 of row 0 to reduce empty cells
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;

    CandidateGrid state(board);

    // Eliminate candidates to create the naked quad pattern in row 0
    // (0,0): keep {1,2} — eliminate 3,4,5
    state.eliminateCandidate(0, 0, 3);
    state.eliminateCandidate(0, 0, 4);
    state.eliminateCandidate(0, 0, 5);

    // (0,1): keep {2,3} — eliminate 1,4,5
    state.eliminateCandidate(0, 1, 1);
    state.eliminateCandidate(0, 1, 4);
    state.eliminateCandidate(0, 1, 5);

    // (0,2): keep {3,4} — eliminate 1,2,5
    state.eliminateCandidate(0, 2, 1);
    state.eliminateCandidate(0, 2, 2);
    state.eliminateCandidate(0, 2, 5);

    // (0,3): keep {1,4} — eliminate 2,3,5
    state.eliminateCandidate(0, 3, 2);
    state.eliminateCandidate(0, 3, 3);
    state.eliminateCandidate(0, 3, 5);

    // (0,4): keep {1,2,3,4,5} — no eliminations needed (has all 5)

    NakedQuadStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::NakedQuad);
    REQUIRE(result->rating == 5.0);
    REQUIRE_FALSE(result->eliminations.empty());

    // All eliminations should remove quad values {1,2,3,4} from cell (0,4)
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.position.row == 0);
        REQUIRE(elim.position.col == 4);
        REQUIRE(elim.value >= 1);
        REQUIRE(elim.value <= 4);
    }
}

TEST_CASE("NakedQuadStrategy - Explanation contains relevant info", "[naked_quad]") {
    auto board = createEmptyBoard();
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;

    CandidateGrid state(board);

    // Same setup as above
    state.eliminateCandidate(0, 0, 3);
    state.eliminateCandidate(0, 0, 4);
    state.eliminateCandidate(0, 0, 5);
    state.eliminateCandidate(0, 1, 1);
    state.eliminateCandidate(0, 1, 4);
    state.eliminateCandidate(0, 1, 5);
    state.eliminateCandidate(0, 2, 1);
    state.eliminateCandidate(0, 2, 2);
    state.eliminateCandidate(0, 2, 5);
    state.eliminateCandidate(0, 3, 2);
    state.eliminateCandidate(0, 3, 3);
    state.eliminateCandidate(0, 3, 5);

    NakedQuadStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Naked Quad") != std::string::npos);
        REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    }
}
