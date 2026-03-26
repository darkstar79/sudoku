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
#include "../../src/core/strategies/finned_x_wing_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("FinnedXWingStrategy - Metadata", "[finned_x_wing]") {
    FinnedXWingStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::FinnedXWing);
    REQUIRE(strategy.getName() == "Finned X-Wing");
    REQUIRE(strategy.getDifficultyRating() == 3.4);
}

TEST_CASE("FinnedXWingStrategy - Returns nullopt for complete board", "[finned_x_wing]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    FinnedXWingStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("FinnedXWingStrategy - Row-based finned X-Wing detection", "[finned_x_wing]") {
    // Row 0 (base): value 5 in cols 0 and 6 (exactly 2)
    // Row 3 (finned): value 5 in cols 0, 6, and 7 (3 positions — col 7 is fin)
    // Fin at (3,7) is in box 4. Eliminate 5 from cells in cols 0,6 that are in box 4.
    // Box 4 covers rows 3-5, cols 3-5. Col 0 and col 6 are NOT in box 4.
    // So we need the base cols to overlap with the fin's box.
    //
    // Better setup: fin's box must overlap with at least one base column.
    // Row 0 (base): value 5 in cols 3 and 6 (exactly 2)
    // Row 3 (finned): value 5 in cols 3, 6, and 4 (fin at col 4)
    // Fin at (3,4) is in box 4 (rows 3-5, cols 3-5). Base col 3 is in box 4.
    // Eliminate 5 from col 3 in rows 4,5 (box 4 rows, not row 0 or 3).
    auto board = createEmptyBoard();

    CandidateGrid state(board);

    // Set up row 0 with value 5 only in cols 3 and 6
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && col != 6 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }

    // Set up row 3 with value 5 only in cols 3, 6, and 4 (fin)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && col != 6 && col != 4 && state.isAllowed(3, col, 5)) {
            state.eliminateCandidate(3, col, 5);
        }
    }

    // Ensure elimination targets exist: value 5 candidate in col 3, rows 4 or 5 (box 4)
    REQUIRE(state.isAllowed(4, 3, 5));

    FinnedXWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::FinnedXWing);
    REQUIRE(result->rating == 3.4);

    // All eliminations should be value 5, in fin's box (box 4: rows 3-5, cols 3-5)
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        REQUIRE(elim.position.row >= 3);
        REQUIRE(elim.position.row <= 5);
        REQUIRE((elim.position.col == 3 || elim.position.col == 6));
        // Must not be in the pattern rows
        REQUIRE(elim.position.row != 0);
        REQUIRE(elim.position.row != 3);
    }
}

TEST_CASE("FinnedXWingStrategy - Eliminations restricted to fin's box", "[finned_x_wing]") {
    // Verify that cells in base cols but NOT in fin's box are not eliminated
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Row 0 (base): value 7 in cols 0 and 3
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && state.isAllowed(0, col, 7)) {
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
    // But NOT from col 0, rows 6-8 (box 6, not fin's box)

    FinnedXWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 7);
        // Must be in fin's box (box 3: rows 3-5, cols 0-2)
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 3);
    }
}

TEST_CASE("FinnedXWingStrategy - Explanation contains relevant info", "[finned_x_wing]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && col != 6 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && col != 6 && col != 4 && state.isAllowed(3, col, 5)) {
            state.eliminateCandidate(3, col, 5);
        }
    }

    FinnedXWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Finned X-Wing") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    REQUIRE(result->explanation.find("fin") != std::string::npos);
}

TEST_CASE("FinnedXWingStrategy - Col-based finned X-Wing detection", "[finned_x_wing]") {
    // base_col=0: value 5 at rows {3,6} (exactly 2)
    // fin_col=6: value 5 at rows {3,6,8} (exactly 3, base rows match, fin_row=8)
    // fin_box = getBoxIndex(8,6) = box 8 (rows 6-8, cols 6-8)
    // Eliminations: base_rows {3,6} ∩ rows 6-8 = {6}, non-fish ∩ cols 6-8 = {7,8}
    // → eliminates 5 from (6,7) and (6,8)
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t row = 0; row < 9; ++row) {
        if (row != 3 && row != 6) {
            state.eliminateCandidate(row, 0, 5);
        }
    }
    for (size_t row = 0; row < 9; ++row) {
        if (row != 3 && row != 6 && row != 8) {
            state.eliminateCandidate(row, 6, 5);
        }
    }

    FinnedXWingStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::FinnedXWing);
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

TEST_CASE("FinnedXWingStrategy - Can be used through ISolvingStrategy interface", "[finned_x_wing]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<FinnedXWingStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::FinnedXWing);
    REQUIRE(strategy->getName() == "Finned X-Wing");
    REQUIRE(strategy->getDifficultyRating() == 3.4);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
