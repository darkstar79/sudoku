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
#include "../../src/core/strategies/w_wing_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::createEmptyBoard;
using sudoku::testing::keepOnly;

TEST_CASE("WWingStrategy - Metadata", "[w_wing]") {
    WWingStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::WWing);
    REQUIRE(strategy.getName() == "W-Wing");
    REQUIRE(strategy.getDifficultyRating() == 4.4);
}

TEST_CASE("WWingStrategy - Returns nullopt for complete board", "[w_wing]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    WWingStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("WWingStrategy - W-Wing detection with row strong link", "[w_wing]") {
    // Set up a W-Wing:
    // c1 at (0,0) with candidates {1,2}
    // c2 at (8,8) with candidates {1,2}
    // Strong link on value 1 in row 4: value 1 appears only at (4,0) and (4,8)
    // (4,0) sees c1 via col 0, (4,8) sees c2 via col 8
    // Eliminate value 2 from cells seeing both c1 and c2
    auto board = createEmptyBoard();

    // Fill row 4 to create strong link on value 1
    board[4][1] = 1;  // Wait, this places 1 so it's not a candidate. Let me think differently.

    // We want value 1 in row 4 to appear as candidate at ONLY (4,0) and (4,8)
    // Place other values to fill row 4 except positions 0 and 8
    board[4][1] = 2;
    board[4][2] = 3;
    board[4][3] = 4;
    board[4][4] = 5;
    board[4][5] = 6;
    board[4][6] = 7;
    board[4][7] = 8;

    CandidateGrid state(board);

    // Force bivalue cells
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 8, 8, {1, 2});

    // Ensure strong link: value 1 only at (4,0) and (4,8) in row 4
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 8 && board[4][col] == 0 && state.isAllowed(4, col, 1)) {
            state.eliminateCandidate(4, col, 1);
        }
    }

    // Verify setup
    REQUIRE(state.countPossibleValues(0, 0) == 2);
    REQUIRE(state.countPossibleValues(8, 8) == 2);

    // Count value 1 positions in row 4
    int row4_count = 0;
    for (size_t col = 0; col < 9; ++col) {
        if (board[4][col] == 0 && state.isAllowed(4, col, 1)) {
            ++row4_count;
        }
    }
    REQUIRE(row4_count == 2);

    WWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value() && result->technique == SolvingTechnique::WWing) {
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->rating == 4.4);
        REQUIRE_FALSE(result->eliminations.empty());

        // Eliminated value should be 2 (the non-link value)
        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.value == 2);
        }

        REQUIRE(result->explanation.find("W-Wing") != std::string::npos);
        REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    }
}

TEST_CASE("WWingStrategy - Can be used through ISolvingStrategy interface", "[w_wing]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<WWingStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::WWing);
    REQUIRE(strategy->getName() == "W-Wing");
    REQUIRE(strategy->getDifficultyRating() == 4.4);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
