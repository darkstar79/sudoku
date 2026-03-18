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

#include "training_exercise_generator.h"

#include "candidate_grid.h"
#include "core/i_game_validator.h"
#include "core/i_puzzle_generator.h"
#include "core/i_sudoku_solver.h"

#include <expected>
#include <string_view>
#include <utility>

#include <fmt/base.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace sudoku::core {

TrainingExerciseGenerator::TrainingExerciseGenerator(std::shared_ptr<IPuzzleGenerator> generator,
                                                     std::shared_ptr<ISudokuSolver> solver)
    : generator_(std::move(generator)), solver_(std::move(solver)) {
}

std::expected<std::vector<TrainingExercise>, std::string>
// NOLINTNEXTLINE(readability-function-cognitive-complexity) — generate-and-verify loop with per-technique filtering; nesting is inherent to exercise generation
TrainingExerciseGenerator::generateExercises(SolvingTechnique technique, int count) const {
    if (count <= 0 || count > 20) {
        return std::unexpected("Exercise count must be between 1 and 20");
    }

    if (technique == SolvingTechnique::Backtracking) {
        return std::unexpected("Cannot generate exercises for Backtracking");
    }

    auto difficulty = getDifficultyForTechnique(technique);
    auto retry_budget = getRetryBudget(technique);
    auto interaction_mode = getInteractionMode(technique);

    std::vector<TrainingExercise> exercises;
    exercises.reserve(static_cast<size_t>(count));

    int attempts = 0;

    while (std::cmp_less(exercises.size(), count) && attempts < retry_budget) {
        ++attempts;

        // Generate a puzzle at the appropriate difficulty
        auto puzzle_result = generator_->generatePuzzle(difficulty);
        if (!puzzle_result.has_value()) {
            continue;
        }

        // Solve it to get the full path
        auto solve_result = solver_->solvePuzzle(puzzle_result->board);
        if (!solve_result.has_value()) {
            continue;
        }

        // Scan the solve path for steps using the target technique
        const auto& path = solve_result->solve_path;
        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i].technique != technique) {
                continue;
            }

            // Found a step using the target technique — snapshot the board state
            auto snapshot = walkForward(puzzle_result->board, path, i);
            if (!snapshot.has_value()) {
                continue;
            }

            // Determine interaction mode per-exercise for ForcingChain
            // (can produce both Placement and Elimination steps)
            auto exercise_mode = interaction_mode;
            if (technique == SolvingTechnique::ForcingChain) {
                exercise_mode = (path[i].type == SolveStepType::Placement) ? TrainingInteractionMode::Placement
                                                                           : TrainingInteractionMode::Elimination;
            }

            // Determine if this is a guided coloring exercise (first 2 of the set)
            bool is_guided =
                (interaction_mode == TrainingInteractionMode::Coloring) && (static_cast<int>(exercises.size()) < 2);

            exercises.push_back(TrainingExercise{
                .board = snapshot->board,
                .candidate_masks = std::move(snapshot->candidate_masks),
                .expected_step = path[i],
                .technique = technique,
                .interaction_mode = exercise_mode,
                .is_guided_coloring = is_guided,
            });

            if (std::cmp_greater_equal(exercises.size(), count)) {
                break;
            }
        }
    }

    if (exercises.empty()) {
        return std::unexpected(fmt::format("Failed to generate any exercises for {} after {} attempts",
                                           getTechniqueName(technique), attempts));
    }

    if (std::cmp_less(exercises.size(), count)) {
        spdlog::warn("Only generated {}/{} exercises for {} after {} attempts", exercises.size(), count,
                     getTechniqueName(technique), attempts);
    }

    return exercises;
}

Difficulty TrainingExerciseGenerator::getDifficultyForTechnique(SolvingTechnique technique) {
    auto points = getTechniqueRating(technique);

    if (points <= 150) {
        // Foundations, Subset Basics, Intersections & Quads
        return Difficulty::Medium;
    }
    if (points <= 280) {
        // Basic Fish & Wings, Single-Digit Patterns
        return Difficulty::Hard;
    }
    if (points <= 400) {
        // Links & Rectangles, Advanced Fish & Wings
        return Difficulty::Expert;
    }
    // Chains & Set Logic, Inference Engines (450+)
    return Difficulty::Master;
}

int TrainingExerciseGenerator::getRetryBudget(SolvingTechnique technique) {
    auto points = getTechniqueRating(technique);

    if (points <= 150) {
        return 10;
    }
    if (points <= 280) {
        return 20;
    }
    if (points <= 400) {
        return 50;
    }
    // 450+ points: rare techniques
    return 100;
}

TrainingInteractionMode TrainingExerciseGenerator::getInteractionMode(SolvingTechnique technique) {
    switch (technique) {
        // Placement techniques
        case SolvingTechnique::NakedSingle:
        case SolvingTechnique::HiddenSingle:
            return TrainingInteractionMode::Placement;

        // Coloring techniques
        case SolvingTechnique::SimpleColoring:
        case SolvingTechnique::MultiColoring:
            return TrainingInteractionMode::Coloring;

        // ForcingChain and everything else is elimination
        case SolvingTechnique::ForcingChain:
        default:
            return TrainingInteractionMode::Elimination;
    }
}

std::optional<TrainingExerciseGenerator::BoardSnapshot>
TrainingExerciseGenerator::walkForward(const BoardData& initial_board, const std::vector<SolveStep>& solve_path,
                                       size_t target_index) {
    auto board = initial_board;
    CandidateGrid candidates(board);

    // Apply all steps before the target
    for (size_t i = 0; i < target_index; ++i) {
        const auto& step = solve_path[i];

        if (step.type == SolveStepType::Placement) {
            board[step.position.row][step.position.col] = step.value;
            candidates.placeValue(step.position.row, step.position.col, step.value);
        } else {
            for (const auto& elim : step.eliminations) {
                candidates.eliminateCandidate(elim.position.row, elim.position.col, elim.value);
            }
        }
    }

    // Snapshot candidate masks for all 81 cells
    // Filled cells get mask 0 (no candidates — they already have a value)
    std::vector<uint16_t> masks(81);
    for (size_t r = 0; r < 9; ++r) {
        for (size_t c = 0; c < 9; ++c) {
            masks[(r * 9) + c] = (board[r][c] != 0) ? uint16_t{0} : candidates.getPossibleValuesMask(r, c);
        }
    }

    return BoardSnapshot{.board = board, .candidate_masks = masks};
}

}  // namespace sudoku::core
