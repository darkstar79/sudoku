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
#include "../../src/core/i_localization_manager.h"
#include "../../src/core/i_training_exercise_generator.h"
#include "../../src/core/i_training_statistics_manager.h"
#include "../../src/view_model/training_view_model.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace sudoku::viewmodel;

// --- Mock Exercise Generator ---

namespace {

/// Creates a predictable set of exercises without actual puzzle generation
class MockExerciseGenerator : public ITrainingExerciseGenerator {
public:
    mutable SolvingTechnique last_requested_technique{SolvingTechnique::NakedSingle};
    mutable int last_requested_count{0};
    bool should_fail{false};
    std::string fail_message{"Mock generation failed"};

    [[nodiscard]] std::expected<std::vector<TrainingExercise>, std::string>
    generateExercises(SolvingTechnique technique, int count) const override {
        last_requested_technique = technique;
        last_requested_count = count;

        if (should_fail) {
            return std::unexpected(fail_message);
        }

        std::vector<TrainingExercise> exercises;
        for (int i = 0; i < count; ++i) {
            TrainingExercise ex;
            ex.board = BoardData{};
            // Place some givens
            ex.board[0][0] = 5;
            ex.board[0][1] = 3;

            // Set up candidate masks (81 entries)
            ex.candidate_masks.resize(81, 0);
            // Cell (0,2) has candidates {1, 7} = bits 1 and 7 = 0x0082
            ex.candidate_masks[2] = 0x0082;
            // Cell (1,0) has candidate {9} only = bit 9 = 0x0200
            ex.candidate_masks[9] = 0x0200;

            if (technique == SolvingTechnique::NakedSingle || technique == SolvingTechnique::HiddenSingle) {
                ex.expected_step = SolveStep{
                    .type = SolveStepType::Placement,
                    .technique = technique,
                    .position = Position{.row = 1, .col = 0},
                    .value = 9,
                    .eliminations = {},
                    .explanation = "Naked Single at R2C1: only value 9 is possible",
                    .rating = getTechniqueRating(technique),
                    .explanation_data = {.positions = {Position{.row = 1, .col = 0}}, .values = {9}},
                };
                ex.interaction_mode = TrainingInteractionMode::Placement;
            } else {
                ex.expected_step = SolveStep{
                    .type = SolveStepType::Elimination,
                    .technique = technique,
                    .position = Position{.row = 0, .col = 0},
                    .value = 0,
                    .eliminations = {{Position{.row = 0, .col = 2}, 1}},
                    .explanation = "Technique eliminates 1 from R1C3",
                    .rating = getTechniqueRating(technique),
                    .explanation_data = {.positions = {Position{.row = 0, .col = 2}}, .values = {1}},
                };
                ex.interaction_mode = TrainingInteractionMode::Elimination;
            }

            ex.technique = technique;
            ex.is_guided_coloring = false;
            exercises.push_back(std::move(ex));
        }
        return exercises;
    }
};

/// Minimal localization manager for tests — returns key as-is, except for
/// keys that are used as fmt::format templates (contain {0} in the real locale).
/// Those need a valid placeholder so locFormat() doesn't crash.
class MockLocManager : public ILocalizationManager {
public:
    [[nodiscard]] std::string_view getString(std::string_view key) const override {
        // Keys used with locFormat() need a valid {0} placeholder.
        // Detect by suffix pattern rather than hardcoding specific keys.
        if (key.find("feedback_") != std::string_view::npos || key.find("correct_continue") != std::string_view::npos) {
            return format_placeholder_;
        }
        return key;
    }

