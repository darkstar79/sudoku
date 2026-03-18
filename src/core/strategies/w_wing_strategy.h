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

/// Strategy for finding W-Wing patterns
/// A W-Wing uses two bivalue cells with the same candidate pair {A,B}, connected
/// by a strong link on one of the values. If a strong link on value A exists
/// (value A appears in exactly 2 cells in some row/col/box, with one end seeing
/// each bivalue cell), then value B can be eliminated from any cell seeing both
/// bivalue cells.
class WWingStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // CPD-OFF — bivalue cell collection pattern shared with UR and remote_pairs
        // Find all bivalue cells
        std::vector<Position> bivalue_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.countPossibleValues(row, col) == 2) {
                    bivalue_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }

        // Try each pair of bivalue cells with the same candidates
        for (size_t i = 0; i < bivalue_cells.size(); ++i) {
            for (size_t j = i + 1; j < bivalue_cells.size(); ++j) {
                const auto& c1 = bivalue_cells[i];
                const auto& c2 = bivalue_cells[j];

                auto cands1 = getCandidates(candidates, c1.row, c1.col);
                auto cands2 = getCandidates(candidates, c2.row, c2.col);

                // Must have the same two candidates
                if (cands1[0] != cands2[0] || cands1[1] != cands2[1]) {
                    continue;
                }

                int val_a = cands1[0];
                int val_b = cands1[1];

                // Try strong link on val_a (eliminate val_b) and on val_b (eliminate val_a)
                auto result = tryStrongLink(board, candidates, c1, c2, val_a, val_b);
                if (result.has_value()) {
                    return result;
                }
                result = tryStrongLink(board, candidates, c1, c2, val_b, val_a);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        // CPD-ON

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::WWing;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "W-Wing";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::WWing);
    }

private:
    /// Try to find a strong link on link_value connecting c1 and c2.
    /// If found, eliminate elim_value from cells seeing both c1 and c2.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — strong link enumeration across rows/cols/boxes; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryStrongLink(const BoardData& board, const CandidateGrid& candidates,
                                                                const Position& c1, const Position& c2, int link_value,
                                                                int elim_value) {
        // Check rows for strong link on link_value
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            std::vector<Position> cells;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, link_value)) {
                    cells.push_back(Position{.row = row, .col = col});
                }
            }
            auto result = tryLinkEndpoints(board, candidates, c1, c2, cells, link_value, elim_value);
            if (result.has_value()) {
                return result;
            }
        }

        // Check columns for strong link
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            std::vector<Position> cells;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, link_value)) {
                    cells.push_back(Position{.row = row, .col = col});
                }
            }
            auto result = tryLinkEndpoints(board, candidates, c1, c2, cells, link_value, elim_value);
            if (result.has_value()) {
                return result;
            }
        }

        // Check boxes for strong link
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            size_t start_row = (box / BOX_SIZE) * BOX_SIZE;
            size_t start_col = (box % BOX_SIZE) * BOX_SIZE;
            std::vector<Position> cells;
            for (size_t r = start_row; r < start_row + BOX_SIZE; ++r) {
                for (size_t c = start_col; c < start_col + BOX_SIZE; ++c) {
                    if (board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, link_value)) {
                        cells.push_back(Position{.row = r, .col = c});
                    }
                }
            }
            auto result = tryLinkEndpoints(board, candidates, c1, c2, cells, link_value, elim_value);
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    /// Given a set of cells forming a conjugate pair (exactly 2 cells with link_value),
    /// check if one endpoint sees c1 and the other sees c2 (or vice versa).
    [[nodiscard]] static std::optional<SolveStep>
    tryLinkEndpoints(const BoardData& board, const CandidateGrid& candidates, const Position& c1, const Position& c2,
                     const std::vector<Position>& link_cells, int link_value, int elim_value) {
        if (link_cells.size() != 2) {
            return std::nullopt;
        }

        const auto& l1 = link_cells[0];
        const auto& l2 = link_cells[1];

        // Link endpoints must not be the bivalue cells themselves
        if (l1 == c1 || l1 == c2 || l2 == c1 || l2 == c2) {
            return std::nullopt;
        }

        // Check both orientations: l1 sees c1 & l2 sees c2, or l1 sees c2 & l2 sees c1
        bool orientation1 = sees(l1, c1) && sees(l2, c2);
        bool orientation2 = sees(l1, c2) && sees(l2, c1);
        if (!orientation1 && !orientation2) {
            return std::nullopt;
        }

        // Eliminate elim_value from cells seeing both c1 and c2
        std::vector<Elimination> eliminations;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                Position pos{.row = row, .col = col};
                if (pos == c1 || pos == c2 || pos == l1 || pos == l2) {
                    continue;
                }
                if (sees(pos, c1) && sees(pos, c2) && candidates.isAllowed(row, col, elim_value)) {
                    eliminations.push_back(Elimination{.position = pos, .value = elim_value});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation = fmt::format("W-Wing: cells {} and {} {{{},{}}} connected by strong link on {} at {},{} — "
                                       "eliminates {} from cells seeing both",
                                       formatPosition(c1), formatPosition(c2), link_value, elim_value, link_value,
                                       formatPosition(l1), formatPosition(l2), elim_value);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::WWing,
                         .position = c1,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .points = getTechniquePoints(SolvingTechnique::WWing),
                         .explanation_data = {.positions = {c1, c2, l1, l2},
                                              .values = {link_value, elim_value},
                                              .position_roles = {CellRole::Pattern, CellRole::Pattern,
                                                                 CellRole::LinkEndpoint, CellRole::LinkEndpoint}}};
    }
};

}  // namespace sudoku::core
