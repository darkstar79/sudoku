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
#include "../../src/core/strategies/bug_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using sudoku::testing::keepOnly;

TEST_CASE("BUGStrategy - BUG+2 returns nullopt (disabled)", "[bug][bug_plus_n]") {
    // BUG+n (n>1) disabled due to incorrect parity analysis.
    // Two trivalue cells should return nullopt.
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

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("BUGStrategy - BUG+3 returns nullopt (disabled)", "[bug][bug_plus_n]") {
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

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("BUGStrategy - BUG+2 returns nullopt (restricted to N=1)", "[bug][bug_plus_n]") {
    // BUG+N restricted to N=1 only. Parity analysis for N>1 can produce wrong
    // placements on non-genuine BUG states (18 false positives across 15000 puzzles).
    // This test verifies BUG+2 is correctly rejected.
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
    keepOnly(state, 6, 6, {2, 5, 8});  // trivalue
    keepOnly(state, 6, 7, {2, 5});
    keepOnly(state, 7, 6, {2, 5});
    keepOnly(state, 7, 7, {2, 5, 7});  // trivalue

    BUGStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("BUGStrategy - BUG+1 still works (regression)", "[bug][bug_plus_n]") {
    // Valid BUG+1 with even cell counts per unit — same layout as main BUG+1 test
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
    REQUIRE(result->type == SolveStepType::Placement);
    REQUIRE(result->position.row == 7);
    REQUIRE(result->position.col == 7);
    REQUIRE(result->value == 8);
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

TEST_CASE("BUGStrategy - BUG+2 returns nullopt for explanation test (disabled)", "[bug][bug_plus_n]") {
    // BUG+n disabled — 2 trivalue cells returns nullopt
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
    REQUIRE_FALSE(result.has_value());
}
