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
#include "../../src/core/solving_technique.h"
#include "../../src/core/training_answer_validator.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

/// A board with a few empty cells that have naked singles and hidden singles.
/// Row 0: 5 3 _ | 6 7 8 | 9 1 2   (cell (0,2) = 4 naked single)
/// Row 1: 6 7 2 | 1 9 5 | 3 4 8
/// Row 2: 1 9 8 | 3 4 2 | 5 6 7
/// Row 3: 8 5 9 | 7 6 1 | 4 2 3
/// Row 4: 4 2 6 | 8 5 3 | 7 9 1
/// Row 5: 7 1 3 | 9 2 4 | 8 5 6
/// Row 6: 9 6 1 | 5 3 7 | 2 8 4
/// Row 7: 2 8 7 | 4 1 9 | 6 3 _   (cell (7,8) = 5 naked single)
/// Row 8: 3 4 5 | 2 8 6 | 1 7 _   (cell (8,8) = 9 naked single)
BoardData makeNakedSingleBoard() {
    return {
        {5, 3, 0, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
        {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
        {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 0}, {3, 4, 5, 2, 8, 6, 1, 7, 0},
    };
}

/// A board with multiple hidden singles.
/// Most cells filled, a few empty cells where value is unique in row/col/box.
/// Row 0: 5 3 _ | _ 7 _ | _ _ 2   (cells (0,2), (0,3), (0,5), (0,6), (0,7) empty)
/// Rows 1-8: fully filled
BoardData makeHiddenSingleBoard() {
    return {
        {5, 3, 0, 0, 7, 0, 0, 0, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
        {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
        {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9},
    };
}

}  // namespace

TEST_CASE("TrainingAnswerValidator - reconstructCandidates", "[training_answer_validator]") {
    auto board = makeNakedSingleBoard();
    CandidateGrid original(board);

    // Snapshot the masks
    std::vector<uint16_t> masks(81);
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            masks[(r * 9) + c] = (board[r][c] != 0) ? uint16_t{0} : original.getPossibleValuesMask(r, c);
        }
    }

    auto reconstructed = TrainingAnswerValidator::reconstructCandidates(board, masks);

    // Verify all cells match
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            REQUIRE(reconstructed.getPossibleValuesMask(r, c) == original.getPossibleValuesMask(r, c));
        }
    }
}

TEST_CASE("TrainingAnswerValidator - NakedSingle accepts all valid placements", "[training_answer_validator]") {
    auto board = makeNakedSingleBoard();
    CandidateGrid candidates(board);

    // Three naked singles: (0,2)=4, (7,8)=5, (8,8)=9
    SECTION("Cell (0,2) value 4 is accepted") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 0, .col = 2}, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->position == Position{.row = 0, .col = 2});
        REQUIRE(result->value == 4);
    }

    SECTION("Cell (7,8) value 5 is accepted") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 7, .col = 8}, 5);
        REQUIRE(result.has_value());
        REQUIRE(result->value == 5);
    }

    SECTION("Cell (8,8) value 9 is accepted") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 8, .col = 8}, 9);
        REQUIRE(result.has_value());
        REQUIRE(result->value == 9);
    }

    SECTION("Wrong value is rejected") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 0, .col = 2}, 7);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Filled cell is rejected") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 0, .col = 0}, 5);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("TrainingAnswerValidator - HiddenSingle accepts all valid placements", "[training_answer_validator]") {
    auto board = makeHiddenSingleBoard();
    CandidateGrid candidates(board);

    // Row 0 empty cells: (0,2), (0,3), (0,5), (0,6), (0,7)
    // Missing values in row 0: 1, 4, 6, 8, 9
    // By constraint propagation, each empty cell should have specific candidates.
    // Cell (0,2) can only be 4 (hidden single in box 0 — only place for 4)
    // These are all hidden singles since row 0 is the only row with empties

    SECTION("findAllSteps finds multiple hidden singles") {
        auto steps = TrainingAnswerValidator::findAllSteps(board, candidates, SolvingTechnique::HiddenSingle);
        REQUIRE(steps.size() >= 2);

        // All should be placements in row 0
        for (const auto& step : steps) {
            REQUIRE(step.type == SolveStepType::Placement);
            REQUIRE(step.technique == SolvingTechnique::HiddenSingle);
        }
    }

    SECTION("Any valid hidden single is accepted") {
        auto steps = TrainingAnswerValidator::findAllSteps(board, candidates, SolvingTechnique::HiddenSingle);
        REQUIRE_FALSE(steps.empty());

        // Each step found should be validated as correct
        for (const auto& step : steps) {
            auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::HiddenSingle,
                                                                     step.position, step.value);
            REQUIRE(result.has_value());
            REQUIRE(result->position == step.position);
            REQUIRE(result->value == step.value);
        }
    }
}

TEST_CASE("TrainingAnswerValidator - findAllSteps for NakedSingle", "[training_answer_validator]") {
    auto board = makeNakedSingleBoard();
    CandidateGrid candidates(board);

    auto steps = TrainingAnswerValidator::findAllSteps(board, candidates, SolvingTechnique::NakedSingle);

    // Should find all 3 naked singles
    REQUIRE(steps.size() == 3);

    // Verify they cover the expected cells
    bool found_02 = false;
    bool found_78 = false;
    bool found_88 = false;
    for (const auto& step : steps) {
        if (step.position == Position{.row = 0, .col = 2} && step.value == 4) {
            found_02 = true;
        }
        if (step.position == Position{.row = 7, .col = 8} && step.value == 5) {
            found_78 = true;
        }
        if (step.position == Position{.row = 8, .col = 8} && step.value == 9) {
            found_88 = true;
        }
    }
    REQUIRE(found_02);
    REQUIRE(found_78);
    REQUIRE(found_88);
}

TEST_CASE("TrainingAnswerValidator - createStrategy", "[training_answer_validator]") {
    SECTION("Returns valid strategy for all non-backtracking techniques") {
        auto ns = TrainingAnswerValidator::createStrategy(SolvingTechnique::NakedSingle);
        REQUIRE(ns != nullptr);
        REQUIRE(ns->getTechnique() == SolvingTechnique::NakedSingle);

        auto hs = TrainingAnswerValidator::createStrategy(SolvingTechnique::HiddenSingle);
        REQUIRE(hs != nullptr);
        REQUIRE(hs->getTechnique() == SolvingTechnique::HiddenSingle);

        auto xw = TrainingAnswerValidator::createStrategy(SolvingTechnique::XWing);
        REQUIRE(xw != nullptr);
    }

    SECTION("Returns nullptr for Backtracking") {
        auto bt = TrainingAnswerValidator::createStrategy(SolvingTechnique::Backtracking);
        REQUIRE(bt == nullptr);
    }
}
