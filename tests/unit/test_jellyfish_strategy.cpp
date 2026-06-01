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
#include "../../src/core/strategies/jellyfish_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] BoardData createEmptyBoard() {
    return BoardData{};
}

}  // namespace

TEST_CASE("JellyfishStrategy - Metadata", "[jellyfish]") {
    JellyfishStrategy strategy;

    REQUIRE(strategy.getTechnique() == SolvingTechnique::Jellyfish);
    REQUIRE(strategy.getName() == "Jellyfish");
    REQUIRE(strategy.getDifficultyRating() == 5.2);
}

TEST_CASE("JellyfishStrategy - Returns nullopt for complete board", "[jellyfish]") {
    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    JellyfishStrategy strategy;

    auto result = strategy.findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("JellyfishStrategy - Row-based Jellyfish detection", "[jellyfish]") {
    // Set up a row-based Jellyfish for value 5:
    // 4 rows where value 5 appears in exactly the same 4 columns
    // Row 0: cols 0,2,5,7
    // Row 2: cols 0,2,5,7
    // Row 4: cols 0,2,5,7
    // Row 6: cols 0,2,5,7
    // Eliminate 5 from cols 0,2,5,7 in all other rows
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    // For each of the 4 pattern rows, keep value 5 only in cols 0,2,5,7
    for (size_t row : {size_t(0), size_t(2), size_t(4), size_t(6)}) {
        for (size_t col = 0; col < 9; ++col) {
            if (col != 0 && col != 2 && col != 5 && col != 7 && state.isAllowed(row, col, 5)) {
                state.eliminateCandidate(row, col, 5);
            }
        }
    }

    // Ensure there are elimination targets in non-pattern rows
    REQUIRE(state.isAllowed(1, 0, 5));

    JellyfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::Jellyfish);
    REQUIRE(result->rating == 5.2);

    // All eliminations: value 5, in cols 0/2/5/7, NOT in rows 0/2/4/6
    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        REQUIRE((elim.position.col == 0 || elim.position.col == 2 || elim.position.col == 5 || elim.position.col == 7));
        REQUIRE(elim.position.row != 0);
        REQUIRE(elim.position.row != 2);
        REQUIRE(elim.position.row != 4);
        REQUIRE(elim.position.row != 6);
    }
}

TEST_CASE("JellyfishStrategy - Explanation contains relevant info", "[jellyfish]") {
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t row : {size_t(0), size_t(2), size_t(4), size_t(6)}) {
        for (size_t col = 0; col < 9; ++col) {
            if (col != 0 && col != 2 && col != 5 && col != 7 && state.isAllowed(row, col, 5)) {
                state.eliminateCandidate(row, col, 5);
            }
        }
    }

    JellyfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->explanation.find("Jellyfish") != std::string::npos);
    REQUIRE(result->explanation.find("eliminates") != std::string::npos);
}

TEST_CASE("JellyfishStrategy - Col-based Jellyfish detection", "[jellyfish]") {
    // Confine value 5 to fish rows {0,2,4,6} in fish cols {0,2,5,7}.
    // Row-based: pattern exists but elimination targets (non-fish rows in those cols)
    // were already eliminated → returns nullopt.
    // Col-based: fires and eliminates 5 from fish rows at non-fish cols.
    auto board = createEmptyBoard();
    CandidateGrid state(board);

    for (size_t col : {size_t(0), size_t(2), size_t(5), size_t(7)}) {
        for (size_t row = 0; row < 9; ++row) {
            if (row != 0 && row != 2 && row != 4 && row != 6) {
                state.eliminateCandidate(row, col, 5);
            }
        }
    }

    JellyfishStrategy strategy;
    auto result = strategy.findStep(board, state);

    REQUIRE(result.has_value());
    REQUIRE(result->type == SolveStepType::Elimination);
    REQUIRE(result->technique == SolvingTechnique::Jellyfish);
    REQUIRE((result.has_value() && result->explanation_data.pattern_axis == RegionType::Col));
    REQUIRE((result.has_value() && result->explanation_data.elimination_axis == RegionType::Row));
    REQUIRE_FALSE(result->eliminations.empty());

    for (const auto& elim : result->eliminations) {
        REQUIRE(elim.value == 5);
        bool in_fish_row =
            (elim.position.row == 0 || elim.position.row == 2 || elim.position.row == 4 || elim.position.row == 6);
        REQUIRE(in_fish_row);
        REQUIRE(elim.position.col != 0);
        REQUIRE(elim.position.col != 2);
        REQUIRE(elim.position.col != 5);
        REQUIRE(elim.position.col != 7);
    }
}

TEST_CASE("JellyfishStrategy - Can be used through ISolvingStrategy interface", "[jellyfish]") {
    std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<JellyfishStrategy>();

    REQUIRE(strategy->getTechnique() == SolvingTechnique::Jellyfish);
    REQUIRE(strategy->getName() == "Jellyfish");
    REQUIRE(strategy->getDifficultyRating() == 5.2);

    BoardData board = sudoku::testing::kSolvedBoard;
    CandidateGrid state(board);
    auto result = strategy->findStep(board, state);
    REQUIRE_FALSE(result.has_value());
}
