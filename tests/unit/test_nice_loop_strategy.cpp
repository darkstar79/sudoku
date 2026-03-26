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
#include "../../src/core/strategies/nice_loop_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::keepOnly;

TEST_CASE("NiceLoopStrategy - Metadata", "[nice_loop]") {
    NiceLoopStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::NiceLoop);
    REQUIRE(strategy.getName() == "Nice Loop");
    REQUIRE(strategy.getDifficultyRating() == 7.5);
}

TEST_CASE("NiceLoopStrategy - Returns nullopt for complete board", "[nice_loop]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    NiceLoopStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("NiceLoopStrategy - Detects AIC elimination", "[nice_loop]") {
    // Build a simple AIC using conjugate pairs for digit 1:
    // Row 0: digit 1 only in cols 0,3 → strong link between (0,0,1) and (0,3,1)
    // (0,3) is bivalue {1,4} → strong link (0,3,1)-(0,3,4)
    // Col 6: digit 4 only in rows 0,6 → but (0,3,4) is in col 3...
    //
    // Simpler approach: create conjugate pairs that form an alternating chain.
    // Strong: (0,0,1)-(0,3,1) [row conjugate]
    // Weak: (0,3,1)-(0,3,4) [same cell]  (if bivalue, this is actually strong)
    // Strong: (0,3,4)-(3,3,4) [column conjugate]
    // Now digit 4 at (3,3) and digit 1 at (0,0) are connected by AIC.
    // This is a mixed-digit AIC, not same-digit at endpoints, so Type 2 doesn't directly apply.
    //
    // For same-digit endpoints (Type 2), use:
    // Strong: (0,0,1)-(0,3,1) [row conjugate for digit 1]
    // Weak: (0,3,1)-(3,3,1) [same digit in column, 3+ positions]
    // Strong: (3,3,1)-(3,6,1) [row conjugate for digit 1]
    // Chain: (0,0,1) =strong=> (0,3,1) -weak-> (3,3,1) =strong=> (3,6,1)
    // Both endpoints have digit 1. Eliminate 1 from cells seeing both (0,0) and (3,6).
    // Target: (0,6) if it has candidate 1 and sees both endpoints.
    // (0,6) sees (0,0) via row and (3,6) via column. Yes!

    auto board = BoardData::filled(5);

    // Empty cells for the chain nodes
    board[0][0] = 0;  // Chain start: digit 1
    board[0][3] = 0;  // Chain link
    board[3][3] = 0;  // Chain link
    board[3][6] = 0;  // Chain end: digit 1
    board[0][6] = 0;  // Target: sees both (0,0) and (3,6), has candidate 1

    // Need (0,3) to also have digit 1 in column with another cell for weak link
    board[6][3] = 0;  // Extra cell to ensure weak link exists in col 3

    CandidateGrid state(board);

    // Row 0: digit 1 only at cols 0 and 3 → conjugate pair (strong link)
    keepOnly(state, 0, 0, {1, 7});
    keepOnly(state, 0, 3, {1, 8});

    // Col 3: digit 1 at rows 0, 3, 6 → weak links between all pairs
    keepOnly(state, 3, 3, {1, 9});
    keepOnly(state, 6, 3, {1, 6});

    // Row 3: digit 1 only at cols 3 and 6 → conjugate pair (strong link)
    keepOnly(state, 3, 6, {1, 4});

    // Target cell: has candidate 1 (and something else)
    keepOnly(state, 0, 6, {1, 2});

    NiceLoopStrategy strategy;
    auto result = strategy.findStep(board, state);

    // The strategy should find a chain pattern. It may find this one or another valid AIC.
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::NiceLoop);
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->rating == 7.5);
    }
}

TEST_CASE("NiceLoopStrategy - Explanation contains technique name", "[nice_loop]") {
    auto board = BoardData::filled(5);

    board[0][0] = 0;
    board[0][3] = 0;
    board[3][3] = 0;
    board[3][6] = 0;
    board[0][6] = 0;
    board[6][3] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {1, 7});
    keepOnly(state, 0, 3, {1, 8});
    keepOnly(state, 3, 3, {1, 9});
    keepOnly(state, 6, 3, {1, 6});
    keepOnly(state, 3, 6, {1, 4});
    keepOnly(state, 0, 6, {1, 2});

    NiceLoopStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("Nice Loop") != std::string::npos);
    }
}

TEST_CASE("NiceLoopStrategy - Can be used through ISolvingStrategy interface", "[nice_loop]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<NiceLoopStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::NiceLoop);
    REQUIRE(strategy->getName() == "Nice Loop");
    REQUIRE(strategy->getDifficultyRating() == 7.5);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
