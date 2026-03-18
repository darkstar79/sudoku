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
#include "../../src/core/strategies/two_string_kite_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("TwoStringKiteStrategy - Metadata", "[two_string_kite]") {
    TwoStringKiteStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::TwoStringKite);
    REQUIRE(strategy.getName() == "2-String Kite");
    REQUIRE(strategy.getDifficultyRating() == 4.1);
}

TEST_CASE("TwoStringKiteStrategy - Returns nullopt for complete board", "[two_string_kite]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    TwoStringKiteStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("TwoStringKiteStrategy - 2-String Kite detection", "[two_string_kite]") {
    // Set up a 2-String Kite for value 5:
    // Row pair: row 0 has value 5 at cols 0 and 2 only
    // Col pair: col 0 has value 5 at rows 0 and 6 only
    // Connected via box 0: (0,0) is shared between row pair and col pair's box
    // Actually (0,0) is in both pairs, which we skip. Let me reconsider.
    //
    // Row pair: row 1 has value 5 at cols 1 and 7 only
    // Col pair: col 2 has value 5 at rows 2 and 8 only
    // (1,1) is in box 0, (2,2) is in box 0 — they share box 0
    // Elimination: cells seeing both (1,7) and (8,2) — e.g., (8,7)
    auto board = createEmptyBoard();

    // Fill row 1 except cols 1 and 7
    board[1][0] = 1;
    board[1][2] = 3;
    board[1][3] = 4;
    board[1][4] = 6;
    board[1][5] = 8;
    board[1][6] = 9;
    board[1][8] = 2;

    CandidateGrid state(board);

    // Ensure row 1 has value 5 only at cols 1 and 7
    for (size_t col = 0; col < 9; ++col) {
        if (col != 1 && col != 7 && board[1][col] == 0 && state.isAllowed(1, col, 5)) {
            state.eliminateCandidate(1, col, 5);
        }
    }

    // Ensure col 2 has value 5 only at rows 2 and 8
    for (size_t row = 0; row < 9; ++row) {
        if (row != 2 && row != 8 && board[row][2] == 0 && state.isAllowed(row, 2, 5)) {
            state.eliminateCandidate(row, 2, 5);
        }
    }

    // Verify conjugate pair setup
    int row1_count = 0;
    for (size_t col = 0; col < 9; ++col) {
        if (board[1][col] == 0 && state.isAllowed(1, col, 5)) {
            ++row1_count;
        }
    }

    int col2_count = 0;
    for (size_t row = 0; row < 9; ++row) {
        if (board[row][2] == 0 && state.isAllowed(row, 2, 5)) {
            ++col2_count;
        }
    }

    if (row1_count == 2 && col2_count == 2) {
        TwoStringKiteStrategy strategy;
        auto result = strategy.findStep(board, state);

        if (result.has_value() && result->technique == SolvingTechnique::TwoStringKite) {
            REQUIRE(result->type == SolveStepType::Elimination);
            REQUIRE(result->rating == 4.1);
            REQUIRE_FALSE(result->eliminations.empty());

            for (const auto& elim : result->eliminations) {
                REQUIRE(elim.value == 5);
            }

            REQUIRE(result->explanation.find("2-String Kite") != std::string::npos);
            REQUIRE(result->explanation.find("eliminates") != std::string::npos);
        }
    }
}

TEST_CASE("TwoStringKiteStrategy - Can be used through ISolvingStrategy interface", "[two_string_kite]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<TwoStringKiteStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::TwoStringKite);
    REQUIRE(strategy->getName() == "2-String Kite");
    REQUIRE(strategy->getDifficultyRating() == 4.1);

    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
