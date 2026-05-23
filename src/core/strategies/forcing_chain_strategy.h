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

#include "../candidate_grid.h"
#include "../i_solving_strategy.h"
#include "../solve_step.h"
#include "../solving_technique.h"
#include "../strategy_base.h"
#include "forcing_chain_helpers.h"

#include <optional>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace sudoku::core {

/// Strategy for finding Cell Forcing Chains.
/// For each bivalue/trivalue cell, assume each candidate is true and propagate
/// implications (naked singles + hidden singles). If ALL branches agree on a
/// placement or elimination, that deduction is forced.
///
/// Type 1 (Placement): All branches place the same value in the same cell.
/// Type 2 (Elimination): All branches eliminate the same candidate from the same cell.
class ForcingChainStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Try bivalue cells first (most common), then trivalue
        for (int target_count = 2; target_count <= 3; ++target_count) {
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] != EMPTY_CELL) {
                        continue;
                    }
                    if (candidates.countPossibleValues(row, col) != target_count) {
                        continue;
                    }

                    auto cands = getCandidates(candidates, row, col);
                    auto result = tryForcingCell(board, candidates, Position{.row = row, .col = col}, cands);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::ForcingChain;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Forcing Chain";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::ForcingChain);
    }

private:
    using PropagationState = ForcingChainHelpers::PropagationState;

    /// Try forcing chains from a specific cell with given candidates
    [[nodiscard]] static std::optional<SolveStep> tryForcingCell(const BoardData& board,
                                                                 const CandidateGrid& candidates, Position source,
                                                                 const std::vector<int>& cands) {
        size_t source_idx = (source.row * BOARD_SIZE) + source.col;

        // Run propagation for each candidate assumption
        std::vector<PropagationState> branch_states;
        branch_states.reserve(cands.size());

        int contradiction_idx = -1;
        for (size_t ci = 0; ci < cands.size(); ++ci) {
            auto state = ForcingChainHelpers::initState(board, candidates);
            ForcingChainHelpers::placeInState(state, source_idx, cands[ci]);
            ForcingChainHelpers::propagate(state);

            if (state.contradiction || !ForcingChainHelpers::isStateConsistent(state)) {
                contradiction_idx = static_cast<int>(ci);
                continue;
            }

            branch_states.push_back(state);
        }

        // Contradiction-based deduction: exactly one branch contradicts, others don't.
        // Only trust this if exactly one out of two branches contradicted.
        // Validate: the forced value must still be a candidate in the actual grid.
        if (contradiction_idx >= 0 && cands.size() == 2 && branch_states.size() == 1) {
            int forced_value = cands[1 - static_cast<size_t>(contradiction_idx)];
            if (candidates.isAllowed(source.row, source.col, forced_value)) {
                // Extra validation: run propagation with the forced value and verify no contradiction
                auto verify = ForcingChainHelpers::initState(board, candidates);
                ForcingChainHelpers::placeInState(verify, source_idx, forced_value);
                ForcingChainHelpers::propagate(verify);
                if (!verify.contradiction && ForcingChainHelpers::isStateConsistent(verify)) {
                    return buildPlacementStep(source, forced_value, source, cands, "contradiction");
                }
            }
        }

        // Need all branches to agree (and at least 2 branches)
        if (branch_states.size() < 2) {
            return std::nullopt;
        }

        // Type 1: All branches place the same value in the same cell
        std::vector<size_t> skip = {source_idx};
        auto placement = ForcingChainHelpers::findCommonPlacement(board, branch_states, skip);
        if (placement.has_value()) {
            auto [target_pos, value] = *placement;
            // Validate: value must be a candidate at the target position
            if (candidates.isAllowed(target_pos.row, target_pos.col, value)) {
                return buildPlacementStep(target_pos, value, source, cands, "placement");
            }
        }

        // Type 2: All branches eliminate the same candidate from the same cell
        auto explanation_prefix = fmt::format("Forcing Chain: assuming each candidate {{{}}} in {}",
                                              fmt::join(cands, ","), formatPosition(source));
        auto elimination = ForcingChainHelpers::findCommonElimination(
            board, candidates, branch_states, SolvingTechnique::ForcingChain, explanation_prefix, {source}, skip);
        if (elimination.has_value()) {
            return elimination;
        }

        return std::nullopt;
    }

    /// Build a placement step from a forcing chain result
    [[nodiscard]] static SolveStep buildPlacementStep(Position target, int value, Position source,
                                                      const std::vector<int>& cands,
                                                      [[maybe_unused]] const char* reason) {
        auto explanation = fmt::format("Forcing Chain: assuming each candidate {{{}}} in {} — places {} in {}",
                                       fmt::join(cands, ","), formatPosition(source), value, formatPosition(target));

        return SolveStep{.type = SolveStepType::Placement,
                         .technique = SolvingTechnique::ForcingChain,
                         .position = target,
                         .value = value,
                         .eliminations = {},
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::ForcingChain),
                         .explanation_data = {.positions = {source, target}, .values = {value}}};
    }
};

}  // namespace sudoku::core
