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
#include "puzzle_rating.h"

#include <cstdint>
#include <expected>
#include <vector>

namespace sudoku::core {

/// Error types for rating operations
enum class RatingError : std::uint8_t {
    InvalidBoard,  ///< Board has conflicts or invalid values
    Unsolvable,    ///< Puzzle cannot be solved (cannot rate)
    Timeout        ///< Rating exceeded time limit
};

/// Interface for rating Sudoku puzzles
/// Rates puzzles on Sudoku Explainer scale (0-2000+) based on solving techniques required
class IPuzzleRater {
public:
    virtual ~IPuzzleRater() = default;

    /// Rates a Sudoku puzzle by solving it and summing technique difficulties
    /// @param board Puzzle to rate (0 = empty, 1-9 = filled)
    /// @return PuzzleRating with score, solve path, and estimated difficulty
    /// @note Rating = sum of technique points (excludes backtracking points)
    /// @note Performance target: < 100ms per puzzle for real-time generation
    [[nodiscard]] virtual std::expected<PuzzleRating, RatingError> ratePuzzle(const BoardData& board) const = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    IPuzzleRater() = default;
    IPuzzleRater(const IPuzzleRater&) = default;
    IPuzzleRater& operator=(const IPuzzleRater&) = default;
    IPuzzleRater(IPuzzleRater&&) = default;
    IPuzzleRater& operator=(IPuzzleRater&&) = default;
};

}  // namespace sudoku::core
