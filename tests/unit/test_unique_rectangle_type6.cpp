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
#include "../../src/core/strategies/unique_rectangle_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

void keepOnly(CandidateGrid& grid, size_t row, size_t col, const std::vector<int>& keep) {
    for (int v = 1; v <= 9; ++v) {
        if (std::find(keep.begin(), keep.end(), v) == keep.end() && grid.isAllowed(row, col, v)) {
            grid.eliminateCandidate(row, col, v);
        }
    }
}

}  // namespace

TEST_CASE("UniqueRectangleStrategy - Type 6 column-based conjugate pairs", "[unique_rectangle][type6]") {
    // Row-based UR with roof in row 0, floor in row 2.
    // Digit 1 is conjugate in BOTH columns of the UR (cols 0 and 3),
    // which locks the diagonal placement, forcing floor cells to {A,B} only.
    //
    // UR cells: (0,0)={1,2} roof, (0,3)={1,2} roof,
    //           (2,0)={1,2,9} floor, (2,3)={1,2,3} floor
    // Spoiler cells: (2,5), (2,6) have digit 1 → breaks row-2 conjugacy → Type 4 skipped
    //
    // Digit 1 conjugate in col 0: only (0,0) and (2,0)
    // Digit 1 conjugate in col 3: only (0,3) and (2,3)
    // → Type 6 fires → eliminate 9 from (2,0), 3 from (2,3)

    auto board = createEmptyBoard();

    // Row 0: fill all except (0,0) and (0,3) — forces {1,2} candidates
    board[0][1] = 3;
    board[0][2] = 4;
    board[0][4] = 5;
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;

    // Row 2: fill most, leave (2,0), (2,3), (2,5), (2,6) empty
    // Placed values {6,7,8,4,5} → missing {1,2,3,9}
    board[2][1] = 6;
    board[2][2] = 7;
    board[2][4] = 8;
    board[2][7] = 4;
    board[2][8] = 5;

    CandidateGrid state(board);

    // Verify natural candidates before keepOnly
    // (0,0): row={1,2}, box 0 has {3,4,6,7} → missing {1,2,5,8,9} → {1,2}
    // (0,3): row={1,2}, box 1 has {5,6,8} → missing {1,2,3,4,7,9} → {1,2}
    REQUIRE(state.countPossibleValues(0, 0) == 2);
    REQUIRE(state.countPossibleValues(0, 3) == 2);

    // Force floor cell candidates
    keepOnly(state, 2, 0, {1, 2, 9});  // floor: extra 9
    keepOnly(state, 2, 3, {1, 2, 3});  // floor: extra 3
    // Spoiler cells keep digit 1 (row 2 has 4 cells with digit 1 → not conjugate)
    keepOnly(state, 2, 5, {1, 3, 9});  // has digit 1
    keepOnly(state, 2, 6, {1, 2, 3});  // has digit 1

    // Make digit 1 conjugate in cols 0 and 3:
    // Eliminate digit 1 from ALL other empty cells in these columns.
    // This also prevents URs through other rows (since c3/c4 won't have digit 1).
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        if (row == 0 || row == 2) {
            continue;
        }
        if (board[row][0] == EMPTY_CELL && state.isAllowed(row, 0, 1)) {
            state.eliminateCandidate(row, 0, 1);
        }
        if (board[row][3] == EMPTY_CELL && state.isAllowed(row, 3, 1)) {
            state.eliminateCandidate(row, 3, 1);
        }
    }

    // Verify setup
    int count_col0_d1 = 0;
    int count_col3_d1 = 0;
    int count_row2_d1 = 0;
    for (size_t i = 0; i < BOARD_SIZE; ++i) {
        if (board[i][0] == EMPTY_CELL && state.isAllowed(i, 0, 1)) {
            ++count_col0_d1;
        }
        if (board[i][3] == EMPTY_CELL && state.isAllowed(i, 3, 1)) {
            ++count_col3_d1;
        }
        if (board[2][i] == EMPTY_CELL && state.isAllowed(2, i, 1)) {
            ++count_row2_d1;
        }
    }
    REQUIRE(count_col0_d1 == 2);  // conjugate in col 0
    REQUIRE(count_col3_d1 == 2);  // conjugate in col 3
    REQUIRE(count_row2_d1 >= 3);  // NOT conjugate in row 2

    UniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::UniqueRectangle);
    REQUIRE(result->explanation_data.technique_subtype == 5);  // Type 6
    REQUIRE(result->explanation.find("Type 6") != std::string::npos);

    // Should eliminate extras: 9 from (2,0) and 3 from (2,3)
    REQUIRE(result->eliminations.size() == 2);
    bool found_9_elim = false;
    bool found_3_elim = false;
    for (const auto& elim : result->eliminations) {
        if (elim.position.row == 2 && elim.position.col == 0 && elim.value == 9) {
            found_9_elim = true;
        }
        if (elim.position.row == 2 && elim.position.col == 3 && elim.value == 3) {
            found_3_elim = true;
        }
    }
    REQUIRE(found_9_elim);
    REQUIRE(found_3_elim);
}

