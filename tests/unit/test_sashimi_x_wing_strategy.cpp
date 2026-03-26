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
#include "../../src/core/strategies/sashimi_x_wing_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("SashimiXWingStrategy - Metadata", "[sashimi_x_wing]") {
    SashimiXWingStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::SashimiXWing);
    REQUIRE(strategy.getName() == "Sashimi X-Wing");
    REQUIRE(strategy.getDifficultyRating() == 3.4);
}

TEST_CASE("SashimiXWingStrategy - Returns nullopt for complete board", "[sashimi_x_wing]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    SashimiXWingStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SashimiXWingStrategy - Row-based sashimi X-Wing detection", "[sashimi_x_wing]") {
    // Sashimi condition: at least one row has exactly 1 position.
    // Row 0 (sashimi): value 5 in col 3 only (exactly 1 — the sashimi row)
    // Row 3 (finned):  value 5 in cols 3, 6, and 4 (fin at col 4)
    // Union of columns = {3, 4, 6} = 3 columns (N+1 for order-2 fish)
    // Fin at (3,4) is in box 4 (rows 3-5, cols 3-5). Base col 3 is in box 4.
    // Eliminate 5 from col 3 in rows 4,5 (box 4 rows, not row 0 or 3).
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Set up row 0 with value 5 only in col 3 (sashimi: exactly 1 position)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }

    // Set up row 3 with value 5 only in cols 3, 6, and 4 (fin at col 4)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && col != 6 && col != 4 && state.isAllowed(3, col, 5)) {
            state.eliminateCandidate(3, col, 5);
        }
    }

    // Ensure elimination targets exist: value 5 candidate in col 3, rows 4 or 5 (box 4)
    REQUIRE(state.isAllowed(4, 3, 5));

    SashimiXWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::SashimiXWing);
    REQUIRE(result->rating == 3.4);

    // All eliminations should be value 5, in fin's box (box 4: rows 3-5, cols 3-5)
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        REQUIRE(elim.position.row >= 3);
        REQUIRE(elim.position.row <= 5);
        // Must not be in the pattern rows
        REQUIRE(elim.position.row != 0);
        REQUIRE(elim.position.row != 3);
    }
}

TEST_CASE("SashimiXWingStrategy - Eliminations restricted to fin's box", "[sashimi_x_wing]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Row 0 (sashimi): value 7 in col 0 only (exactly 1 position)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && state.isAllowed(0, col, 7)) {
            state.eliminateCandidate(0, col, 7);
        }
    }

    // Row 3 (finned): value 7 in cols 0, 3, and 1 (fin at col 1)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 1 && state.isAllowed(3, col, 7)) {
            state.eliminateCandidate(3, col, 7);
        }
    }

    // Fin at (3,1) is in box 3 (rows 3-5, cols 0-2). Base col 0 is in box 3.
    // Eliminate 7 from col 0, rows 4-5 (in box 3, not row 0/3)

    SashimiXWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 7);
        // Must be in fin's box (box 3: rows 3-5, cols 0-2)
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 3);
    }
}

TEST_CASE("SashimiXWingStrategy - Explanation contains relevant info", "[sashimi_x_wing]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Row 0 (sashimi): value 5 in col 3 only
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }
    // Row 3: value 5 in cols 3, 6, and 4 (fin)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && col != 6 && col != 4 && state.isAllowed(3, col, 5)) {
            state.eliminateCandidate(3, col, 5);
        }
    }

    SashimiXWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Sashimi X-Wing") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    REQUIRE(result->explanation.find("fin") != std::string::npos);
}

TEST_CASE("SashimiXWingStrategy - Col-based sashimi X-Wing detection", "[sashimi_x_wing]") {
    // Col 0 (sashimi): value 5 at row 3 only (exactly 1 — sashimi condition)
    // Col 6: value 5 at rows 3, 6, and 8 (fin at row 8)
    // Union of rows = {3, 6, 8} = 3 (N+1 for order-2 fish)
    // Fin at (8,6) is in box 8 (rows 6-8, cols 6-8).
    // Eliminate 5 from row 6 in cols 7,8 (box 8, not fish cols)
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Col 0 (sashimi): value 5 at row 3 only
    for (size_t row = 0; row < 9; ++row) {
        if (row != 3 && state.isAllowed(row, 0, 5)) {
            state.eliminateCandidate(row, 0, 5);
        }
    }
    // Col 6: value 5 at rows 3, 6, and 8 (fin at row 8)
    for (size_t row = 0; row < 9; ++row) {
        if (row != 3 && row != 6 && row != 8 && state.isAllowed(row, 6, 5)) {
            state.eliminateCandidate(row, 6, 5);
        }
    }

    SashimiXWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::SashimiXWing);
    REQUIRE(result->explanation_data.region_type == RegionType::Col);
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        // Must be in fin's box (box 8: rows 6-8, cols 6-8)
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 8);
        // Not in fish cols
        REQUIRE(elim.position.col != 0);
        REQUIRE(elim.position.col != 6);
    }
}

TEST_CASE("SashimiXWingStrategy - Can be used through ISolvingStrategy interface", "[sashimi_x_wing]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<SashimiXWingStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::SashimiXWing);
    REQUIRE(strategy->getName() == "Sashimi X-Wing");
    REQUIRE(strategy->getDifficultyRating() == 3.4);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