    [[nodiscard]] std::expected<void, std::string> setLocale(std::string_view /*locale_code*/) override {
        return {};
    }
    [[nodiscard]] std::string_view getCurrentLocale() const override {
        return "en";
    }
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> getAvailableLocales() const override {
        return {{"en", "English"}};
    }

private:
    // Generic format template for any key that locFormat() will expand
    std::string format_placeholder_ = "{0}";
};

struct VMFixture {
    std::shared_ptr<MockExerciseGenerator> mock_gen = std::make_shared<MockExerciseGenerator>();
    std::shared_ptr<MockLocManager> mock_loc = std::make_shared<MockLocManager>();
    TrainingViewModel vm{mock_gen, mock_loc};
};

/// Mock stats manager for verifying recordLesson calls
class MockStatsManager : public ITrainingStatisticsManager {
public:
    mutable std::vector<TrainingLessonResult> recorded_lessons;
    bool should_fail{false};

    [[nodiscard]] std::expected<void, TrainingStatsError> recordLesson(const TrainingLessonResult& result) override {
        recorded_lessons.push_back(result);
        if (should_fail) {
            return std::unexpected(TrainingStatsError::FileAccessError);
        }
        return {};
    }

    [[nodiscard]] TechniqueStats getStats(SolvingTechnique /*technique*/) const override {
        return {};
    }

    [[nodiscard]] std::unordered_map<SolvingTechnique, TechniqueStats> getAllStats() const override {
        return {};
    }

    [[nodiscard]] MasteryLevel getMastery(SolvingTechnique /*technique*/) const override {
        return MasteryLevel::Beginner;
    }

    void resetAllStats() override {
    }
};

}  // namespace

// --- State machine transitions ---

TEST_CASE("TrainingViewModel - Initial State", "[training_view_model]") {
    VMFixture f;

    SECTION("Starts in TechniqueSelection phase") {
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }
}

TEST_CASE("TrainingViewModel - selectTechnique", "[training_view_model]") {
    VMFixture f;

    SECTION("Transitions to TheoryReview") {
        f.vm.selectTechnique(SolvingTechnique::NakedPair);

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TheoryReview);
        REQUIRE(f.vm.trainingState.get().current_technique == SolvingTechnique::NakedPair);
    }

    SECTION("Resets counters") {
        // First do a full cycle to set some state
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        // Now select a new technique
        f.vm.selectTechnique(SolvingTechnique::NakedPair);

        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
        REQUIRE(f.vm.trainingState.get().correct_count == 0);
        REQUIRE(f.vm.trainingState.get().hints_used == 0);
    }

    SECTION("Rejects Backtracking") {
        f.vm.selectTechnique(SolvingTechnique::Backtracking);

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
        REQUIRE_FALSE(f.vm.errorMessage.get().empty());
    }

    SECTION("Populates theory text") {
        f.vm.selectTechnique(SolvingTechnique::XWing);

        auto desc = f.vm.currentDescription();
        REQUIRE(desc.title == "X-Wing");
        REQUIRE_FALSE(desc.what_it_is.empty());
    }
}

TEST_CASE("TrainingViewModel - startExercises", "[training_view_model]") {
    VMFixture f;

    SECTION("Transitions to Exercise phase") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
    }

    SECTION("Calls generator with correct technique") {
        f.vm.selectTechnique(SolvingTechnique::HiddenPair);
        f.vm.startExercises();

        REQUIRE(f.mock_gen->last_requested_technique == SolvingTechnique::HiddenPair);
        REQUIRE(f.mock_gen->last_requested_count == 5);
    }

    SECTION("Populates training board") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        const auto& board = f.vm.trainingBoard.get();
        REQUIRE(board[0][0].value == 5);
        REQUIRE(board[0][0].is_given == true);
        REQUIRE(board[0][1].value == 3);
    }

    SECTION("Sets candidate lists from masks") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        const auto& board = f.vm.trainingBoard.get();
        // Cell (1,0) has candidate {9} only
        REQUIRE(board[1][0].candidates.size() == 1);
        REQUIRE(board[1][0].candidates[0] == 9);
    }

    SECTION("Does nothing outside TheoryReview phase") {
        // Still in TechniqueSelection
        f.vm.startExercises();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }

    SECTION("Handles generator failure") {
        f.mock_gen->should_fail = true;
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        // Should stay in TheoryReview since generation failed
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TheoryReview);
        REQUIRE_FALSE(f.vm.errorMessage.get().empty());
    }
}