TEST_CASE("UniqueRectangleStrategy - Type 6 no match when not conjugate in both cols", "[unique_rectangle][type6]") {
    // Same UR setup but digit 1 is NOT conjugate in col 0.
    // Type 6 should not fire.
    auto board = createEmptyBoard();

    board[0][1] = 3;
    board[0][2] = 4;
    board[0][4] = 5;
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;

    board[2][1] = 6;
    board[2][2] = 7;
    board[2][4] = 8;
    board[2][7] = 4;
    board[2][8] = 5;

    CandidateGrid state(board);

    keepOnly(state, 2, 0, {1, 2, 9});
    keepOnly(state, 2, 3, {1, 2, 3});
    keepOnly(state, 2, 5, {1, 3, 9});
    keepOnly(state, 2, 6, {1, 2, 3});

    // Only make digit 1 conjugate in col 3, NOT in col 0
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        if (row == 0 || row == 2) {
            continue;
        }
        if (board[row][3] == EMPTY_CELL && state.isAllowed(row, 3, 1)) {
            state.eliminateCandidate(row, 3, 1);
        }
        // Do NOT eliminate from col 0 → digit 1 not conjugate there
    }

    UniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    // Should not find Type 6 (only conjugate in one column)
    if (result.has_value()) {
        CHECK(result->explanation_data.technique_subtype != 5);
    }
}

TEST_CASE("UniqueRectangleStrategy - Type 6 with multiple extras on floor cells", "[unique_rectangle][type6]") {
    // Floor cells each have 2+ extras. Type 6 should still eliminate all of them.
    auto board = createEmptyBoard();

    board[0][1] = 3;
    board[0][2] = 4;
    board[0][4] = 5;
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][7] = 8;
    board[0][8] = 9;

    board[2][1] = 6;
    board[2][2] = 7;
    board[2][4] = 8;
    board[2][7] = 4;
    board[2][8] = 5;

    CandidateGrid state(board);

    keepOnly(state, 2, 0, {1, 2, 9});     // floor: extra 9 (3 not available — box 0 has 3)
    keepOnly(state, 2, 3, {1, 2, 3, 9});  // floor: extras 3 and 9
    keepOnly(state, 2, 5, {1, 3, 9});
    keepOnly(state, 2, 6, {1, 2, 3});

    // Make digit 1 conjugate in both cols
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        if (row == 0 || row == 2) {
            continue;
        }
        if (board[row][0] == EMPTY_CELL && state.isAllowed(row, 0, 1)) {
            state.eliminateCandidate(row, 0, 1);
        }
        if (board[row][3] == EMPTY_CELL && state.isAllowed(row, 3, 1)) {
            state.eliminateCandidate(row, 3, 1);
        }
    }

    UniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::UniqueRectangle);
    REQUIRE(result->explanation_data.technique_subtype == 5);  // Type 6

    // Should eliminate ALL extras: 9 from (2,0) and 3,9 from (2,3) = 3 eliminations
    REQUIRE(result->eliminations.size() == 3);
}
