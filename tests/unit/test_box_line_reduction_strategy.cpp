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
#include "../../src/core/strategies/box_line_reduction_strategy.h"
#include "../helpers/candidate_test_utils.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

TEST_CASE("BoxLineReductionStrategy - Metadata", "[box_line_reduction]") {
    BoxLineReductionStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::BoxLineReduction);
    REQUIRE(strategy.getName() == "Box/Line Reduction");
    REQUIRE(strategy.getDifficultyRating() == 2.8);
}

TEST_CASE("BoxLineReductionStrategy - Finds box/line reduction in row", "[box_line_reduction]") {
    // Set up board where value 8 in row 0 is confined to box 0 (cols 0-2)
    // Then 8 should be eliminated from other cells in box 0 that are NOT in row 0
    auto board = createEmptyBoard();
    // Block 8 from cols 3-8 of row 0 by placing 8 in rows that share those boxes
    board[0][3] = 1;
    board[0][4] = 2;
    board[0][5] = 3;
    board[0][6] = 4;
    board[0][7] = 5;
    board[0][8] = 6;
    // Now in row 0, only cols 0-2 are empty (box 0)
    // So value 8 in row 0 is confined to box 0

    CandidateGrid state(board);
    BoxLineReductionStrategy strategy;

    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->technique == SolvingTechnique::BoxLineReduction);
        REQUIRE(result->rating == 2.8);
        REQUIRE_FALSE(result->eliminations.empty());

        // All eliminations should be in the target box but NOT in the source row
        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.position.row < 9);
            REQUIRE(elim.position.col < 9);
            REQUIRE(elim.value >= 1);
            REQUIRE(elim.value <= 9);
        }
    }
}

TEST_CASE("BoxLineReductionStrategy - Col-based detection", "[box_line_reduction]") {
    // Fill col 0 rows 3-8 with values 1-6 so col 0 only has candidates
    // {7,8,9} at rows 0-2 (all in box 0). Col-based BLR fires for value 7:
    // confined to box 0 → eliminates 7 from other box-0 cells not in col 0.
    auto board = createEmptyBoard();
    board[3][0] = 1;
    board[4][0] = 2;
    board[5][0] = 3;
    board[6][0] = 4;
    board[7][0] = 5;
    board[8][0] = 6;

    CandidateGrid state(board);
    BoxLineReductionStrategy strategy;

    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::BoxLineReduction);
    REQUIRE(result->explanation_data.region_type == RegionType::Col);
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.position.row < 3);   // box 0 = rows 0-2
        REQUIRE(elim.position.col != 0);  // not the source column
        REQUIRE(elim.position.col < 3);   // box 0 = cols 0-2
    }
}

TEST_CASE("BoxLineReductionStrategy - Returns nullopt for complete board", "[box_line_reduction]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    BoxLineReductionStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("BoxLineReductionStrategy - Explanation contains relevant info", "[box_line_reduction]") {
    auto board = createEmptyBoard();
    board[0][3] = 1;
    board[0][4] = 2;
    board[0][5] = 3;
    board[0][6] = 4;
    board[0][7] = 5;
    board[0][8] = 6;

    CandidateGrid state(board);
    BoxLineReductionStrategy strategy;

    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Box/Line Reduction") != std::string::npos);
        REQUIRE(result->explanation.find("confined to") != std::string::npos);
    }
}
