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

#include "training_view_model.h"

#include "core/i18n_helpers.h"
#include "core/i_game_validator.h"
#include "core/i_training_exercise_generator.h"
#include "core/observable.h"
#include "core/technique_descriptions.h"
#include "core/training_answer_validator.h"
#include "core/training_hints.h"
#include "core/training_types.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <tuple>
#include <utility>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::viewmodel {

using namespace core;

TrainingViewModel::TrainingViewModel(std::shared_ptr<ITrainingExerciseGenerator> exercise_generator,
                                     std::shared_ptr<ITrainingStatisticsManager> stats_manager)
    : exercise_generator_(std::move(exercise_generator)), stats_manager_(std::move(stats_manager)) {
}

void TrainingViewModel::selectTechnique(SolvingTechnique technique) {
    if (technique == SolvingTechnique::Backtracking) {
        errorMessage.set(std::string(core::loc("Cannot practice Backtracking — it is not a logical technique.")));
        return;
    }

    trainingState.update([technique](TrainingUIState& state) {
        state.phase = TrainingPhase::TheoryReview;
        state.current_technique = technique;
        state.exercise_index = 0;
        state.correct_count = 0;
        state.hints_used = 0;
        state.current_hint_level = 0;
        state.feedback_message.clear();
    });
}

void TrainingViewModel::startExercises() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::TheoryReview) {
        return;
    }

    if (is_analysis_mode_) {
        // In analysis mode, create a single exercise from the matching analysis step
        auto technique = state.current_technique;
        exercises_.clear();
        for (const auto& step : analysis_steps_) {
            if (step.technique == technique) {
                TrainingExercise exercise;
                exercise.board = analysis_board_;
                exercise.candidate_masks = analysis_candidate_masks_;
                exercise.expected_step = step;
                exercise.technique = technique;
                exercise.interaction_mode = interactionModeForStep(step);
                exercises_.push_back(std::move(exercise));
                break;
            }
        }
        if (exercises_.empty()) {
            errorMessage.set(std::string(core::loc("No applicable step found for this technique.")));
            return;
        }
    } else {
        auto result = exercise_generator_->generateExercises(state.current_technique, state.total_exercises);
        if (!result.has_value()) {
            errorMessage.set(result.error());
            spdlog::warn("Failed to generate exercises: {}", result.error());
            return;
        }
        exercises_ = std::move(*result);
    }

    trainingState.update([this](TrainingUIState& s) {
        s.phase = TrainingPhase::Exercise;
        s.exercise_index = 0;
        s.total_exercises = static_cast<int>(exercises_.size());
        s.current_hint_level = 0;
    });

    loadExerciseBoard();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — submit flow with continue-on-correct logic; nesting is inherent
void TrainingViewModel::submitAnswer() {
    const auto& player_board = trainingBoard.get();
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Exercise) {
        return;
    }

    auto idx = static_cast<size_t>(state.exercise_index);
    if (idx >= exercises_.size()) {
        return;
    }

    const auto& exercise = exercises_[idx];

    EvalResult eval{};
    if (exercise.interaction_mode == TrainingInteractionMode::Placement) {
        eval = evaluatePlacement(player_board, exercise);
    } else {
        eval = evaluateElimination(player_board, exercise);
    }

    // Use matched step for feedback if available, otherwise fall back to expected
    const auto& feedback_step = eval.matched_step.has_value() ? eval.matched_step.value() : exercise.expected_step;

    if (eval.result == AnswerResult::Correct && eval.matched_step.has_value()) {
        // Check if more of the original steps remain to be found
        if (found_step_count_ + 1 < initial_step_count_) {
            // More steps remain — apply and continue
            applyContinue(feedback_step);
            found_step_count_++;
            auto msg = core::locFormat("Correct! {0} Find the next one.", feedback_step.explanation);
            trainingState.update([&msg](TrainingUIState& s) {
                s.correct_count++;
                s.found_step_message = msg;
                s.current_hint_level = 0;
            });
            return;
        }
        // Last step found — fall through to normal Feedback
    }

    auto feedback = buildFeedback(eval.result, feedback_step);
    buildDiffBoard(player_board, exercise, feedback_step);

    auto result = eval.result;
    trainingState.update([result, &feedback](TrainingUIState& s) {
        s.phase = TrainingPhase::Feedback;
        s.last_result = result;
        s.feedback_message = feedback;
        s.found_step_message.clear();
        if (result == AnswerResult::Correct) {
            s.correct_count++;
        }
    });
}

