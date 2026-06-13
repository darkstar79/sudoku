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
#include "../../src/core/strategies/full_house_strategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// Box 0 has exactly one empty cell R3C3 (2,2) — the only Full House region. Its row (2) and column
// (2) each still have many empties, so this isolates the *box* case. Box 0 holds {1..8}, so R3C3 = 9.
BoardData boxOnlyFullHouse() {
    return {{1, 2, 3, 0, 0, 0, 0, 0, 0}, {4, 5, 6, 0, 0, 0, 0, 0, 0}, {7, 8, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

// Row 0 is filled 2..9 leaving only R1C9 (0,8) empty — the only Full House region. Its column (8) is
// fully empty and its box still has empties, isolating the *row* case. Row 0 holds {2..9}, so R1C9 = 1.
BoardData rowOnlyFullHouse() {
    return {{2, 3, 4, 5, 6, 7, 8, 9, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

// Column 0 is filled 1..8 (rows 0-7) leaving only R9C1 (8,0) empty — the only Full House region. Its
// row (8) is fully empty and its box still has empties, isolating the *column* case. Column 0 = 9.
BoardData colOnlyFullHouse() {
    BoardData board{};
    for (size_t r = 0; r < 8; ++r) {
        board[r][0] = static_cast<int>(r) + 1;
    }
    return board;
}

// A single empty cell R1C1 (0,0) — the last cell of its row, column, AND box at once. Precedence must
// report Box. Box 0 / row 0 / column 0 all hold {everything but 5}, so R1C1 = 5.
BoardData allRegionsFullHouse() {
    return {{0, 3, 4, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
}

// R1C1 (0,0) is a Naked Single pinned to 1 (row forbids 2-8 → {1,9}; R2C1=9 forbids 9 → {1}) but it is
// NOT a region's last cell: row 0 has two empties (C1, C9), column 0 has eight empties, box 0 has six.
// Full House must gate on region emptiness, not candidate count, so it must NOT fire here.
BoardData nakedSingleNotFullHouse() {
    return {{0, 2, 3, 4, 5, 6, 7, 8, 0}, {9, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

}  // namespace

TEST_CASE("FullHouseStrategy - Interface", "[full_house]") {
    FullHouseStrategy strategy;

    SECTION("getTechnique returns FullHouse") {
        REQUIRE(strategy.getTechnique() == SolvingTechnique::FullHouse);
    }
    SECTION("getName returns Full House") {
        REQUIRE(strategy.getName() == "Full House");
    }
    SECTION("getDifficultyRating returns the SE 1.0 base") {
        REQUIRE(strategy.getDifficultyRating() == 1.0);
        REQUIRE(strategy.getDifficultyRating() == getTechniqueRating(SolvingTechnique::FullHouse));
    }
}

// The SECTIONs + has_value() short-circuit guards expand to nested conditionals; complexity is inherent.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("FullHouseStrategy - Finds the last cell of each region type", "[full_house]") {
    FullHouseStrategy strategy;

    SECTION("Box-last cell rates 1.0 and reports region Box") {
        auto board = boxOnlyFullHouse();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(step->type == SolveStepType::Placement);
        REQUIRE(step->technique == SolvingTechnique::FullHouse);
        REQUIRE(step->position.row == 2);
        REQUIRE(step->position.col == 2);
        REQUIRE(step->value == 9);
        REQUIRE(step->rating == 1.0);
        REQUIRE(step->explanation_data.region_type == RegionType::Box);
        REQUIRE(step->explanation_data.region_index == 0);
    }

    SECTION("Row-last cell rates 1.0 and reports region Row") {
        auto board = rowOnlyFullHouse();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(step->technique == SolvingTechnique::FullHouse);
        REQUIRE(step->position.row == 0);
        REQUIRE(step->position.col == 8);
        REQUIRE(step->value == 1);
        REQUIRE(step->rating == 1.0);
        REQUIRE(step->explanation_data.region_type == RegionType::Row);
        REQUIRE(step->explanation_data.region_index == 0);
    }

    SECTION("Column-last cell rates 1.0 and reports region Col") {
        auto board = colOnlyFullHouse();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(step->technique == SolvingTechnique::FullHouse);
        REQUIRE(step->position.row == 8);
        REQUIRE(step->position.col == 0);
        REQUIRE(step->value == 9);
        REQUIRE(step->rating == 1.0);
        REQUIRE(step->explanation_data.region_type == RegionType::Col);
        REQUIRE(step->explanation_data.region_index == 0);
    }
}

TEST_CASE("FullHouseStrategy - Region precedence is box -> row -> col", "[full_house]") {
    FullHouseStrategy strategy;

    // R1C1 completes its row, column, AND box simultaneously; the reported region must be Box.
    auto board = allRegionsFullHouse();
    CandidateGrid state(board);

    auto step = strategy.findStep(board, state);

    REQUIRE(step.has_value());
    REQUIRE(step->technique == SolvingTechnique::FullHouse);
    REQUIRE(step->position.row == 0);
    REQUIRE(step->position.col == 0);
    REQUIRE(step->value == 5);
    REQUIRE(step->explanation_data.region_type == RegionType::Box);
    REQUIRE(step->explanation_data.region_index == 0);
}

TEST_CASE("FullHouseStrategy - Does not fire on a non-region-last naked single", "[full_house]") {
    FullHouseStrategy strategy;

    // A one-candidate cell whose regions all still have other empties is a Naked Single, not a Full
    // House — the false-positive guard. The strategy must decline it (region-emptiness, not count).
    auto board = nakedSingleNotFullHouse();
    CandidateGrid state(board);

    auto step = strategy.findStep(board, state);

    REQUIRE_FALSE(step.has_value());
}
