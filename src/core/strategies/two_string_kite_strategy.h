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
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding 2-String Kite patterns
/// A 2-String Kite uses a row conjugate pair and a column conjugate pair for the same
/// candidate, connected through a shared box. One endpoint from each pair is in the same box.
/// The candidate can be eliminated from cells that see both of the other two endpoints.
class TwoStringKiteStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — value×row×col conjugate pair matching; nesting is inherent to 2-string kite search
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            // Find row conjugate pairs (rows with exactly 2 candidate positions)
            std::vector<std::pair<Position, Position>> row_pairs;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                std::vector<size_t> cols;
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                        cols.push_back(col);
                    }
                }
                if (cols.size() == 2) {
                    row_pairs.emplace_back(Position{.row = row, .col = cols[0]}, Position{.row = row, .col = cols[1]});
                }
            }

            // Find column conjugate pairs
            std::vector<std::pair<Position, Position>> col_pairs;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                std::vector<size_t> rows;
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                        rows.push_back(row);
                    }
                }
                if (rows.size() == 2) {
                    col_pairs.emplace_back(Position{.row = rows[0], .col = col}, Position{.row = rows[1], .col = col});
                }
            }

            // Try each row-pair + col-pair combination
            for (const auto& [rp1, rp2] : row_pairs) {
                for (const auto& [cp1, cp2] : col_pairs) {
                    // Skip if the row pair and col pair share a cell
                    if (rp1 == cp1 || rp1 == cp2 || rp2 == cp1 || rp2 == cp2) {
                        continue;
                    }

                    // Try all 4 combinations of connecting endpoints through a shared box
                    auto result = tryConnection(board, candidates, rp1, rp2, cp1, cp2, value);
                    if (result.has_value()) {
                        return result;
                    }
                    result = tryConnection(board, candidates, rp1, rp2, cp2, cp1, value);
                    if (result.has_value()) {
                        return result;
                    }
                    result = tryConnection(board, candidates, rp2, rp1, cp1, cp2, value);
                    if (result.has_value()) {
                        return result;
                    }
                    result = tryConnection(board, candidates, rp2, rp1, cp2, cp1, value);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::TwoStringKite;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "2-String Kite";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::TwoStringKite);
    }

private:
    /// Try connecting row_connected (from row pair) and col_connected (from col pair) via shared box.
    /// If they share a box, eliminate value from cells seeing both row_other and col_other.
    [[nodiscard]] static std::optional<SolveStep>
    tryConnection(const BoardData& board, const CandidateGrid& candidates, const Position& row_connected,
                  const Position& row_other, const Position& col_connected, const Position& col_other, int value) {
        if (!sameBox(row_connected, col_connected)) {
            return std::nullopt;
        }

        // Eliminate value from cells that see both row_other and col_other
        std::vector<Elimination> eliminations;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                Position pos{.row = row, .col = col};
                if (pos == row_connected || pos == row_other || pos == col_connected || pos == col_other) {
                    continue;
                }
                if (sees(pos, row_other) && sees(pos, col_other) && candidates.isAllowed(row, col, value)) {
                    eliminations.push_back(Elimination{.position = pos, .value = value});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation =
            fmt::format("2-String Kite on value {}: row pair {},{} and column pair {},{}"
                        " connected through shared box \xe2\x80\x94 eliminates {} from cells seeing both endpoints",
                        value, formatPosition(row_other), formatPosition(row_connected), formatPosition(col_connected),
                        formatPosition(col_other), value);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::TwoStringKite,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::TwoStringKite),
                         .explanation_data = {.positions = {row_other, row_connected, col_connected, col_other},
                                              .values = {value},
                                              .position_roles = {CellRole::ChainA, CellRole::LinkEndpoint,
                                                                 CellRole::LinkEndpoint, CellRole::ChainB}}};
    }
};

}  // namespace sudoku::core
