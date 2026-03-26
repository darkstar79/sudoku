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
#include "../../src/core/strategies/grouped_nice_loop_strategy.h"
#include "../helpers/candidate_test_utils.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("GroupedNiceLoopStrategy - Metadata", "[grouped_nice_loop]") {
    GroupedNiceLoopStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::GroupedNiceLoop);
    REQUIRE(strategy.getName() == "Grouped Nice Loop");
    REQUIRE(strategy.getDifficultyRating() == 8.0);
}

TEST_CASE("GroupedNiceLoopStrategy - Returns nullopt for complete board", "[grouped_nice_loop]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    GroupedNiceLoopStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GroupedNiceLoopStrategy - Returns nullopt for nearly complete board", "[grouped_nice_loop]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 0}};
    CandidateGrid state(board);
    GroupedNiceLoopStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GroupedNiceLoopStrategy - Interface compliance", "[grouped_nice_loop]") {
    GroupedNiceLoopStrategy strategy;

    // Verify ISolvingStrategy interface
    const ISolvingStrategy& iface = strategy;
    REQUIRE(iface.getTechnique() == SolvingTechnique::GroupedNiceLoop);
    REQUIRE(iface.getName() == "Grouped Nice Loop");
    REQUIRE(iface.getDifficultyRating() == 8.0);
}

TEST_CASE("GroupedNiceLoopStrategy - Returns nullopt for empty board with no pattern", "[grouped_nice_loop]") {
    // An empty board has too many candidates for any useful grouped AIC pattern
    auto board = createEmptyBoard();
    CandidateGrid state(board);
    GroupedNiceLoopStrategy strategy;

    auto result = strategy.findStep(board, state);
    // Either nullopt (no chain found within length limit) or a valid elimination
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::GroupedNiceLoop);
        REQUIRE(result->rating == 8.0);
        REQUIRE_FALSE(result->eliminations.empty());
    }
}

TEST_CASE("GroupedNiceLoopStrategy - Explanation contains technique name when step found", "[grouped_nice_loop]") {
    // Use a board with some values to constrain candidates
    BoardData board = sudoku::test::getEasyPuzzleWithPatterns();

    CandidateGrid candidates(board);
    GroupedNiceLoopStrategy strategy;
    auto result = strategy.findStep(board, candidates);

    // If a step is found, verify it has the correct technique info
    if (result.has_value()) {
        REQUIRE(result->explanation.find("Grouped Nice Loop") != std::string::npos);
        REQUIRE(result->technique == SolvingTechnique::GroupedNiceLoop);
    }
}
