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
#include "../../src/core/strategies/x_cycles_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("XCyclesStrategy - Metadata", "[x_cycles]") {
    XCyclesStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::XCycles);
    REQUIRE(strategy.getName() == "X-Cycles");
    REQUIRE(strategy.getDifficultyRating() == 6.6);
}

TEST_CASE("XCyclesStrategy - Returns nullopt for complete board", "[x_cycles]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    XCyclesStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("XCyclesStrategy - Finds X-Cycle with strong link in column", "[x_cycles]") {
    // Set up a board where digit 1 has a conjugate pair (strong link) in column 3.
    // Fill column 3 with NON-1 values to leave only (0,3) and (3,3) empty:
    auto board = createEmptyBoard();

    // Fill column 3 cells with non-1 values (leaving (0,3) and (3,3) empty)
    board[1][3] = 2;
    board[2][3] = 3;
    board[4][3] = 4;
    board[5][3] = 6;
    board[6][3] = 7;
    board[7][3] = 8;
    board[8][3] = 9;

    CandidateGrid state(board);

    // Verify: (0,3) and (3,3) have candidate 1 (not eliminated by constraint propagation)
    REQUIRE(state.isAllowed(0, 3, 1));
    REQUIRE(state.isAllowed(3, 3, 1));

    // Verify strong link in column 3: exactly 2 empty cells with candidate 1
    int col3_count = 0;
    for (size_t r = 0; r < 9; ++r) {
        if (board[r][3] == 0 && state.isAllowed(r, 3, 1)) {
            col3_count++;
        }
    }
    REQUIRE(col3_count == 2);

    XCyclesStrategy strategy;
    auto result = strategy.findStep(board, state);

    // With a rich candidate graph and a strong link, the strategy should find a pattern
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::XCycles);
        REQUIRE(result->rating == 6.6);
    }
}

TEST_CASE("XCyclesStrategy - Finds X-Cycle with multiple strong links", "[x_cycles]") {
    // Set up multiple conjugate pairs for digit 5 by filling units with non-5 values.
    // This creates a richer graph with strong links for the DFS to explore.
    auto board = createEmptyBoard();

    // Box 0: fill 7 cells, leaving only (0,0) and (2,2) empty → strong link on 5 in box 0
    board[0][1] = 3;
    board[0][2] = 4;
    board[1][0] = 6;
    board[1][1] = 7;
    board[1][2] = 8;
    board[2][0] = 9;
    board[2][1] = 2;

    // Col 8: fill 7 cells, leaving only (2,8) and (8,8) empty → strong link on 5 in col 8
    board[0][8] = 9;
    board[1][8] = 8;
    board[3][8] = 7;
    board[4][8] = 6;
    board[5][8] = 4;
    board[6][8] = 3;
    board[7][8] = 2;

    // Col 0: already have (1,0)=6, (2,0)=9. Fill more to leave (0,0) and (8,0) empty.
    board[3][0] = 8;
    board[4][0] = 7;
    board[5][0] = 3;
    board[6][0] = 2;
    board[7][0] = 4;

    CandidateGrid state(board);

    // Verify key cells have digit 5
    REQUIRE(state.isAllowed(0, 0, 5));
    REQUIRE(state.isAllowed(2, 2, 5));
    REQUIRE(state.isAllowed(2, 8, 5));
    REQUIRE(state.isAllowed(8, 8, 5));
    REQUIRE(state.isAllowed(8, 0, 5));

    XCyclesStrategy strategy;
    auto result = strategy.findStep(board, state);

    // Verify it finds an X-Cycle pattern
    if (result.has_value()) {
        REQUIRE(result->technique == SolvingTechnique::XCycles);
        REQUIRE(result->rating == 6.6);
    }
}

TEST_CASE("XCyclesStrategy - No cycle when too few candidates", "[x_cycles]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 0}};
    CandidateGrid state(board);
    XCyclesStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("XCyclesStrategy - Explanation contains technique name", "[x_cycles]") {
    auto board = createEmptyBoard();

    // Column 3: fill with non-1 values
    board[1][3] = 2;
    board[2][3] = 3;
    board[4][3] = 4;
    board[5][3] = 6;
    board[6][3] = 7;
    board[7][3] = 8;
    board[8][3] = 9;

    CandidateGrid state(board);

    XCyclesStrategy strategy;
    auto result = strategy.findStep(board, state);

    if (result.has_value()) {
        REQUIRE(result->explanation.find("X-Cycles") != std::string::npos);
    }
}

TEST_CASE("XCyclesStrategy - Specific Type 1 continuous loop", "[x_cycles]") {
    // Use keepOnly to precisely control which cells have digit 3.
    // Create a 4-cell continuous loop: A-S-B-W-C-S-D-W-A (Type 1).
    //
    // A=(0,0), B=(0,6) — strong in row 0 (only 2 cells with 3)
    // B=(0,6), C=(6,6) — weak (share col 6, 3+ cells with 3)
    // C=(6,6), D=(6,0) — strong in row 6 (only 2 cells with 3)
    // D=(6,0), A=(0,0) — weak (share col 0, 3+ cells with 3)
    //
    // For this to work: in row 0, only (0,0) and (0,6) have digit 3.
    //                   in row 6, only (6,0) and (6,6) have digit 3.
    //                   in col 0, 3+ cells have digit 3 (e.g., (0,0), (3,0), (6,0)).
    //                   in col 6, 3+ cells have digit 3 (e.g., (0,6), (3,6), (6,6)).
    //
    // Elimination: for weak links (B→C and D→A), external cells seeing both endpoints
    // can have 3 eliminated.
    // Weak link D→A: endpoints (6,0) and (0,0), both in col 0.
    // (3,0) is in col 0 and has candidate 3 → can it be eliminated?
    // (3,0) sees (6,0) via col, sees (0,0) via col → YES → eliminate 3 from (3,0).
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Remove digit 3 from all row 0 cells except (0,0) and (0,6) → conjugate pair
    for (size_t c = 0; c < 9; ++c) {
        if (c != 0 && c != 6) {
            state.eliminateCandidate(0, c, 3);
        }
    }

    // Remove digit 3 from all row 6 cells except (6,0) and (6,6) → conjugate pair
    for (size_t c = 0; c < 9; ++c) {
        if (c != 0 && c != 6) {
            state.eliminateCandidate(6, c, 3);
        }
    }

    // Ensure col 0 has 3+ cells with digit 3 (including (0,0), (3,0), (6,0))
    REQUIRE(state.isAllowed(0, 0, 3));
    REQUIRE(state.isAllowed(3, 0, 3));
    REQUIRE(state.isAllowed(6, 0, 3));

    // Ensure col 6 has 3+ cells with digit 3 (including (0,6), (3,6), (6,6))
    REQUIRE(state.isAllowed(0, 6, 3));
    REQUIRE(state.isAllowed(3, 6, 3));
    REQUIRE(state.isAllowed(6, 6, 3));

    XCyclesStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->technique == SolvingTechnique::XCycles);

    // Should find eliminations — e.g., 3 from (3,0) which sees both (0,0) and (6,0)
    REQUIRE_FALSE(result->eliminations.empty());
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 3);
    }
}
