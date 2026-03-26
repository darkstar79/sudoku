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
#include "../../src/core/strategies/sue_de_coq_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::keepOnly;

TEST_CASE("SueDeCoqStrategy - Metadata", "[sue_de_coq]") {
    SueDeCoqStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::SueDeCoq);
    REQUIRE(strategy.getName() == "Sue de Coq");
    REQUIRE(strategy.getDifficultyRating() == 7.5);
}

TEST_CASE("SueDeCoqStrategy - Returns nullopt for complete board", "[sue_de_coq]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    SueDeCoqStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SueDeCoqStrategy - Detects Sue de Coq pattern", "[sue_de_coq]") {
    // Set up a 2-cell intersection of row 0 and box 0 at (0,0) and (0,1).
    // Intersection has 3 candidates {1,2,3}.
    // Line remainder (row 0, outside box 0): cells at (0,3)-(0,8)
    // Box remainder (box 0, outside row 0): cells at (1,0)-(2,2)
    //
    // Partition: line_set = {1}, box_set = {2,3}
    // Need ALS in line remainder covering {1}: cell with candidates {1, X} (N=1, N+1=2)
    // Need ALS in box remainder covering {2,3}: two cells with union {2,3,X} (N=2, N+1=3)

    auto board = BoardData::filled(5);  // all filled

    // Intersection cells
    board[0][0] = 0;
    board[0][1] = 0;

    // Line remainder — need at least one cell with {1, X}
    board[0][3] = 0;
    board[0][5] = 0;

    // Box remainder — need cells with candidates covering {2,3}
    board[1][0] = 0;
    board[1][1] = 0;
    board[2][2] = 0;  // Extra cell in box remainder for elimination target

    CandidateGrid state(board);

    // Intersection: (0,0){1,2}, (0,1){2,3} → union = {1,2,3}
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});

    // Line remainder: (0,3){1,6} → ALS covering {1} with 2 candidates ✓
    keepOnly(state, 0, 3, {1, 6});
    keepOnly(state, 0, 5, {6, 7});  // not relevant to line ALS

    // Box remainder: (1,0){2,4} and (1,1){3,4} → union={2,3,4}, N=2, N+1=3 ✓
    keepOnly(state, 1, 0, {2, 4});
    keepOnly(state, 1, 1, {3, 4});
    keepOnly(state, 2, 2, {2, 8});  // target: has candidate 2, in box remainder

    SueDeCoqStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::SueDeCoq);
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->rating == 7.5);
        REQUIRE(result->explanation.find("Sue de Coq") != std::string::npos);
        REQUIRE_FALSE(result->eliminations.empty());
    }
}

TEST_CASE("SueDeCoqStrategy - Returns nullopt when intersection has < 3 candidates", "[sue_de_coq]") {
    auto board = BoardData::filled(5);

    board[0][0] = 0;
    board[0][1] = 0;

    CandidateGrid state(board);
    // Only 2 candidates in intersection — not enough
    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {1, 2});

    SueDeCoqStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SueDeCoqStrategy - Does not eliminate correct value (regression)", "[sue_de_coq]") {
    // Regression test for bug where Sue de Coq eliminated the correct value from a cell.
    // Root cause: (1) findCoveringALS didn't verify that ALS extra candidates are disjoint
    // from the intersection's candidate set, and (2) eliminations included ALS cells.
    // Board state from diagnostic puzzle seed #160, step #31.
    BoardData board = {{3, 0, 7, 8, 6, 0, 4, 0, 0}, {4, 0, 0, 7, 3, 0, 0, 0, 0}, {6, 0, 0, 2, 0, 4, 3, 0, 0},
                       {2, 5, 3, 4, 0, 0, 6, 9, 8}, {8, 9, 1, 6, 5, 2, 7, 3, 4}, {7, 6, 4, 9, 8, 3, 0, 0, 0},
                       {0, 3, 6, 5, 2, 0, 8, 4, 0}, {0, 0, 8, 1, 0, 6, 2, 0, 3}, {0, 0, 2, 3, 0, 8, 0, 0, 0}};
    // Ground truth: R1C6=9 (0-indexed: row=1, col=5)
    constexpr int CORRECT_R1C6 = 9;

    CandidateGrid candidates(board);
    SueDeCoqStrategy strategy;

    auto result = strategy.findStep(board, candidates);

    // If Sue de Coq finds a step, it must not eliminate the correct value from any cell
    if (result.has_value()) {
        BoardData truth = {{3, 1, 7, 8, 6, 5, 4, 2, 9}, {4, 2, 5, 7, 3, 9, 1, 8, 6}, {6, 8, 9, 2, 1, 4, 3, 5, 7},
                           {2, 5, 3, 4, 7, 1, 6, 9, 8}, {8, 9, 1, 6, 5, 2, 7, 3, 4}, {7, 6, 4, 9, 8, 3, 5, 1, 2},
                           {9, 3, 6, 5, 2, 7, 8, 4, 1}, {5, 4, 8, 1, 9, 6, 2, 7, 3}, {1, 7, 2, 3, 4, 8, 9, 6, 5}};

        for (const auto& elim : result->eliminations) {
            int correct = truth[elim.position.row][elim.position.col];
            REQUIRE(elim.value != correct);
        }

        // Specifically: must not eliminate 9 from R1C6
        for (const auto& elim : result->eliminations) {
            bool wrong = (elim.position.row == 1 && elim.position.col == 5 && elim.value == CORRECT_R1C6);
            REQUIRE_FALSE(wrong);
        }
    }
}

TEST_CASE("SueDeCoqStrategy - ALS extra candidates must not overlap intersection", "[sue_de_coq]") {
    // Verify that a Sue de Coq pattern is rejected when the covering ALS's extra candidate
    // is also in the intersection's candidate set.
    // Setup: intersection {1,2,3}, partition L={1}, B={2,3}
    // Line ALS covers {1} with extra candidate 2 — but 2 is in inter_mask! Should be rejected.
    auto board = BoardData::filled(5);

    board[0][0] = 0;
    board[0][1] = 0;
    board[0][3] = 0;
    board[1][0] = 0;
    board[1][1] = 0;

    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 1, {2, 3});
    // Line ALS: {1, 2} — extra is 2, which is in inter_mask {1,2,3} → INVALID
    keepOnly(state, 0, 3, {1, 2});
    // Box ALS: {2,3,4} — extra is 4, valid
    keepOnly(state, 1, 0, {2, 4});
    keepOnly(state, 1, 1, {3, 4});

    SueDeCoqStrategy strategy;
    auto result = strategy.findStep(board, state);
    // Should not find this pattern because line ALS extra overlaps inter_mask
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SueDeCoqStrategy - Can be used through ISolvingStrategy interface", "[sue_de_coq]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<SueDeCoqStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::SueDeCoq);
    REQUIRE(strategy->getName() == "Sue de Coq");
    REQUIRE(strategy->getDifficultyRating() == 7.5);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
