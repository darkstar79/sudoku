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
#include "../../src/core/strategies/sashimi_swordfish_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("SashimiSwordfishStrategy - Metadata", "[sashimi_swordfish]") {
    SashimiSwordfishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::SashimiSwordfish);
    REQUIRE(strategy.getName() == "Sashimi Swordfish");
    REQUIRE(strategy.getDifficultyRating() == 4.2);
}

TEST_CASE("SashimiSwordfishStrategy - Returns nullopt for complete board", "[sashimi_swordfish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    SashimiSwordfishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SashimiSwordfishStrategy - Row-based sashimi Swordfish detection", "[sashimi_swordfish]") {
    // Sashimi condition: at least one row has exactly 1 position.
    // Row 0 (sashimi): value 5 in col 0 only (exactly 1 — sashimi row)
    // Row 3:           value 5 in cols 0, 3, 6 (3 positions)
    // Row 6 (finned):  value 5 in cols 0, 3, 6, 7 (fin at col 7)
    // Union = {0, 3, 6, 7} = 4 cols (N+1 for order-3 fish)
    // Fin at (6,7) in box 8 (rows 6-8, cols 6-8). Base col 6 is in box 8.
    // Eliminate 5 from col 6, rows 7-8 (in box 8, not pattern rows)
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Row 0 (sashimi): value 5 in col 0 only
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }

    // Row 3: value 5 in cols 0, 3, 6
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 6 && state.isAllowed(3, col, 5)) {
            state.eliminateCandidate(3, col, 5);
        }
    }

    // Row 6: value 5 in cols 0, 3, 6, 7 (fin at col 7)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 6 && col != 7 && state.isAllowed(6, col, 5)) {
            state.eliminateCandidate(6, col, 5);
        }
    }

    // Ensure targets exist
    REQUIRE(state.isAllowed(7, 6, 5));

    SashimiSwordfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::SashimiSwordfish);
    REQUIRE(result->rating == 4.2);

    // All eliminations must be value 5, in fin's box (box 8)
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 8);
        // Not in pattern rows
        REQUIRE(elim.position.row != 0);
        REQUIRE(elim.position.row != 3);
        REQUIRE(elim.position.row != 6);
    }
}

TEST_CASE("SashimiSwordfishStrategy - Explanation contains relevant info", "[sashimi_swordfish]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Same sashimi pattern as row-based detection test
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 6 && state.isAllowed(3, col, 5)) {
            state.eliminateCandidate(3, col, 5);
        }
    }
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 6 && col != 7 && state.isAllowed(6, col, 5)) {
            state.eliminateCandidate(6, col, 5);
        }
    }

    SashimiSwordfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Sashimi Swordfish") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    REQUIRE(result->explanation.find("fin") != std::string::npos);
}

TEST_CASE("SashimiSwordfishStrategy - Col-based sashimi Swordfish detection", "[sashimi_swordfish]") {
    // Col 0 (sashimi): value 5 at row 0 only (exactly 1 — sashimi condition)
    // Col 3:           value 5 at rows 0, 3, 6
    // Col 6:           value 5 at rows 0, 3, 6, 7 (fin at row 7)
    // Union of rows = {0, 3, 6, 7} = 4 (N+1 for order-3 fish)
    // Fin at (7,6) in box 8 (rows 6-8, cols 6-8).
    // Eliminate 5 from rows in box 8, not in fish cols
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Col 0 (sashimi): value 5 at row 0 only
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && state.isAllowed(row, 0, 5)) {
            state.eliminateCandidate(row, 0, 5);
        }
    }
    // Col 3: value 5 at rows 0, 3, 6
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 6 && state.isAllowed(row, 3, 5)) {
            state.eliminateCandidate(row, 3, 5);
        }
    }
    // Col 6: value 5 at rows 0, 3, 6, 7 (fin at row 7)
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 6 && row != 7 && state.isAllowed(row, 6, 5)) {
            state.eliminateCandidate(row, 6, 5);
        }
    }

    SashimiSwordfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::SashimiSwordfish);
    REQUIRE(result->explanation_data.pattern_axis == RegionType::Col);
    REQUIRE(result->explanation_data.elimination_axis == RegionType::Box);
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 8);
        // Not in fish cols
        REQUIRE(elim.position.col != 0);
        REQUIRE(elim.position.col != 3);
        REQUIRE(elim.position.col != 6);
    }
}

TEST_CASE("SashimiSwordfishStrategy - Can be used through ISolvingStrategy interface", "[sashimi_swordfish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<SashimiSwordfishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::SashimiSwordfish);
    REQUIRE(strategy->getName() == "Sashimi Swordfish");
    REQUIRE(strategy->getDifficultyRating() == 4.2);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
