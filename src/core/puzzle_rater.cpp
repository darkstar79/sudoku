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

#include "puzzle_rater.h"

#include "core/i_puzzle_generator.h"
#include "core/i_puzzle_rater.h"
#include "core/i_sudoku_solver.h"
#include "core/puzzle_rating.h"
#include "core/solving_technique.h"

#include <expected>
#include <utility>
#include <vector>

namespace sudoku::core {

PuzzleRater::PuzzleRater(std::shared_ptr<ISudokuSolver> solver) : solver_(std::move(solver)) {
}

std::expected<PuzzleRating, RatingError> PuzzleRater::ratePuzzle(const BoardData& board) const {
    // Solve puzzle using logical techniques
    auto result = solver_->solvePuzzle(board);

    if (!result.has_value()) {
        // Map solver errors to rating errors
        switch (result.error()) {
            case SolverError::InvalidBoard:
                return std::unexpected(RatingError::InvalidBoard);
            case SolverError::Unsolvable:
                return std::unexpected(RatingError::Unsolvable);
            case SolverError::Timeout:
                return std::unexpected(RatingError::Timeout);
        }
    }

    // Calculate SE rating from solve path (max technique difficulty)
    const auto& solver_result = result.value();
    double se_rating = calculateRating(solver_result.solve_path);

    // Determine difficulty from SE rating
    Difficulty estimated_difficulty = ratingToDifficulty(se_rating);

    return PuzzleRating{.se_rating = se_rating,
                        .solve_path = solver_result.solve_path,
                        .requires_backtracking = solver_result.used_backtracking,
                        .estimated_difficulty = estimated_difficulty};
}

double PuzzleRater::calculateRating(const std::vector<SolveStep>& solve_path) {
    double max_rating = 0.0;
    for (const auto& step : solve_path) {
        if (step.technique != SolvingTechnique::Backtracking) {
            max_rating = std::max(max_rating, step.rating);
        }
    }
    return max_rating;
}

}  // namespace sudoku::core
