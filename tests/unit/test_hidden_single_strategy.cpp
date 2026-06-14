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
#include "../../src/core/strategies/hidden_single_strategy.h"
#include "../../src/core/strategies/naked_single_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <array>
#include <optional>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// White-box probe: exposes StrategyBase's protected region-last predicate so tests can assert against
// the exact production predicate the single strategies defer on (story 0b.4d).
struct RegionLastProbe : StrategyBase {
    using StrategyBase::isRegionLastCell;
};

// Test helper: Create a board with a GENUINE (non-region-last) BLOCK hidden single (Story 0b.4a/0b.4d).
// From the canonical solved grid we empty (0,0),(0,1),(1,1): the first cell in scan order, R1C2 (0,1),
// is a hidden single for digit 3 inside box 0 (the only box-0 cell that can hold 3) — a *block* single
// → SE 1.2. All three regions of (0,1) keep ≥2 empties (row0: C1&C2, col1: (0,1)&(1,1), box0: all three),
// so it is NOT a Full House. (R1C1 (0,0) needs 5, R2C2 (1,1) needs 7 — neither is hidden first in box 0.)
BoardData createBoardWithHiddenSingle() {
    BoardData board = sudoku::testing::kSolvedBoard;
    board[0][0] = EMPTY_CELL;
    board[0][1] = EMPTY_CELL;  // R1C2 -> 3, block hidden single (first in scan order)
    board[1][1] = EMPTY_CELL;
    return board;
}

BoardData createBoardWithoutHiddenSingles() {
    // Board where no value is confined to a single cell in any region
    BoardData board;
    return board;
}

// A *line* (row) hidden single that is deliberately NOT a block single, AND not a Full House
// (Story 0b.4a Class A + 0b.4d). From the canonical solved grid we empty (0,0),(0,1),(1,1),(1,6):
// the first cell in scan order, R1C2 (0,1), is a hidden single for digit 3 in row 0 (the only row-0
// empty that can hold 3). Box 0 also lost its 3 at (1,1) but (1,1) can still hold 3, so 3 is NOT
// confined to a single box cell — block precedence resolves it as a *row* single → SE line 1.5.
// Every region of (0,1) keeps ≥2 empties, so it is not region-last (not a Full House).
BoardData createBoardWithLineHiddenSingle() {
    BoardData board = sudoku::testing::kSolvedBoard;
    board[0][0] = EMPTY_CELL;
    board[0][1] = EMPTY_CELL;  // R1C2 -> 3, line (row) hidden single (first in scan order)
    board[1][1] = EMPTY_CELL;
    board[1][6] = EMPTY_CELL;
    return board;
}

BoardData createCompleteBoard() {
    BoardData board = sudoku::testing::kSolvedBoard;
    return board;
}

TEST_CASE("HiddenSingleStrategy - Interface Implementation", "[hidden_single]") {
    HiddenSingleStrategy strategy;

    SECTION("getTechnique returns HiddenSingle") {
        REQUIRE(strategy.getTechnique() == SolvingTechnique::HiddenSingle);
    }

    SECTION("getName returns correct name") {
        REQUIRE(strategy.getName() == "Hidden Single");
    }

    SECTION("getDifficultyRating returns 15") {
        REQUIRE(strategy.getDifficultyRating() == 1.5);
        REQUIRE(strategy.getDifficultyRating() == getTechniqueRating(SolvingTechnique::HiddenSingle));
    }
}

// The SECTIONs + per-assertion has_value() short-circuit guards expand to many nested conditionals,
// tripping the cognitive-complexity threshold; complexity is inherent to the Catch2 expansion.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("HiddenSingleStrategy - Finds Hidden Single", "[hidden_single]") {
    HiddenSingleStrategy strategy;

    SECTION("Finds hidden single in box") {
        auto board = createBoardWithHiddenSingle();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE((step.has_value() && step->type == SolveStepType::Placement));
        REQUIRE((step.has_value() && step->technique == SolvingTechnique::HiddenSingle));
        REQUIRE((step.has_value() && step->position.row == 0));
        REQUIRE((step.has_value() && step->position.col == 1));
        REQUIRE((step.has_value() && step->value == 3));
        // Class A (0b.4a): 3 is confined to R1C2 within box 0 (box keeps ≥2 empties, so not a Full
        // House) → a *block* hidden single → SE 1.2.
        REQUIRE((step.has_value() && step->rating == 1.2));  // has_value() guards the deref for clang-tidy
        REQUIRE((step.has_value() && step->explanation_data.region_type == RegionType::Box));
        REQUIRE((step.has_value() && !step->explanation.empty()));
    }

    SECTION("A line (row) hidden single rates 1.5 and records the row region") {
        // Class A (0b.4a): the single is confined to a row but not its box → SE line rating 1.5.
        auto board = createBoardWithLineHiddenSingle();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        // has_value() guards each deref for clang-tidy (bugprone-unchecked-optional-access).
        REQUIRE((step.has_value() && step->technique == SolvingTechnique::HiddenSingle));
        REQUIRE((step.has_value() && step->position.row == 0));
        REQUIRE((step.has_value() && step->position.col == 1));
        REQUIRE((step.has_value() && step->value == 3));
        REQUIRE((step.has_value() && step->rating == 1.5));
        REQUIRE((step.has_value() && step->explanation_data.region_type == RegionType::Row));
    }

    SECTION("Leverages existing CandidateGrid::findHiddenSingle") {
        auto board = createBoardWithHiddenSingle();
        CandidateGrid state(board);

        // Verify CandidateGrid method finds it
        auto hidden_single = state.findHiddenSingle(board);
        REQUIRE(hidden_single.has_value());

        // Verify strategy finds same result
        auto step = strategy.findStep(board, state);
        REQUIRE(step.has_value());
        REQUIRE(step->position == hidden_single->first);
        REQUIRE(step->value == hidden_single->second);
    }
}