TEST_CASE("TrainingViewModel - submitAnswer", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Correct placement transitions to Feedback") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;  // Expected placement

        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Feedback);
        REQUIRE(f.vm.trainingState.get().last_result == AnswerResult::Correct);
        REQUIRE(f.vm.trainingState.get().correct_count == 1);
    }

    SECTION("Wrong placement gives Incorrect") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 5;  // Wrong value

        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Feedback);
        REQUIRE(f.vm.trainingState.get().last_result == AnswerResult::Incorrect);
        REQUIRE(f.vm.trainingState.get().correct_count == 0);
    }

    SECTION("Feedback message contains explanation") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;

        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().feedback_message.find("Naked Single") != std::string::npos);
    }
}

TEST_CASE("TrainingViewModel - requestHint", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Increments hint level") {
        f.vm.requestHint();
        REQUIRE(f.vm.trainingState.get().current_hint_level == 1);
        REQUIRE(f.vm.trainingState.get().hints_used == 1);

        f.vm.requestHint();
        REQUIRE(f.vm.trainingState.get().current_hint_level == 2);
        REQUIRE(f.vm.trainingState.get().hints_used == 2);

        f.vm.requestHint();
        REQUIRE(f.vm.trainingState.get().current_hint_level == 3);
        REQUIRE(f.vm.trainingState.get().hints_used == 3);
    }

    SECTION("Caps at level 3") {
        f.vm.requestHint();
        f.vm.requestHint();
        f.vm.requestHint();
        f.vm.requestHint();  // Should be no-op

        REQUIRE(f.vm.trainingState.get().current_hint_level == 3);
        REQUIRE(f.vm.trainingState.get().hints_used == 3);
    }
}

TEST_CASE("TrainingViewModel - skipExercise", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Advances to next exercise") {
        f.vm.skipExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 1);
    }

    SECTION("Does not change score") {
        f.vm.skipExercise();

        REQUIRE(f.vm.trainingState.get().correct_count == 0);
    }

    SECTION("Resets hint level") {
        f.vm.requestHint();
        f.vm.skipExercise();

        REQUIRE(f.vm.trainingState.get().current_hint_level == 0);
    }
}

TEST_CASE("TrainingViewModel - retryExercise", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Reloads same exercise from Exercise phase") {
        f.vm.requestHint();
        f.vm.retryExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
        REQUIRE(f.vm.trainingState.get().current_hint_level == 0);
    }

    SECTION("Works from Feedback phase") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 5;  // Wrong answer
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Feedback);

        f.vm.retryExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
    }
}

TEST_CASE("TrainingViewModel - nextExercise", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Advances from Feedback to next Exercise") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        f.vm.nextExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 1);
    }

    SECTION("Transitions to LessonComplete on last exercise") {
        // Skip through all exercises via submit+next
        for (int i = 0; i < 5; ++i) {
            auto board = f.vm.trainingBoard.get();
            board[1][0].value = 9;
            f.vm.trainingBoard.set(board);
            f.vm.submitAnswer();
            if (i < 4) {
                f.vm.nextExercise();
            }
        }

        f.vm.nextExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::LessonComplete);
    }

    SECTION("Does nothing outside Feedback phase") {
        // Still in Exercise phase
        f.vm.nextExercise();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(f.vm.trainingState.get().exercise_index == 0);
    }
}

