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
#include "../../src/core/strategies/hidden_single_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// Test helper: Create a board with a hidden single
BoardData createBoardWithHiddenSingle() {
    // Valid complete board with one cell emptied
    // The empty cell will have a hidden single (only possible value)
    BoardData board = {{0, 3, 4, 6, 7, 8, 9, 1, 2},  // R1C1 empty, must be 5 (hidden single in row/col/box)
                       {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7}, {8, 5, 9, 7, 6, 1, 4, 2, 3},
                       {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6}, {9, 6, 1, 5, 3, 7, 2, 8, 4},
                       {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9}};
    return board;
}

BoardData createBoardWithoutHiddenSingles() {
    // Board where no value is confined to a single cell in any region
    BoardData board;
    return board;
}

// A *line* (row) hidden single that is deliberately NOT a block single (Story 0b.4a Class A).
// Row 1 (index 0) is filled 2..9 leaving only R1C9 (0,8) empty, so value 1 is a hidden single in
// that row. Value 1 appears nowhere on the board, so box 3 (rows 0-2, cols 6-8) still has several
// cells that allow 1 — the single is NOT confined to the box. Block precedence ("easiest region
// wins") therefore resolves it as a row single → SE line rating 1.5.
BoardData createBoardWithLineHiddenSingle() {
    BoardData board = {{2, 3, 4, 5, 6, 7, 8, 9, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
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

TEST_CASE("HiddenSingleStrategy - Finds Hidden Single", "[hidden_single]") {
    HiddenSingleStrategy strategy;

    SECTION("Finds hidden single in box") {
        auto board = createBoardWithHiddenSingle();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(step->type == SolveStepType::Placement);
        REQUIRE(step->technique == SolvingTechnique::HiddenSingle);
        REQUIRE(step->position.row == 0);
        REQUIRE(step->position.col == 0);
        REQUIRE(step->value == 5);
        // Class A (0b.4a): R1C1 is the lone empty cell in its box → a *block* hidden single → SE 1.2.
        REQUIRE((step.has_value() && step->rating == 1.2));  // has_value() guards the deref for clang-tidy
        REQUIRE((step.has_value() && step->explanation_data.region_type == RegionType::Box));
        REQUIRE_FALSE(step->explanation.empty());
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
        REQUIRE((step.has_value() && step->position.col == 8));
        REQUIRE((step.has_value() && step->value == 1));
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
        REQUIRE(step->explanation.find("R1C1") != std::string::npos);
    }

    SECTION("Explanation includes value") {
        REQUIRE(step->explanation.find("5") != std::string::npos);
    }

    SECTION("Explanation describes why value is hidden") {
        // Should mention region constraint (row/column/box)
        REQUIRE(step->explanation.length() > 30);
    }
}

TEST_CASE("HiddenSingleStrategy - Hidden vs Naked Singles", "[hidden_single]") {
    SECTION("Hidden single may have multiple candidates in cell") {
        // Use the standard test board where R1C1 has candidates {5,6,7}
        // but 5 is hidden (only cell in box 1 that can have 5)
        auto board = createBoardWithHiddenSingle();
        CandidateGrid state(board);

        HiddenSingleStrategy hidden_strategy;
        auto hidden_step = hidden_strategy.findStep(board, state);

        // Should find that 5 can only go in R1C1 within box 1
        // (even though R1C1 has other candidates like 6, 7)
        REQUIRE(hidden_step.has_value());
        REQUIRE(hidden_step->value == 5);
        REQUIRE(hidden_step->position.row == 0);
        REQUIRE(hidden_step->position.col == 0);
    }
}

TEST_CASE("HiddenSingleStrategy - Edge Cases", "[hidden_single]") {
    HiddenSingleStrategy strategy;

    SECTION("Handles single empty cell") {
        auto board = createCompleteBoard();
        board[0][0] = 0;  // Make one cell empty
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(step->position.row == 0);
        REQUIRE(step->position.col == 0);
        REQUIRE(step->value == 5);
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

}  // anonymous namespace
