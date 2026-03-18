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
#include "../../src/core/strategies/avoidable_rectangle_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("AvoidableRectangleStrategy - Metadata", "[avoidable_rectangle]") {
    AvoidableRectangleStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::AvoidableRectangle);
    REQUIRE(strategy.getName() == "Avoidable Rectangle");
    REQUIRE(strategy.getDifficultyRating() == 4.5);
}

TEST_CASE("AvoidableRectangleStrategy - Returns nullopt for complete board", "[avoidable_rectangle]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    // Even with givens info, complete board → nullopt (no empty cells)
    state.setGivensFromPuzzle(board);
    AvoidableRectangleStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("AvoidableRectangleStrategy - Returns nullopt without givens info", "[avoidable_rectangle]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);
    // Do NOT call setGivensFromPuzzle → hasGivensInfo() is false
    AvoidableRectangleStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("AvoidableRectangleStrategy - Detects avoidable rectangle pattern", "[avoidable_rectangle]") {
    // Set up an avoidable rectangle across boxes 0 and 1:
    // Rectangle corners: (0,0), (0,3), (2,0), (2,3)
    //
    // Original puzzle (givens): only (0,0)=5 is given. All others were solved by the solver.
    // Current board state: 3 corners are solver-placed with values forming {A,B} pattern:
    //   (0,0) = 1 (solver-placed, not given in original)
    //   (0,3) = 2 (solver-placed)
    //   (2,0) = 2 (solver-placed)
    //   (2,3) = empty, has candidates {1, 2, 3, 4}
    //
    // Pattern: c1=(0,0)=1, c2=(0,3)=2 in same row, different boxes
    //          c3=(2,0)=2 solver-placed, same col as c1
    //          c4=(2,3)=empty → needs 1 to complete deadly pattern → eliminate 1 from (2,3)

    // The original puzzle: only a few cells are givens
    auto original_puzzle = createEmptyBoard();
    original_puzzle[0][1] = 5;  // some given (not part of the rectangle)
    original_puzzle[1][0] = 6;  // some given
    original_puzzle[1][3] = 7;  // some given

    // Current board: solver has placed values at the 3 rectangle corners
    auto board = createEmptyBoard();
    board[0][0] = 1;  // solver-placed (not in original)
    board[0][3] = 2;  // solver-placed
    board[2][0] = 2;  // solver-placed
    // (2,3) remains empty
    // Also copy the givens
    board[0][1] = 5;
    board[1][0] = 6;
    board[1][3] = 7;

    CandidateGrid state(board);
    state.setGivensFromPuzzle(original_puzzle);

    // Verify the 3 corners are solver-placed (not given)
    REQUIRE_FALSE(state.isGiven(0, 0));
    REQUIRE_FALSE(state.isGiven(0, 3));
    REQUIRE_FALSE(state.isGiven(2, 0));

    // Verify (2,3) is empty and has candidate 1 (the value to be eliminated)
    // Note: candidate 2 is eliminated by constraint propagation (2 at (0,3) col 3 and (2,0) row 2)
    REQUIRE(board[2][3] == 0);
    REQUIRE(state.isAllowed(2, 3, 1));

    AvoidableRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::AvoidableRectangle);
    REQUIRE(result->rating == 4.5);
    REQUIRE(result->eliminations.size() == 1);

    // c1=1, c2=2, c3=2 (same col as c1, has val_b) → eliminate val_a=1 from c4=(2,3)
    REQUIRE(result->eliminations[0].position.row == 2);
    REQUIRE(result->eliminations[0].position.col == 3);
    REQUIRE(result->eliminations[0].value == 1);
}

TEST_CASE("AvoidableRectangleStrategy - No pattern when corners are givens", "[avoidable_rectangle]") {
    // All filled corners are givens → not solver-placed → no avoidable rectangle
    auto original_puzzle = createEmptyBoard();
    original_puzzle[0][0] = 1;  // given
    original_puzzle[0][3] = 2;  // given
    original_puzzle[2][0] = 2;  // given

    auto board = createEmptyBoard();
    board[0][0] = 1;
    board[0][3] = 2;
    board[2][0] = 2;
    board[0][1] = 5;
    board[1][0] = 6;
    board[1][3] = 7;

    CandidateGrid state(board);
    state.setGivensFromPuzzle(original_puzzle);

    // Verify corners are given
    REQUIRE(state.isGiven(0, 0));
    REQUIRE(state.isGiven(0, 3));
    REQUIRE(state.isGiven(2, 0));

    AvoidableRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    // Should NOT find an avoidable rectangle because corners are givens
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("AvoidableRectangleStrategy - Explanation contains technique name", "[avoidable_rectangle]") {
    auto original_puzzle = createEmptyBoard();
    original_puzzle[0][1] = 5;
    original_puzzle[1][0] = 6;
    original_puzzle[1][3] = 7;

    auto board = createEmptyBoard();
    board[0][0] = 1;
    board[0][3] = 2;
    board[2][0] = 2;
    board[0][1] = 5;
    board[1][0] = 6;
    board[1][3] = 7;

    CandidateGrid state(board);
    state.setGivensFromPuzzle(original_puzzle);

    AvoidableRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Avoidable Rectangle") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}

TEST_CASE("AvoidableRectangleStrategy - Column-based rectangle", "[avoidable_rectangle]") {
    // Rectangle with starting pair in same column: (0,0) and (3,0)
    // Use cols 0 and 1 (same column-band) so rectangle spans exactly 2 boxes (0 and 3).
    // (0,0)=1, (3,0)=2 — same col 0, different boxes (0 vs 3)
    // (0,1)=2 — solver-placed (same box 0 as (0,0))
    // (3,1)=empty — target
    // Pattern: c1=(0,0)=1=A, c2=(3,0)=2=B → col pair
    //          c3=(0,1)=2=B solver-placed → c4=(3,1) needs A=1 → eliminate 1 from (3,1)

    auto original_puzzle = createEmptyBoard();
    original_puzzle[1][0] = 5;  // some given
    original_puzzle[1][1] = 6;  // some given

    auto board = createEmptyBoard();
    board[0][0] = 1;  // solver-placed
    board[3][0] = 2;  // solver-placed
    board[0][1] = 2;  // solver-placed
    // (3,1) = empty
    board[1][0] = 5;
    board[1][1] = 6;

    CandidateGrid state(board);
    state.setGivensFromPuzzle(original_puzzle);

    REQUIRE_FALSE(state.isGiven(0, 0));
    REQUIRE_FALSE(state.isGiven(3, 0));
    REQUIRE_FALSE(state.isGiven(0, 1));
    REQUIRE(board[3][1] == 0);
    // 1 is at (0,0): row 0, col 0, box 0. (3,1) is row 3, col 1, box 3. No overlap.
    REQUIRE(state.isAllowed(3, 1, 1));

    AvoidableRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::AvoidableRectangle);
    REQUIRE(result->eliminations.size() == 1);
    REQUIRE(result->eliminations[0].position.row == 3);
    REQUIRE(result->eliminations[0].position.col == 1);
    REQUIRE(result->eliminations[0].value == 1);
}
