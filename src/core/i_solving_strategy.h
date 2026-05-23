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
#include "core/board_data.h"
#include "solve_step.h"
#include "solving_technique.h"

#include <optional>
#include <string_view>
#include <vector>

namespace sudoku::core {

/// Interface for Sudoku solving strategies
/// Each strategy implements one solving technique (Naked Singles, Hidden Pairs, etc.)
/// Strategies are stateless and thread-safe
class ISolvingStrategy {
public:
    virtual ~ISolvingStrategy() = default;

    /// Attempts to find a solving step using this strategy
    /// @param board Current board state (0 = empty, 1-9 = filled)
    /// @param candidates Candidate grid with O(1) candidate tracking and elimination support
    /// @return SolveStep if technique found, nullopt otherwise
    [[nodiscard]] virtual std::optional<SolveStep> findStep(const BoardData& board,
                                                            const CandidateGrid& candidates) const = 0;

    /// Returns the technique this strategy implements
    /// @return Solving technique type
    [[nodiscard]] virtual SolvingTechnique getTechnique() const = 0;

    /// Returns human-readable name of this strategy
    /// @return Strategy name (e.g., "Naked Single Strategy")
    [[nodiscard]] virtual std::string_view getName() const = 0;

    /// Returns Sudoku Explainer (SE) difficulty rating for this technique
    /// @return SE difficulty rating (1.0-12.0 scale)
    [[nodiscard]] virtual double getDifficultyRating() const = 0;

protected:
    // Protected special member functions to prevent slicing while allowing derived classes
    ISolvingStrategy() = default;
    ISolvingStrategy(const ISolvingStrategy&) = default;
    ISolvingStrategy& operator=(const ISolvingStrategy&) = default;
    ISolvingStrategy(ISolvingStrategy&&) = default;
    ISolvingStrategy& operator=(ISolvingStrategy&&) = default;
};

}  // namespace sudoku::core
