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

#pragma once

#include "core/solve_step.h"
#include "core/solving_technique.h"
#include "core/training_types.h"
#include "i_puzzle_generator.h"
#include "i_sudoku_solver.h"
#include "i_training_exercise_generator.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sudoku::core {

/// Generates training exercises by solving puzzles and extracting steps
/// that use the requested technique.
class TrainingExerciseGenerator : public ITrainingExerciseGenerator {
public:
    /// Constructor
    /// @param generator Puzzle generator for creating puzzles
    /// @param solver Solver for finding solve paths
    TrainingExerciseGenerator(std::shared_ptr<IPuzzleGenerator> generator, std::shared_ptr<ISudokuSolver> solver);

    /// @copydoc ITrainingExerciseGenerator::generateExercises
    [[nodiscard]] std::expected<std::vector<TrainingExercise>, std::string>
    generateExercises(SolvingTechnique technique, int count = 5) const override;

private:
    std::shared_ptr<IPuzzleGenerator> generator_;
    std::shared_ptr<ISudokuSolver> solver_;

    /// Map a technique to the puzzle difficulty that's likely to contain it
    [[nodiscard]] static Difficulty getDifficultyForTechnique(SolvingTechnique technique);

    /// Get maximum number of puzzle generation attempts for a technique
    [[nodiscard]] static int getRetryBudget(SolvingTechnique technique);

    /// Determine interaction mode from technique
    [[nodiscard]] static TrainingInteractionMode getInteractionMode(SolvingTechnique technique);

    /// Walk forward through a solve path, applying steps to board + candidates,
    /// then snapshot the state at the target step index.
    /// @param board Initial puzzle board
    /// @param solve_path Complete solve path from solver
    /// @param target_index Index of the step that uses the target technique
    /// @return {board_at_target, candidate_masks} or nullopt on error
    struct BoardSnapshot {
        BoardData board;
        std::vector<uint16_t> candidate_masks;
    };

    [[nodiscard]] static std::optional<BoardSnapshot>
    walkForward(const BoardData& initial_board, const std::vector<SolveStep>& solve_path, size_t target_index);
};

}  // namespace sudoku::core
