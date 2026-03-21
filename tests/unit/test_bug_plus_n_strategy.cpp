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

TEST_CASE("BUGStrategy - BUG+2 detects two trivalue cells", "[bug][bug_plus_n]") {
    // BUG+2: all empty cells bivalue except exactly 2 with 3 candidates.
    // Trivalue cells must be in different rows, columns, and boxes so their
    // odd candidates (which may be the same value) don't conflict.
    BoardData board = BoardData::filled(1);

    // 6 empty cells: 4 bivalue + 2 trivalue
    board[0][0] = 0;
    board[0][3] = 0;
    board[3][0] = 0;
    board[3][3] = 0;
    board[6][0] = 0;  // trivalue cell 1 (row 6, col 0, box 6)
    board[0][6] = 0;  // trivalue cell 2 (row 0, col 6, box 2) — different row/col/box

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {2, 5});
    keepOnly(state, 0, 3, {2, 5});
    keepOnly(state, 3, 0, {2, 5});
    keepOnly(state, 3, 3, {2, 5});
    keepOnly(state, 6, 0, {2, 5, 8});  // trivalue
    keepOnly(state, 0, 6, {2, 5, 7});  // trivalue

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
    REQUIRE(bivalue_count == 4);
    REQUIRE(trivalue_count == 2);

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Placement);
    REQUIRE(result->technique == SolvingTechnique::BUG);
    REQUIRE(result->value > 0);
    // The placement should be in one of the trivalue cells
    bool is_trivalue_cell = (result->position.row == 0 && result->position.col == 6) ||
                            (result->position.row == 6 && result->position.col == 0);
    REQUIRE(is_trivalue_cell);
}

TEST_CASE("BUGStrategy - BUG+3 detects three trivalue cells", "[bug][bug_plus_n]") {
    BoardData board = BoardData::filled(1);

    // 7 empty cells: 4 bivalue + 3 trivalue (all in different rows/cols/boxes)
    board[0][0] = 0;
    board[0][3] = 0;
    board[3][0] = 0;
    board[3][3] = 0;
    board[6][0] = 0;  // trivalue 1 (row 6, col 0, box 6)
    board[0][6] = 0;  // trivalue 2 (row 0, col 6, box 2)
    board[3][8] = 0;  // trivalue 3 (row 3, col 8, box 5) — all different rows/cols/boxes

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {2, 5});
    keepOnly(state, 0, 3, {2, 5});
    keepOnly(state, 3, 0, {2, 5});
    keepOnly(state, 3, 3, {2, 5});
    keepOnly(state, 6, 0, {2, 5, 8});
    keepOnly(state, 0, 6, {2, 5, 7});
    keepOnly(state, 3, 8, {2, 5, 4});

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Placement);
    REQUIRE(result->technique == SolvingTechnique::BUG);
    REQUIRE(result->value > 0);
}

TEST_CASE("BUGStrategy - BUG+1 still works (regression)", "[bug][bug_plus_n]") {
    // Existing BUG+1 behavior should not be affected
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][3] = 0;
    board[3][0] = 0;
    board[3][3] = 0;
    board[6][6] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {2, 5});
    keepOnly(state, 0, 3, {2, 5});
    keepOnly(state, 3, 0, {2, 5});
    keepOnly(state, 3, 3, {2, 5});
    keepOnly(state, 6, 6, {2, 5, 8});

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Placement);
    REQUIRE(result->position.row == 6);
    REQUIRE(result->position.col == 6);
}

TEST_CASE("BUGStrategy - Returns nullopt for 4+ trivalue cells", "[bug][bug_plus_n]") {
    // Too many trivalue cells — BUG+n only handles up to 3
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][3] = 0;
    board[3][0] = 0;
    board[3][3] = 0;
    board[6][0] = 0;
    board[0][6] = 0;
    board[3][6] = 0;
    board[6][6] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {2, 5});
    keepOnly(state, 0, 3, {2, 5});
    keepOnly(state, 3, 0, {2, 5});
    keepOnly(state, 3, 3, {2, 5});
    keepOnly(state, 6, 0, {2, 5, 8});
    keepOnly(state, 0, 6, {2, 5, 7});
    keepOnly(state, 3, 6, {2, 5, 4});
    keepOnly(state, 6, 6, {2, 5, 3});

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("BUGStrategy - BUG+2 explanation mentions BUG", "[bug][bug_plus_n]") {
    BoardData board = BoardData::filled(1);
    board[0][0] = 0;
    board[0][3] = 0;
    board[3][0] = 0;
    board[3][3] = 0;
    board[6][0] = 0;
    board[0][6] = 0;

    CandidateGrid state(board);
    keepOnly(state, 0, 0, {2, 5});
    keepOnly(state, 0, 3, {2, 5});
    keepOnly(state, 3, 0, {2, 5});
    keepOnly(state, 3, 3, {2, 5});
    keepOnly(state, 6, 0, {2, 5, 8});
    keepOnly(state, 0, 6, {2, 5, 7});

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("BUG") != std::string::npos);
}
