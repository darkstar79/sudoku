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

#include "../../src/core/solve_step.h"
#include "../../src/core/training_types.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// --- CellRole enum ---

TEST_CASE("CellRole - Enum Values", "[training_types]") {
    SECTION("All CellRole values are distinct") {
        auto pattern = CellRole::Pattern;
        auto pivot = CellRole::Pivot;
        auto wing = CellRole::Wing;
        auto fin = CellRole::Fin;
        auto roof = CellRole::Roof;
        auto floor = CellRole::Floor;
        auto chain_a = CellRole::ChainA;
        auto chain_b = CellRole::ChainB;
        auto link_endpoint = CellRole::LinkEndpoint;
        auto set_a = CellRole::SetA;
        auto set_b = CellRole::SetB;

        REQUIRE(pattern != pivot);
        REQUIRE(pivot != wing);
        REQUIRE(wing != fin);
        REQUIRE(fin != roof);
        REQUIRE(roof != floor);
        REQUIRE(floor != chain_a);
        REQUIRE(chain_a != chain_b);
        REQUIRE(chain_b != link_endpoint);
        REQUIRE(link_endpoint != set_a);
        REQUIRE(set_a != set_b);
    }

    SECTION("CellRole round-trips through uint8_t") {
        auto original = CellRole::ChainA;
        auto as_int = static_cast<uint8_t>(original);
        auto restored = static_cast<CellRole>(as_int);

        REQUIRE(restored == original);
    }
}

// --- TrainingPhase enum ---

TEST_CASE("TrainingPhase - Enum Values", "[training_types]") {
    SECTION("All phases are distinct") {
        REQUIRE(TrainingPhase::TechniqueSelection != TrainingPhase::TheoryReview);
        REQUIRE(TrainingPhase::TheoryReview != TrainingPhase::Exercise);
        REQUIRE(TrainingPhase::Exercise != TrainingPhase::Feedback);
        REQUIRE(TrainingPhase::Feedback != TrainingPhase::LessonComplete);
    }
}

// --- TrainingInteractionMode enum ---

TEST_CASE("TrainingInteractionMode - Enum Values", "[training_types]") {
    SECTION("All interaction modes are distinct") {
        REQUIRE(TrainingInteractionMode::Placement != TrainingInteractionMode::Elimination);
        REQUIRE(TrainingInteractionMode::Elimination != TrainingInteractionMode::Coloring);
        REQUIRE(TrainingInteractionMode::Placement != TrainingInteractionMode::Coloring);
    }
}

// --- AnswerResult enum ---

TEST_CASE("AnswerResult - Enum Values", "[training_types]") {
    SECTION("All answer results are distinct") {
        REQUIRE(AnswerResult::Correct != AnswerResult::PartiallyCorrect);
        REQUIRE(AnswerResult::PartiallyCorrect != AnswerResult::Incorrect);
        REQUIRE(AnswerResult::Correct != AnswerResult::Incorrect);
    }
}

// --- TrainingExercise ---

TEST_CASE("TrainingExercise - Default Construction", "[training_types]") {
    SECTION("Default exercise has empty board and candidates") {
        TrainingExercise exercise;

        REQUIRE(exercise.board == BoardData{});
        REQUIRE(exercise.candidate_masks.empty());
        REQUIRE(exercise.is_guided_coloring == false);
    }
}

// --- TrainingUIState ---

TEST_CASE("TrainingUIState - Default Construction", "[training_types]") {
    SECTION("Default UI state starts at TechniqueSelection") {
        TrainingUIState state;

        REQUIRE(state.phase == TrainingPhase::TechniqueSelection);
        REQUIRE(state.current_technique == SolvingTechnique::NakedSingle);
        REQUIRE(state.exercise_index == 0);
        REQUIRE(state.total_exercises == 5);
        REQUIRE(state.correct_count == 0);
        REQUIRE(state.hints_used == 0);
        REQUIRE(state.current_hint_level == 0);
        REQUIRE(state.feedback_message.empty());
        REQUIRE(state.last_result == AnswerResult::Incorrect);
    }
}

// --- TrainingBoard ---

TEST_CASE("TrainingBoard - Default Construction", "[training_types]") {
    SECTION("Default board has 9x9 cells with default values") {
        TrainingBoard board{};

        REQUIRE(board[0][0].value == 0);
        REQUIRE(board[0][0].is_given == false);
        REQUIRE(board[0][0].candidates.empty());
        REQUIRE(board[0][0].highlight_role == CellRole::Pattern);
        REQUIRE(board[0][0].player_selected == false);
        REQUIRE(board[0][0].player_color == 0);
        REQUIRE(board[8][8].value == 0);
    }
}

// --- TrainingCellState ---

TEST_CASE("TrainingCellState - Construction", "[training_types]") {
    SECTION("Can create cell with specific values") {
        TrainingCellState cell;
        cell.value = 5;
        cell.is_given = true;
        cell.candidates = {1, 3, 7};
        cell.highlight_role = CellRole::ChainA;
        cell.player_selected = true;
        cell.player_color = 1;

        REQUIRE(cell.value == 5);
        REQUIRE(cell.is_given == true);
        REQUIRE(cell.candidates.size() == 3);
        REQUIRE(cell.highlight_role == CellRole::ChainA);
        REQUIRE(cell.player_selected == true);
        REQUIRE(cell.player_color == 1);
    }
}

// --- ExplanationData position_roles ---

TEST_CASE("ExplanationData - position_roles defaults to empty", "[training_types][solve_step]") {
    ExplanationData data;

    REQUIRE(data.position_roles.empty());
}

TEST_CASE("ExplanationData - equality with matching position_roles", "[training_types][solve_step]") {
    ExplanationData lhs{.positions = {Position{.row = 0, .col = 0}, Position{.row = 1, .col = 1}},
                        .values = {5},
                        .position_roles = {CellRole::Pivot, CellRole::Wing}};
    ExplanationData rhs{.positions = {Position{.row = 0, .col = 0}, Position{.row = 1, .col = 1}},
                        .values = {5},
                        .position_roles = {CellRole::Pivot, CellRole::Wing}};

    REQUIRE(lhs == rhs);
}

TEST_CASE("ExplanationData - inequality with different position_roles", "[training_types][solve_step]") {
    ExplanationData lhs{.positions = {Position{.row = 0, .col = 0}, Position{.row = 1, .col = 1}},
                        .values = {5},
                        .position_roles = {CellRole::Pivot, CellRole::Wing}};
    ExplanationData rhs{.positions = {Position{.row = 0, .col = 0}, Position{.row = 1, .col = 1}},
                        .values = {5},
                        .position_roles = {CellRole::ChainA, CellRole::ChainB}};

    REQUIRE_FALSE(lhs == rhs);
}

TEST_CASE("ExplanationData - empty roles equals empty roles", "[training_types][solve_step]") {
    ExplanationData lhs{.positions = {Position{.row = 0, .col = 0}}, .values = {3}};
    ExplanationData rhs{.positions = {Position{.row = 0, .col = 0}}, .values = {3}};

    REQUIRE(lhs == rhs);
    REQUIRE(lhs.position_roles.empty());
    REQUIRE(rhs.position_roles.empty());
}

TEST_CASE("ExplanationData - populated roles vs empty roles are unequal", "[training_types][solve_step]") {
    ExplanationData lhs{
        .positions = {Position{.row = 0, .col = 0}}, .values = {3}, .position_roles = {CellRole::Pattern}};
    ExplanationData rhs{.positions = {Position{.row = 0, .col = 0}}, .values = {3}};

    REQUIRE_FALSE(lhs == rhs);
}
