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
        // Leverage CandidateGrid::findHiddenSingle which respects per-cell eliminations
        auto result = state.findHiddenSingle(board);

        if (!result.has_value()) {
            return std::nullopt;
        }

        const auto& [position, value] = result.value();

        // Create explanation
        auto explanation = fmt::format("Hidden Single at {}: value {} can only appear in this cell within its region",
                                       formatPosition(position), value);

        return SolveStep{.type = SolveStepType::Placement,
                         .technique = SolvingTechnique::HiddenSingle,
                         .position = position,
                         .value = value,
                         .eliminations = {},
                         .explanation = explanation,
                         .points = getTechniquePoints(SolvingTechnique::HiddenSingle),
                         .explanation_data = {.positions = {position}, .values = {value}}};
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::HiddenSingle;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Hidden Single";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::HiddenSingle);
    }
};

}  // namespace sudoku::core
