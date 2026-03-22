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
#include "../../src/core/strategies/bug_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

void keepOnly(CandidateGrid& grid, size_t row, size_t col, const std::vector<int>& keep) {
    for (int v = 1; v <= 9; ++v) {
        if (std::find(keep.begin(), keep.end(), v) == keep.end() && grid.isAllowed(row, col, v)) {
            grid.eliminateCandidate(row, col, v);
        }
    }
}

}  // namespace

TEST_CASE("BUGStrategy - Metadata", "[bug]") {
    BUGStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::BUG);
    REQUIRE(strategy.getName() == "BUG");
    REQUIRE(strategy.getDifficultyRating() == 5.6);
}

TEST_CASE("BUGStrategy - Returns nullopt for complete board", "[bug]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    BUGStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("BUGStrategy - Detects BUG+1 and produces placement", "[bug]") {
    // Valid BUG+1: every unit has even number of empty cells so bivalue values
    // {2,5} have even parity everywhere. Only the extra value 8 has odd parity.
    //   Box 0: (0,0),(0,1)   Box 3: (3,0),(3,1)   Box 8: (6,6),(6,7),(7,6),(7,7)
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][1] = 0;
    board[3][0] = 0;
    board[3][1] = 0;
    board[6][6] = 0;
    board[6][7] = 0;
    board[7][6] = 0;
    board[7][7] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {2, 5});
    keepOnly(state, 0, 1, {2, 5});
    keepOnly(state, 3, 0, {2, 5});
    keepOnly(state, 3, 1, {2, 5});
    keepOnly(state, 6, 6, {2, 5});
    keepOnly(state, 6, 7, {2, 5});
    keepOnly(state, 7, 6, {2, 5});
    keepOnly(state, 7, 7, {2, 5, 8});  // trivalue — BUG cell, extra is 8

    // Verify setup
    int bivalue_count = 0;
    int trivalue_count = 0;
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            if (board[row][col] == 0) {
                int cnt = state.countPossibleValues(row, col);
                if (cnt == 2) {
                    ++bivalue_count;
                } else if (cnt == 3) {
                    ++trivalue_count;
                }
            }
        }
    }
    REQUIRE(bivalue_count == 7);
    REQUIRE(trivalue_count == 1);

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Placement);
    REQUIRE(result->technique == SolvingTechnique::BUG);
    REQUIRE(result->rating == 5.6);
    REQUIRE(result->position.row == 7);
    REQUIRE(result->position.col == 7);
    REQUIRE(result->value == 8);
}

TEST_CASE("BUGStrategy - Returns nullopt for invalid BUG state with multiple trivalue cells", "[bug]") {
    // Invalid BUG state: bivalue values {2,5} have odd parity in boxes with single cells.
    // Global parity analysis correctly rejects this — no valid extra assignment exists.
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][3] = 0;
    board[3][0] = 0;
    board[3][3] = 0;

    CandidateGrid state(board);

    keepOnly(state, 0, 0, {2, 5, 8});  // trivalue
    keepOnly(state, 0, 3, {2, 5});
    keepOnly(state, 3, 0, {2, 5});
    keepOnly(state, 3, 3, {2, 5, 8});  // trivalue

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("BUGStrategy - Explanation contains relevant info", "[bug]") {
    // Same valid BUG+1 layout as detection test
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][1] = 0;
    board[3][0] = 0;
    board[3][1] = 0;
    board[6][6] = 0;
    board[6][7] = 0;
    board[7][6] = 0;
    board[7][7] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {2, 5});
    keepOnly(state, 0, 1, {2, 5});
    keepOnly(state, 3, 0, {2, 5});
    keepOnly(state, 3, 1, {2, 5});
    keepOnly(state, 6, 6, {2, 5});
    keepOnly(state, 6, 7, {2, 5});
    keepOnly(state, 7, 6, {2, 5});
    keepOnly(state, 7, 7, {2, 5, 8});

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("BUG") != std::string::npos);
    REQUIRE(result->explanation.find("placed") != std::string::npos);
}

TEST_CASE("BUGStrategy - Can be used through ISolvingStrategy interface", "[bug]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<BUGStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::BUG);
    REQUIRE(strategy->getName() == "BUG");
    REQUIRE(strategy->getDifficultyRating() == 5.6);

    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
