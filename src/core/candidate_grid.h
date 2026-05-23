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

#include "constraint_state.h"
#include "core/board.h"
#include "core/constants.h"
#include "i_game_validator.h"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <bit>

namespace sudoku::core {

/// Bitmask constant for valid Sudoku values (bits 1-9)
inline constexpr uint16_t ALL_VALUES_MASK = 0x03FE;  // 0b0000001111111110

/**
 * @brief Candidate tracking grid that wraps ConstraintState with per-cell elimination support.
 *
 * ConstraintState tracks which values are used in each row/column/box and computes
 * candidates as the inverse. However, it cannot represent per-cell eliminations from
 * logical strategies (e.g., Naked Pair eliminating candidates from specific cells).
 *
 * CandidateGrid overlays per-cell elimination masks on top of ConstraintState,
 * enabling elimination-based strategies to drive the solving loop forward.
 *
 * Memory overhead: 81 * 2 bytes = 162 bytes for the elimination masks.
 */
class CandidateGrid {
public:
    /**
     * @brief Construct CandidateGrid from current board state.
     * @param board 9x9 Sudoku board (0 = empty, 1-9 = filled)
     */
    explicit CandidateGrid(const BoardData& board) : constraint_state_(board) {
    }

    /**
     * @brief Construct CandidateGrid from flat Board representation.
     * @param board Flat board (0 = empty, 1-9 = filled)
     */
    explicit CandidateGrid(const Board& board) : constraint_state_(board) {
    }

    /**
     * @brief Get bitmask of possible values for a cell (structural + elimination constraints).
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @return Bitmask where bit N is set if value N is still a candidate (bits 1-9)
     * @complexity O(1)
     */
    [[nodiscard]] uint16_t getPossibleValuesMask(size_t row, size_t col) const {
        return constraint_state_.getPossibleValuesMask(row, col) & ~eliminated_[(row * BOARD_SIZE) + col];
    }

    /**
     * @brief Count how many values are possible for a cell.
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @return Number of remaining candidates (0-9)
     * @complexity O(1) - POPCNT instruction
     */
    [[nodiscard]] int countPossibleValues(size_t row, size_t col) const {
        return std::popcount(getPossibleValuesMask(row, col));
    }

    /**
     * @brief Check if a value is still a candidate at the given position.
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @param value Value to check (1-9)
     * @return true if value is allowed (not eliminated and not structurally blocked)
     * @complexity O(1)
     */
    [[nodiscard]] bool isAllowed(size_t row, size_t col, int value) const {
        return (getPossibleValuesMask(row, col) & valueToBit(value)) != 0;
    }

    /**
     * @brief Eliminate a candidate from a specific cell.
     *
     * Marks a value as no longer a candidate for a specific cell, independent of
     * row/col/box constraints. Idempotent: eliminating the same candidate twice is safe.
     *
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @param value Value to eliminate (1-9)
     */
    void eliminateCandidate(size_t row, size_t col, int value) {
        eliminated_[(row * BOARD_SIZE) + col] |= valueToBit(value);
    }

    /**
     * @brief Update constraints when placing a value on the board.
     *
     * Updates the underlying ConstraintState (structural constraints for row/col/box)
     * and clears the elimination mask for this cell (since it's now filled).
     *
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @param value Value to place (1-9)
     */
    void placeValue(size_t row, size_t col, int value) {
        constraint_state_.placeValue(row, col, value);
        eliminated_[(row * BOARD_SIZE) + col] = ALL_VALUES_MASK;  // All candidates gone (cell filled)
    }

    /**
     * @brief Find a hidden single on the board using elimination-aware candidate checking.
     *
     * A hidden single is a value that can only appear in one cell within a region.
     * Uses CandidateGrid's isAllowed() which respects per-cell eliminations.
     *
     * @param board Current board state
     * @return Optional (Position, value) if any hidden single found
     */
    [[nodiscard]] std::optional<std::pair<Position, int>> findHiddenSingle(const BoardData& board) const {
        // Check rows
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
                auto result = findSingleCandidateInRow(row, value, board);
                if (result.has_value()) {
                    return std::make_pair(result.value(), value);
                }
            }
        }