TEST_CASE("TrainingViewModel - returnToSelection", "[training_view_model]") {
    VMFixture f;

    SECTION("Returns from TheoryReview") {
        f.vm.selectTechnique(SolvingTechnique::NakedPair);
        f.vm.returnToSelection();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }

    SECTION("Returns from Exercise") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();
        f.vm.returnToSelection();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }

    SECTION("Returns from LessonComplete") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        // Complete all exercises
        for (int i = 0; i < 5; ++i) {
            auto board = f.vm.trainingBoard.get();
            board[1][0].value = 9;
            f.vm.trainingBoard.set(board);
            f.vm.submitAnswer();
            if (i < 4) {
                f.vm.nextExercise();
            }
        }
        f.vm.nextExercise();
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::LessonComplete);

        f.vm.returnToSelection();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }

    SECTION("Clears exercises") {
        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();
        REQUIRE_FALSE(f.vm.exercises().empty());

        f.vm.returnToSelection();

        REQUIRE(f.vm.exercises().empty());
    }
}

TEST_CASE("TrainingViewModel - Observable notifications", "[training_view_model]") {
    VMFixture f;

    SECTION("trainingState fires on selectTechnique") {
        int notify_count = 0;
        f.vm.trainingState.subscribe([&notify_count](const TrainingUIState&) { notify_count++; });

        f.vm.selectTechnique(SolvingTechnique::NakedPair);

        REQUIRE(notify_count == 1);
    }

    SECTION("trainingBoard fires on startExercises") {
        int notify_count = 0;
        f.vm.trainingBoard.subscribe([&notify_count](const TrainingBoard&) { notify_count++; });

        f.vm.selectTechnique(SolvingTechnique::NakedSingle);
        f.vm.startExercises();

        REQUIRE(notify_count >= 1);
    }
}

TEST_CASE("TrainingViewModel - Score tracking", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Correct answers increment score") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().correct_count == 1);

        f.vm.nextExercise();
        board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().correct_count == 2);
    }

    SECTION("Incorrect answers do not increment score") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 5;
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().correct_count == 0);
    }
}

// --- Elimination evaluation ---

TEST_CASE("TrainingViewModel - submitAnswer elimination mode", "[training_view_model]") {
    VMFixture f;
    // Use NakedPair (triggers elimination mode in mock generator)
    f.vm.selectTechnique(SolvingTechnique::NakedPair);
    f.vm.startExercises();

    SECTION("Correct elimination gives Correct") {
        auto board = f.vm.trainingBoard.get();
        // Expected elimination: remove candidate 1 from cell (0,2)
        // Cell (0,2) has candidates {1, 7}. Remove 1, keep 7.
        board[0][2].player_selected = true;
        board[0][2].candidates = {7};  // Removed 1

        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Feedback);
        REQUIRE(f.vm.trainingState.get().last_result == AnswerResult::Correct);
    }

    SECTION("No eliminations gives Incorrect") {
        auto board = f.vm.trainingBoard.get();
        // Don't mark any cells
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().last_result == AnswerResult::Incorrect);
    }

    SECTION("Wrong elimination gives Incorrect") {
        auto board = f.vm.trainingBoard.get();
        // Remove wrong candidate (7 instead of 1) from cell (0,2)
        board[0][2].player_selected = true;
        board[0][2].candidates = {1};  // Removed 7 (wrong)

        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        REQUIRE(f.vm.trainingState.get().last_result == AnswerResult::Incorrect);
    }
}

// --- submitAnswer guard paths ---

TEST_CASE("TrainingViewModel - submitAnswer guards", "[training_view_model]") {
    VMFixture f;

    SECTION("Does nothing outside Exercise phase") {
        // Still in TechniqueSelection
        TrainingBoard board{};
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::TechniqueSelection);
    }
}

// --- revealSolution ---

TEST_CASE("TrainingViewModel - revealSolution", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Applies hint highlights to feedback board") {
        // Submit correct answer to get to Feedback
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Feedback);

        f.vm.revealSolution();

        const auto& fb = f.vm.feedbackBoard.get();
        // Level 3 hint for Singles reveals the value and highlights cell (1,0)
        CHECK(fb[1][0].player_selected == true);
    }

    SECTION("Does nothing outside Feedback phase") {
        // Still in Exercise phase
        f.vm.revealSolution();
        // Should not crash or change state
        REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::Exercise);
    }
}

