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
#include "../../src/core/strategies/region_forcing_chain_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("RegionForcingChainStrategy - Metadata", "[region_forcing_chain]") {
    RegionForcingChainStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::RegionForcingChain);
    REQUIRE(strategy.getName() == "Region Forcing Chain");
    REQUIRE(strategy.getDifficultyRating() == 8.0);
}

TEST_CASE("RegionForcingChainStrategy - Returns nullopt for complete board", "[region_forcing_chain]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    RegionForcingChainStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("RegionForcingChainStrategy - Can be used through ISolvingStrategy interface", "[region_forcing_chain]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<RegionForcingChainStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::RegionForcingChain);
    REQUIRE(strategy->getName() == "Region Forcing Chain");
    REQUIRE(strategy->getDifficultyRating() == 8.0);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
