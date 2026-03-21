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
#include "../../src/core/strategies/unit_forcing_chain_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("UnitForcingChainStrategy - Metadata", "[unit_forcing_chain]") {
    UnitForcingChainStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::UnitForcingChain);
    REQUIRE(strategy.getName() == "Unit Forcing Chain");
    REQUIRE(strategy.getDifficultyRating() == 7.8);
}

TEST_CASE("UnitForcingChainStrategy - Returns nullopt for complete board", "[unit_forcing_chain]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    UnitForcingChainStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("UnitForcingChainStrategy - Can be used through ISolvingStrategy interface", "[unit_forcing_chain]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<UnitForcingChainStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::UnitForcingChain);
    REQUIRE(strategy->getName() == "Unit Forcing Chain");
    REQUIRE(strategy->getDifficultyRating() == 7.8);

    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
