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

/// Strategy for finding Hidden Singles
/// A Hidden Single is a value that can only appear in one cell within a region (row/col/box)
/// Even if that cell has multiple candidates, the value is "hidden" among them
class HiddenSingleStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board, const CandidateGrid& state) const override {
        // Leverage CandidateGrid::findHiddenSingle which respects per-cell eliminations. Skip a region's
        // last empty cell so it is deferred to FullHouseStrategy (FullHouse 1.0, not Hidden Single 1.2/1.5):
        // this makes the FullHouse label intrinsic — order-independent — rather than a registration-order
        // artefact. Skipping (rather than aborting the scan) keeps a *genuine* hidden single elsewhere
        // visible to single-strategy callers like findNextStepByTechnique (story 0b.4d).
        auto result = state.findHiddenSingle(
            board, [&board](const Position& pos) { return isRegionLastCell(board, pos.row, pos.col); });

        if (!result.has_value()) {
            return std::nullopt;
        }

        const auto& [position, value] = result.value();

        // Class A region split (Story 0b.4a): record which region resolves the single and rate by it —
        // a block single is the easy SE 1.2 case, a line single 1.5.
        const RegionType region = resolvingRegion(board, state, position, value);

        // Create explanation
        auto explanation = fmt::format("Hidden Single at {}: value {} can only appear in this cell within its region",
                                       formatPosition(position), value);

        return SolveStep{.type = SolveStepType::Placement,
                         .technique = SolvingTechnique::HiddenSingle,
                         .position = position,
                         .value = value,
                         .eliminations = {},
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::HiddenSingle, RatingContext{.region = region}),
                         .explanation_data = {.positions = {position},
                                              .values = {value},
                                              .region_type = region,
                                              .region_index = getUnitIndex(region, position)}};
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::HiddenSingle;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Hidden Single";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        // Static, context-free base rating (SE line value). Per-step ratings are context-derived in
        // findStep(); this interface method reports the technique's flat base for ordering/UI.
        return getTechniqueRating(SolvingTechnique::HiddenSingle);
    }

private:
    /// Determines the region that resolves a known hidden single at `position`/`value`, applying SE
    /// block precedence ("easiest region wins"): a single confined to its box is the easy 1.2 case, so
    /// the box is tested first, then the row, then the column. The finder guarantees the value is a
    /// single in at least one region, so one branch always matches; None is a defensive fallback.
    ///
    /// The box→row→col branch order is deliberate: it is first-match short-circuit, and it encodes the
    /// rating preference (Box 1.2 < Line 1.5), so it must be read as policy, not a bug. It intentionally
    /// diverges from CandidateGrid::findHiddenSingle's row→col→box scan order — that order only decides
    /// WHICH single is returned; this order decides how the returned single is RATED.
    [[nodiscard]] static RegionType resolvingRegion(const BoardData& board, const CandidateGrid& state,
                                                    const Position& position, int value) {
        if (getCellsWithCandidate(state, getEmptyCellsInBox(board, getBoxIndex(position.row, position.col)), value)
                .size() == 1) {
            return RegionType::Box;
        }
        if (getCellsWithCandidate(state, getEmptyCellsInRow(board, position.row), value).size() == 1) {
            return RegionType::Row;
        }
        if (getCellsWithCandidate(state, getEmptyCellsInCol(board, position.col), value).size() == 1) {
            return RegionType::Col;
        }
        return RegionType::None;
    }
};

}  // namespace sudoku::core
