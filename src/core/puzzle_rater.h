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
#include "i_puzzle_rater.h"
#include "i_sudoku_solver.h"

#include <chrono>
#include <format>
#include <memory>

namespace sudoku::core {

/// Default wall-clock budget the rater enforces per solve. Generous enough that any legitimately
/// rateable puzzle (incl. Master) finishes well within it, but bounded so an adversarial
/// no-logical-path board (AI Escargot) cannot livelock the solver during generation/rating.
inline constexpr std::chrono::milliseconds kDefaultRatingBudget{5000};

/// Concrete implementation of puzzle rating using the Sudoku Explainer (SE) scale.
/// Rates puzzles by the hardest logical technique required to solve them.
class PuzzleRater : public IPuzzleRater {
public:
    /// Constructor with dependency injection
    /// @param solver Solver for determining required techniques
    /// @param solve_budget Wall-clock budget passed to the solver's budget overload so rating
    ///        a pathological board returns RatingError::Timeout instead of livelocking (#24 H2).
    ///        Defaults to kDefaultRatingBudget; tests inject a small/negative budget to assert
    ///        the budget is actually honored.
    explicit PuzzleRater(std::shared_ptr<ISudokuSolver> solver,
                         std::chrono::milliseconds solve_budget = kDefaultRatingBudget);

    [[nodiscard]] std::expected<PuzzleRating, RatingError> ratePuzzle(const BoardData& board) const override;

private:
    std::shared_ptr<ISudokuSolver> solver_;
    std::chrono::milliseconds solve_budget_;

    /// Calculates SE rating from solve path (max technique difficulty)
    /// @param solve_path Steps used to solve puzzle
    /// @return Maximum SE difficulty rating across all logical techniques (excludes backtracking)
    [[nodiscard]] static double calculateRating(const std::vector<SolveStep>& solve_path);
};

}  // namespace sudoku::core
