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

/// Strategy for finding Avoidable Rectangle patterns.
/// An Avoidable Rectangle has 4 cells forming a rectangle across 2 boxes where:
///   - 3 corners are solver-placed (not given) with exactly 2 values {A,B}
///   - 1 corner is unsolved with both A,B as candidates
///   - The value completing the deadly pattern is eliminated from the unsolved corner.
/// Requires givens info from CandidateGrid::setGivensFromPuzzle().
class AvoidableRectangleStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        if (!candidates.hasGivensInfo()) {
            return std::nullopt;
        }

        return findAvoidableRectangle(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::AvoidableRectangle;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Avoidable Rectangle";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::AvoidableRectangle);
    }

private:
    // CPD-OFF — rectangle-based strategy structurally similar to UR but with solver-placed cell semantics
    /// Search for Avoidable Rectangle patterns.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — rectangle enumeration with solver-placed value checks; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findAvoidableRectangle(const BoardData& board,
                                                                         const CandidateGrid& candidates) {
        // Find all solver-placed cells (filled but not given)
        std::vector<Position> placed_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL && !candidates.isGiven(row, col)) {
                    placed_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }

        // Try pairs of solver-placed cells in the same row with different values
        for (size_t i = 0; i < placed_cells.size(); ++i) {
            for (size_t j = i + 1; j < placed_cells.size(); ++j) {
                const auto& c1 = placed_cells[i];
                const auto& c2 = placed_cells[j];

                if (c1.row != c2.row) {
                    continue;
                }
                if (sameBox(c1, c2)) {
                    continue;
                }

                int v1 = board[c1.row][c1.col];
                int v2 = board[c2.row][c2.col];
                if (v1 == v2) {
                    continue;
                }

                auto result = tryRectangleCompletion(board, candidates, c1, c2, v1, v2);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        // Also try pairs in the same column
        for (size_t i = 0; i < placed_cells.size(); ++i) {
            for (size_t j = i + 1; j < placed_cells.size(); ++j) {
                const auto& c1 = placed_cells[i];
                const auto& c2 = placed_cells[j];

                if (c1.col != c2.col) {
                    continue;
                }
                if (sameBox(c1, c2)) {
                    continue;
                }

                int v1 = board[c1.row][c1.col];
                int v2 = board[c2.row][c2.col];
                if (v1 == v2) {
                    continue;
                }

                auto result = tryRectangleCompletionCol(board, candidates, c1, c2, v1, v2);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    /// Try to complete rectangle from two cells in the same row.
    [[nodiscard]] static std::optional<SolveStep> tryRectangleCompletion(const BoardData& board,
                                                                         const CandidateGrid& candidates,
                                                                         const Position& c1, const Position& c2,
                                                                         int val_a, int val_b) {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            if (row == c1.row) {
                continue;
            }
            Position c3{.row = row, .col = c1.col};
            Position c4{.row = row, .col = c2.col};
            auto result = checkRectangle(board, candidates, c1, c2, c3, c4, val_a, val_b);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Try to complete rectangle from two cells in the same column.
    [[nodiscard]] static std::optional<SolveStep> tryRectangleCompletionCol(const BoardData& board,
                                                                            const CandidateGrid& candidates,
                                                                            const Position& c1, const Position& c2,
                                                                            int val_a, int val_b) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (col == c1.col) {
                continue;
            }
            Position c3{.row = c1.row, .col = col};
            Position c4{.row = c2.row, .col = col};
            auto result = checkRectangle(board, candidates, c1, c2, c3, c4, val_a, val_b);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Check if 4 cells form a valid Avoidable Rectangle.
    /// c1 and c2 are solver-placed with values {A,B}. Check c3 and c4.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — checks 3 placed + 1 unsolved corner patterns; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> checkRectangle(const BoardData& board,
                                                                 const CandidateGrid& candidates, const Position& c1,
                                                                 const Position& c2, const Position& c3,
                                                                 const Position& c4, int val_a, int val_b) {
        // Must span exactly 2 boxes
        if (countUniqueBoxes(c1, c2, c3, c4) != 2) {
            return std::nullopt;
        }

        // Need exactly 3 solver-placed corners + 1 unsolved corner.
        // c1 and c2 are already solver-placed with values val_a and val_b.
        // Check all arrangements: c3 or c4 could be the unsolved corner.

        // Case 1: c3 is solver-placed, c4 is unsolved
        if (board[c3.row][c3.col] != EMPTY_CELL && !candidates.isGiven(c3.row, c3.col) &&
            board[c4.row][c4.col] == EMPTY_CELL) {
            int v3 = board[c3.row][c3.col];
            // c3 must have one of {val_a, val_b}
            // The 3 placed cells must form the deadly pattern with exactly {val_a, val_b}
            // c1 has val_a, c2 has val_b, c3 must have val_b (same col as c1, so same value as c2)
            // or c3 has val_a (completing the rectangle differently)
            if (v3 == val_b) {
                // Pattern: c1=A, c2=B, c3=B → c4 would need A to complete deadly pattern
                // Eliminate A from c4
                if (candidates.isAllowed(c4.row, c4.col, val_a)) {
                    return buildStep(c1, c2, c3, c4, val_a, val_b, c4, val_a);
                }
            } else if (v3 == val_a) {
                // Pattern: c1=A, c2=B, c3=A → c4 would need B to complete deadly pattern
                // Eliminate B from c4
                if (candidates.isAllowed(c4.row, c4.col, val_b)) {
                    return buildStep(c1, c2, c3, c4, val_a, val_b, c4, val_b);
                }
            }
        }

        // Case 2: c4 is solver-placed, c3 is unsolved
        if (board[c4.row][c4.col] != EMPTY_CELL && !candidates.isGiven(c4.row, c4.col) &&
            board[c3.row][c3.col] == EMPTY_CELL) {
            int v4 = board[c4.row][c4.col];
            if (v4 == val_a) {
                // Pattern: c1=A, c2=B, c4=A → c3 would need B to complete deadly pattern
                // Eliminate B from c3
                if (candidates.isAllowed(c3.row, c3.col, val_b)) {
                    return buildStep(c1, c2, c3, c4, val_a, val_b, c3, val_b);
                }
            } else if (v4 == val_b) {
                // Pattern: c1=A, c2=B, c4=B → c3 would need A to complete deadly pattern
                // Eliminate A from c3
                if (candidates.isAllowed(c3.row, c3.col, val_a)) {
                    return buildStep(c1, c2, c3, c4, val_a, val_b, c3, val_a);
                }
            }
        }

        return std::nullopt;
    }

    /// Build the SolveStep for an Avoidable Rectangle elimination.
    [[nodiscard]] static SolveStep buildStep(const Position& c1, const Position& c2, const Position& c3,
                                             const Position& c4, int val_a, int val_b, const Position& target,
                                             int elim_val) {
        auto positions_str = fmt::format("{}, {}, {}, {}", formatPosition(c1), formatPosition(c2), formatPosition(c3),
                                         formatPosition(c4));
        auto explanation = fmt::format("Avoidable Rectangle: cells {} with solved values {{{},{}}} — "
                                       "eliminates {} from {} to avoid deadly pattern",
                                       positions_str, val_a, val_b, elim_val, formatPosition(target));

        std::vector<Position> all_positions = {c1, c2, c3, c4};
        std::vector<CellRole> roles;
        roles.reserve(all_positions.size());
        for (const auto& pos : all_positions) {
            roles.push_back((pos == target) ? CellRole::Floor : CellRole::Roof);
        }

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::AvoidableRectangle,
                         .position = target,
                         .value = 0,
                         .eliminations = {Elimination{.position = target, .value = elim_val}},
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::AvoidableRectangle),
                         .explanation_data = {.positions = all_positions,
                                              .values = {val_a, val_b, elim_val},
                                              .position_roles = std::move(roles)}};
    }
    // CPD-ON
};

}  // namespace sudoku::core