void TrainingViewModel::requestHint() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Exercise || state.current_hint_level >= 3) {
        return;
    }

    auto idx = static_cast<size_t>(state.exercise_index);
    if (idx >= exercises_.size()) {
        return;
    }

    const auto& exercise = exercises_[idx];
    const auto& expected = exercise.expected_step;

    int new_level = state.current_hint_level + 1;
    auto hint = getTrainingHint(exercise.technique, new_level, expected);

    trainingState.update([new_level, &hint](TrainingUIState& s) {
        s.current_hint_level = new_level;
        s.hints_used++;
        s.feedback_message = hint.text;
    });

    // Apply hint highlights to the board
    trainingBoard.update([&hint](TrainingBoard& board) { applyHintHighlights(board, hint); });
}

void TrainingViewModel::skipExercise() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Exercise) {
        return;
    }

    int next_idx = state.exercise_index + 1;
    if (next_idx >= state.total_exercises) {
        recordLessonStats();
        trainingState.update([](TrainingUIState& s) { s.phase = TrainingPhase::LessonComplete; });
    } else {
        trainingState.update([next_idx](TrainingUIState& s) {
            s.exercise_index = next_idx;
            s.current_hint_level = 0;
        });
        loadExerciseBoard();
    }
}

void TrainingViewModel::retryExercise() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Feedback && state.phase != TrainingPhase::Exercise) {
        return;
    }

    trainingState.update([](TrainingUIState& s) {
        s.phase = TrainingPhase::Exercise;
        s.current_hint_level = 0;
    });

    loadExerciseBoard();
}

void TrainingViewModel::nextExercise() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Feedback) {
        return;
    }

    int next_idx = state.exercise_index + 1;
    if (next_idx >= state.total_exercises) {
        recordLessonStats();
        trainingState.update([](TrainingUIState& s) { s.phase = TrainingPhase::LessonComplete; });
    } else {
        trainingState.update([next_idx](TrainingUIState& s) {
            s.phase = TrainingPhase::Exercise;
            s.exercise_index = next_idx;
            s.current_hint_level = 0;
        });
        loadExerciseBoard();
    }
}

void TrainingViewModel::returnToSelection() {
    exercises_.clear();
    clearHistory();
    original_board_ = {};
    is_analysis_mode_ = false;
    analysis_steps_.clear();
    analysis_candidate_masks_.clear();
    trainingState.set(TrainingUIState{});  // Reset to defaults (TechniqueSelection)
    trainingBoard.set(TrainingBoard{});
    feedbackBoard.set(TrainingBoard{});
}

void TrainingViewModel::analyzePosition(const BoardData& board, const BoardData& given_board,
                                        const std::vector<uint16_t>& candidate_masks,
                                        const std::vector<SolveStep>& applicable_steps) {
    is_analysis_mode_ = true;
    analysis_board_ = board;
    analysis_given_board_ = given_board;
    analysis_candidate_masks_ = candidate_masks;
    analysis_steps_ = applicable_steps;
    exercises_.clear();
    clearHistory();

    TrainingUIState initial_state;
    initial_state.phase = TrainingPhase::TechniqueSelection;
    trainingState.set(initial_state);
    trainingBoard.set(TrainingBoard{});
    feedbackBoard.set(TrainingBoard{});
}

void TrainingViewModel::selectCell(size_t row, size_t col) {
    if (row >= BOARD_SIZE || col >= BOARD_SIZE) {
        return;
    }
    trainingState.update([row, col](TrainingUIState& s) { s.selected_cell = Position{.row = row, .col = col}; });
}

void TrainingViewModel::applyNumber(int value) {
    const auto& state = trainingState.get();
    if (!state.selected_cell.has_value()) {
        return;
    }

    const auto& pos = *state.selected_cell;
    auto board = trainingBoard.get();
    auto& cell = board[pos.row][pos.col];

    if (cell.is_given || cell.is_found || cell.value != 0) {
        return;
    }

    if (state.interaction_mode == TrainingInteractionMode::Placement) {
        if (std::ranges::find(cell.candidates, value) == cell.candidates.end()) {
            return;
        }
        cell.value = value;
        cell.player_selected = true;
    } else {
        // Elimination mode: toggle candidate (remove if present, restore if was in original)
        auto iter = std::ranges::find(cell.candidates, value);
        if (iter != cell.candidates.end()) {
            cell.candidates.erase(iter);
            cell.player_selected = true;
        } else {
            // Restore only if this candidate was present in the original board
            const auto& orig = original_board_[pos.row][pos.col];
            if (std::ranges::find(orig.candidates, value) != orig.candidates.end()) {
                cell.candidates.push_back(value);
                std::ranges::sort(cell.candidates);
                cell.player_selected = true;
            }
        }
    }

    recordBoardChange(board);
}

