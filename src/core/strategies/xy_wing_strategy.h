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

/// Strategy for finding XY-Wing patterns
/// An XY-Wing consists of a pivot cell with candidates {A,B} and two wing cells:
///   - Wing 1 has candidates {A,C} and sees the pivot
///   - Wing 2 has candidates {B,C} and sees the pivot
/// Value C can be eliminated from any cell that sees both wings.
class XYWingStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Find all bi-value cells (cells with exactly 2 candidates)
        std::vector<Position> bi_value_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.countPossibleValues(row, col) == 2) {
                    bi_value_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }

        // Try each bi-value cell as pivot
        for (const auto& pivot : bi_value_cells) {
            auto result = tryPivot(board, candidates, pivot, bi_value_cells);
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::XYWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "XY-Wing";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::XYWing);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — XY candidate matching across wing pairs; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryPivot(const BoardData& board, const CandidateGrid& candidates,
                                                           const Position& pivot,
                                                           const std::vector<Position>& bi_value_cells) {
        auto pivot_cands = getCandidates(candidates, pivot.row, pivot.col);
        int val_a = pivot_cands[0];
        int val_b = pivot_cands[1];

        // Find wings that see the pivot
        std::vector<Position> wings_ac;  // cells with {A,C} for some C
        std::vector<Position> wings_bc;  // cells with {B,C} for some C

        for (const auto& cell : bi_value_cells) {
            if (cell == pivot) {
                continue;
            }
            if (!sees(pivot, cell)) {
                continue;
            }

            auto cands = getCandidates(candidates, cell.row, cell.col);
            bool has_a = (cands[0] == val_a || cands[1] == val_a);
            bool has_b = (cands[0] == val_b || cands[1] == val_b);

            if (has_a && !has_b) {
                wings_ac.push_back(cell);
            } else if (has_b && !has_a) {
                wings_bc.push_back(cell);
            }
        }

        // Try each pair of wings
        for (const auto& wing1 : wings_ac) {
            // wing1 has {A, C} where C != B
            auto w1_cands = getCandidates(candidates, wing1.row, wing1.col);
            int val_c_from_w1 = (w1_cands[0] == val_a) ? w1_cands[1] : w1_cands[0];

            for (const auto& wing2 : wings_bc) {
                // wing2 has {B, C2} where C2 != A
                auto w2_cands = getCandidates(candidates, wing2.row, wing2.col);
                int val_c_from_w2 = (w2_cands[0] == val_b) ? w2_cands[1] : w2_cands[0];

                // Both wings must share the same elimination value C
                if (val_c_from_w1 != val_c_from_w2) {
                    continue;
                }

                int val_c = val_c_from_w1;

                // Find cells that see both wings and have C as candidate
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
                        if (sees(pos, wing1) && sees(pos, wing2) && candidates.isAllowed(row, col, val_c)) {
                            eliminations.push_back(Elimination{.position = pos, .value = val_c});
                        }
                    }
                }

                if (!eliminations.empty()) {
                    auto explanation = fmt::format("XY-Wing: pivot {} {{{},{}}}, wing {} {{{},{}}}, wing {} {{{},{}}} "
                                                   "eliminates {} from cells seeing both wings",
                                                   formatPosition(pivot), val_a, val_b, formatPosition(wing1), val_a,
                                                   val_c, formatPosition(wing2), val_b, val_c, val_c);

                    return SolveStep{
                        .type = SolveStepType::Elimination,
                        .technique = SolvingTechnique::XYWing,
                        .position = pivot,
                        .value = 0,
                        .eliminations = eliminations,
                        .explanation = explanation,
                        .points = getTechniquePoints(SolvingTechnique::XYWing),
                        .explanation_data = {.positions = {pivot, wing1, wing2},
                                             .values = {val_a, val_b, val_c},
                                             .position_roles = {CellRole::Pivot, CellRole::Wing, CellRole::Wing}}};
                }
            }
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
