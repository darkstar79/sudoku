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
#include "../helpers/candidate_test_utils.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("TrainingAnswerValidator::createStrategy covers every logical technique",
          "[training_answer_validator][coverage]") {
    // Drive the entire switch in createStrategy by iterating the contiguous
    // logical-technique range (NakedSingle..kLastLogicalTechnique). Mirrors the
    // pattern used in test_technique_descriptions for full-enum exhaustiveness.
    for (uint8_t v = 0; v <= static_cast<uint8_t>(kLastLogicalTechnique); ++v) {
        auto technique = static_cast<SolvingTechnique>(v);
        CAPTURE(static_cast<int>(v));
        auto strategy = TrainingAnswerValidator::createStrategy(technique);
        REQUIRE(strategy != nullptr);
        REQUIRE(strategy->getTechnique() == technique);
    }
}

TEST_CASE("TrainingAnswerValidator::createStrategy returns nullptr for non-logical techniques",
          "[training_answer_validator][coverage]") {
    REQUIRE(TrainingAnswerValidator::createStrategy(SolvingTechnique::Backtracking) == nullptr);
}

TEST_CASE("TrainingAnswerValidator::validatePlacement rejection branches", "[training_answer_validator][coverage]") {
    // Same board used by the existing happy-path tests: three naked singles.
    BoardData board = {
        {5, 3, 0, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
        {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
        {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 0}, {3, 4, 5, 2, 8, 6, 1, 7, 0},
    };
    CandidateGrid candidates(board);

    SECTION("Out-of-range position is rejected") {
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 9, .col = 0}, 1);
        REQUIRE_FALSE(result.has_value());

        result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                            Position{.row = 0, .col = 9}, 1);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Value that is not a candidate is rejected") {
        // (0,2) is empty and a naked single for 4 — any other digit fails the candidate check.
        auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::NakedSingle,
                                                                 Position{.row = 0, .col = 2}, 5);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("NakedSingle with more than one candidate is rejected") {
        // On an essentially empty board the (0,2) cell has many candidates,
        // so NakedSingle must reject even a valid candidate value.
        auto empty_board = sudoku::testing::createEmptyBoard();
        CandidateGrid empty_candidates(empty_board);
        auto result = TrainingAnswerValidator::validatePlacement(
            empty_board, empty_candidates, SolvingTechnique::NakedSingle, Position{.row = 0, .col = 2}, 4);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("HiddenSingle rejection when value is not unique in row/col/box") {
        // Empty board: every value has 9 candidates in every row/col/box.
        auto empty_board = sudoku::testing::createEmptyBoard();
        CandidateGrid empty_candidates(empty_board);
        auto result = TrainingAnswerValidator::validatePlacement(
            empty_board, empty_candidates, SolvingTechnique::HiddenSingle, Position{.row = 0, .col = 0}, 1);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("TrainingAnswerValidator hidden-single detection accepts a column-only hidden single",
          "[training_answer_validator][coverage]") {
    // Story 0b.4d: the single must not be region-last (else it is a Full House, not a Hidden Single).
    // Cells (0,0), (0,2) and (1,2) are cleared, so row 0, column 2 and box 0 each keep >=2 empties.
    // Digit 4 is a hidden single at (0,2) (column 2 has no other cell that can hold 4) — the test
    // confirms validatePlacement accepts a hidden single on that genuine (non-region-last) cell.
    BoardData board = {
        {0, 3, 0, 6, 7, 8, 9, 1, 2}, {6, 7, 0, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
        {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
        {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 5}, {3, 4, 5, 2, 8, 6, 1, 7, 9},
    };
    CandidateGrid candidates(board);

    auto result = TrainingAnswerValidator::validatePlacement(board, candidates, SolvingTechnique::HiddenSingle,
                                                             Position{.row = 0, .col = 2}, 4);
    REQUIRE(result.has_value());
    REQUIRE(result->value == 4);
}

TEST_CASE("TrainingAnswerValidator::validateElimination", "[training_answer_validator][coverage]") {
    // Construct a Naked Pair: cells (0,0) and (0,1) restricted to {1,2}.
    // Every other empty cell in row 0 must drop 1 and 2 as candidates.
    auto board = sudoku::testing::createEmptyBoard();
    CandidateGrid candidates(board);
    sudoku::testing::keepOnly(candidates, 0, 0, {1, 2});
    sudoku::testing::keepOnly(candidates, 0, 1, {1, 2});

    SECTION("Empty player elimination set on board with no naked pair fires nullopt") {
        auto empty_board = sudoku::testing::createEmptyBoard();
        CandidateGrid empty_candidates(empty_board);
        auto result = TrainingAnswerValidator::validateElimination(empty_board, empty_candidates,
                                                                   SolvingTechnique::NakedPair, {});
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Matching naked-pair elimination set is accepted") {
        // Find what eliminations the strategy produces, then play them back verbatim.
        auto steps = TrainingAnswerValidator::findAllSteps(board, candidates, SolvingTechnique::NakedPair);
        REQUIRE_FALSE(steps.empty());

        std::set<std::tuple<size_t, size_t, int>> player_elims;
        for (const auto& elim : steps.front().eliminations) {
            player_elims.emplace(elim.position.row, elim.position.col, elim.value);
        }
        REQUIRE_FALSE(player_elims.empty());

        auto result =
            TrainingAnswerValidator::validateElimination(board, candidates, SolvingTechnique::NakedPair, player_elims);
        REQUIRE(result.has_value());
        REQUIRE(result->technique == SolvingTechnique::NakedPair);
    }

    SECTION("Non-matching elimination set is rejected") {
        std::set<std::tuple<size_t, size_t, int>> bogus = {{8, 8, 9}};
        auto result =
            TrainingAnswerValidator::validateElimination(board, candidates, SolvingTechnique::NakedPair, bogus);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("TrainingAnswerValidator::findAllSteps exercises the elimination-application branch",
          "[training_answer_validator][coverage]") {
    // A board with a naked pair lets findAllSteps execute the elimination-apply branch
    // (lines applying step->eliminations to working_candidates). The loop runs at least
    // once and produces at least one step.
    auto board = sudoku::testing::createEmptyBoard();
    CandidateGrid candidates(board);
    sudoku::testing::keepOnly(candidates, 0, 0, {1, 2});
    sudoku::testing::keepOnly(candidates, 0, 1, {1, 2});

    auto steps = TrainingAnswerValidator::findAllSteps(board, candidates, SolvingTechnique::NakedPair);
    REQUIRE_FALSE(steps.empty());
    for (const auto& step : steps) {
        REQUIRE(step.type == SolveStepType::Elimination);
        REQUIRE_FALSE(step.eliminations.empty());
    }
}

TEST_CASE("TrainingAnswerValidator::findAllSteps returns empty for Backtracking",
          "[training_answer_validator][coverage]") {
    auto board = sudoku::testing::createEmptyBoard();
    CandidateGrid candidates(board);
    auto steps = TrainingAnswerValidator::findAllSteps(board, candidates, SolvingTechnique::Backtracking);
    REQUIRE(steps.empty());
}
