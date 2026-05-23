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
#include <optional>
#include <unordered_map>
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
    findNextStepByTechnique(const BoardData& board, const BoardData& original_puzzle,
                            SolvingTechnique technique) const override;

    [[nodiscard]] std::expected<SolverResult, SolverError> solvePuzzle(const BoardData& board) const override;

    [[nodiscard]] std::expected<SolverResult, SolverError> solvePuzzle(const BoardData& board,
                                                                       std::chrono::milliseconds budget) const override;

    [[nodiscard]] bool applyStep(BoardData& board, const SolveStep& step) const override;

    [[nodiscard]] std::vector<SolveStep> findAllApplicableSteps(const BoardData& board) const override;

private:
    std::shared_ptr<IGameValidator> validator_;
    std::shared_ptr<ITimeProvider> time_provider_;
    std::vector<std::unique_ptr<ISolvingStrategy>> strategies_;
    /// O(1) lookup from technique → strategy pointer (into strategies_). Built once in the
    /// constructor after initializeStrategies(); never mutated afterward, so raw pointers
    /// remain valid for the lifetime of the solver.
    std::unordered_map<SolvingTechnique, ISolvingStrategy*> technique_index_;

    /// Initializes strategy chain in difficulty order
    void initializeStrategies();

    /// Builds technique_index_ from strategies_. Must be called after initializeStrategies()
    /// and before strategies_ is mutated again (which it never is in practice).
    void buildTechniqueIndex();

    /// Shared body for both solvePuzzle overloads. When `deadline` is set, checks the wall
    /// clock between strategy iterations and propagates the deadline into the backtracking
    /// fallback. When unset, behaves identically to the original no-budget loop.
    [[nodiscard]] std::expected<SolverResult, SolverError>
    solvePuzzleImpl(const BoardData& board, std::optional<std::chrono::steady_clock::time_point> deadline) const;

    /// Checks if board is complete (no empty cells)
    [[nodiscard]] static bool isComplete(const BoardData& board);

    /// Finds next logical step using an existing CandidateGrid (for internal solving loop)
    [[nodiscard]] std::expected<SolveStep, SolverError> findNextStep(const BoardData& board,
                                                                     const CandidateGrid& candidates) const;

    /// Applies step to both board and candidate grid (for internal solving loop)
    [[nodiscard]] static bool applyStep(BoardData& board, CandidateGrid& candidates, const SolveStep& step);

    /// Solves puzzle using simple backtracking (no circular dependency on generator)
    /// Uses unified BacktrackingSolver with MostConstrained strategy.
    /// @param board Board to solve (modified in-place)
    /// @param deadline Optional wall-clock cutoff; aborts recursion past the deadline so the
    ///        budgeted solvePuzzle overload can't be defeated by deep backtracking fallback.
    /// @return true if solution found, false if unsolvable or aborted by deadline.
    bool solveWithBacktracking(BoardData& board,
                               std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt) const;
};

}  // namespace sudoku::core
