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

#include "i_puzzle_generator.h"  // For Difficulty enum
#include "solve_step.h"

#include <limits>
#include <utility>
#include <vector>

namespace sudoku::core {

/// Complete rating information for a Sudoku puzzle.
/// Rating uses the Sudoku Explainer (SE) scale by Nicolas Juillerat:
/// the puzzle's difficulty equals the hardest technique required to solve it.
struct PuzzleRating {
    double se_rating;                   ///< SE difficulty rating (max technique, 1.0-12.0 scale)
    std::vector<SolveStep> solve_path;  ///< All steps required to solve puzzle
    bool requires_backtracking;         ///< True if logical techniques alone insufficient
    Difficulty estimated_difficulty;    ///< Difficulty level based on rating

    bool operator==(const PuzzleRating& other) const = default;
};

/// Returns the SE rating range [min, max) for a difficulty level
/// @param difficulty Difficulty level
/// @return Pair of (min_rating, max_rating) - inclusive min, exclusive max
/// @note Single source of truth for difficulty thresholds on the SE scale
[[nodiscard]] constexpr std::pair<double, double> getDifficultyRatingRange(Difficulty difficulty) {
    switch (difficulty) {
        case Difficulty::Easy:
            return {0.0, 2.5};
        case Difficulty::Medium:
            return {2.5, 3.8};
        case Difficulty::Hard:
            return {3.8, 5.5};
        case Difficulty::Expert:
            return {5.5, 7.5};
        case Difficulty::Master:
            return {7.5, std::numeric_limits<double>::max()};
    }
    return {0.0, 0.0};  // Unreachable, but silences compiler warning
}

/// Converts SE rating to difficulty level
/// @param rating Sudoku Explainer difficulty rating (max technique)
/// @return Estimated difficulty level
[[nodiscard]] constexpr Difficulty ratingToDifficulty(double rating) {
    if (rating >= getDifficultyRatingRange(Difficulty::Master).first) {
        return Difficulty::Master;
    }
    if (rating >= getDifficultyRatingRange(Difficulty::Expert).first) {
        return Difficulty::Expert;
    }
    if (rating >= getDifficultyRatingRange(Difficulty::Hard).first) {
        return Difficulty::Hard;
    }
    if (rating >= getDifficultyRatingRange(Difficulty::Medium).first) {
        return Difficulty::Medium;
    }
    return Difficulty::Easy;
}

}  // namespace sudoku::core
