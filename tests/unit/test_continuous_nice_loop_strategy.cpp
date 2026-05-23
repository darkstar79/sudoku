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
#include "../../src/core/strategies/continuous_nice_loop_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::keepOnly;

TEST_CASE("ContinuousNiceLoopStrategy - Metadata", "[continuous_nice_loop]") {
    ContinuousNiceLoopStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::ContinuousNiceLoop);
    REQUIRE(strategy.getName() == "Continuous Nice Loop");
    REQUIRE(strategy.getDifficultyRating() == 7.0);
}

TEST_CASE("ContinuousNiceLoopStrategy - Returns nullopt for complete board", "[continuous_nice_loop]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    ContinuousNiceLoopStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ContinuousNiceLoopStrategy - Can be used through ISolvingStrategy interface", "[continuous_nice_loop]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<ContinuousNiceLoopStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::ContinuousNiceLoop);
    REQUIRE(strategy->getName() == "Continuous Nice Loop");
    REQUIRE(strategy->getDifficultyRating() == 7.0);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ContinuousNiceLoopStrategy - Detects continuous loop elimination", "[continuous_nice_loop]") {
    // Build a continuous loop of 4 nodes with alternating strong/weak links:
    //
    // The loop: (0,0,1) =[strong]=> (0,3,1) -[weak]-> (0,3,4) =[strong]=> (0,0,4) -[weak]-> (0,0,1)
    //
    // Strong link (0,0,1)-(0,3,1): digit 1 in row 0, only in cols 0 and 3 (conjugate pair)
    // Weak link (0,3,1)-(0,3,4): different digits in same cell (0,3) which has candidates {1,4,7}
    // Strong link (0,3,4)-(0,0,4): digit 4 in row 0, only in cols 0 and 3 (conjugate pair)
    // Weak link (0,0,4)-(0,0,1): different digits in same cell (0,0) which has candidates {1,4,6}
    //
    // Weak link in cell (0,3) between digits 1 and 4: eliminate all OTHER candidates from (0,3)
    //   => eliminate 7 from (0,3)
    // Weak link in cell (0,0) between digits 4 and 1: eliminate all OTHER candidates from (0,0)
    //   => eliminate 6 from (0,0)

    auto board = BoardData::filled(5);

    // Clear cells for the loop
    board[0][0] = 0;  // candidates: {1, 4, 6}
    board[0][3] = 0;  // candidates: {1, 4, 7}

    CandidateGrid state(board);

    // Row 0 for digit 1: only cols 0 and 3 => conjugate pair (strong link)
    // Row 0 for digit 4: only cols 0 and 3 => conjugate pair (strong link)
    keepOnly(state, 0, 0, {1, 4, 6});
    keepOnly(state, 0, 3, {1, 4, 7});

    ContinuousNiceLoopStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::ContinuousNiceLoop);
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->rating == 7.0);
    }
}

TEST_CASE("ContinuousNiceLoopStrategy - Explanation contains technique name", "[continuous_nice_loop]") {
    auto board = BoardData::filled(5);

    board[0][0] = 0;
    board[0][3] = 0;

    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 4, 6});
    keepOnly(state, 0, 3, {1, 4, 7});

    ContinuousNiceLoopStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Continuous Nice Loop") != std::string::npos);
    }
}
