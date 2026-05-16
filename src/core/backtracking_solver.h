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

#include "i_game_validator.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <vector>

namespace sudoku::core {

// Forward declarations
struct Board;
class ConstraintState;

// Strategy for determining which values to try during backtracking
enum class ValueSelectionStrategy : std::uint8_t {
    Sequential,      // Try 1-9 in order (deterministic)
    Randomized,      // Try 1-9 in random order (for puzzle generation)
    MostConstrained  // Fewest options first (optimal pruning via ConstraintState)
};

/**
 * Unified backtracking solver that consolidates three previously duplicated implementations:
 * - PuzzleGenerator::solvePuzzleBacktrack() (sequential)
 * - PuzzleGenerator::fillBoardRecursively() (randomized)
 * - SudokuSolver::solveWithBacktracking() (most constrained)
 *
 * This class eliminates ~75 lines of duplicated code by providing a single, parameterized
 * backtracking implementation.
 */
class BacktrackingSolver {
public:
    explicit BacktrackingSolver(std::shared_ptr<IGameValidator> validator);

    /**
     * Solve the given Sudoku board using backtracking.
     *
     * @param board The board to solve (modified in-place)
     * @param strategy Which value selection strategy to use
     * @param rng Random number generator (required if strategy == Randomized)
     * @param deadline Optional wall-clock cutoff. When set, solve() short-circuits on entry if
     *        the deadline has already passed and aborts mid-recursion at ~1024-node intervals.
     *        Returns false on abort so callers can map it to a solver-level Timeout.
     * @return true if a solution was found, false otherwise
     */
    [[nodiscard]] bool solve(BoardData& board, ValueSelectionStrategy strategy = ValueSelectionStrategy::Sequential,
                             std::mt19937* rng = nullptr,
                             std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt) const;

    /**
     * Solve using flat Board representation with O(1) ConstraintState validation.
     * This is the performance-optimized path used by PuzzleGenerator hot paths.
     *
     * @param board Flat board to solve (modified in-place)
     * @param strategy Which value selection strategy to use
     * @param rng Random number generator (required if strategy == Randomized)
     * @param deadline See deadline parameter on the BoardData overload.
     * @return true if a solution was found, false otherwise
     */
    [[nodiscard]] bool solve(Board& board, ValueSelectionStrategy strategy = ValueSelectionStrategy::Sequential,
                             std::mt19937* rng = nullptr,
                             std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt) const;

private:
    std::shared_ptr<IGameValidator> validator_;

    // Board path (uses ConstraintState for O(1) validation instead of O(27) validator scan)
    [[nodiscard]] static std::optional<Position> findEmptyPosition(const Board& board);
    [[nodiscard]] static std::vector<int> getValuesToTry(const Position& pos, ConstraintState& state,
                                                         ValueSelectionStrategy strategy, std::mt19937* rng);
    [[nodiscard]] bool solveRecursive(Board& board, ConstraintState& state, ValueSelectionStrategy strategy,
                                      std::mt19937* rng, std::optional<std::chrono::steady_clock::time_point> deadline,
                                      std::uint32_t& recursion_count, bool& timed_out) const;
};

}  // namespace sudoku::core
