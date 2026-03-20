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

#include "solve_step.h"
#include "solving_technique.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sudoku::core {

/// Phase of the training mode lesson flow
enum class TrainingPhase : uint8_t {
    TechniqueSelection,  ///< Player picks a technique to practice
    TheoryReview,        ///< Shows technique description before exercises
    Exercise,            ///< Active exercise — player interacts with board
    Feedback,            ///< Shows result after submission
    LessonComplete       ///< Score summary after all exercises
};

/// How the player interacts with the exercise board
enum class TrainingInteractionMode : uint8_t {
    Placement,    ///< Player places a value in a cell
    Elimination,  ///< Player marks candidates for elimination
    Coloring      ///< Player colors cells (chain/coloring techniques)
};

/// Result of evaluating a player's answer
enum class AnswerResult : uint8_t {
    Correct,           ///< Exact match
    PartiallyCorrect,  ///< Some correct eliminations/colors, but incomplete
    Incorrect          ///< Wrong answer
};

/// State of a single cell in a training exercise board
struct TrainingCellState {
    int value{0};                                ///< Placed value (0 = empty)
    bool is_given{false};                        ///< Part of the puzzle clue
    bool is_found{false};                        ///< Correctly identified by player (locked, non-editable)
    std::vector<int> candidates;                 ///< Available candidates for this cell
    CellRole highlight_role{CellRole::Pattern};  ///< Visual role for coloring
    bool player_selected{false};                 ///< Player has selected/marked this cell
    int player_color{0};                         ///< 0=none, 1=Color A, 2=Color B

    bool operator==(const TrainingCellState& other) const = default;
};

/// 9x9 board of training cell states
using TrainingBoard = std::array<std::array<TrainingCellState, 9>, 9>;

/// A single training exercise: board state + expected solution step
struct TrainingExercise {
    BoardData board;                           ///< 9x9 board state at exercise point
    std::vector<uint16_t> candidate_masks;     ///< 81 entries: per-cell candidate bitmasks
    SolveStep expected_step;                   ///< The step the player must find
    SolvingTechnique technique;                ///< Technique being practiced
    TrainingInteractionMode interaction_mode;  ///< How to interact
    bool is_guided_coloring{false};            ///< Exercises 1-2 show chain pre-colored
};

/// Observable UI state for the training mode view model
struct TrainingUIState {
    TrainingPhase phase{TrainingPhase::TechniqueSelection};
    SolvingTechnique current_technique{SolvingTechnique::NakedSingle};
    int exercise_index{0};
    int total_exercises{5};
    int correct_count{0};
    int hints_used{0};
    int current_hint_level{0};  ///< 0-3 (0 = no hint shown)
    std::string feedback_message;
    std::string found_step_message;  ///< Brief message when a step is applied mid-exercise
    AnswerResult last_result{AnswerResult::Incorrect};
    std::optional<std::pair<size_t, size_t>> selected_cell;  ///< Currently selected cell (row, col)
    TrainingInteractionMode interaction_mode{TrainingInteractionMode::Elimination};  ///< Current interaction mode

    bool operator==(const TrainingUIState& other) const = default;
};

/// Convert a game board state to a TrainingBoard for position analysis.
/// @param board Current board values
/// @param given_board Original puzzle (non-zero = given)
/// @param candidate_masks Per-cell candidate bitmasks (81 entries, bit N = value N is candidate)
[[nodiscard]] inline TrainingBoard boardStateToTrainingBoard(const BoardData& board, const BoardData& given_board,
                                                             const std::vector<uint16_t>& candidate_masks) {
    TrainingBoard result{};
    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            auto& cell = result[r][c];
            cell.value = board[r][c];
            cell.is_given = given_board[r][c] != 0;
            if (cell.value == 0) {
                uint16_t mask = candidate_masks[(r * BOARD_SIZE) + c];
                for (int v = 1; v <= 9; ++v) {
                    if ((mask >> v) & 1) {
                        cell.candidates.push_back(v);
                    }
                }
            }
        }
    }
    return result;
}

/// Determine the training interaction mode from a SolveStep type and technique.
[[nodiscard]] inline TrainingInteractionMode interactionModeForStep(const SolveStep& step) {
    // Coloring techniques use coloring mode
    switch (step.technique) {
        case SolvingTechnique::SimpleColoring:
        case SolvingTechnique::MultiColoring:
        case SolvingTechnique::ThreeDMedusa:
            return TrainingInteractionMode::Coloring;
        default:
            break;
    }
    // Placement vs elimination based on step type
    return step.type == SolveStepType::Placement ? TrainingInteractionMode::Placement
                                                 : TrainingInteractionMode::Elimination;
}

}  // namespace sudoku::core
