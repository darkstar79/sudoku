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
#include "i_solving_strategy.h"
#include "solve_step.h"
#include "solving_technique.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

namespace sudoku::core {

/// Validates training mode answers by checking against all valid instances of a technique,
/// not just the single step the solver happened to find first.
class TrainingAnswerValidator {
public:
    /// Check if a placement is a valid instance of the given technique at this board state.
    /// @return The matching SolveStep if valid, nullopt if not
    [[nodiscard]] static std::optional<SolveStep> validatePlacement(const BoardData& board,
                                                                    const CandidateGrid& candidates,
                                                                    SolvingTechnique technique, Position position,
                                                                    int value);

    /// Check if an elimination set matches any valid instance of the technique.
    /// @return The matching SolveStep if valid, nullopt if not
    [[nodiscard]] static std::optional<SolveStep>
    validateElimination(const BoardData& board, const CandidateGrid& candidates, SolvingTechnique technique,
                        const std::set<std::tuple<size_t, size_t, int>>& player_eliminations);

    /// Find all valid steps for a technique at the given board state.
    [[nodiscard]] static std::vector<SolveStep> findAllSteps(const BoardData& board, const CandidateGrid& candidates,
                                                             SolvingTechnique technique);

    /// Reconstruct a CandidateGrid from stored board + candidate masks.
    [[nodiscard]] static CandidateGrid reconstructCandidates(const BoardData& board,
                                                             const std::vector<uint16_t>& candidate_masks);

    /// Create a strategy instance for the given technique.
    [[nodiscard]] static std::unique_ptr<ISolvingStrategy> createStrategy(SolvingTechnique technique);
};

}  // namespace sudoku::core
