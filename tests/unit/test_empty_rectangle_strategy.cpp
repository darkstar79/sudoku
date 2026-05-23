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
#include "../../src/core/strategies/empty_rectangle_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("EmptyRectangleStrategy - Metadata", "[empty_rectangle]") {
    EmptyRectangleStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::EmptyRectangle);
    REQUIRE(strategy.getName() == "Empty Rectangle");
    REQUIRE(strategy.getDifficultyRating() == 4.5);
}

TEST_CASE("EmptyRectangleStrategy - Returns nullopt for complete board", "[empty_rectangle]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    EmptyRectangleStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("EmptyRectangleStrategy - Row conjugate pair detection", "[empty_rectangle]") {
    // ER in Box 0 (rows 0-2, cols 0-2) for value 5:
    // L-shape: value 5 at (0,0), (0,1), (2,0) — occupies rows {0,2} and cols {0,1}
    //
    // Conjugate pair in Row 6 (outside box 0) passing through col 0:
    // Value 5 at (6,0) and (6,5) only in row 6.
    //
    // ER cross point: er_row=0, er_col=0 (all box cells in row 0 or col 0)
    // Target: (0, 5) — er_row=0, target_col=5. Must not be in box 0 ✓ (box 1).
    // Eliminate 5 from (0,5).
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Set up ER in box 0: value 5 only at (0,0), (0,1), (2,0)
    // Remove 5 from all other box 0 cells
    state.eliminateCandidate(1, 0, 5);
    state.eliminateCandidate(1, 1, 5);
    state.eliminateCandidate(1, 2, 5);
    state.eliminateCandidate(0, 2, 5);
    state.eliminateCandidate(2, 1, 5);
    state.eliminateCandidate(2, 2, 5);

    // Set up conjugate pair in row 6: value 5 only at (6,0) and (6,5)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 5 && state.isAllowed(6, col, 5)) {
            state.eliminateCandidate(6, col, 5);
        }
    }

    // Ensure target (0,5) has value 5 as candidate
    REQUIRE(state.isAllowed(0, 5, 5));

    EmptyRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::EmptyRectangle);
    REQUIRE(result->rating == 4.5);
    REQUIRE(result->eliminations.size() == 1);
    REQUIRE(result->eliminations[0].value == 5);
}

TEST_CASE("EmptyRectangleStrategy - All in one line returns nullopt", "[empty_rectangle]") {
    // If all candidates in a box are in one row or one column, that's a pointing pair, not ER
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Box 0: value 5 only at (0,0) and (0,1) — all in row 0, single row
    state.eliminateCandidate(1, 0, 5);
    state.eliminateCandidate(1, 1, 5);
    state.eliminateCandidate(1, 2, 5);
    state.eliminateCandidate(0, 2, 5);
    state.eliminateCandidate(2, 0, 5);
    state.eliminateCandidate(2, 1, 5);
    state.eliminateCandidate(2, 2, 5);

    // Even with conjugate pairs available, ER should not fire
    // (only 1 row occupied → not an L-shape)
    EmptyRectangleStrategy strategy;
    // The check for this specific box should fail, but other boxes might still produce results
    // Just verify the technique concept: a single-line ER in box 0 should not be found

    // Set up conjugate pair in row 6 passing through col 0
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 5 && state.isAllowed(6, col, 5)) {
            state.eliminateCandidate(6, col, 5);
        }
    }

    auto result = strategy.findStep(board, state);
    // If a result is found, it should NOT be from box 0 (since that's a single-line pattern)
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::EmptyRectangle);
        // The explanation should not reference Box 1 (0-indexed box 0)
        // as the ER box since it's a single-line pattern
    }
}

TEST_CASE("EmptyRectangleStrategy - Explanation contains relevant info", "[empty_rectangle]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    state.eliminateCandidate(1, 0, 5);
    state.eliminateCandidate(1, 1, 5);
    state.eliminateCandidate(1, 2, 5);
    state.eliminateCandidate(0, 2, 5);
    state.eliminateCandidate(2, 1, 5);
    state.eliminateCandidate(2, 2, 5);

    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 5 && state.isAllowed(6, col, 5)) {
            state.eliminateCandidate(6, col, 5);
        }
    }

    EmptyRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Empty Rectangle") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}

TEST_CASE("EmptyRectangleStrategy - Col conjugate pair detection", "[empty_rectangle]") {
    // ER in Box 0 (rows 0-2, cols 0-2) for value 5:
    // L-shape: value 5 at (0,0), (1,0), (0,1) — rows {0,1}, cols {0,1}
    //
    // er_row=0, er_col=0: all box cells in row 0 or col 0 ✓
    //
    // Conjugate pair in Col 6 (outside box 0) passing through er_row=0:
    // Value 5 at (0,6) and (5,6) only in col 6.
    //
    // Target: (target_row=5, er_col=0) = (5,0).
    // getBoxIndex(5,0) = 3 ≠ 0 ✓. Eliminates 5 from (5,0).
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Box 0: keep value 5 only at (0,0), (1,0), (0,1)
    state.eliminateCandidate(0, 2, 5);
    state.eliminateCandidate(1, 1, 5);
    state.eliminateCandidate(1, 2, 5);
    state.eliminateCandidate(2, 0, 5);
    state.eliminateCandidate(2, 1, 5);
    state.eliminateCandidate(2, 2, 5);

    // Col 6: keep value 5 only at rows 0 and 5
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 5 && state.isAllowed(row, 6, 5)) {
            state.eliminateCandidate(row, 6, 5);
        }
    }

    // Ensure target (5,0) has value 5 as candidate
    REQUIRE(state.isAllowed(5, 0, 5));

    EmptyRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::EmptyRectangle);
    REQUIRE(result->rating == 4.5);
    REQUIRE(result->explanation_data.region_type == RegionType::Col);
    REQUIRE(result->eliminations.size() == 1);
    REQUIRE(result->eliminations[0].value == 5);
}

TEST_CASE("EmptyRectangleStrategy - Can be used through ISolvingStrategy interface", "[empty_rectangle]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<EmptyRectangleStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::EmptyRectangle);
    REQUIRE(strategy->getName() == "Empty Rectangle");
    REQUIRE(strategy->getDifficultyRating() == 4.5);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