// --- buildDiffBoard ---

TEST_CASE("TrainingViewModel - diff board placement mode", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    SECTION("Correct placement shows CorrectAnswer") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        board[1][0].player_selected = true;
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        const auto& fb = f.vm.feedbackBoard.get();
        CHECK(fb[1][0].highlight_role == CellRole::CorrectAnswer);
    }

    SECTION("Wrong value at expected cell shows MissedAnswer (overwrite)") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 5;
        board[1][0].player_selected = true;
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        const auto& fb = f.vm.feedbackBoard.get();
        // The final pass sets MissedAnswer since highlight != CorrectAnswer
        CHECK(fb[1][0].highlight_role == CellRole::MissedAnswer);
    }

    SECTION("No placement at expected cell shows MissedAnswer") {
        auto board = f.vm.trainingBoard.get();
        // Player placed nothing at expected cell, but placed elsewhere
        board[2][2].value = 3;
        board[2][2].player_selected = true;
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        const auto& fb = f.vm.feedbackBoard.get();
        CHECK(fb[1][0].highlight_role == CellRole::MissedAnswer);
        CHECK(fb[2][2].highlight_role == CellRole::WrongAnswer);
    }
}

TEST_CASE("TrainingViewModel - diff board elimination mode", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedPair);
    f.vm.startExercises();

    SECTION("Correct elimination shows CorrectAnswer") {
        auto board = f.vm.trainingBoard.get();
        board[0][2].player_selected = true;
        board[0][2].candidates = {7};  // Removed 1 (correct)
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        const auto& fb = f.vm.feedbackBoard.get();
        CHECK(fb[0][2].highlight_role == CellRole::CorrectAnswer);
    }

    SECTION("Wrong elimination shows WrongAnswer") {
        auto board = f.vm.trainingBoard.get();
        board[0][2].player_selected = true;
        board[0][2].candidates = {1};  // Removed 7 (wrong)
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        const auto& fb = f.vm.feedbackBoard.get();
        CHECK(fb[0][2].highlight_role == CellRole::WrongAnswer);
    }

    SECTION("Missed elimination shows MissedAnswer") {
        auto board = f.vm.trainingBoard.get();
        // Don't touch cell (0,2) at all
        f.vm.trainingBoard.set(board);
        f.vm.submitAnswer();

        const auto& fb = f.vm.feedbackBoard.get();
        CHECK(fb[0][2].highlight_role == CellRole::MissedAnswer);
    }
}

// --- skipExercise to LessonComplete ---

TEST_CASE("TrainingViewModel - skipExercise to LessonComplete", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    // Skip all 5 exercises
    for (int i = 0; i < 5; ++i) {
        f.vm.skipExercise();
    }

    REQUIRE(f.vm.trainingState.get().phase == TrainingPhase::LessonComplete);
    CHECK(f.vm.trainingState.get().correct_count == 0);
}

// --- recordLessonStats with mock ---

TEST_CASE("TrainingViewModel - records stats on lesson complete", "[training_view_model]") {
    auto mock_gen = std::make_shared<MockExerciseGenerator>();
    auto mock_loc = std::make_shared<MockLocManager>();
    auto mock_stats = std::make_shared<MockStatsManager>();
    TrainingViewModel vm{mock_gen, mock_loc, mock_stats};

    vm.selectTechnique(SolvingTechnique::NakedSingle);
    vm.startExercises();

    // Complete all exercises with correct answers
    for (int i = 0; i < 5; ++i) {
        auto board = vm.trainingBoard.get();
        board[1][0].value = 9;
        vm.trainingBoard.set(board);
        vm.submitAnswer();
        if (i < 4) {
            vm.nextExercise();
        }
    }
    vm.nextExercise();

    REQUIRE(vm.trainingState.get().phase == TrainingPhase::LessonComplete);
    REQUIRE(mock_stats->recorded_lessons.size() == 1);

    const auto& recorded = mock_stats->recorded_lessons[0];
    CHECK(recorded.technique == SolvingTechnique::NakedSingle);
    CHECK(recorded.correct_count == 5);
    CHECK(recorded.total_exercises == 5);
}

