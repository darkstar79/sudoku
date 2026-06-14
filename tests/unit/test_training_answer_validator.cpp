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
#include "../../src/core/solving_technique.h"
#include "../../src/core/training_answer_validator.h"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

/// A board with exactly 3 GENUINE (non-region-last) naked singles (Story 0b.4d). Each anchor cell is
/// forced to a single value while every one of its regions keeps >=2 empties (so it is a Naked Single,
/// not a Full House): (0,0)=5, (4,4)=5, (8,8)=9. The other emptied cells are satellites that keep the
/// regions populated and themselves have >1 candidate, so they are not naked singles.
BoardData makeNakedSingleBoard() {
    return {
        {0, 3, 0, 6, 7, 8, 9, 1, 2}, {6, 0, 2, 1, 9, 5, 3, 4, 8}, {0, 9, 8, 3, 4, 2, 5, 6, 7},
        {8, 5, 9, 7, 0, 1, 4, 2, 3}, {4, 2, 6, 0, 0, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 0, 8, 5, 6},
        {9, 6, 1, 5, 3, 7, 2, 8, 0}, {2, 8, 7, 4, 1, 9, 6, 0, 5}, {3, 4, 5, 2, 8, 6, 0, 7, 0},
    };
}

/// A board with multiple GENUINE (non-region-last) hidden singles (Story 0b.4d). Box 0 is fully empty
/// plus (0,3) is empty, so box 0 keeps 9 empties and each of (0,0)=5, (0,1)=3, (0,2)=4 is a hidden
/// single (its digit is confined to that one box-0 cell) while no region is region-last.
BoardData makeHiddenSingleBoard() {
    return {
        {0, 0, 0, 0, 7, 8, 9, 1, 2}, {0, 0, 0, 1, 9, 5, 3, 4, 8}, {0, 0, 0, 3, 4, 2, 5, 6, 7},
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

    // Three genuine naked singles: (0,0)=5, (4,4)=5, (8,8)=9
    SECTION("Cell (0,0) value 5 is accepted") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 0, .col = 0}, 5);
        REQUIRE(result.has_value());
        REQUIRE(result->position == Position{.row = 0, .col = 0});
        REQUIRE(result->value == 5);
    }

    SECTION("Cell (4,4) value 5 is accepted") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 4, .col = 4}, 5);
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
                                                                 Position{.row = 0, .col = 0}, 7);
        REQUIRE(!result.has_value());
    }

    SECTION("Filled cell is rejected") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 0, .col = 1}, 3);
        REQUIRE(!result.has_value());
    }
}

TEST_CASE("TrainingAnswerValidator - HiddenSingle accepts all valid placements", "[training_answer_validator]") {
    auto board = makeHiddenSingleBoard();
    CandidateGrid candidates(board);

    // Box 0 is fully empty (plus (0,3)). Several digits are confined to a single box-0 cell, e.g.
    // (0,0)=5, (0,1)=3, (0,2)=4 — genuine hidden singles whose regions all keep >=2 empties.

    SECTION("findAllSteps finds the named genuine hidden singles") {
        auto steps = TrainingAnswerValidator::findAllSteps(board, candidates, SolvingTechnique::HiddenSingle);
        REQUIRE(steps.size() >= 2);

        // All should be placements via the hidden single technique
        for (const auto& step : steps) {
            REQUIRE(step.type == SolveStepType::Placement);
            REQUIRE(step.technique == SolvingTechnique::HiddenSingle);
        }

        // Pin the actual cells, not just the count: the named genuine singles must be present.
        auto contains = [&steps](const Position& pos, int value) {
            return std::ranges::any_of(
                steps, [&](const SolveStep& step) { return step.position == pos && step.value == value; });
        };
        REQUIRE(contains(Position{.row = 0, .col = 1}, 3));
        REQUIRE(contains(Position{.row = 0, .col = 2}, 4));
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
    bool found_00 = false;
    bool found_44 = false;
    bool found_88 = false;
    for (const auto& step : steps) {
        if (step.position == Position{.row = 0, .col = 0} && step.value == 5) {
            found_00 = true;
        }
        if (step.position == Position{.row = 4, .col = 4} && step.value == 5) {
            found_44 = true;
        }
        if (step.position == Position{.row = 8, .col = 8} && step.value == 9) {
            found_88 = true;
        }
    }
    REQUIRE(found_00);
    REQUIRE(found_44);
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
