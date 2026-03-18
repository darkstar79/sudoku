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
#include "../../src/core/strategies/hidden_unique_rectangle_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

/// Helper: keep only specified candidates for a cell
void keepOnly(CandidateGrid& grid, size_t row, size_t col, const std::vector<int>& keep) {
    for (int v = 1; v <= 9; ++v) {
        if (std::find(keep.begin(), keep.end(), v) == keep.end() && grid.isAllowed(row, col, v)) {
            grid.eliminateCandidate(row, col, v);
        }
    }
}

}  // namespace

TEST_CASE("HiddenUniqueRectangleStrategy - Metadata", "[hidden_unique_rectangle]") {
    HiddenUniqueRectangleStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::HiddenUniqueRectangle);
    REQUIRE(strategy.getName() == "Hidden Unique Rectangle");
    REQUIRE(strategy.getDifficultyPoints() == 350);
}

TEST_CASE("HiddenUniqueRectangleStrategy - Returns nullopt for complete board", "[hidden_unique_rectangle]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    HiddenUniqueRectangleStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("HiddenUniqueRectangleStrategy - Detects Hidden UR with conjugate pairs", "[hidden_unique_rectangle]") {
    // Set up a Hidden Unique Rectangle:
    // 4 cells forming a rectangle across 2 boxes, all containing {1,2}.
    // Target corner has value 1 as a conjugate pair in both its row and column.
    // Rectangle: (0,0), (0,3), (2,0), (2,3) — spans boxes 0 and 1
    //
    // Target = (2,3): value 1 must be conjugate pair in row 3 and column 4
    // This means value 1 appears in exactly 2 cells in row 3 (at cols 0 and 3)
    // and exactly 2 cells in column 4 (at rows 0 and 2).
    auto board = createEmptyBoard();

    // Fill most of row 2 and column 3 so value 1 is restricted
    // Row 2: fill other cells so 1 only appears at (2,0) and (2,3)
    board[2][1] = 3;
    board[2][2] = 4;
    board[2][4] = 5;
    board[2][5] = 6;
    board[2][6] = 7;
    board[2][7] = 8;
    board[2][8] = 9;

    // Column 3: fill other cells so 1 only appears at (0,3) and (2,3)
    board[1][3] = 4;
    board[3][3] = 5;
    board[4][3] = 6;
    board[5][3] = 7;
    board[6][3] = 8;
    board[7][3] = 9;
    board[8][3] = 3;

    CandidateGrid state(board);

    // All 4 corners must have both 1 and 2 as candidates
    // (0,0): diagonal partner of target — must be bivalue {1,2} for deadly pattern
    keepOnly(state, 0, 0, {1, 2});
    // (0,3): has {1, 2, ...} — keep {1, 2, 6}
    keepOnly(state, 0, 3, {1, 2, 6});
    // (2,0): has {1, 2} — keep {1, 2}
    keepOnly(state, 2, 0, {1, 2});
    // (2,3): has {1, 2, ...} — keep {1, 2, 9} (target: elim 2)
    keepOnly(state, 2, 3, {1, 2, 9});

    // Verify: value 1 must be conjugate pair in row 2
    // Row 2: only (2,0) and (2,3) are empty → 1 is in exactly 2 cells ✓
    REQUIRE(state.isAllowed(2, 0, 1));
    REQUIRE(state.isAllowed(2, 3, 1));

    // Verify: value 1 must be conjugate pair in column 3
    // Column 3: only (0,3) and (2,3) are empty → 1 is in exactly 2 cells ✓
    REQUIRE(state.isAllowed(0, 3, 1));
    REQUIRE(state.isAllowed(2, 3, 1));

    HiddenUniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::HiddenUniqueRectangle);
    REQUIRE(result->points == 350);
    REQUIRE(result->eliminations.size() == 1);

    // Should eliminate 2 from target (2,3) since 1 must go there
    REQUIRE(result->eliminations[0].position.row == 2);
    REQUIRE(result->eliminations[0].position.col == 3);
    REQUIRE(result->eliminations[0].value == 2);
}

TEST_CASE("HiddenUniqueRectangleStrategy - No elimination when conjugate pair missing", "[hidden_unique_rectangle]") {
    // Same rectangle but value 1 is NOT a conjugate pair (3 cells have it in the row)
    auto board = createEmptyBoard();

    // Row 2: leave one extra cell empty so 1 appears in 3 cells
    board[2][1] = 3;
    board[2][2] = 4;
    board[2][4] = 5;
    board[2][5] = 6;
    board[2][6] = 7;
    board[2][7] = 8;
    // (2,8) left empty — will have 1 as candidate, breaking the conjugate pair

    // Column 3: restrict to 2 cells
    board[1][3] = 4;
    board[3][3] = 5;
    board[4][3] = 6;
    board[5][3] = 7;
    board[6][3] = 8;
    board[7][3] = 9;
    board[8][3] = 3;

    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2, 5});
    keepOnly(state, 0, 3, {1, 2, 6});
    keepOnly(state, 2, 0, {1, 2});
    keepOnly(state, 2, 3, {1, 2, 9});
    // (2,8) still has 1 as candidate → 1 is in 3 cells in row 2, not conjugate pair

    HiddenUniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    // Should NOT find a Hidden UR because conjugate pair condition is broken
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("HiddenUniqueRectangleStrategy - Explanation contains technique name", "[hidden_unique_rectangle]") {
    auto board = createEmptyBoard();

    board[2][1] = 3;
    board[2][2] = 4;
    board[2][4] = 5;
    board[2][5] = 6;
    board[2][6] = 7;
    board[2][7] = 8;
    board[2][8] = 9;

    board[1][3] = 4;
    board[3][3] = 5;
    board[4][3] = 6;
    board[5][3] = 7;
    board[6][3] = 8;
    board[7][3] = 9;
    board[8][3] = 3;

    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2});
    keepOnly(state, 0, 3, {1, 2, 6});
    keepOnly(state, 2, 0, {1, 2});
    keepOnly(state, 2, 3, {1, 2, 9});

    HiddenUniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Hidden Unique Rectangle") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}

TEST_CASE("HiddenUniqueRectangleStrategy - No pattern when cells lack both values", "[hidden_unique_rectangle]") {
    // Rectangle where one corner doesn't have both values
    auto board = createEmptyBoard();

    board[2][1] = 3;
    board[2][2] = 4;
    board[2][4] = 5;
    board[2][5] = 6;
    board[2][6] = 7;
    board[2][7] = 8;
    board[2][8] = 9;

    board[1][3] = 4;
    board[3][3] = 5;
    board[4][3] = 6;
    board[5][3] = 7;
    board[6][3] = 8;
    board[7][3] = 9;
    board[8][3] = 3;

    CandidateGrid state(board);

    keepOnly(state, 0, 0, {1, 2, 5});
    keepOnly(state, 0, 3, {1, 3, 6});  // No 2! Doesn't have both values
    keepOnly(state, 2, 0, {1, 2});
    keepOnly(state, 2, 3, {1, 2, 9});

    HiddenUniqueRectangleStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE_FALSE(result.has_value());
}