TEST_CASE("TrainingViewModel - records stats on skip to complete", "[training_view_model]") {
    auto mock_gen = std::make_shared<MockExerciseGenerator>();
    auto mock_loc = std::make_shared<MockLocManager>();
    auto mock_stats = std::make_shared<MockStatsManager>();
    TrainingViewModel vm{mock_gen, mock_loc, mock_stats};

    vm.selectTechnique(SolvingTechnique::NakedSingle);
    vm.startExercises();

    for (int i = 0; i < 5; ++i) {
        vm.skipExercise();
    }

    REQUIRE(mock_stats->recorded_lessons.size() == 1);
    CHECK(mock_stats->recorded_lessons[0].correct_count == 0);
}

TEST_CASE("TrainingViewModel - stats recording failure does not crash", "[training_view_model]") {
    auto mock_gen = std::make_shared<MockExerciseGenerator>();
    auto mock_loc = std::make_shared<MockLocManager>();
    auto mock_stats = std::make_shared<MockStatsManager>();
    mock_stats->should_fail = true;
    TrainingViewModel vm{mock_gen, mock_loc, mock_stats};

    vm.selectTechnique(SolvingTechnique::NakedSingle);
    vm.startExercises();

    for (int i = 0; i < 5; ++i) {
        vm.skipExercise();
    }

    // Should reach LessonComplete despite stats failure
    REQUIRE(vm.trainingState.get().phase == TrainingPhase::LessonComplete);
}

// --- Undo/Redo ---

TEST_CASE("TrainingViewModel - undo/redo basics", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    // Initial board loaded — history has one snapshot (index 0)
    CHECK_FALSE(f.vm.canUndo());
    CHECK_FALSE(f.vm.canRedo());

    SECTION("recordBoardChange pushes snapshot") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.recordBoardChange(board);

        CHECK(f.vm.canUndo());
        CHECK_FALSE(f.vm.canRedo());
        CHECK(f.vm.trainingBoard.get()[1][0].value == 9);
    }

    SECTION("undo restores previous state") {
        auto board = f.vm.trainingBoard.get();
        auto original_board = board;
        board[1][0].value = 9;
        f.vm.recordBoardChange(board);

        f.vm.undo();

        CHECK(f.vm.trainingBoard.get()[1][0].value == original_board[1][0].value);
        CHECK_FALSE(f.vm.canUndo());
        CHECK(f.vm.canRedo());
    }

    SECTION("redo re-applies undone change") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.recordBoardChange(board);

        f.vm.undo();
        f.vm.redo();

        CHECK(f.vm.trainingBoard.get()[1][0].value == 9);
        CHECK(f.vm.canUndo());
        CHECK_FALSE(f.vm.canRedo());
    }

    SECTION("undo at initial state is no-op") {
        auto board_before = f.vm.trainingBoard.get();
        f.vm.undo();
        CHECK(f.vm.trainingBoard.get() == board_before);
    }

    SECTION("redo at latest state is no-op") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.recordBoardChange(board);

        f.vm.redo();  // Already at latest
        CHECK(f.vm.trainingBoard.get()[1][0].value == 9);
    }

    SECTION("new change after undo truncates redo history") {
        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.recordBoardChange(board);

        board[1][0].value = 5;
        f.vm.recordBoardChange(board);

        f.vm.undo();  // Back to value=9
        CHECK(f.vm.canRedo());

        // New change should truncate the redo (value=5 is gone)
        auto board2 = f.vm.trainingBoard.get();
        board2[1][0].value = 3;
        f.vm.recordBoardChange(board2);

        CHECK_FALSE(f.vm.canRedo());
        CHECK(f.vm.trainingBoard.get()[1][0].value == 3);
    }

    SECTION("undo resets hint level") {
        f.vm.requestHint();
        CHECK(f.vm.trainingState.get().current_hint_level == 1);

        auto board = f.vm.trainingBoard.get();
        board[1][0].value = 9;
        f.vm.recordBoardChange(board);

        f.vm.undo();
        CHECK(f.vm.trainingState.get().current_hint_level == 0);
    }
}

