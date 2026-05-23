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
#include "../../src/core/strategies/swordfish_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("SwordfishStrategy - Metadata", "[swordfish]") {
    SwordfishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::Swordfish);
    REQUIRE(strategy.getName() == "Swordfish");
    REQUIRE(strategy.getDifficultyRating() == 3.8);
}

TEST_CASE("SwordfishStrategy - Returns nullopt for complete board", "[swordfish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    SwordfishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SwordfishStrategy - Row-based Swordfish detection", "[swordfish]") {
    // Set up a board where value 5 forms a Swordfish pattern:
    // In rows 0, 3, 6: value 5 only appears as candidate in columns 1, 4, 7
    // Value 5 in other rows at those columns should be eliminated
    auto board = createEmptyBoard();

    // Place values to constrain where 5 can appear
    // Row 0: place values in most cells, leave cols 1, 4, 7 empty with 5 as candidate
    board[0][0] = 1;
    board[0][2] = 3;
    board[0][3] = 4;
    board[0][5] = 6;
    board[0][6] = 7;
    board[0][8] = 9;

    // Row 3: similar pattern
    board[3][0] = 2;
    board[3][2] = 4;
    board[3][3] = 6;
    board[3][5] = 8;
    board[3][6] = 9;
    board[3][8] = 1;

    // Row 6: similar pattern
    board[6][0] = 3;
    board[6][2] = 6;
    board[6][3] = 7;
    board[6][5] = 1;
    board[6][6] = 2;
    board[6][8] = 4;

    CandidateGrid state(board);

    // Eliminate 5 from rows 0, 3, 6 except at columns 1, 4, 7
    // (The placed values already handle most eliminations via constraints,
    // but we need to ensure 5 only appears at exactly cols 1, 4, 7 in these rows)
    for (size_t row : {size_t(0), size_t(3), size_t(6)}) {
        for (size_t col = 0; col < 9; ++col) {
            if (col != 1 && col != 4 && col != 7 && board[row][col] == 0) {
                if (state.isAllowed(row, col, 5)) {
                    state.eliminateCandidate(row, col, 5);
                }
            }
        }
    }

    // Ensure 5 IS a candidate at the pattern cells
    REQUIRE(state.isAllowed(0, 1, 5));
    REQUIRE(state.isAllowed(0, 4, 5));
    REQUIRE(state.isAllowed(0, 7, 5));
    REQUIRE(state.isAllowed(3, 1, 5));
    REQUIRE(state.isAllowed(3, 4, 5));
    REQUIRE(state.isAllowed(3, 7, 5));
    REQUIRE(state.isAllowed(6, 1, 5));
    REQUIRE(state.isAllowed(6, 4, 5));
    REQUIRE(state.isAllowed(6, 7, 5));

    // Place a 5-candidate in another row at one of these columns to be eliminated
    // Make sure row 1, col 1 has 5 as a candidate (should be natural from empty board)
    bool has_elimination_target = false;
    for (size_t row : {size_t(1), size_t(2), size_t(4), size_t(5), size_t(7), size_t(8)}) {
        for (size_t col : {size_t(1), size_t(4), size_t(7)}) {
            if (board[row][col] == 0 && state.isAllowed(row, col, 5)) {
                has_elimination_target = true;
            }
        }
    }

    if (has_elimination_target) {
        SwordfishStrategy strategy;
        auto result = strategy.findStep(board, state);

        REQUIRE(result.has_value());
        REQUIRE(result->type == SolveStepType::Elimination);
        REQUIRE(result->technique == SolvingTechnique::Swordfish);
        REQUIRE(result->rating == 3.8);
        REQUIRE_FALSE(result->eliminations.empty());

        // All eliminations should be for value 5
        for (const auto& elim : result->eliminations) {
            REQUIRE(elim.value == 5);
            // Should NOT be in the pattern rows
            REQUIRE(elim.position.row != 0);
            REQUIRE(elim.position.row != 3);
            REQUIRE(elim.position.row != 6);
            // Should be in pattern columns
            bool in_pattern_col = (elim.position.col == 1 || elim.position.col == 4 || elim.position.col == 7);
            REQUIRE(in_pattern_col);
        }
    }
}

TEST_CASE("SwordfishStrategy - Explanation contains relevant info", "[swordfish]") {
    auto board = createEmptyBoard();

    // Simple setup: constrain value 3 to form a Swordfish in rows 0, 3, 6 at cols 0, 3, 6
    board[0][1] = 3;  // Place 3 in row 0 box 0 to remove from other cols in box
    board[3][4] = 3;
    board[6][7] = 3;

    CandidateGrid state(board);

    // Eliminate 3 from all positions in rows 0, 3, 6 except cols 0, 3, 6
    for (size_t row : {size_t(0), size_t(3), size_t(6)}) {
        for (size_t col = 0; col < 9; ++col) {
            if (col != 0 && col != 3 && col != 6 && board[row][col] == 0) {
                if (state.isAllowed(row, col, 3)) {
                    state.eliminateCandidate(row, col, 3);
                }
            }
        }
    }

    // Need elimination targets: 3 as candidate in cols 0/3/6 but NOT rows 0/3/6
    bool has_target = false;
    for (size_t row : {size_t(1), size_t(2), size_t(4), size_t(5), size_t(7), size_t(8)}) {
        for (size_t col : {size_t(0), size_t(3), size_t(6)}) {
            if (board[row][col] == 0 && state.isAllowed(row, col, 3)) {
                has_target = true;
            }
        }
    }

    if (has_target) {
        // Need to verify rows 0, 3, 6 have exactly 2-3 positions each for value 3
        bool valid_pattern = true;
        for (size_t row : {size_t(0), size_t(3), size_t(6)}) {
            int count = 0;
            for (size_t col = 0; col < 9; ++col) {
                if (board[row][col] == 0 && state.isAllowed(row, col, 3)) {
                    ++count;
                }
            }
            if (count < 2 || count > 3) {
                valid_pattern = false;
            }
        }

        if (valid_pattern) {
            SwordfishStrategy strategy;
            auto result = strategy.findStep(board, state);

            if (result.has_value()) {
                REQUIRE(result->explanation.find("Swordfish") != std::string::npos);
                REQUIRE(result->explanation.find("eliminates") != std::string::npos);
            }
        }
    }
}

TEST_CASE("SwordfishStrategy - Col-based Swordfish detection", "[swordfish]") {
    // Confine value 5 to fish rows {0,3,6} in fish cols {1,4,7}.
    // Row-based scanner sees the same pattern but has no elimination targets
    // (non-fish rows in those cols were already eliminated) → returns nullopt.
    // Col-based scanner fires: eliminates 5 from fish rows at non-fish cols.
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t col : {size_t(1), size_t(4), size_t(7)}) {
        for (size_t row = 0; row < 9; ++row) {
            if (row != 0 && row != 3 && row != 6) {
                state.eliminateCandidate(row, col, 5);
            }
        }
    }

    SwordfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::Swordfish);
    REQUIRE(result->explanation_data.region_type == RegionType::Col);
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        bool in_fish_row = (elim.position.row == 0 || elim.position.row == 3 || elim.position.row == 6);
        REQUIRE(in_fish_row);
        REQUIRE(elim.position.col != 1);
        REQUIRE(elim.position.col != 4);
        REQUIRE(elim.position.col != 7);
    }
}

TEST_CASE("SwordfishStrategy - Can be used through ISolvingStrategy interface", "[swordfish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<SwordfishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::Swordfish);
    REQUIRE(strategy->getName() == "Swordfish");
    REQUIRE(strategy->getDifficultyRating() == 3.8);

    // Complete board should return nullopt
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
