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

#include "core/solve_step.h"
#include "i_puzzle_rater.h"
#include "i_sudoku_solver.h"

#include <format>
#include <memory>

namespace sudoku::core {

/// Concrete implementation of puzzle rating
/// Rates puzzles by solving with logical techniques and summing difficulty points
class PuzzleRater : public IPuzzleRater {
public:
    /// Constructor with dependency injection
    /// @param solver Solver for determining required techniques
    explicit PuzzleRater(std::shared_ptr<ISudokuSolver> solver);

    [[nodiscard]] std::expected<PuzzleRating, RatingError> ratePuzzle(const BoardData& board) const override;

private:
    std::shared_ptr<ISudokuSolver> solver_;

    /// Calculates total score from solve path (sums technique points)
    /// @param solve_path Steps used to solve puzzle
    /// @return Sum of technique points (excludes backtracking)
    [[nodiscard]] static int calculateScore(const std::vector<SolveStep>& solve_path);
};

}  // namespace sudoku::core
