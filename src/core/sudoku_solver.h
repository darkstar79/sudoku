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

#include "candidate_grid.h"
#include "core/solve_step.h"
#include "i_game_validator.h"
#include "i_solving_strategy.h"
#include "i_sudoku_solver.h"
#include "i_time_provider.h"
#include "solving_technique.h"

#include <chrono>
#include <expected>
#include <memory>
#include <vector>

namespace sudoku::core {

/// Concrete implementation of Sudoku solver using logical techniques
/// Chains strategies in difficulty order and falls back to backtracking if needed
class SudokuSolver : public ISudokuSolver {
public:
    /// Constructor with dependency injection.
    /// @param validator Validator for checking board conflicts.
    /// @param time_provider Time source for budget-bearing overloads. Defaults to SystemTimeProvider
    ///        so the no-budget call path stays drop-in compatible with existing callers.
    explicit SudokuSolver(std::shared_ptr<IGameValidator> validator,
                          std::shared_ptr<ITimeProvider> time_provider = std::make_shared<SystemTimeProvider>());

    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& board) const override;

    /// Find next step with givens information for Avoidable Rectangle support.
    /// @param board Current board state
    /// @param original_puzzle Original puzzle (non-zero cells are givens)
    /// @return Next SolveStep or error
    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& board,
                                                                     const BoardData& original_puzzle) const override;

    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& board,
                                                                     const BoardData& original_puzzle,
                                                                     std::chrono::milliseconds budget) const override;

    [[nodiscard]] std::expected<SolveStep, SolverError>
    findNextStepByTechnique(const BoardData& board, SolvingTechnique technique) const override;

    [[nodiscard]] std::expected<SolverResult, SolverError> solvePuzzle(const BoardData& board) const override;

    [[nodiscard]] std::expected<SolverResult, SolverError> solvePuzzle(const BoardData& board,
                                                                       std::chrono::milliseconds budget) const override;

    [[nodiscard]] bool applyStep(BoardData& board, const SolveStep& step) const override;

    [[nodiscard]] std::vector<SolveStep> findAllApplicableSteps(const BoardData& board) const override;

private:
    std::shared_ptr<IGameValidator> validator_;
    std::shared_ptr<ITimeProvider> time_provider_;
    std::vector<std::unique_ptr<ISolvingStrategy>> strategies_;

    /// Initializes strategy chain in difficulty order
    void initializeStrategies();

    /// Checks if board is complete (no empty cells)
    [[nodiscard]] static bool isComplete(const BoardData& board);

    /// Finds next logical step using an existing CandidateGrid (for internal solving loop)
    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& board,
                                                                     const CandidateGrid& candidates) const;

    /// Applies step to both board and candidate grid (for internal solving loop)
    [[nodiscard]] static bool applyStep(BoardData& board, CandidateGrid& candidates, const SolveStep& step);

    /// Solves puzzle using simple backtracking (no circular dependency on generator)
    /// Uses unified BacktrackingSolver with MostConstrained strategy
    /// @param board Board to solve (modified in-place)
    /// @return true if solution found, false if unsolvable
    bool solveWithBacktracking(BoardData& board) const;
};

}  // namespace sudoku::core
