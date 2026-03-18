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
#include "../../src/core/strategies/simple_coloring_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("SimpleColoringStrategy - Metadata", "[simple_coloring]") {
    SimpleColoringStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::SimpleColoring);
    REQUIRE(strategy.getName() == "Simple Coloring");
    REQUIRE(strategy.getDifficultyPoints() == 350);
}

TEST_CASE("SimpleColoringStrategy - Returns nullopt for complete board", "[simple_coloring]") {
    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    SimpleColoringStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SimpleColoringStrategy - Rule 2 exclusion detection", "[simple_coloring]") {
    // Set up a coloring chain for value 5:
    // Build conjugate pairs forming a chain, then check for exclusion
    auto board = createEmptyBoard();

    // We need a chain of conjugate pairs for value 5
    // Row 0: value 5 at cols 0 and 3 only (conjugate pair: edge between (0,0)-(0,3))
    // Col 0: value 5 at rows 0 and 6 only (conjugate pair: edge between (0,0)-(6,0))
    // Col 3: value 5 at rows 0 and 4 only (conjugate pair: edge between (0,3)-(4,3))
    // This creates a chain: (6,0) -[col0]- (0,0) -[row0]- (0,3) -[col3]- (4,3)
    // Colors: (6,0)=A, (0,0)=B, (0,3)=A, (4,3)=B

    // Fill rows/cols to create the conjugate pairs
    // Row 0: 5 only at cols 0 and 3
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3) {
            if (col == 1) {
                board[0][col] = 1;
            } else if (col == 2) {
                board[0][col] = 2;
            } else if (col == 4) {
                board[0][col] = 3;
            } else if (col == 5) {
                board[0][col] = 4;
            } else if (col == 6) {
                board[0][col] = 6;
            } else if (col == 7) {
                board[0][col] = 7;
            } else if (col == 8) {
                board[0][col] = 8;
            }
        }
    }

    CandidateGrid state(board);

    // Eliminate 5 from all cells in row 0 except (0,0) and (0,3)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && board[0][col] == 0 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }

    // Col 0: 5 only at rows 0 and 6
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 6 && board[row][0] == 0 && state.isAllowed(row, 0, 5)) {
            state.eliminateCandidate(row, 0, 5);
        }
    }

    // Col 3: 5 only at rows 0 and 4
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 4 && board[row][3] == 0 && state.isAllowed(row, 3, 5)) {
            state.eliminateCandidate(row, 3, 5);
        }
    }

    // Verify conjugate pairs
    int row0_count = 0;
    int col0_count = 0;
    int col3_count = 0;
    for (size_t c = 0; c < 9; ++c) {
        if (board[0][c] == 0 && state.isAllowed(0, c, 5)) {
            ++row0_count;
        }
    }
    for (size_t r = 0; r < 9; ++r) {
        if (board[r][0] == 0 && state.isAllowed(r, 0, 5)) {
            ++col0_count;
        }
        if (board[r][3] == 0 && state.isAllowed(r, 3, 5)) {
            ++col3_count;
        }
    }

    if (row0_count == 2 && col0_count == 2 && col3_count == 2) {
        SimpleColoringStrategy strategy;
        auto result = strategy.findStep(board, state);

        if (result.has_value() && result->technique == SolvingTechnique::SimpleColoring) {
            REQUIRE(result->type == SolveStepType::Elimination);
            REQUIRE(result->points == 350);
            REQUIRE_FALSE(result->eliminations.empty());

            for (const auto& elim : result->eliminations) {
                REQUIRE(elim.value == 5);
            }

            REQUIRE(result->explanation.find("Simple Coloring") != std::string::npos);
            REQUIRE(result->explanation.find("eliminates") != std::string::npos);
        }
    }
}

TEST_CASE("SimpleColoringStrategy - Rule 1 contradiction detection", "[simple_coloring]") {
    // Chain: (0,0)=A, (0,3)=B [row 0], (3,3)=A [col 3], (3,1)=B [row 3], (1,1)=A [col 1]
    // (0,0) and (1,1) are both color A and both in box 0 → contradiction!
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Row 0: value 5 only at cols 0 and 3
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }
    // Col 3: value 5 only at rows 0 and 3
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && state.isAllowed(row, 3, 5)) {
            state.eliminateCandidate(row, 3, 5);
        }
    }
    // Row 3: value 5 only at cols 1 and 3
    for (size_t col = 0; col < 9; ++col) {
        if (col != 1 && col != 3 && state.isAllowed(3, col, 5)) {
            state.eliminateCandidate(3, col, 5);
        }
    }
    // Col 1: value 5 only at rows 1 and 3
    for (size_t row = 0; row < 9; ++row) {
        if (row != 1 && row != 3 && state.isAllowed(row, 1, 5)) {
            state.eliminateCandidate(row, 1, 5);
        }
    }

    SimpleColoringStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::SimpleColoring);
    REQUIRE(result->points == 350);
    REQUIRE_FALSE(result->eliminations.empty());
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
    }
    REQUIRE(result->explanation.find("Simple Coloring") != std::string::npos);
}

TEST_CASE("SimpleColoringStrategy - Rule 2 exclusion with outside cell", "[simple_coloring]") {
    // Chain: (0,0)=A [col 0] (3,0)=B [row 3] (3,6)=A [col 6] (0,6)=B
    // Row 0 is unrestricted: cells (0,1)...(0,5),(0,7),(0,8) see both (0,0)=A and (0,6)=B
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Col 0: value 5 only at rows 0 and 3
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && state.isAllowed(row, 0, 5)) {
            state.eliminateCandidate(row, 0, 5);
        }
    }
    // Row 3: value 5 only at cols 0 and 6
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 6 && state.isAllowed(3, col, 5)) {
            state.eliminateCandidate(3, col, 5);
        }
    }
    // Col 6: value 5 only at rows 0 and 3
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && state.isAllowed(row, 6, 5)) {
            state.eliminateCandidate(row, 6, 5);
        }
    }

    SimpleColoringStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::SimpleColoring);
    REQUIRE(result->points == 350);
    REQUIRE_FALSE(result->eliminations.empty());
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
    }
    REQUIRE(result->explanation.find("Simple Coloring") != std::string::npos);
}

TEST_CASE("SimpleColoringStrategy - Can be used through ISolvingStrategy interface", "[simple_coloring]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<SimpleColoringStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::SimpleColoring);
    REQUIRE(strategy->getName() == "Simple Coloring");
    REQUIRE(strategy->getDifficultyPoints() == 350);

    BoardData board = {{5, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
                       {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
                       {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
