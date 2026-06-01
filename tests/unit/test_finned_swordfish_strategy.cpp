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
#include "../../src/core/strategies/finned_swordfish_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("FinnedSwordfishStrategy - Metadata", "[finned_swordfish]") {
    FinnedSwordfishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::FinnedSwordfish);
    REQUIRE(strategy.getName() == "Finned Swordfish");
    REQUIRE(strategy.getDifficultyRating() == 4.0);
}

TEST_CASE("FinnedSwordfishStrategy - Returns nullopt for complete board", "[finned_swordfish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    FinnedSwordfishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("FinnedSwordfishStrategy - Row-based finned Swordfish detection", "[finned_swordfish]") {
    // Finned Swordfish for value 5:
    // Row 0: cols 0, 3         (2 positions — base)
    // Row 3: cols 0, 3         (2 positions — base)
    // Row 6: cols 0, 3, 4      (3 positions — finned, col 4 is fin)
    // Union = {0,3,4} = 4 cols? No, that's 3. We need union = 4 for finned swordfish.
    //
    // Correct: 3 rows with 2-4 positions each, union of cols = 4
    // Row 0: cols 0, 3, 6       (3 positions)
    // Row 3: cols 0, 3, 6       (3 positions)
    // Row 6: cols 0, 3, 6, 7    (4 positions — col 7 is fin)
    // Union = {0,3,6,7} = 4 cols. Base cols = {0,3,6}, fin col = 7.
    // Fin at (6,7) in box 8 (rows 6-8, cols 6-8).
    // Eliminate 5 from base cols {0,3,6} in non-pattern rows, but only in fin's box (box 8).
    // Box 8 = rows 6-8, cols 6-8. Base col 6 is in box 8.
    // Target: col 6, rows 7,8 (in box 8, not in pattern rows)
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Row 0: value 5 in cols 0, 3, 6 only
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 6 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }

    // Row 3: value 5 in cols 0, 3, 6 only
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

    FinnedSwordfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::FinnedSwordfish);
    REQUIRE(result->rating == 4.0);

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

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 TEST_CASE with candidate-setup loops; complexity is inherent to test coverage (gh#39)
TEST_CASE("FinnedSwordfishStrategy - Col-based finned Swordfish detection", "[finned_swordfish]") {
    // Fish cols {0,3,6}. Cols 0,3: rows {0,3,6}. Col 6: rows {0,3,6,7} (fin at row 7).
    // Union = {0,3,6,7} = 4 rows. fin_row=7, fin_col=6, fin_box=8 (rows 6-8, cols 6-8).
    // Base rows = {0,3,6}. base_rows ∩ rows 6-8 = {6}.
    // Non-fish ∩ cols 6-8 = {7,8}. Eliminates 5 from (6,7) and (6,8).
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 6) {
            state.eliminateCandidate(row, 0, 5);
            state.eliminateCandidate(row, 3, 5);
        }
    }
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 6 && row != 7) {
            state.eliminateCandidate(row, 6, 5);
        }
    }

    FinnedSwordfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::FinnedSwordfish);
    REQUIRE((result.has_value() && result->explanation_data.pattern_axis == RegionType::Col));
    REQUIRE((result.has_value() && result->explanation_data.elimination_axis == RegionType::Box));
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        // Must be in fin's box (box 8: rows 6-8, cols 6-8)
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 8);
        // Not in fish cols
        REQUIRE(elim.position.col != 0);
        REQUIRE(elim.position.col != 3);
        REQUIRE(elim.position.col != 6);
    }
}

TEST_CASE("FinnedSwordfishStrategy - Explanation contains relevant info", "[finned_swordfish]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 6 && state.isAllowed(0, col, 5)) {
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

    FinnedSwordfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Finned Swordfish") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    REQUIRE(result->explanation.find("fin") != std::string::npos);
}

TEST_CASE("FinnedSwordfishStrategy - Can be used through ISolvingStrategy interface", "[finned_swordfish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<FinnedSwordfishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::FinnedSwordfish);
    REQUIRE(strategy->getName() == "Finned Swordfish");
    REQUIRE(strategy->getDifficultyRating() == 4.0);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