void TrainingViewModel::applyColor(int color) {
    const auto& state = trainingState.get();
    if (!state.selected_cell.has_value()) {
        return;
    }

    const auto& pos = *state.selected_cell;
    auto board = trainingBoard.get();
    auto& cell = board[pos.row][pos.col];

    if (cell.is_given) {
        return;
    }

    // Toggle: if already this color, remove it
    cell.player_color = (cell.player_color == color) ? 0 : color;
    cell.player_selected = true;

    recordBoardChange(board);
}

void TrainingViewModel::recordBoardChange(const TrainingBoard& board) {
    pushSnapshot(board);
    trainingBoard.set(board);
}

void TrainingViewModel::undo() {
    if (!canUndo()) {
        return;
    }

    --history_index_;
    trainingBoard.set(board_history_[static_cast<size_t>(history_index_)]);

    // Clear hint highlights (snapshot won't have them)
    trainingState.update([](TrainingUIState& s) { s.current_hint_level = 0; });
}

void TrainingViewModel::redo() {
    if (!canRedo()) {
        return;
    }

    ++history_index_;
    trainingBoard.set(board_history_[static_cast<size_t>(history_index_)]);
}

bool TrainingViewModel::canUndo() const {
    return history_index_ > 0;
}

bool TrainingViewModel::canRedo() const {
    return history_index_ < static_cast<int>(board_history_.size()) - 1;
}

void TrainingViewModel::revealSolution() {
    const auto& state = trainingState.get();
    if (state.phase != TrainingPhase::Feedback) {
        return;
    }

    auto idx = static_cast<size_t>(state.exercise_index);
    if (idx >= exercises_.size()) {
        return;
    }

    const auto& exercise = exercises_[idx];
    auto hint = getTrainingHint(exercise.technique, 3, exercise.expected_step);

    feedbackBoard.update([&hint](TrainingBoard& board) { applyHintHighlights(board, hint); });
}

TechniqueDescription TrainingViewModel::currentDescription() const {
    return getTechniqueDescription(trainingState.get().current_technique);
}

// --- Private helpers ---

void TrainingViewModel::pushSnapshot(const TrainingBoard& board) {
    // Truncate redo history beyond current position
    if (history_index_ + 1 < static_cast<int>(board_history_.size())) {
        board_history_.erase(board_history_.begin() + history_index_ + 1, board_history_.end());
    }

    board_history_.push_back(board);

    // Cap at maximum history size
    if (static_cast<int>(board_history_.size()) > kMaxUndoHistory) {
        board_history_.erase(board_history_.begin());
    }

    history_index_ = static_cast<int>(board_history_.size()) - 1;
}

void TrainingViewModel::clearHistory() {
    board_history_.clear();
    history_index_ = -1;
}

void TrainingViewModel::loadExerciseBoard() {
    auto idx = static_cast<size_t>(trainingState.get().exercise_index);
    if (idx >= exercises_.size()) {
        return;
    }

    const auto& exercise = exercises_[idx];
    TrainingBoard board{};

    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            auto& cell = board[r][c];
            int value = exercise.board[r][c];
            cell.value = value;
            cell.is_given = (value != 0);

            if (value == 0) {
                // Populate candidates from bitmask
                uint16_t mask = exercise.candidate_masks[(r * 9) + c];
                for (int v = 1; v <= 9; ++v) {
                    if ((mask & (1 << v)) != 0) {
                        cell.candidates.push_back(v);
                    }
                }
            }
        }
    }

    original_board_ = board;
    clearHistory();
    pushSnapshot(board);
    trainingBoard.set(board);

    // Set interaction mode and clear selection for new exercise
    trainingState.update([&exercise](TrainingUIState& s) {
        s.interaction_mode = exercise.interaction_mode;
        s.selected_cell.reset();
    });

    // Precompute how many valid steps exist on the original board
    auto candidates = TrainingAnswerValidator::reconstructCandidates(exercise.board, exercise.candidate_masks);
    auto all_steps = TrainingAnswerValidator::findAllSteps(exercise.board, candidates, exercise.technique);
    initial_step_count_ = static_cast<int>(all_steps.size());
    found_step_count_ = 0;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — applies step to board+masks with placement/elimination paths; nesting is inherent
