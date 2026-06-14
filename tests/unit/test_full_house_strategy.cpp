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
#include "../../src/core/game_validator.h"
#include "../../src/core/strategies/full_house_strategy.h"
#include "../../src/core/sudoku_solver.h"
#include "../helpers/strategy_test_utils.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>

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

// clang-tidy bugprone-unchecked-optional-access (CI error): the has_value() guard must short-circuit each
// deref in the same expression, and the analysis does not cross a helper boundary — so this helper checks
// has_value() itself. See docs/CODE_QUALITY.md "optional access in Catch2 tests".
[[nodiscard]] bool isFullHousePlacement(const std::optional<SolveStep>& s, size_t row, size_t col, int value,
                                        RegionType region, size_t region_index) {
    return s.has_value() && s->type == SolveStepType::Placement && s->technique == SolvingTechnique::FullHouse &&
           s->position.row == row && s->position.col == col && s->value == value && s->rating == 1.0 &&
           s->explanation_data.region_type == region && s->explanation_data.region_index == region_index;
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
        REQUIRE(isFullHousePlacement(step, 2, 2, 9, RegionType::Box, 0));
    }

    SECTION("Row-last cell rates 1.0 and reports region Row") {
        auto board = rowOnlyFullHouse();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(isFullHousePlacement(step, 0, 8, 1, RegionType::Row, 0));
    }

    SECTION("Column-last cell rates 1.0 and reports region Col") {
        auto board = colOnlyFullHouse();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(isFullHousePlacement(step, 8, 0, 9, RegionType::Col, 0));
    }
}

TEST_CASE("FullHouseStrategy - Region precedence is box -> row -> col", "[full_house]") {
    FullHouseStrategy strategy;

    // R1C1 completes its row, column, AND box simultaneously; the reported region must be Box.
    auto board = allRegionsFullHouse();
    CandidateGrid state(board);

    auto step = strategy.findStep(board, state);

    REQUIRE(step.has_value());
    REQUIRE(isFullHousePlacement(step, 0, 0, 5, RegionType::Box, 0));
}

// D1 order contract (story 0b.4b): a region's last empty cell is ALSO a naked/hidden single, so whichever
// of the three strategies runs first labels it. FullHouse must run first (SE 1.0), else every region-last
// placement reverts to an over-rated 2.3 Naked Single and nearly every puzzle is mis-rated. These two
// tests pin that contract — the order in the chain, and the resulting label from the real solver — so a
// silent reorder of SudokuSolver::initializeStrategies() fails loudly instead of silently mis-rating.
TEST_CASE("FullHouse precedes Naked/Hidden Single in the strategy chain (D1 order contract)", "[full_house]") {
    auto chain = sudoku::testing::createStrategyChain();
    auto indexOf = [&chain](SolvingTechnique t) -> std::ptrdiff_t {
        auto it = std::ranges::find_if(chain, [t](const auto& s) { return s->getTechnique() == t; });
        return it == chain.end() ? -1 : std::ranges::distance(chain.begin(), it);
    };
    const std::ptrdiff_t full_house = indexOf(SolvingTechnique::FullHouse);
    REQUIRE(full_house >= 0);
    REQUIRE(full_house < indexOf(SolvingTechnique::NakedSingle));
    REQUIRE(full_house < indexOf(SolvingTechnique::HiddenSingle));
}

TEST_CASE("Solver labels a region-last cell FullHouse, not NakedSingle (D1 production pin)", "[full_house]") {
    auto validator = std::make_shared<GameValidator>();
    SudokuSolver solver(validator);
    auto board = allRegionsFullHouse();  // single empty cell R1C1 — last in its row, column, and box

    auto step = solver.findNextStep(board);

    REQUIRE(step.has_value());
    REQUIRE((step.has_value() && step->technique == SolvingTechnique::FullHouse));
    REQUIRE((step.has_value() && step->rating == 1.0));
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
