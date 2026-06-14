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

#include "../i_solving_strategy.h"
#include "../solve_step.h"
#include "../solving_technique.h"
#include "../strategy_base.h"

#include <optional>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Full Houses (SE "Last value in block/row/col", rating 1.0).
///
/// A Full House is a cell that is the *only* empty cell in one of its regions (box, row, or col):
/// the region is missing exactly one value, so the cell is forced. It is SE's easiest technique and
/// the natural first rung of the learning path. A last-empty-cell always has exactly one candidate,
/// so without this strategy it would surface as a Naked Single (2.3) and be over-rated — hence this
/// strategy is registered *before* NakedSingle/HiddenSingle so the placement is labelled FullHouse 1.0.
///
/// The strategy *classifies* a Full House purely by the region-emptiness predicate (independent of which
/// cell is scanned first). The *system-level* label is **intrinsic**, not order-dependent: NakedSingle and
/// HiddenSingle defer every region-last cell (`StrategyBase::isRegionLastCell`, story 0b.4d), so a region's
/// last empty cell is labelled FullHouse 1.0 regardless of strategy registration order — the shared
/// `getRegionLastCell` predicate below is the single source of truth they both consult. The classification
/// gates on region emptiness (not candidate count), so a one-candidate cell whose regions still have other
/// empties stays a Naked Single. When a cell completes more than one region at once, region precedence is
/// **box → row → col** for the reported region (deterministic explanation_data).
class FullHouseStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board, const CandidateGrid& state) const override {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }

                // Region-emptiness predicate with box → row → col precedence (shared single source of
                // truth — the same predicate NakedSingle/HiddenSingle defer on). A region qualifies when
                // this is its only empty cell; nullopt means every region still has another empty.
                auto region_last = getRegionLastCell(board, row, col);
                if (!region_last.has_value()) {
                    continue;  // Not a Full House: every region of this cell has other empties.
                }
                const auto [region, region_index] = region_last.value();

                auto candidates = getCandidates(state, row, col);
                if (candidates.size() != 1) {
                    continue;  // Safety: a true Full House is forced to exactly one value.
                }

                int value = candidates[0];
                Position pos{.row = row, .col = col};
                auto explanation = fmt::format("Full House at {}: {} has only this cell empty, so it must be {}",
                                               formatPosition(pos), formatRegion(region, region_index), value);

                return SolveStep{
                    .type = SolveStepType::Placement,
                    .technique = SolvingTechnique::FullHouse,
                    .position = pos,
                    .value = value,
                    .eliminations = {},
                    .explanation = explanation,
                    .rating = getTechniqueRating(SolvingTechnique::FullHouse),
                    .explanation_data = {
                        .positions = {pos}, .values = {value}, .region_type = region, .region_index = region_index}};
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::FullHouse;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Full House";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::FullHouse);
    }
};

}  // namespace sudoku::core