TEST_CASE("TrainingViewModel - undo history cleared on retry", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    auto board = f.vm.trainingBoard.get();
    board[1][0].value = 9;
    f.vm.recordBoardChange(board);
    CHECK(f.vm.canUndo());

    f.vm.retryExercise();

    CHECK_FALSE(f.vm.canUndo());
    CHECK_FALSE(f.vm.canRedo());
}

TEST_CASE("TrainingViewModel - undo history cleared on next exercise", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    auto board = f.vm.trainingBoard.get();
    board[1][0].value = 9;
    f.vm.recordBoardChange(board);

    // Submit and move to next
    f.vm.trainingBoard.set(board);
    f.vm.submitAnswer();
    f.vm.nextExercise();

    CHECK_FALSE(f.vm.canUndo());
    CHECK_FALSE(f.vm.canRedo());
}

TEST_CASE("TrainingViewModel - undo history capped at max", "[training_view_model]") {
    VMFixture f;
    f.vm.selectTechnique(SolvingTechnique::NakedSingle);
    f.vm.startExercises();

    // Push more than kMaxUndoHistory snapshots
    for (int i = 0; i < 55; ++i) {
        auto board = f.vm.trainingBoard.get();
        board[0][2].player_color = i;  // Use player_color as a distinguishing marker
        f.vm.recordBoardChange(board);
    }

    // Should still be able to undo, but not all the way back to the very first
    CHECK(f.vm.canUndo());

    // Count how many undos are possible
    int undo_count = 0;
    while (f.vm.canUndo()) {
        f.vm.undo();
        ++undo_count;
    }

    // History is capped at 50, so max undos = 49 (from index 49 back to 0)
    CHECK(undo_count <= 50);
}

// --- Continue-on-correct flow ---

namespace {

/// Mock that generates exercises with a real board containing multiple naked singles.
/// Board has 3 naked singles: (0,2)=4, (7,8)=5, (8,8)=9
class MultiStepMockGenerator : public ITrainingExerciseGenerator {
public:
    [[nodiscard]] std::expected<std::vector<TrainingExercise>, std::string>
    generateExercises(SolvingTechnique technique, int count) const override {
        // Real board with 3 naked singles
        BoardData board = {
            {5, 3, 0, 6, 7, 8, 9, 1, 2}, {6, 7, 2, 1, 9, 5, 3, 4, 8}, {1, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 6, 1, 4, 2, 3}, {4, 2, 6, 8, 5, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 4, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 4}, {2, 8, 7, 4, 1, 9, 6, 3, 0}, {3, 4, 5, 2, 8, 6, 1, 7, 0},
        };

        CandidateGrid candidates(board);
        std::vector<uint16_t> masks(81);
        for (size_t r = 0; r < 9; ++r) {
            for (size_t c = 0; c < 9; ++c) {
                masks[(r * 9) + c] = (board[r][c] != 0) ? uint16_t{0} : candidates.getPossibleValuesMask(r, c);
            }
        }

        std::vector<TrainingExercise> exercises;
        for (int i = 0; i < count; ++i) {
            TrainingExercise ex;
            ex.board = board;
            ex.candidate_masks = masks;
            ex.expected_step = SolveStep{
                .type = SolveStepType::Placement,
                .technique = technique,
                .position = Position{.row = 0, .col = 2},
                .value = 4,
                .eliminations = {},
                .explanation = "Naked Single at R1C3: only value 4 is possible",
                .rating = getTechniqueRating(technique),
                .explanation_data = {.positions = {Position{.row = 0, .col = 2}}, .values = {4}},
            };
            ex.technique = technique;
            ex.interaction_mode = TrainingInteractionMode::Placement;
            exercises.push_back(std::move(ex));
        }
        return exercises;
    }
};

}  // namespace