void TrainingViewModel::applyContinue(const SolveStep& matched_step) {
    auto idx = static_cast<size_t>(trainingState.get().exercise_index);
    if (idx >= exercises_.size()) {
        return;
    }

    auto& exercise = exercises_[idx];

    // Apply the step to the exercise's stored board and candidate masks
    if (matched_step.type == SolveStepType::Placement) {
        exercise.board[matched_step.position.row][matched_step.position.col] = matched_step.value;
        // Clear candidate mask for the placed cell
        exercise.candidate_masks[(matched_step.position.row * 9) + matched_step.position.col] = 0;
        // Remove this value as a candidate from peers (same row/col/box)
        for (size_t i = 0; i < 9; ++i) {
            // Same row
            exercise.candidate_masks[(matched_step.position.row * 9) + i] &=
                ~static_cast<uint16_t>(1 << matched_step.value);
            // Same col
            exercise.candidate_masks[(i * 9) + matched_step.position.col] &=
                ~static_cast<uint16_t>(1 << matched_step.value);
        }
        // Same box
        size_t box_r = (matched_step.position.row / 3) * 3;
        size_t box_c = (matched_step.position.col / 3) * 3;
        for (size_t br = 0; br < 3; ++br) {
            for (size_t bc = 0; bc < 3; ++bc) {
                exercise.candidate_masks[((box_r + br) * 9) + box_c + bc] &=
                    ~static_cast<uint16_t>(1 << matched_step.value);
            }
        }
    } else {
        for (const auto& elim : matched_step.eliminations) {
            exercise.candidate_masks[(elim.position.row * 9) + elim.position.col] &=
                ~static_cast<uint16_t>(1 << elim.value);
        }
    }

    // Update the training board: apply the step and mark cells as found
    trainingBoard.update([&matched_step](TrainingBoard& board) {
        if (matched_step.type == SolveStepType::Placement) {
            auto& cell = board[matched_step.position.row][matched_step.position.col];
            cell.value = matched_step.value;
            cell.candidates.clear();
            cell.is_found = true;
            cell.highlight_role = CellRole::CorrectAnswer;

            // Remove placed value from peer cells' candidates (same row/col/box)
            size_t row = matched_step.position.row;
            size_t col = matched_step.position.col;
            for (size_t i = 0; i < 9; ++i) {
                auto& row_peer = board[row][i];
                std::erase(row_peer.candidates, matched_step.value);
                auto& col_peer = board[i][col];
                std::erase(col_peer.candidates, matched_step.value);
            }
            size_t box_r = (row / 3) * 3;
            size_t box_c = (col / 3) * 3;
            for (size_t br = 0; br < 3; ++br) {
                for (size_t bc = 0; bc < 3; ++bc) {
                    std::erase(board[box_r + br][box_c + bc].candidates, matched_step.value);
                }
            }
        } else {
            for (const auto& elim : matched_step.eliminations) {
                auto& cell = board[elim.position.row][elim.position.col];
                auto iter = std::ranges::find(cell.candidates, elim.value);
                if (iter != cell.candidates.end()) {
                    cell.candidates.erase(iter);
                }
                cell.is_found = true;
                cell.highlight_role = CellRole::CorrectAnswer;
            }
        }

        // Clear player_selected on all cells for the next attempt
        for (auto& row : board) {
            for (auto& cell : row) {
                if (!cell.is_found) {
                    cell.player_selected = false;
                }
            }
        }
    });

    clearHistory();
    pushSnapshot(trainingBoard.get());

    // Update expected_step to point to a remaining valid step (for hints)
    auto candidates = TrainingAnswerValidator::reconstructCandidates(exercise.board, exercise.candidate_masks);
    auto remaining = TrainingAnswerValidator::findAllSteps(exercise.board, candidates, exercise.technique);
    if (!remaining.empty()) {
        exercise.expected_step = remaining.front();
    }
}

bool TrainingViewModel::hasMoreSteps() const {
    return found_step_count_ < initial_step_count_;
}

