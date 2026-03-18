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
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding XYZ-Wing patterns
/// An XYZ-Wing consists of a pivot cell with candidates {A,B,C} and two wing cells:
///   - Wing 1 has candidates {A,C} and sees the pivot
///   - Wing 2 has candidates {B,C} and sees the pivot
/// Value C can be eliminated from any cell that sees all three (pivot, wing1, wing2).
/// This extends XY-Wing where the pivot has only 2 candidates.
class XYZWingStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Find all trivalue cells (potential pivots) and bivalue cells (potential wings)
        std::vector<Position> trivalue_cells;
        std::vector<Position> bivalue_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                int count = candidates.countPossibleValues(row, col);
                if (count == 3) {
                    trivalue_cells.push_back(Position{.row = row, .col = col});
                } else if (count == 2) {
                    bivalue_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }

        // Try each trivalue cell as pivot
        for (const auto& pivot : trivalue_cells) {
            auto result = tryPivot(board, candidates, pivot, bivalue_cells);
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::XYZWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "XYZ-Wing";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::XYZWing);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — XYZ candidate matching with wing pairs; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryPivot(const BoardData& board, const CandidateGrid& candidates,
                                                           const Position& pivot,
                                                           const std::vector<Position>& bivalue_cells) {
        auto pivot_cands = getCandidates(candidates, pivot.row, pivot.col);
        // pivot_cands has exactly 3 values: {A, B, C}

        // Find bivalue wings that see the pivot and share exactly 2 candidates with it
        std::vector<std::pair<Position, int>> wings;  // (position, the value NOT shared with other wing)
        for (const auto& cell : bivalue_cells) {
            if (!sees(pivot, cell)) {
                continue;
            }
            auto cell_cands = getCandidates(candidates, cell.row, cell.col);
            // Check both cell candidates are in pivot's candidates
            bool first_in_pivot = false;
            bool second_in_pivot = false;
            for (int pv : pivot_cands) {
                if (cell_cands[0] == pv) {
                    first_in_pivot = true;
                }
                if (cell_cands[1] == pv) {
                    second_in_pivot = true;
                }
            }
            if (first_in_pivot && second_in_pivot) {
                wings.emplace_back(cell, 0);  // placeholder, we'll determine pairing below
            }
        }

        // Try all pairs of wings
        for (size_t i = 0; i < wings.size(); ++i) {
            for (size_t j = i + 1; j < wings.size(); ++j) {
                const auto& wing1 = wings[i].first;
                const auto& wing2 = wings[j].first;

                auto w1_cands = getCandidates(candidates, wing1.row, wing1.col);
                auto w2_cands = getCandidates(candidates, wing2.row, wing2.col);

                // Find value common to all three (pivot, wing1, wing2) — this is the elimination value
                int elim_value = 0;
                for (int pv : pivot_cands) {
                    bool in_w1 = (w1_cands[0] == pv || w1_cands[1] == pv);
                    bool in_w2 = (w2_cands[0] == pv || w2_cands[1] == pv);
                    if (in_w1 && in_w2) {
                        elim_value = pv;
                    }
                }
                if (elim_value == 0) {
                    continue;
                }

                // Verify the wings together cover all 3 pivot candidates
                // (each wing shares 2 of 3 with pivot, and they share exactly 1 with each other = elim_value)
                // The non-shared values from each wing must be different and cover the remaining 2 pivot cands
                int w1_unique = (w1_cands[0] == elim_value) ? w1_cands[1] : w1_cands[0];
                int w2_unique = (w2_cands[0] == elim_value) ? w2_cands[1] : w2_cands[0];
                if (w1_unique == w2_unique) {
                    continue;  // Wings are identical — not a valid XYZ-Wing
                }

                // Eliminate elim_value from cells that see ALL THREE of pivot, wing1, wing2
                std::vector<Elimination> eliminations;
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    for (size_t col = 0; col < BOARD_SIZE; ++col) {
                        if (board[row][col] != EMPTY_CELL) {
                            continue;
                        }
                        Position pos{.row = row, .col = col};
                        if (pos == pivot || pos == wing1 || pos == wing2) {
                            continue;
                        }
                        if (sees(pos, pivot) && sees(pos, wing1) && sees(pos, wing2) &&
                            candidates.isAllowed(row, col, elim_value)) {
                            eliminations.push_back(Elimination{.position = pos, .value = elim_value});
                        }
                    }
                }

                if (!eliminations.empty()) {
                    auto explanation = fmt::format("XYZ-Wing: pivot {} {{{},{},{}}}, wing {} {{{},{}}}, wing {} "
                                                   "{{{},{}}} eliminates {} from cells seeing all three",
                                                   formatPosition(pivot), pivot_cands[0], pivot_cands[1],
                                                   pivot_cands[2], formatPosition(wing1), w1_cands[0], w1_cands[1],
                                                   formatPosition(wing2), w2_cands[0], w2_cands[1], elim_value);

                    return SolveStep{
                        .type = SolveStepType::Elimination,
                        .technique = SolvingTechnique::XYZWing,
                        .position = pivot,
                        .value = 0,
                        .eliminations = eliminations,
                        .explanation = explanation,
                        .points = getTechniquePoints(SolvingTechnique::XYZWing),
                        .explanation_data = {.positions = {pivot, wing1, wing2},
                                             .values = {pivot_cands[0], pivot_cands[1], pivot_cands[2]},
                                             .position_roles = {CellRole::Pivot, CellRole::Wing, CellRole::Wing}}};
                }
            }
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
