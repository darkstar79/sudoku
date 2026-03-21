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
#include "../../src/core/strategies/unique_loop_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

void keepOnly(CandidateGrid& grid, size_t row, size_t col, const std::vector<int>& keep) {
    for (int v = 1; v <= 9; ++v) {
        if (std::ranges::find(keep, v) == keep.end() && grid.isAllowed(row, col, v)) {
            grid.eliminateCandidate(row, col, v);
        }
    }
}

}  // namespace

TEST_CASE("UniqueLoopStrategy - Metadata", "[unique_loop]") {
    UniqueLoopStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::UniqueLoop);
    REQUIRE(strategy.getName() == "Unique Loop");
    REQUIRE(strategy.getDifficultyRating() == 4.5);
}

TEST_CASE("UniqueLoopStrategy - Returns nullopt for complete board", "[unique_loop]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    UniqueLoopStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("UniqueLoopStrategy - ISolvingStrategy interface compliance", "[unique_loop]") {
    UniqueLoopStrategy strategy;
    ISolvingStrategy* base = &strategy;

    CHECK(base->getTechnique() == SolvingTechnique::UniqueLoop);
    CHECK(base->getName() == "Unique Loop");
    CHECK(base->getDifficultyRating() == 4.5);
}

TEST_CASE("UniqueLoopStrategy - Type 1 four-cell loop", "[unique_loop]") {
    // Use filled(1) board to avoid constraint propagation issues.
    // 4-cell loop with values {1,2}: (0,0), (0,3), (2,3), (2,0)
    // spanning boxes 0 and 1.
    // 3 roof cells bivalue {1,2}, 1 floor cell {1,2,5}
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][3] = 0;
    board[2][3] = 0;
    board[2][0] = 0;

    CandidateGrid state(board);

    // Use values {3,4} (not 1, since filled(1) eliminates 1 from empty cells)
    keepOnly(state, 0, 0, {3, 4});
    keepOnly(state, 0, 3, {3, 4});
    keepOnly(state, 2, 3, {3, 4});
    keepOnly(state, 2, 0, {3, 4, 5});  // floor cell

    REQUIRE(state.countPossibleValues(0, 0) == 2);
    REQUIRE(state.countPossibleValues(0, 3) == 2);
    REQUIRE(state.countPossibleValues(2, 3) == 2);
    REQUIRE(state.countPossibleValues(2, 0) == 3);

    UniqueLoopStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::UniqueLoop);
    REQUIRE(result->eliminations.size() == 2);

    bool elim_3 = false;
    bool elim_4 = false;
    for (const auto& elim : result->eliminations) {
        CHECK(elim.position.row == 2);
        CHECK(elim.position.col == 0);
        if (elim.value == 3) {
            elim_3 = true;
        }
        if (elim.value == 4) {
            elim_4 = true;
        }
    }
    CHECK(elim_3);
    CHECK(elim_4);
    CHECK(result->explanation.find("Unique Loop") != std::string::npos);
}

TEST_CASE("UniqueLoopStrategy - No loop when fewer than 4 cells", "[unique_loop]") {
    // Only 3 empty cells — cannot form a 4-cell loop
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][3] = 0;
    board[2][0] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {3, 4});
    keepOnly(state, 0, 3, {3, 4});
    keepOnly(state, 2, 0, {3, 4, 5});

    UniqueLoopStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("UniqueLoopStrategy - No elimination when all bivalue", "[unique_loop]") {
    // All 4 cells bivalue {3,4} — 0 floor cells, Type 1 requires exactly 1
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][3] = 0;
    board[2][3] = 0;
    board[2][0] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {3, 4});
    keepOnly(state, 0, 3, {3, 4});
    keepOnly(state, 2, 3, {3, 4});
    keepOnly(state, 2, 0, {3, 4});

    UniqueLoopStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
