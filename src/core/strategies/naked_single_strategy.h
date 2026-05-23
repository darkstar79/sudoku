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

/// Strategy for finding Naked Singles
/// A Naked Single is a cell with only one possible candidate value
/// This is the simplest and most fundamental Sudoku solving technique
class NakedSingleStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board, const CandidateGrid& state) const override {
        // Scan all cells in row-major order
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                // Skip filled cells
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }

                // Check if cell has exactly one candidate
                if (state.countPossibleValues(row, col) == 1) {
                    // Extract the single candidate
                    auto candidates = getCandidates(state, row, col);
                    if (candidates.empty()) {
                        continue;  // Safety check (shouldn't happen)
                    }

                    int value = candidates[0];
                    Position pos{.row = row, .col = col};

                    // Create explanation
                    auto explanation =
                        fmt::format("Naked Single at {}: only value {} is possible", formatPosition(pos), value);

                    return SolveStep{.type = SolveStepType::Placement,
                                     .technique = SolvingTechnique::NakedSingle,
                                     .position = pos,
                                     .value = value,
                                     .eliminations = {},
                                     .explanation = explanation,
                                     .rating = getTechniqueRating(SolvingTechnique::NakedSingle),
                                     .explanation_data = {.positions = {pos}, .values = {value}}};
                }
            }
        }

        // No naked single found
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::NakedSingle;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Naked Single";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::NakedSingle);
    }
};

}  // namespace sudoku::core
