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
#include "../../src/core/strategies/als_chain_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::keepOnly;

TEST_CASE("ALSChainStrategy - Metadata", "[als_chain]") {
    ALSChainStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::ALSChain);
    REQUIRE(strategy.getName() == "ALS Chain");
    REQUIRE(strategy.getDifficultyRating() == 8.5);
}

TEST_CASE("ALSChainStrategy - Returns nullopt for complete board", "[als_chain]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    ALSChainStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ALSChainStrategy - Returns nullopt with fewer than 4 ALSs", "[als_chain]") {
    // Only 2 empty cells => at most 1 ALS => cannot form a 4-ALS chain
    auto board = BoardData::filled(5);
    board[0][0] = 0;
    board[0][1] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});

    ALSChainStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ALSChainStrategy - Can be used through ISolvingStrategy interface", "[als_chain]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<ALSChainStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::ALSChain);
    REQUIRE(strategy->getName() == "ALS Chain");
    REQUIRE(strategy->getDifficultyRating() == 8.5);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