        // Check columns
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
                auto result = findSingleCandidateInCol(col, value, board);
                if (result.has_value()) {
                    return std::make_pair(result.value(), value);
                }
            }
        }

        // Check boxes
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
                auto result = findSingleCandidateInBox(box, value, board);
                if (result.has_value()) {
                    return std::make_pair(result.value(), value);
                }
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Initialize givens mask from original puzzle (non-zero cells = given).
     * @param original_puzzle The original puzzle board before any solving
     */
    void setGivensFromPuzzle(const BoardData& original_puzzle) {
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                givens_[(r * BOARD_SIZE) + c] = (original_puzzle[r][c] != EMPTY_CELL);
            }
        }
        has_givens_info_ = true;
    }

    /**
     * @brief Check if a cell is a given (original puzzle clue).
     * @param row Row index (0-8)
     * @param col Column index (0-8)
     * @return true if cell was part of the original puzzle
     */
    [[nodiscard]] bool isGiven(size_t row, size_t col) const {
        return givens_[(row * BOARD_SIZE) + col];
    }

    /**
     * @brief Check if givens information has been set.
     * @return true if setGivensFromPuzzle() was called
     */
    [[nodiscard]] bool hasGivensInfo() const {
        return has_givens_info_;
    }

    /**
     * @brief Access the underlying ConstraintState for backward compatibility.
     * @return Const reference to the wrapped ConstraintState
     */
    [[nodiscard]] const ConstraintState& constraintState() const {
        return constraint_state_;
    }

private:
    ConstraintState constraint_state_;
    std::array<uint16_t, BOARD_SIZE * BOARD_SIZE> eliminated_{};
    std::array<bool, BOARD_SIZE * BOARD_SIZE> givens_{};
    bool has_givens_info_{false};

    /// Find the single cell in a row where value is a candidate, or nullopt if 0 or 2+ cells
    [[nodiscard]] std::optional<Position> findSingleCandidateInRow(size_t row, int value,
                                                                   const BoardData& board) const {
        Position candidate_pos{.row = BOARD_SIZE, .col = BOARD_SIZE};
        int count = 0;

        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (board[row][col] == 0 && isAllowed(row, col, value)) {
                count++;
                candidate_pos = Position{.row = row, .col = col};
                if (count > 1) {
                    return std::nullopt;
                }
            }
        }

        return count == 1 ? std::optional{candidate_pos} : std::nullopt;
    }

    /// Find the single cell in a column where value is a candidate, or nullopt
    [[nodiscard]] std::optional<Position> findSingleCandidateInCol(size_t col, int value,
                                                                   const BoardData& board) const {
        Position candidate_pos{.row = BOARD_SIZE, .col = BOARD_SIZE};
        int count = 0;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            if (board[row][col] == 0 && isAllowed(row, col, value)) {
                count++;
                candidate_pos = Position{.row = row, .col = col};
                if (count > 1) {
                    return std::nullopt;
                }
            }
        }

        return count == 1 ? std::optional{candidate_pos} : std::nullopt;
    }

    /// Find the single cell in a box where value is a candidate, or nullopt
    [[nodiscard]] std::optional<Position> findSingleCandidateInBox(size_t box, int value,
                                                                   const BoardData& board) const {
        size_t box_row = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_col = (box % BOX_SIZE) * BOX_SIZE;

        Position candidate_pos{.row = BOARD_SIZE, .col = BOARD_SIZE};
        int count = 0;

        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t row = box_row + br;
                size_t col = box_col + bc;

                if (board[row][col] == 0 && isAllowed(row, col, value)) {
                    count++;
                    candidate_pos = Position{.row = row, .col = col};
                    if (count > 1) {
                        return std::nullopt;
                    }
                }
            }
        }

        return count == 1 ? std::optional{candidate_pos} : std::nullopt;
    }
};

}  // namespace sudoku::core
