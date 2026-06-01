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
#include "../../src/core/strategies/sashimi_jellyfish_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("SashimiJellyfishStrategy - Metadata", "[sashimi_jellyfish]") {
    SashimiJellyfishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::SashimiJellyfish);
    REQUIRE(strategy.getName() == "Sashimi Jellyfish");
    REQUIRE(strategy.getDifficultyRating() == 5.6);
}

TEST_CASE("SashimiJellyfishStrategy - Returns nullopt for complete board", "[sashimi_jellyfish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    SashimiJellyfishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("SashimiJellyfishStrategy - Row-based sashimi Jellyfish detection", "[sashimi_jellyfish]") {
    // Sashimi condition: at least one row has exactly 1 position.
    // Row 0 (sashimi): value 5 in col 0 only (exactly 1 — sashimi row)
    // Row 1:           value 5 in cols 0, 3 (2 positions)
    // Row 3:           value 5 in cols 0, 3, 6 (3 positions)
    // Row 6 (finned):  value 5 in cols 0, 3, 6, 7 (fin at col 7)
    // Union = {0, 3, 6, 7} = 4 cols. Wait — jellyfish needs union = 5 (N+1 for order-4).
    //
    // Revised: need 4 rows, union of 5 columns.
    // Row 0 (sashimi): value 5 in col 0 only (exactly 1 — sashimi row)
    // Row 2:           value 5 in cols 0, 3, 6 (3 positions)
    // Row 5:           value 5 in cols 0, 3, 8 (3 positions)
    // Row 7 (finned):  value 5 in cols 3, 6, 8, 7 (fin at col 7)
    // Union = {0, 3, 6, 7, 8} = 5 cols (N+1 for order-4 fish)
    // Fin at (7,7) in box 8 (rows 6-8, cols 6-8). Base cols 6 and 8 are in box 8.
    // Eliminate 5 from base cols {0,3,6,8} in box 8, not in pattern rows.
    // Box 8 = rows 6-8, cols 6-8. Base col 6: rows 6,8 (not pattern rows 7). Col 8: rows 6,8.
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Row 0 (sashimi): value 5 in col 0 only
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }

    // Row 2: value 5 in cols 0, 3, 6
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 6 && state.isAllowed(2, col, 5)) {
            state.eliminateCandidate(2, col, 5);
        }
    }

    // Row 5: value 5 in cols 0, 3, 8
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 8 && state.isAllowed(5, col, 5)) {
            state.eliminateCandidate(5, col, 5);
        }
    }

    // Row 7 (finned): value 5 in cols 3, 6, 8, 7 (fin at col 7)
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && col != 6 && col != 8 && col != 7 && state.isAllowed(7, col, 5)) {
            state.eliminateCandidate(7, col, 5);
        }
    }

    // Ensure targets exist in fin's box
    REQUIRE(state.isAllowed(6, 6, 5));

    SashimiJellyfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::SashimiJellyfish);
    REQUIRE(result->rating == 5.6);

    // All eliminations must be value 5, in fin's box (box 8: rows 6-8, cols 6-8)
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 8);
        // Not in pattern rows
        REQUIRE(elim.position.row != 0);
        REQUIRE(elim.position.row != 2);
        REQUIRE(elim.position.row != 5);
        REQUIRE(elim.position.row != 7);
    }
}

TEST_CASE("SashimiJellyfishStrategy - Explanation contains relevant info", "[sashimi_jellyfish]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Same sashimi pattern as row-based detection test
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && state.isAllowed(0, col, 5)) {
            state.eliminateCandidate(0, col, 5);
        }
    }
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 6 && state.isAllowed(2, col, 5)) {
            state.eliminateCandidate(2, col, 5);
        }
    }
    for (size_t col = 0; col < 9; ++col) {
        if (col != 0 && col != 3 && col != 8 && state.isAllowed(5, col, 5)) {
            state.eliminateCandidate(5, col, 5);
        }
    }
    for (size_t col = 0; col < 9; ++col) {
        if (col != 3 && col != 6 && col != 8 && col != 7 && state.isAllowed(7, col, 5)) {
            state.eliminateCandidate(7, col, 5);
        }
    }

    SashimiJellyfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Sashimi Jellyfish") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
    REQUIRE(result->explanation.find("fin") != std::string::npos);
}

TEST_CASE("SashimiJellyfishStrategy - Col-based sashimi Jellyfish detection", "[sashimi_jellyfish]") {
    // Col 0 (sashimi): value 5 at row 0 only (exactly 1 — sashimi condition)
    // Col 2:           value 5 at rows 0, 3, 6
    // Col 5:           value 5 at rows 0, 3, 8
    // Col 7 (finned):  value 5 at rows 3, 6, 8, 7 (fin at row 7)
    // Union of rows = {0, 3, 6, 7, 8} = 5 (N+1 for order-4 fish)
    // Fin at (7,7) in box 8 (rows 6-8, cols 6-8).
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // Col 0 (sashimi): value 5 at row 0 only
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && state.isAllowed(row, 0, 5)) {
            state.eliminateCandidate(row, 0, 5);
        }
    }
    // Col 2: value 5 at rows 0, 3, 6
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 6 && state.isAllowed(row, 2, 5)) {
            state.eliminateCandidate(row, 2, 5);
        }
    }
    // Col 5: value 5 at rows 0, 3, 8
    for (size_t row = 0; row < 9; ++row) {
        if (row != 0 && row != 3 && row != 8 && state.isAllowed(row, 5, 5)) {
            state.eliminateCandidate(row, 5, 5);
        }
    }
    // Col 7 (finned): value 5 at rows 3, 6, 8, 7 (fin at row 7)
    for (size_t row = 0; row < 9; ++row) {
        if (row != 3 && row != 6 && row != 8 && row != 7 && state.isAllowed(row, 7, 5)) {
            state.eliminateCandidate(row, 7, 5);
        }
    }

    SashimiJellyfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::SashimiJellyfish);
    REQUIRE(result->explanation_data.pattern_axis == RegionType::Col);
    REQUIRE(result->explanation_data.elimination_axis == RegionType::Box);
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        size_t box = (elim.position.row / 3) * 3 + (elim.position.col / 3);
        REQUIRE(box == 8);
        // Not in fish cols
        REQUIRE(elim.position.col != 0);
        REQUIRE(elim.position.col != 2);
        REQUIRE(elim.position.col != 5);
        REQUIRE(elim.position.col != 7);
    }
}

TEST_CASE("SashimiJellyfishStrategy - Can be used through ISolvingStrategy interface", "[sashimi_jellyfish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<SashimiJellyfishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::SashimiJellyfish);
    REQUIRE(strategy->getName() == "Sashimi Jellyfish");
    REQUIRE(strategy->getDifficultyRating() == 5.6);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
