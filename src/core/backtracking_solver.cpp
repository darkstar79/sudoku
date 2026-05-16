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

#include "backtracking_solver.h"

#include "constraint_state.h"
#include "core/board.h"
#include "core/constants.h"
#include "core/i_game_validator.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace sudoku::core {

BacktrackingSolver::BacktrackingSolver(std::shared_ptr<IGameValidator> validator) : validator_(std::move(validator)) {
}

bool BacktrackingSolver::solve(BoardData& board, ValueSelectionStrategy strategy, std::mt19937* rng,
                               std::optional<std::chrono::steady_clock::time_point> deadline) const {
    // Legacy path: convert to Board, solve, convert back
    auto board_flat = Board::fromBoardData(board);
    bool result = solve(board_flat, strategy, rng, deadline);
    if (result) {
        // Copy solution back to original vector
        board = board_flat.toBoardData();
    }
    return result;
}

bool BacktrackingSolver::solve(Board& board, ValueSelectionStrategy strategy, std::mt19937* rng,
                               std::optional<std::chrono::steady_clock::time_point> deadline) const {
    // Performance-optimized path: uses ConstraintState for O(1) validation
    // Early exit if the deadline has already elapsed before any recursion. Matches the
    // SudokuSolver wrapper's wall-clock contract so unsolvable-by-deadline returns fast.
    if (deadline.has_value() && std::chrono::steady_clock::now() >= *deadline) {
        return false;
    }
    ConstraintState state(board);
    std::uint32_t recursion_count = 0;
    bool timed_out = false;
    return solveRecursive(board, state, strategy, rng, deadline, recursion_count, timed_out);
}

std::optional<Position> BacktrackingSolver::findEmptyPosition(const Board& board) {
    // Flat iteration over Board (more efficient than nested loops)
    for (size_t i = 0; i < TOTAL_CELLS; ++i) {
        if (board.cells[i] == 0) {
            return Position{.row = i / BOARD_SIZE, .col = i % BOARD_SIZE};
        }
    }
    return std::nullopt;
}

std::vector<int> BacktrackingSolver::getValuesToTry(const Position& pos, ConstraintState& state,
                                                    ValueSelectionStrategy strategy, std::mt19937* rng) {
    // Performance-optimized path: uses ConstraintState bitmask (O(1)) instead of validator scan (O(27))
    std::vector<int> values;

    // Get bitmask of possible values from ConstraintState
    uint16_t mask = state.getPossibleValuesMask(pos.row, pos.col);

    // Extract values from bitmask (bit N set means value N is allowed)
    for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
        if ((mask & valueToBit(value)) != 0) {
            values.push_back(value);
        }
    }

    // Apply strategy-specific ordering
    switch (strategy) {
        case ValueSelectionStrategy::Sequential:
            // Values already in sequential order (MIN_VALUE to MAX_VALUE)
            break;

        case ValueSelectionStrategy::Randomized:
            if (rng != nullptr) {
                std::shuffle(values.begin(), values.end(), *rng);
            }
            // else: fallback to sequential (values already in order)
            break;

        case ValueSelectionStrategy::MostConstrained:
            // For MostConstrained, values are already in a valid order
            // (the MCV heuristic is applied at cell selection, not value ordering)
            break;
    }

    return values;
}

bool BacktrackingSolver::solveRecursive(Board& board, ConstraintState& state, ValueSelectionStrategy strategy,
                                        std::mt19937* rng,
                                        std::optional<std::chrono::steady_clock::time_point> deadline,
                                        std::uint32_t& recursion_count, bool& timed_out) const {
    // Wall-clock sampling: 64-node cadence. Looser than per-node (avoids vDSO bottleneck) but
    // tighter than SolutionCounter's 1024-stride — backtracking nodes here are ConstraintState
    // operations, not the SIMD hot path counter uses, so we measure per-node at ~hundreds of ns
    // and the cadence must keep overshoot inside R2's 4× budget envelope (50 ms budget ⇒ ≤200 ms
    // elapsed). The 1024-stride overshot by ~230 ms on AI Escargot fallback.
    if (deadline.has_value() && (++recursion_count & 0x3FU) == 0) {
        if (std::chrono::steady_clock::now() >= *deadline) {
            timed_out = true;
            return false;
        }
    }
    if (timed_out) {
        return false;
    }

    auto pos_opt = findEmptyPosition(board);

    // Base case: If no empty position, puzzle is solved
    if (!pos_opt.has_value()) {
        return true;
    }

    const auto& pos = pos_opt.value();

    // Get values to try based on strategy (uses ConstraintState for O(1) candidate lookup)
    auto values = getValuesToTry(pos, state, strategy, rng);

    // Try each value
    for (int num : values) {
        // Place number on board and update ConstraintState
        board[pos.row][pos.col] = static_cast<int8_t>(num);
        state.placeValue(pos.row, pos.col, num);

        // Recurse
        if (solveRecursive(board, state, strategy, rng, deadline, recursion_count, timed_out)) {
            return true;  // Solution found
        }

        // Backtrack: remove number from board and ConstraintState
        board[pos.row][pos.col] = 0;
        state.removeValue(pos.row, pos.col, num);

        // Abort partway through the children loop if a deeper recursion tripped the deadline.
        if (timed_out) {
            return false;
        }
    }

    return false;  // No solution found
}

}  // namespace sudoku::core
