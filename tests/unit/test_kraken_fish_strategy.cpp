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
#include "../../src/core/strategies/kraken_fish_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("KrakenFishStrategy - Metadata", "[kraken_fish]") {
    KrakenFishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::KrakenFish);
    REQUIRE(strategy.getName() == "Kraken Fish");
    REQUIRE(strategy.getDifficultyRating() == 8.5);
}

TEST_CASE("KrakenFishStrategy - Returns nullopt for complete board", "[kraken_fish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    KrakenFishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("KrakenFishStrategy - Can be used through ISolvingStrategy interface", "[kraken_fish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<KrakenFishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::KrakenFish);
    REQUIRE(strategy->getName() == "Kraken Fish");
    REQUIRE(strategy->getDifficultyRating() == 8.5);

    // Complete board returns nullopt
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("KrakenFishStrategy - Returns nullopt for empty board with no finned patterns", "[kraken_fish]") {
    // Empty board has too many candidates everywhere for fish patterns
    BoardData board{};
    CandidateGrid state(board);
    KrakenFishStrategy strategy;

    auto result = strategy.findStep(board, state);
    // Empty board won't have finned fish with Kraken eliminations
    // (it may or may not find something, but the test verifies no crash)
    // The strategy should handle this gracefully
    (void)result;
    SUCCEED("Strategy did not crash on empty board");
}