TrainingViewModel::EvalResult TrainingViewModel::evaluatePlacement(const TrainingBoard& player_board,
                                                                   const TrainingExercise& exercise) const {
    // Find what the player placed: either a player_selected cell with a value,
    // or a cell that was empty in the exercise but now has a value
    std::optional<Position> player_pos;
    int player_value = 0;
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            const auto& cell = player_board[r][c];
            bool was_empty = (exercise.board[r][c] == 0);
            bool has_placement = (cell.value != 0) && (cell.player_selected || was_empty);
            if (has_placement && was_empty) {
                player_pos = Position{.row = r, .col = c};
                player_value = cell.value;
                break;
            }
        }
        if (player_pos.has_value()) {
            break;
        }
    }

    if (!player_pos.has_value()) {
        return {.result = AnswerResult::Incorrect, .matched_step = std::nullopt};
    }

    // Fast path: check against the stored expected step first
    if (player_pos.value() == exercise.expected_step.position && player_value == exercise.expected_step.value) {
        return {.result = AnswerResult::Correct, .matched_step = exercise.expected_step};
    }

    // Check against ALL valid instances of the technique on this board state
    auto candidates = TrainingAnswerValidator::reconstructCandidates(exercise.board, exercise.candidate_masks);
    auto matched = TrainingAnswerValidator::validatePlacement(exercise.board, candidates, exercise.technique,
                                                              player_pos.value(), player_value);
    if (matched.has_value()) {
        return {.result = AnswerResult::Correct, .matched_step = std::move(matched)};
    }

    return {.result = AnswerResult::Incorrect, .matched_step = std::nullopt};
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — evaluates player's elimination against all valid steps; set-difference logic with multiple correctness criteria; nesting is inherent
TrainingViewModel::EvalResult TrainingViewModel::evaluateElimination(const TrainingBoard& player_board,
                                                                     const TrainingExercise& exercise) const {
    // Build set of player-marked eliminations
    auto player_set = extractPlayerEliminations(player_board, exercise);

    if (player_set.empty()) {
        return {.result = AnswerResult::Incorrect, .matched_step = std::nullopt};
    }

    // First check against the stored expected step (always valid)
    std::set<std::tuple<size_t, size_t, int>> expected_set;
    for (const auto& elim : exercise.expected_step.eliminations) {
        expected_set.emplace(elim.position.row, elim.position.col, elim.value);
    }

    if (player_set == expected_set) {
        return {.result = AnswerResult::Correct, .matched_step = exercise.expected_step};
    }

    // Then check against ALL other valid instances of the technique
    auto candidates = TrainingAnswerValidator::reconstructCandidates(exercise.board, exercise.candidate_masks);
    auto matched =
        TrainingAnswerValidator::validateElimination(exercise.board, candidates, exercise.technique, player_set);
    if (matched.has_value()) {
        return {.result = AnswerResult::Correct, .matched_step = std::move(matched)};
    }

    size_t correct_count = 0;
    for (const auto& p : player_set) {
        if (expected_set.contains(p)) {
            correct_count++;
        }
    }

    // Also check partial correctness against all valid steps
    auto all_steps = TrainingAnswerValidator::findAllSteps(exercise.board, candidates, exercise.technique);
    for (const auto& step : all_steps) {
        std::set<std::tuple<size_t, size_t, int>> step_set;
        for (const auto& elim : step.eliminations) {
            step_set.emplace(elim.position.row, elim.position.col, elim.value);
        }
        size_t step_correct = 0;
        for (const auto& p : player_set) {
            if (step_set.contains(p)) {
                step_correct++;
            }
        }
        correct_count = std::max(correct_count, step_correct);
    }

    if (correct_count > 0) {
        return {.result = AnswerResult::PartiallyCorrect, .matched_step = std::nullopt};
    }

    return {.result = AnswerResult::Incorrect, .matched_step = std::nullopt};
}