TEST_CASE("HiddenSingleStrategy - No Hidden Single Found", "[hidden_single]") {
    HiddenSingleStrategy strategy;

    SECTION("Returns nullopt when no hidden singles exist") {
        auto board = createBoardWithoutHiddenSingles();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE_FALSE(step.has_value());
    }

    SECTION("Returns nullopt for complete board") {
        auto board = createCompleteBoard();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE_FALSE(step.has_value());
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — has_value() guards inflate the expansion
TEST_CASE("HiddenSingleStrategy - Explanation Content", "[hidden_single]") {
    HiddenSingleStrategy strategy;
    auto board = createBoardWithHiddenSingle();
    CandidateGrid state(board);

    auto step = strategy.findStep(board, state);
    REQUIRE(step.has_value());

    SECTION("Explanation mentions technique name") {
        REQUIRE(step->explanation.find("Hidden Single") != std::string::npos);
    }

    SECTION("Explanation includes position") {
        REQUIRE((step.has_value() && step->explanation.contains("R1C2")));
    }

    SECTION("Explanation includes value") {
        REQUIRE((step.has_value() && step->explanation.contains('3')));
    }

    SECTION("Explanation describes why value is hidden") {
        // Should mention region constraint (row/column/box)
        REQUIRE(step->explanation.length() > 30);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — has_value() guards inflate the expansion
TEST_CASE("HiddenSingleStrategy - Hidden vs Naked Singles", "[hidden_single]") {
    SECTION("Hidden single may have multiple candidates in cell") {
        // Standard test board: R1C2 has multiple candidates but 3 is hidden (only box-0 cell for 3).
        auto board = createBoardWithHiddenSingle();
        CandidateGrid state(board);

        HiddenSingleStrategy hidden_strategy;
        auto hidden_step = hidden_strategy.findStep(board, state);

        // Should find that 3 can only go in R1C2 within box 0 (even though R1C2 has other candidates).
        REQUIRE(hidden_step.has_value());
        REQUIRE((hidden_step.has_value() && hidden_step->value == 3));
        REQUIRE((hidden_step.has_value() && hidden_step->position.row == 0));
        REQUIRE((hidden_step.has_value() && hidden_step->position.col == 1));
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — has_value() guards inflate the expansion
TEST_CASE("HiddenSingleStrategy - Edge Cases", "[hidden_single]") {
    HiddenSingleStrategy strategy;

    SECTION("Handles near-complete board (genuine hidden single)") {
        // Story 0b.4d: a lone empty cell is now a Full House (1.0), not a Hidden Single. Use the genuine
        // multi-empty fixture so R1C2 stays a true hidden single (≥2 empties in box/row/col).
        auto board = createBoardWithHiddenSingle();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(step->position.row == 0);
        REQUIRE((step.has_value() && step->position.col == 1));
        REQUIRE((step.has_value() && step->value == 3));
    }

    SECTION("Handles board with conflicts gracefully") {
        // Board with duplicate values (invalid)
        BoardData board = {{1, 1, 0, 0, 0, 0, 0, 0, 0},  // Duplicate 1s
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
        CandidateGrid state(board);

        // Should not crash
        auto step = strategy.findStep(board, state);
        // No specific requirement on return value for invalid boards
    }
}

TEST_CASE("HiddenSingleStrategy - Polymorphic Usage", "[hidden_single]") {
    SECTION("Can be used through ISolvingStrategy interface") {
        std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<HiddenSingleStrategy>();

        auto board = createBoardWithHiddenSingle();
        CandidateGrid state(board);

        auto step = strategy->findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(strategy->getTechnique() == SolvingTechnique::HiddenSingle);
        REQUIRE(strategy->getDifficultyRating() == 1.5);
    }
}

// Story 0b.4d AC2 — intrinsic (registration-order-independent) Full House labelling.
// A region-last cell must be DEFERRED by both single strategies and claimed only by FullHouseStrategy,
// even when the single strategies run first. This proves the FullHouse 1.0 label is intrinsic to the
// deferral predicate (StrategyBase::isRegionLastCell) rather than an artifact of registration order.
// SECTION expansion + has_value() guards trip cognitive-complexity; inherent to the Catch2 expansion.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("FullHouse order-independence - singles defer the region-last cell to FullHouse",
          "[hidden_single][naked_single][full_house][order_independence]") {
    // Arrange: a board whose only empty cell (R1C1) is region-last in its box, row, AND col.
    BoardData board = sudoku::testing::kSolvedBoard;
    board[0][0] = EMPTY_CELL;  // must be 5; lone empty in box 0 / row 0 / col 0 -> a Full House
    CandidateGrid state(board);

    NakedSingleStrategy naked;
    HiddenSingleStrategy hidden;
    FullHouseStrategy full_house;

    SECTION("NakedSingle (run first) does not claim the region-last cell") {
        // Act
        auto step = naked.findStep(board, state);
        // Assert: it defers — either nullopt or some other (non-(0,0)) cell.
        const bool claimed_region_last = step.has_value() && step->position == Position{.row = 0, .col = 0};
        REQUIRE(!claimed_region_last);
    }

    SECTION("HiddenSingle (run first) does not claim the region-last cell") {
        // Act
        auto step = hidden.findStep(board, state);
        // Assert
        const bool claimed_region_last = step.has_value() && step->position == Position{.row = 0, .col = 0};
        REQUIRE(!claimed_region_last);
    }

    SECTION("FullHouse claims it as FullHouse with rating 1.0") {
        // Act
        auto step = full_house.findStep(board, state);
        // Assert
        REQUIRE(step.has_value());
        REQUIRE((step.has_value() && step->position.row == 0));
        REQUIRE((step.has_value() && step->position.col == 0));
        REQUIRE((step.has_value() && step->value == 5));
        REQUIRE((step.has_value() && step->technique == SolvingTechnique::FullHouse));
        REQUIRE((step.has_value() && step->rating == 1.0));
    }

    SECTION("Singles registered AHEAD of FullHouse still yield FullHouse 1.0 (perturbed order)") {
        // Build a chain with the single strategies BEFORE FullHouse — the inverse of production order.
        // If the label were a registration-order artefact, NakedSingle/HiddenSingle would steal (0,0) and
        // mis-rate it 2.3/1.2; because they defer, FullHouse claims it regardless of order.
        const std::array<ISolvingStrategy*, 3> perturbed_chain{&naked, &hidden, &full_house};
        std::optional<SolveStep> step;
        for (auto* strategy : perturbed_chain) {
            step = strategy->findStep(board, state);
            if (step.has_value()) {
                break;
            }
        }
        REQUIRE(step.has_value());
        REQUIRE((step.has_value() && step->technique == SolvingTechnique::FullHouse));
        REQUIRE((step.has_value() && step->rating == 1.0));
        REQUIRE((step.has_value() && step->position == Position{.row = 0, .col = 0}));
    }
}

// Story 0b.4d masking guard — deferring a region-last cell must NOT hide a genuine hidden single that
// lies later in scan order. HiddenSingleStrategy SKIPS region-last cells (via the findHiddenSingle skip
// predicate) instead of aborting the scan, so single-strategy callers (findNextStepByTechnique, training
// findAllSteps) still surface a real hidden single even when a Full House sits earlier on the board.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("findHiddenSingle(skip) skips region-last cells without masking a genuine single", "[hidden_single]") {
    // createBoardWithHiddenSingle: (0,1)=3 is the genuine non-region-last hidden single; (0,0)=5 and
    // (1,1)=7 are region-last (Full Houses) that are ALSO hidden singles for their forced values.
    BoardData board = createBoardWithHiddenSingle();
    CandidateGrid state(board);

    // With the PRODUCTION predicate, region-last cells are skipped and the genuine single is surfaced —
    // exactly what stops a Full House from masking a real hidden single in single-strategy callers.
    auto genuine = state.findHiddenSingle(
        board, [&board](const Position& pos) { return RegionLastProbe::isRegionLastCell(board, pos.row, pos.col); });
    REQUIRE(genuine.has_value());
    REQUIRE((genuine.has_value() && genuine->first == Position{.row = 0, .col = 1}));
    REQUIRE((genuine.has_value() && genuine->second == 3));
    REQUIRE((genuine.has_value() && !RegionLastProbe::isRegionLastCell(board, genuine->first.row, genuine->first.col)));

    // And skipping continues the scan rather than aborting: skipping the first-found single yields a
    // DIFFERENT later single, never nullopt.
    auto first = state.findHiddenSingle(board);
    REQUIRE(first.has_value());
    auto next = state.findHiddenSingle(board, [&first](const Position& pos) { return pos == first->first; });
    REQUIRE(next.has_value());
    REQUIRE((first.has_value() && next.has_value() && next->first != first->first));
}

}  // anonymous namespace
