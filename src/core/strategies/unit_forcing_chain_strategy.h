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

/// Strategy for finding Unit Forcing Chains.
/// For each unit (row, column, box) and each digit D that has 2-4 positions in the unit,
/// assume D is placed at each position and propagate implications. If ALL branches agree
/// on a placement or elimination, that deduction is forced.
///
/// Type 1 (Placement): All branches place the same value in the same cell.
/// Type 2 (Elimination): All branches eliminate the same candidate from the same cell.
class UnitForcingChainStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Try rows, columns, then boxes
        for (size_t unit_type = 0; unit_type < 3; ++unit_type) {
            auto region = static_cast<RegionType>(unit_type);
            for (size_t unit_idx = 0; unit_idx < BOARD_SIZE; ++unit_idx) {
                auto result = tryUnit(board, candidates, region, unit_idx);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::UnitForcingChain;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Unit Forcing Chain";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::UnitForcingChain);
    }

private:
    static constexpr size_t MAX_BRANCHES = 4;
    using PropagationState = ForcingChainHelpers::PropagationState;

    /// Try forcing chains for all digits in a specific unit
    [[nodiscard]] static std::optional<SolveStep> tryUnit(const BoardData& board, const CandidateGrid& candidates,
                                                          RegionType region, size_t unit_idx) {
        auto empty_cells = getEmptyCellsInUnit(board, region, unit_idx);

        for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
            // Collect positions where digit is a candidate in this unit
            std::vector<Position> positions;
            for (const auto& pos : empty_cells) {
                if (candidates.isAllowed(pos.row, pos.col, digit)) {
                    positions.push_back(pos);
                }
            }

            if (positions.size() < 2 || positions.size() > MAX_BRANCHES) {
                continue;
            }

            auto result = tryDigitInUnit(board, candidates, region, unit_idx, digit, positions);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Try forcing chain for a specific digit in a specific unit
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) -- branches over positions; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryDigitInUnit(const BoardData& board,
                                                                 const CandidateGrid& candidates, RegionType region,
                                                                 size_t unit_idx, int digit,
                                                                 const std::vector<Position>& positions) {
        std::vector<PropagationState> branch_states;
        std::vector<size_t> skip_indices;
        branch_states.reserve(positions.size());
        skip_indices.reserve(positions.size());

        for (const auto& pos : positions) {
            size_t idx = (pos.row * BOARD_SIZE) + pos.col;
            skip_indices.push_back(idx);

            auto state = ForcingChainHelpers::initState(board, candidates);
            ForcingChainHelpers::placeInState(state, idx, digit);
            ForcingChainHelpers::propagate(state);

            if (state.contradiction || !ForcingChainHelpers::isStateConsistent(state)) {
                continue;
            }

            branch_states.push_back(state);
        }

        // Need all branches to be valid (and at least 2)
        if (branch_states.size() < 2 || branch_states.size() != positions.size()) {
            return std::nullopt;
        }

        auto unit_name = formatRegion(region, unit_idx);

        // Type 1: All branches place the same value in the same cell
        auto placement = ForcingChainHelpers::findCommonPlacement(board, branch_states, skip_indices);
        if (placement.has_value()) {
            auto [target_pos, value] = *placement;
            if (candidates.isAllowed(target_pos.row, target_pos.col, value)) {
                return buildPlacementStep(target_pos, value, unit_name, digit, positions);
            }
        }

        // Type 2: All branches eliminate the same candidate from the same cell
        auto explanation_prefix = fmt::format("Unit Forcing Chain: placing {} in each position of {} in {}", digit,
                                              unit_name, formatPositionList(positions));
        std::vector<Position> explanation_positions(positions.begin(), positions.end());
        auto elimination = ForcingChainHelpers::findCommonElimination(
            board, candidates, branch_states, SolvingTechnique::UnitForcingChain, explanation_prefix,
            explanation_positions, skip_indices);
        if (elimination.has_value()) {
            return elimination;
        }

        return std::nullopt;
    }

    /// Build a placement step from a unit forcing chain result
    [[nodiscard]] static SolveStep buildPlacementStep(Position target, int value, const std::string& unit_name,
                                                      int digit, const std::vector<Position>& positions) {
        auto explanation = fmt::format("Unit Forcing Chain: placing {} in each position of {} in {} — places {} in {}",
                                       digit, unit_name, formatPositionList(positions), value, formatPosition(target));

        std::vector<Position> all_positions(positions.begin(), positions.end());
        all_positions.push_back(target);

        return SolveStep{.type = SolveStepType::Placement,
                         .technique = SolvingTechnique::UnitForcingChain,
                         .position = target,
                         .value = value,
                         .eliminations = {},
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::UnitForcingChain),
                         .explanation_data = {.positions = all_positions, .values = {value}}};
    }

    /// Format a list of positions for explanation strings
    [[nodiscard]] static std::string formatPositionList(const std::vector<Position>& positions) {
        std::vector<std::string> formatted;
        formatted.reserve(positions.size());
        for (const auto& pos : positions) {
            formatted.push_back(formatPosition(pos));
        }
        return fmt::format("{{{}}}", fmt::join(formatted, ","));
    }
};

}  // namespace sudoku::core
