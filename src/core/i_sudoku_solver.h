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

#include "core/board_data.h"
#include "solve_step.h"
#include "solving_technique.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <vector>

namespace sudoku::core {

/// Error types for solver operations
enum class SolverError : std::uint8_t {
    InvalidBoard,  ///< Board has conflicts or invalid values
    Unsolvable,    ///< Puzzle cannot be solved
    Timeout        ///< Solver exceeded time limit
};

/// Result from solving a complete puzzle
struct SolverResult {
    BoardData solution;                 ///< Complete solved board
    std::vector<SolveStep> solve_path;  ///< All steps taken (for rating/replay)
    bool used_backtracking;             ///< True if logical techniques insufficient

    bool operator==(const SolverResult& other) const = default;
};

/// Interface for Sudoku puzzle solving
/// Provides both educational (next step) and complete solving capabilities
class ISudokuSolver {
public:
    virtual ~ISudokuSolver() = default;

    /// Finds the next logical step to progress toward solution
    /// @param board Current board state (0 = empty, 1-9 = filled)
    /// @return Next SolveStep using easiest applicable technique, or error if none found
    /// @note Tries techniques in difficulty order (Singles → Pairs → Triples → ...)
    [[nodiscard]] virtual std::expected<SolveStep, SolverError> findNextStep(const BoardData& board) const = 0;

    /// Finds the next logical step with givens information for Avoidable Rectangle support.
    /// @param board Current board state (0 = empty, 1-9 = filled)
    /// @param original_puzzle Original puzzle (non-zero cells are givens)
    /// @return Next SolveStep or error
    /// @note Default: delegates to findNextStep(board), ignoring givens info
    [[nodiscard]] virtual std::expected<SolveStep, SolverError>
    findNextStep(const BoardData& board, const BoardData& /*original_puzzle*/) const {
        return findNextStep(board);
    }

    /// Solves puzzle completely using logical techniques and optional backtracking
    /// @param board Initial board state (0 = empty, 1-9 = filled)
    /// @return SolverResult with solution and solve path, or error if unsolvable
    /// @note Returns full solve path for rating calculation
    [[nodiscard]] virtual std::expected<SolverResult, SolverError> solvePuzzle(const BoardData& board) const = 0;

    /// Solves puzzle with a wall-clock budget enforced between strategy iterations.
    /// @param board Initial board state (0 = empty, 1-9 = filled)
    /// @param budget Maximum wall-clock duration before SolverError::Timeout is returned
    /// @return SolverResult on success, SolverError::Timeout if budget exceeded, or other error
    /// @note Granularity is "between strategy iterations" — individual strategies are not interrupted.
    /// @note Default delegates to the no-budget overload, ignoring the budget. Implementations
    ///       (e.g. SudokuSolver) override to actually enforce wall-clock cancellation.
    [[nodiscard]] virtual std::expected<SolverResult, SolverError>
    solvePuzzle(const BoardData& board, std::chrono::milliseconds /*budget*/) const {
        return solvePuzzle(board);
    }

    /// Finds next logical step with givens and a wall-clock budget for adversarial inputs.
    /// @param board Current board state
    /// @param original_puzzle Original puzzle (non-zero cells are givens)
    /// @param budget Maximum wall-clock duration before SolverError::Timeout is returned
    /// @note Default delegates to the no-budget overload. Implementations override to enforce the budget.
    [[nodiscard]] virtual std::expected<SolveStep, SolverError>
    findNextStep(const BoardData& board, const BoardData& original_puzzle, std::chrono::milliseconds /*budget*/) const {
        return findNextStep(board, original_puzzle);
    }

    /// Finds the next step using a specific technique (for technique-targeted hints / learning mode).
    /// @param board Current board state
    /// @param technique Technique to attempt
    /// @return SolveStep produced by the matching strategy, or
    ///   - SolverError::InvalidBoard if no strategy is registered for the technique
    ///     (e.g. SolvingTechnique::Backtracking, which is a fallback sentinel).
    ///   - SolverError::Unsolvable if the strategy is registered but produces no step.
    /// @note Default returns Unsolvable; implementations override to dispatch to the matching strategy.
    [[nodiscard]] virtual std::expected<SolveStep, SolverError>
    findNextStepByTechnique(const BoardData& board, SolvingTechnique /*technique*/) const {
        // Generic fallback — implementations should override for real technique dispatch.
        return findNextStep(board);
    }

    /// Applies a solving step to modify the board
    /// @param board Board to modify (in-place modification)
    /// @param step Step to apply (placement or eliminations)
    /// @return true if step was applied successfully
    /// @note For placements: sets cell value; for eliminations: updates notes (future)
    [[nodiscard]] virtual bool applyStep(BoardData& board, const SolveStep& step) const = 0;

    /// Finds all applicable strategies at the current board state.
    /// Returns one step per technique (deduplicated). Useful for position analysis.
    [[nodiscard]] virtual std::vector<SolveStep> findAllApplicableSteps(const BoardData& board) const {
        // Default: return only the first step
        auto step = findNextStep(board);
        if (step.has_value()) {
            return {std::move(*step)};
        }
        return {};
    }

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    ISudokuSolver() = default;
    ISudokuSolver(const ISudokuSolver&) = default;
    ISudokuSolver& operator=(const ISudokuSolver&) = default;
    ISudokuSolver(ISudokuSolver&&) = default;
    ISudokuSolver& operator=(ISudokuSolver&&) = default;
};

}  // namespace sudoku::core
