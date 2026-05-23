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

#include "solving_technique.h"
#include "training_types.h"

#include <expected>
#include <string>
#include <vector>

namespace sudoku::core {

/// Interface for generating training exercises for a specific solving technique
class ITrainingExerciseGenerator {
public:
    virtual ~ITrainingExerciseGenerator() = default;

    /// Generate exercises that require a specific technique to solve
    /// @param technique The technique to practice
    /// @param count Number of exercises to generate (default 5)
    /// @return Vector of exercises, or error message if generation fails
    [[nodiscard]] virtual std::expected<std::vector<TrainingExercise>, std::string>
    generateExercises(SolvingTechnique technique, int count = 5) const = 0;

protected:
    ITrainingExerciseGenerator() = default;
    ITrainingExerciseGenerator(const ITrainingExerciseGenerator&) = default;
    ITrainingExerciseGenerator& operator=(const ITrainingExerciseGenerator&) = default;
    ITrainingExerciseGenerator(ITrainingExerciseGenerator&&) = default;
    ITrainingExerciseGenerator& operator=(ITrainingExerciseGenerator&&) = default;
};

}  // namespace sudoku::core
