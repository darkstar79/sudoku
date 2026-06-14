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
#include "../../src/core/strategies/naked_single_strategy.h"
#include "../helpers/candidate_test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

// Test helper: Create a board with a GENUINE (non-region-last) naked single at R1C1.
// Story 0b.4d: a cell that is the only empty in its box/row/col is now a Full House (1.0), not a
// Naked Single, so the fixture must keep ≥2 empties in EVERY region of the single. From the canonical
// solved grid we empty (0,0),(0,8),(2,2),(4,0): R1C1 (=5) is forced (its peers exclude 1-4,6-9), while
// row 0 keeps empties at C1 & C9, box 0 keeps (0,0)&(2,2), col 0 keeps (0,0)&(4,0) — none region-last.
BoardData createBoardWithNakedSingle() {
    BoardData board = sudoku::testing::kSolvedBoard;
    board[0][0] = EMPTY_CELL;  // R1C1 -> 5 (the genuine naked single, first in scan order)
    board[0][8] = EMPTY_CELL;  // keeps row 0 with 2 empties
    board[2][2] = EMPTY_CELL;  // keeps box 0 with 2 empties
    board[4][0] = EMPTY_CELL;  // keeps col 0 with 2 empties
    return board;
}

BoardData createBoardWithMultipleNakedSingles() {
    // Reuses the genuine-single base. (0,0)=5 is the only naked single NakedSingleStrategy returns here:
    // the other emptied cells are region-last (Full Houses) and are deferred (story 0b.4d). This
    // exercises scan order returning the first genuine naked single at R1C1.
    return createBoardWithNakedSingle();
}

BoardData createBoardWithoutNakedSingles() {
    // Board where all empty cells have multiple candidates
    BoardData board = {{0, 0, 3, 4, 5, 6, 7, 8, 9},  // R1C1 and R1C2: both have {1, 2}
                       {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
    return board;
}

BoardData createCompleteBoard() {
    // Fully solved board (no empty cells)
    BoardData board = sudoku::testing::kSolvedBoard;
    return board;
}

TEST_CASE("NakedSingleStrategy - Interface Implementation", "[naked_single]") {
    NakedSingleStrategy strategy;

    SECTION("getTechnique returns NakedSingle") {
        REQUIRE(strategy.getTechnique() == SolvingTechnique::NakedSingle);
    }

    SECTION("getName returns correct name") {
        REQUIRE(strategy.getName() == "Naked Single");
    }

    SECTION("getDifficultyRating returns 10") {
        REQUIRE(strategy.getDifficultyRating() == 2.3);
        REQUIRE(strategy.getDifficultyRating() == getTechniqueRating(SolvingTechnique::NakedSingle));
    }
}

TEST_CASE("NakedSingleStrategy - Finds Naked Single", "[naked_single]") {
    NakedSingleStrategy strategy;

    SECTION("Finds naked single in simple board") {
        auto board = createBoardWithNakedSingle();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(step->type == SolveStepType::Placement);
        REQUIRE(step->technique == SolvingTechnique::NakedSingle);
        REQUIRE(step->position.row == 0);
        REQUIRE(step->position.col == 0);
        REQUIRE(step->value == 5);
        REQUIRE(step->rating == 2.3);
        REQUIRE_FALSE(step->explanation.empty());
    }

    SECTION("Returns first naked single found (scan order)") {
        auto board = createBoardWithMultipleNakedSingles();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        // Should find R1C1 first (scan order: row 0, col 0)
        REQUIRE(step->position.row == 0);
        REQUIRE(step->position.col == 0);
    }
}

TEST_CASE("NakedSingleStrategy - No Naked Single Found", "[naked_single]") {
    NakedSingleStrategy strategy;

    SECTION("Returns nullopt when no naked singles exist") {
        auto board = createBoardWithoutNakedSingles();
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

    SECTION("Returns nullopt for empty board") {
        BoardData board;
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        // Empty board has many candidates per cell, no naked singles
        REQUIRE_FALSE(step.has_value());
    }
}

TEST_CASE("NakedSingleStrategy - Explanation Content", "[naked_single]") {
    NakedSingleStrategy strategy;
    auto board = createBoardWithNakedSingle();
    CandidateGrid state(board);

    auto step = strategy.findStep(board, state);
    REQUIRE(step.has_value());

    SECTION("Explanation mentions technique name") {
        REQUIRE(step->explanation.find("Naked Single") != std::string::npos);
    }

    SECTION("Explanation includes position") {
        REQUIRE(step->explanation.find("R1C1") != std::string::npos);
    }

    SECTION("Explanation includes value") {
        REQUIRE(step->explanation.find("5") != std::string::npos);
    }

    SECTION("Explanation is descriptive") {
        // Should mention "only" and "possible" or similar language
        REQUIRE(step->explanation.length() > 20);
    }
}

TEST_CASE("NakedSingleStrategy - Edge Cases", "[naked_single]") {
    NakedSingleStrategy strategy;

    SECTION("Handles board with conflicts gracefully") {
        // Board with duplicate values (invalid but shouldn't crash)
        BoardData board = {{1, 1, 0, 0, 0, 0, 0, 0, 0},  // Duplicate 1s in row
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0}};
        CandidateGrid state(board);

        // Should not crash, may or may not find step
        auto step = strategy.findStep(board, state);
        // No specific requirement on return value for invalid boards
    }

    SECTION("Handles near-complete board (genuine naked single)") {
        // Story 0b.4d: a lone empty cell is now a Full House (1.0), not a Naked Single. Use the genuine
        // multi-empty fixture so R1C1 stays a true naked single (≥2 empties in box/row/col).
        auto board = createBoardWithNakedSingle();
        CandidateGrid state(board);

        auto step = strategy.findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(step->position.row == 0);
        REQUIRE(step->position.col == 0);
        REQUIRE(step->value == 5);
    }
}

TEST_CASE("NakedSingleStrategy - Polymorphic Usage", "[naked_single]") {
    SECTION("Can be used through ISolvingStrategy interface") {
        std::unique_ptr<ISolvingStrategy> strategy = std::make_unique<NakedSingleStrategy>();

        auto board = createBoardWithNakedSingle();
        CandidateGrid state(board);

        auto step = strategy->findStep(board, state);

        REQUIRE(step.has_value());
        REQUIRE(strategy->getTechnique() == SolvingTechnique::NakedSingle);
        REQUIRE(strategy->getDifficultyRating() == 2.3);
    }
}

}  // anonymous namespace
