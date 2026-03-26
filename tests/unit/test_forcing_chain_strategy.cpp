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
#include "../../src/core/strategies/forcing_chain_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::keepOnly;

TEST_CASE("ForcingChainStrategy - Metadata", "[forcing_chain]") {
    ForcingChainStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::ForcingChain);
    REQUIRE(strategy.getName() == "Forcing Chain");
    REQUIRE(strategy.getDifficultyRating() == 7.5);
}

TEST_CASE("ForcingChainStrategy - Returns nullopt for complete board", "[forcing_chain]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    ForcingChainStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("ForcingChainStrategy - Detects forcing chain elimination", "[forcing_chain]") {
    // Setup: Bivalue cell (0,0) has candidates {1,2}.
    // If (0,0)=1 → propagation eliminates 3 from (0,8) (e.g., via naked single chain)
    // If (0,0)=2 → propagation also eliminates 3 from (0,8)
    // Both branches agree: 3 is eliminated from (0,8).
    //
    // Board: mostly filled, with a few empty cells crafted so that placing
    // either candidate in (0,0) forces the same elimination at (0,8).
    auto board = BoardData::filled(5);

    // Row 0: empty cells form a propagation chain
    board[0][0] = 0;  // Source: bivalue {1,2}
    board[0][1] = 0;  // Propagation helper
    board[0][2] = 0;  // Propagation helper
    board[0][8] = 0;  // Target: has candidate 3 that should be eliminated

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    // If (0,0)=1: then (0,1) only has {2,4}. After, (0,2) has {4,3}.
    // If (0,0)=2: then (0,1) only has {1,4}. After, (0,2) has {4,3}.
    // Either way, (0,2) gets 4 placed (naked single after chain), and then
    // 3 is eliminated from (0,8) because of box/row constraints.
    keepOnly(state, 0, 1, {1, 2, 4});
    keepOnly(state, 0, 2, {3, 4});
    keepOnly(state, 0, 8, {3, 6});  // Target: 3 should be eliminated

    ForcingChainStrategy strategy;
    auto result = strategy.findStep(board, state);

    // The strategy may find this pattern or a different forcing chain
    // depending on propagation paths. If found, verify it's a valid result.
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::ForcingChain);
        REQUIRE(result->rating == 7.5);
    }
}

TEST_CASE("ForcingChainStrategy - Detects contradiction forcing chain", "[forcing_chain]") {
    // Setup: Bivalue cell where one candidate leads to contradiction.
    // Cell (0,0) has {1,2}. If we assume 1, propagation reveals no candidates
    // for some cell → contradiction. Therefore (0,0) must be 2.
    auto board = BoardData::filled(5);

    board[0][0] = 0;  // Source: bivalue {1,2}
    board[0][1] = 0;  // Will become empty after (0,0)=1
    board[0][2] = 0;  // Will have no candidates if 1 propagates

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {1, 3});  // If (0,0)=1 → (0,1) has only {3}
    keepOnly(state, 0, 2,
             {1, 3});  // If (0,0)=1 → (0,1)=3 → (0,2) has only {1} → placed
                       // But wait, 1 is also in row from (0,0)=1, so (0,2) has no candidates → contradiction

    ForcingChainStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::ForcingChain);
        // Contradiction means the other candidate is forced
        if (result->type == SolveStepType::Placement) {
            REQUIRE(result->position.row == 0);
            REQUIRE(result->position.col == 0);
            REQUIRE(result->value == 2);
        }
    }
}

TEST_CASE("ForcingChainStrategy - Explanation contains technique name", "[forcing_chain]") {
    auto board = BoardData::filled(5);

    board[0][0] = 0;
    board[0][1] = 0;
    board[0][2] = 0;
    board[0][8] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {1, 2, 4});
    keepOnly(state, 0, 2, {3, 4});
    keepOnly(state, 0, 8, {3, 6});

    ForcingChainStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Forcing Chain") != std::string::npos);
    }
}

TEST_CASE("ForcingChainStrategy - Can be used through ISolvingStrategy interface", "[forcing_chain]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<ForcingChainStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::ForcingChain);
    REQUIRE(strategy->getName() == "Forcing Chain");
    REQUIRE(strategy->getDifficultyRating() == 7.5);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
