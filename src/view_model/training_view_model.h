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

#pragma once

#include "../core/i_localization_manager.h"
#include "../core/i_training_exercise_generator.h"
#include "../core/i_training_statistics_manager.h"
#include "../core/observable.h"
#include "../core/technique_descriptions.h"
#include "../core/training_hints.h"
#include "../core/training_types.h"
#include "core/solve_step.h"
#include "core/solving_technique.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace sudoku::viewmodel {

/// ViewModel for Training Mode — manages state machine, exercises, and answer evaluation
class TrainingViewModel {
public:
    /// Constructor
    /// @param exercise_generator Exercise generation engine
    /// @param loc_manager Localization manager for UI strings
    TrainingViewModel(std::shared_ptr<core::ITrainingExerciseGenerator> exercise_generator,
                      std::shared_ptr<core::ILocalizationManager> loc_manager,
                      std::shared_ptr<core::ITrainingStatisticsManager> stats_manager = nullptr);

    // --- Observable properties ---

    core::Observable<core::TrainingUIState> trainingState;
    core::Observable<core::TrainingBoard> trainingBoard;
    core::Observable<core::TrainingBoard> feedbackBoard;  ///< Board shown on feedback page with diff highlights
    core::Observable<std::string> errorMessage;

    // --- Public actions ---

    /// Select a technique to practice (TechniqueSelection → TheoryReview)
    void selectTechnique(core::SolvingTechnique technique);

    /// Start exercises after reading theory (TheoryReview → Exercise)
    void startExercises();

    /// Request a progressive hint (up to 3 levels)
    void requestHint();

    /// Skip current exercise (advance without scoring)
    void skipExercise();

    /// Retry current exercise (reset player selections)
    void retryExercise();

    /// Advance to next exercise after feedback (Feedback → Exercise or LessonComplete)
    void nextExercise();

    /// Return to technique selection from any phase
    void returnToSelection();

    /// Enter analysis mode: analyze a game position and show applicable techniques.
    /// The board is read-only (returning to game leaves it unchanged).
    void analyzePosition(const core::BoardData& board, const core::BoardData& given_board,
                         const std::vector<uint16_t>& candidate_masks,
                         const std::vector<core::SolveStep>& applicable_steps);

    /// Whether we are in analysis mode (game position analysis vs standalone training)
    [[nodiscard]] bool isAnalysisMode() const {
        return is_analysis_mode_;
    }

    /// Get applicable techniques in analysis mode
    [[nodiscard]] const std::vector<core::SolveStep>& analysisSteps() const {
        return analysis_steps_;
    }

    /// Reveal the full expected solution on the feedback board
    void revealSolution();

    /// Select a cell on the training board
    void selectCell(size_t row, size_t col);

    /// Apply a number to the selected cell (placement or elimination based on current mode)
    void applyNumber(int value);

    /// Apply a color to the selected cell (toggle)
    void applyColor(int color);

    /// Submit the player's answer for evaluation (Exercise → Feedback)
    /// Reads current board from trainingBoard observable
    void submitAnswer();

    /// Record a board change (pushes undo snapshot). Used internally by applyNumber/applyColor.
    void recordBoardChange(const core::TrainingBoard& board);

    /// Undo the last board modification
    void undo();

    /// Redo a previously undone board modification
    void redo();

    /// Whether undo is available
    [[nodiscard]] bool canUndo() const;

    /// Whether redo is available
    [[nodiscard]] bool canRedo() const;

    // --- Getters for current state ---

    /// Get the description for the currently selected technique
    [[nodiscard]] core::TechniqueDescription currentDescription() const;

    /// Get the current exercises (for testing/inspection)
    [[nodiscard]] const std::vector<core::TrainingExercise>& exercises() const {
        return exercises_;
    }

    /// Get the training statistics manager (for UI mastery badges)
    [[nodiscard]] std::shared_ptr<core::ITrainingStatisticsManager> statsManager() const {
        return stats_manager_;
    }

private:
    std::shared_ptr<core::ITrainingExerciseGenerator> exercise_generator_;
    std::shared_ptr<core::ILocalizationManager> loc_manager_;
    std::shared_ptr<core::ITrainingStatisticsManager> stats_manager_;

    // Exercise state
    std::vector<core::TrainingExercise> exercises_;
    int initial_step_count_{0};  ///< Number of valid steps when exercise loaded
    int found_step_count_{0};    ///< Number of steps the player has found so far

    // Analysis mode state (game position analysis)
    bool is_analysis_mode_{false};
    std::vector<core::SolveStep> analysis_steps_;
    core::BoardData analysis_board_;
    core::BoardData analysis_given_board_;
    std::vector<uint16_t> analysis_candidate_masks_;

    // Original board snapshot (for candidate toggle-restore in elimination mode)
    core::TrainingBoard original_board_{};

    // Undo/redo history (snapshot-based)
    static constexpr int kMaxUndoHistory = 50;
    std::vector<core::TrainingBoard> board_history_;
    int history_index_{-1};
    void pushSnapshot(const core::TrainingBoard& board);
    void clearHistory();

    /// Populate the training board from the current exercise
    void loadExerciseBoard();

    /// Result of answer evaluation: the verdict and the matched step (if any)
    struct EvalResult {
        core::AnswerResult result{core::AnswerResult::Incorrect};
        std::optional<core::SolveStep> matched_step;  ///< The step that matched, or nullopt
    };

    /// Evaluate a placement answer against all valid instances of the technique
    [[nodiscard]] EvalResult evaluatePlacement(const core::TrainingBoard& player_board,
                                               const core::TrainingExercise& exercise) const;

    /// Evaluate an elimination answer against all valid instances of the technique
    [[nodiscard]] EvalResult evaluateElimination(const core::TrainingBoard& player_board,
                                                 const core::TrainingExercise& exercise) const;

    /// Build feedback message based on result
    [[nodiscard]] static std::string buildFeedback(core::AnswerResult result, const core::SolveStep& step);

    /// Build a diff board highlighting correct/wrong/missed answers
    void buildDiffBoard(const core::TrainingBoard& player_board, const core::TrainingExercise& exercise,
                        const core::SolveStep& matched_step);

    /// Apply a correct step to the board and continue the exercise
    void applyContinue(const core::SolveStep& matched_step);

    /// Check if more valid steps remain for the current technique
    [[nodiscard]] bool hasMoreSteps() const;

    /// Record lesson stats when transitioning to LessonComplete
    void recordLessonStats();

    /// Extract the set of eliminations the player made (comparing current board to original masks)
    [[nodiscard]] static std::set<std::tuple<size_t, size_t, int>>
    extractPlayerEliminations(const core::TrainingBoard& player_board, const core::TrainingExercise& exercise);

    /// Apply hint highlight cells and roles to a training board
    static void applyHintHighlights(core::TrainingBoard& board, const core::TrainingHint& hint);
};

}  // namespace sudoku::viewmodel
