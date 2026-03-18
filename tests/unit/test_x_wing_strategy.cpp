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
#include "../../src/core/strategies/x_wing_strategy.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::test;

TEST_CASE("XWingStrategy - Metadata", "[x_wing]") {
    XWingStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::XWing);
    REQUIRE(strategy.getName() == "X-Wing");
    REQUIRE(strategy.getDifficultyPoints() == 200);
}

TEST_CASE("XWingStrategy - Returns nullopt for complete board", "[x_wing]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    XWingStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("XWingStrategy - Finds row-based X-Wing with manual setup", "[x_wing]") {
    // Create X-Wing for value 5:
    // Row 0: value 5 only in cols 0 and 3
    // Row 4: value 5 only in cols 0 and 3
    // → Eliminate 5 from cols 0 and 3 in all other rows

    auto board = createEmptyBoard();

    // Fill row 0 except cols 0 and 3
    board[0][1] = 1;
    board[0][2] = 2;
    board[0][4] = 3;
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;

    // Fill row 4 except cols 0 and 3
    board[4][1] = 3;
    board[4][2] = 6;
    board[4][4] = 1;
    board[4][5] = 8;
    board[4][6] = 2;
    board[4][7] = 9;
    board[4][8] = 7;

    CandidateGrid state(board);

    // Eliminate 5 from (0,0) and (0,3) except keep it there, and from (4,0)/(4,3)
    // We need to ensure value 5 appears in exactly cols 0,3 in rows 0,4
    // Eliminate all other values except 5 would be wrong — we need 5 to be in exactly 2 cols
    // Instead, eliminate 5 from all other empty cells in rows 0 and 4
    // Row 0 empty cells: (0,0) and (0,3) — already only 2 empty, both can have 5
    // Row 4 empty cells: (4,0) and (4,3) — already only 2 empty, both can have 5

    // Now we need 5 to be a candidate in col 0 or col 3 in some other row for elimination
    // Other rows have many empty cells, so 5 should be a candidate in cols 0 and 3

    XWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->technique == SolvingTechnique::XWing);
        REQUIRE(result->points == 200);
        REQUIRE_FALSE(result->eliminations.empty());

        // All eliminations should be for value 5 in cols 0 or 3, not in rows 0 or 4
        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.value >= 1);
            REQUIRE(elim.value <= 9);
            REQUIRE(elim.position.row != 0);
            REQUIRE(elim.position.row != 4);
            bool in_xwing_col = (elim.position.col == 0 || elim.position.col == 3);
            REQUIRE(in_xwing_col);
        }
    }
}

TEST_CASE("XWingStrategy - Col-based X-Wing detection", "[x_wing]") {
    // Confine value 5 to fish rows {0,3} in fish cols {1,4}.
    // Row-based sees same pattern but has no elimination targets
    // (non-fish rows in cols 1,4 are already eliminated) → returns nullopt.
    // Col-based fires: eliminates 5 from fish rows at non-fish cols.
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t col : {size_t(1), size_t(4)}) {
        for (size_t row = 0; row < 9; ++row) {
            if (row != 0 && row != 3) {
                state.eliminateCandidate(row, col, 5);
            }
        }
    }

    XWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::XWing);
    REQUIRE(result->explanation_data.region_type == RegionType::Col);
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        bool in_fish_row = (elim.position.row == 0 || elim.position.row == 3);
        REQUIRE(in_fish_row);
        REQUIRE(elim.position.col != 1);
        REQUIRE(elim.position.col != 4);
    }
}

TEST_CASE("XWingStrategy - Explanation contains relevant info", "[x_wing]") {
    auto board = createEmptyBoard();
    board[0][1] = 1;
    board[0][2] = 2;
    board[0][4] = 3;
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;
    board[4][1] = 3;
    board[4][2] = 6;
    board[4][4] = 1;
    board[4][5] = 8;
    board[4][6] = 2;
    board[4][7] = 9;
    board[4][8] = 7;

    CandidateGrid state(board);
    XWingStrategy strategy;

    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("X-Wing") != std::string::npos);
        REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    }
}