TEST_CASE("TrainingViewModel - continue on correct with multiple steps", "[training_view_model]") {
    auto mock_gen = std::make_shared<MultiStepMockGenerator>();
    auto mock_loc = std::make_shared<MockLocManager>();
    TrainingViewModel vm{mock_gen, mock_loc};

    vm.selectTechnique(SolvingTechnique::NakedSingle);
    vm.startExercises();

    SECTION("First correct answer stays in Exercise phase") {
        auto board = vm.trainingBoard.get();
        // Place value 4 at (0,2) — first naked single
        board[0][2].value = 4;
        vm.trainingBoard.set(board);
        vm.submitAnswer();

        // Should stay in Exercise (more naked singles remain)
        REQUIRE(vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(vm.trainingState.get().correct_count == 1);
        REQUIRE_FALSE(vm.trainingState.get().found_step_message.empty());

        // Cell (0,2) should be marked as found on the board
        const auto& updated = vm.trainingBoard.get();
        CHECK(updated[0][2].is_found);
        CHECK(updated[0][2].value == 4);
        CHECK(updated[0][2].highlight_role == CellRole::CorrectAnswer);
    }

    SECTION("Second correct answer also stays in Exercise") {
        auto board = vm.trainingBoard.get();
        board[0][2].value = 4;
        vm.trainingBoard.set(board);
        vm.submitAnswer();

        // Now find second one: (7,8)=5
        auto board2 = vm.trainingBoard.get();
        board2[7][8].value = 5;
        vm.trainingBoard.set(board2);
        vm.submitAnswer();

        // Should still stay in Exercise (one more naked single at (8,8))
        REQUIRE(vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(vm.trainingState.get().correct_count == 2);

        // Both found cells should be locked
        const auto& updated = vm.trainingBoard.get();
        CHECK(updated[0][2].is_found);
        CHECK(updated[7][8].is_found);
    }

    SECTION("Last correct answer transitions to Feedback") {
        // Find all 3 naked singles
        auto board = vm.trainingBoard.get();
        board[0][2].value = 4;
        vm.trainingBoard.set(board);
        vm.submitAnswer();

        auto board2 = vm.trainingBoard.get();
        board2[7][8].value = 5;
        vm.trainingBoard.set(board2);
        vm.submitAnswer();

        auto board3 = vm.trainingBoard.get();
        board3[8][8].value = 9;
        vm.trainingBoard.set(board3);
        vm.submitAnswer();

        // No more steps — should transition to Feedback
        REQUIRE(vm.trainingState.get().phase == TrainingPhase::Feedback);
        REQUIRE(vm.trainingState.get().correct_count == 3);
        REQUIRE(vm.trainingState.get().last_result == AnswerResult::Correct);
    }

    SECTION("Wrong answer during multi-step goes to Feedback") {
        auto board = vm.trainingBoard.get();
        board[0][2].value = 7;  // Wrong value
        vm.trainingBoard.set(board);
        vm.submitAnswer();

        REQUIRE(vm.trainingState.get().phase == TrainingPhase::Feedback);
        REQUIRE(vm.trainingState.get().last_result == AnswerResult::Incorrect);
    }

    SECTION("Can find steps in any order") {
        // Start with (8,8)=9 instead of (0,2)=4
        auto board = vm.trainingBoard.get();
        board[8][8].value = 9;
        vm.trainingBoard.set(board);
        vm.submitAnswer();

        REQUIRE(vm.trainingState.get().phase == TrainingPhase::Exercise);
        REQUIRE(vm.trainingState.get().correct_count == 1);
        CHECK(vm.trainingBoard.get()[8][8].is_found);
    }
}
