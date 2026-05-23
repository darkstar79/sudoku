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
#include "../../src/core/strategies/remote_pairs_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::createEmptyBoard;
using sudoku::testing::keepOnly;

TEST_CASE("RemotePairsStrategy - Metadata", "[remote_pairs]") {
    RemotePairsStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::RemotePairs);
    REQUIRE(strategy.getName() == "Remote Pairs");
    REQUIRE(strategy.getDifficultyRating() == 4.5);
}

TEST_CASE("RemotePairsStrategy - Returns nullopt for complete board", "[remote_pairs]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    RemotePairsStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("RemotePairsStrategy - Even chain length 4 produces eliminations", "[remote_pairs]") {
    // Chain of 4 bivalue {3,7} cells where endpoints don't directly see each other.
    // The chain must form a path where each consecutive pair sees each other but
    // the start and end do NOT share row/col/box (otherwise BFS finds shorter path).
    //
    // Chain: (0,0) → (0,4) → (4,4) → (4,8), all {3,7}
    //   (0,0) sees (0,4): same row
    //   (0,4) sees (4,4): same col
    //   (4,4) sees (4,8): same row
    //   (0,0) does NOT see (4,8): different row, col, box
    //
    // BFS from (0,0): depth 0=(0,0), depth 1=(0,4), depth 2=(4,4), depth 3=(4,8)
    // depth 3, odd → check eliminations. Chain length=4 cells (even).
    // Eliminate {3,7} from cells seeing both (0,0) and (4,8).
    // (0,8) sees (0,0) via row 0, sees (4,8) via col 8 → target
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    keepOnly(state, 0, 0, {3, 7});
    keepOnly(state, 0, 4, {3, 7});
    keepOnly(state, 4, 4, {3, 7});
    keepOnly(state, 4, 8, {3, 7});

    // Target cell sees both (0,0) and (4,8)
    REQUIRE(state.isAllowed(0, 8, 3));

    RemotePairsStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::RemotePairs);
    REQUIRE(result->rating == 4.5);

    // All eliminations should be value 3 or 7
    for (const auto& elim : result->eliminations) {
        REQUIRE((elim.value == 3 || elim.value == 7));
    }
}

TEST_CASE("RemotePairsStrategy - Explanation contains relevant info", "[remote_pairs]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Same chain as detection test
    keepOnly(state, 0, 0, {3, 7});
    keepOnly(state, 0, 4, {3, 7});
    keepOnly(state, 4, 4, {3, 7});
    keepOnly(state, 4, 8, {3, 7});

    RemotePairsStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Remote Pairs") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}

TEST_CASE("RemotePairsStrategy - Can be used through ISolvingStrategy interface", "[remote_pairs]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<RemotePairsStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::RemotePairs);
    REQUIRE(strategy->getName() == "Remote Pairs");
    REQUIRE(strategy->getDifficultyRating() == 4.5);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