std::string TrainingViewModel::buildFeedback(AnswerResult result, const SolveStep& step) const {
    switch (result) {
        case AnswerResult::Correct:
            return core::locFormat("Correct! {0}", step.explanation);
        case AnswerResult::PartiallyCorrect:
            return core::locFormat("Partially correct. {0}", step.explanation);
        case AnswerResult::Incorrect:
            return core::locFormat("Not quite. {0}", step.explanation);
    }
    return std::string(core::loc("Unknown result."));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — builds diff board with placement/elimination logic; nesting is inherent
void TrainingViewModel::buildDiffBoard(const TrainingBoard& player_board, const TrainingExercise& exercise,
                                       const SolveStep& matched_step) {
    // Start from the player's board as a snapshot
    TrainingBoard diff = player_board;

    const auto& expected = matched_step;

    if (exercise.interaction_mode == TrainingInteractionMode::Placement) {
        const auto& pos = expected.position;
        if (pos.row < 9 && pos.col < 9) {
            auto& cell = diff[pos.row][pos.col];
            if (cell.value == expected.value) {
                cell.highlight_role = CellRole::CorrectAnswer;
            } else if (cell.player_selected && cell.value != 0) {
                cell.highlight_role = CellRole::WrongAnswer;
                // Also mark the expected cell as missed
                diff[pos.row][pos.col].highlight_role = CellRole::WrongAnswer;
            }
        }
        // Mark any player-placed value at the wrong cell
        for (size_t r = 0; r < 9; ++r) {
            for (size_t c = 0; c < 9; ++c) {
                if (player_board[r][c].player_selected && player_board[r][c].value != 0) {
                    if (r != pos.row || c != pos.col) {
                        diff[r][c].highlight_role = CellRole::WrongAnswer;
                    }
                }
            }
        }
        // Mark expected cell as missed if player didn't place there
        if (pos.row < 9 && pos.col < 9 && diff[pos.row][pos.col].highlight_role != CellRole::CorrectAnswer) {
            diff[pos.row][pos.col].highlight_role = CellRole::MissedAnswer;
            diff[pos.row][pos.col].player_selected = true;
        }
    } else {
        // Elimination mode: compare expected vs player elimination sets
        std::set<std::tuple<size_t, size_t, int>> expected_set;
        for (const auto& elim : expected.eliminations) {
            expected_set.emplace(elim.position.row, elim.position.col, elim.value);
        }

        // Find player eliminations
        auto player_set = extractPlayerEliminations(player_board, exercise);

        // Collect cells that need highlighting
        std::set<std::pair<size_t, size_t>> correct_cells;
        std::set<std::pair<size_t, size_t>> wrong_cells;
        std::set<std::pair<size_t, size_t>> missed_cells;

        for (const auto& [r, c, v] : player_set) {
            if (expected_set.contains({r, c, v})) {
                correct_cells.emplace(r, c);
            } else {
                wrong_cells.emplace(r, c);
            }
        }
        for (const auto& [r, c, v] : expected_set) {
            if (!player_set.contains({r, c, v})) {
                missed_cells.emplace(r, c);
            }
        }

        // Apply highlights (wrong takes priority over correct if both on same cell)
        for (const auto& [r, c] : correct_cells) {
            if (!wrong_cells.contains({r, c})) {
                diff[r][c].highlight_role = CellRole::CorrectAnswer;
                diff[r][c].player_selected = true;
            }
        }
        for (const auto& [r, c] : wrong_cells) {
            diff[r][c].highlight_role = CellRole::WrongAnswer;
            diff[r][c].player_selected = true;
        }
        for (const auto& [r, c] : missed_cells) {
            if (!correct_cells.contains({r, c}) && !wrong_cells.contains({r, c})) {
                diff[r][c].highlight_role = CellRole::MissedAnswer;
                diff[r][c].player_selected = true;
            }
        }
    }

    feedbackBoard.set(diff);
}

void TrainingViewModel::recordLessonStats() {
    if (!stats_manager_) {
        return;
    }

    const auto& state = trainingState.get();
    TrainingLessonResult result;
    result.technique = state.current_technique;
    result.correct_count = state.correct_count;
    result.total_exercises = state.total_exercises;
    result.hints_used = state.hints_used;

    auto record_result = stats_manager_->recordLesson(result);
    if (!record_result) {
        spdlog::warn("Failed to record training lesson stats");
    }
}

std::set<std::tuple<size_t, size_t, int>>
TrainingViewModel::extractPlayerEliminations(const TrainingBoard& player_board, const TrainingExercise& exercise) {
    std::set<std::tuple<size_t, size_t, int>> player_set;
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            if (player_board[r][c].player_selected) {
                uint16_t original_mask = exercise.candidate_masks[(r * 9) + c];
                for (int v = 1; v <= 9; ++v) {
                    bool was_candidate = (original_mask & (1 << v)) != 0;
                    bool still_in_player = std::ranges::contains(player_board[r][c].candidates, v);
                    if (was_candidate && !still_in_player) {
                        player_set.emplace(r, c, v);
                    }
                }
            }
        }
    }
    return player_set;
}

void TrainingViewModel::applyHintHighlights(TrainingBoard& board, const TrainingHint& hint) {
    for (const auto& [pos, role] : hint.highlights) {
        if (pos.row < 9 && pos.col < 9) {
            board[pos.row][pos.col].highlight_role = role;
            board[pos.row][pos.col].player_selected = true;
        }
    }
}

}  // namespace sudoku::viewmodel
