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
#include <queue>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Remote Pairs patterns.
/// A chain of bivalue cells with the same candidate pair {A,B}, where each consecutive
/// cell sees the previous one. At even chain length (≥4), A and B can be eliminated
/// from cells that see both the start and end of the chain.
class RemotePairsStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // CPD-OFF — bivalue cell collection pattern shared with UR and w_wing
        // Collect all bivalue cells, grouped by candidate pair
        std::vector<Position> bivalue_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.countPossibleValues(row, col) == 2) {
                    bivalue_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }
        // CPD-ON

        // Group by candidate pair (using mask as key)
        for (size_t start = 0; start < bivalue_cells.size(); ++start) {
            uint16_t pair_mask = candidates.getPossibleValuesMask(bivalue_cells[start].row, bivalue_cells[start].col);

            // BFS from this cell through connected bivalue cells with same pair
            auto result = bfsFromCell(board, candidates, bivalue_cells, start, pair_mask);
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::RemotePairs;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Remote Pairs";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::RemotePairs);
    }

private:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — BFS over bivalue chain with pair-mask checking; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> bfsFromCell(const BoardData& board, const CandidateGrid& candidates,
                                                              const std::vector<Position>& bivalue_cells,
                                                              size_t start_idx, uint16_t pair_mask) {
        // Extract values A and B
        int val_a = 0;
        int val_b = 0;
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((pair_mask & valueToBit(v)) != 0) {
                if (val_a == 0) {
                    val_a = v;
                } else {
                    val_b = v;
                }
            }
        }

        // BFS: queue of (cell_index_in_bivalue_cells, depth)
        std::vector<int> depth(bivalue_cells.size(), -1);
        std::queue<size_t> queue;
        depth[start_idx] = 0;
        queue.push(start_idx);

        while (!queue.empty()) {
            size_t curr = queue.front();
            queue.pop();
            int curr_depth = depth[curr];

            for (size_t next = 0; next < bivalue_cells.size(); ++next) {
                if (depth[next] >= 0) {
                    continue;  // already visited
                }
                // Must have same pair
                if (candidates.getPossibleValuesMask(bivalue_cells[next].row, bivalue_cells[next].col) != pair_mask) {
                    continue;
                }
                // Must see current cell
                if (!sees(bivalue_cells[curr], bivalue_cells[next])) {
                    continue;
                }

                depth[next] = curr_depth + 1;
                queue.push(next);

                // Chain of N cells = depth N-1. Even-length chain (4,6,...) means
                // depth is odd (3,5,...). Start and end have opposite polarity.
                if (depth[next] >= 3 && depth[next] % 2 == 1) {
                    auto result = checkEliminations(board, candidates, bivalue_cells[start_idx], bivalue_cells[next],
                                                    val_a, val_b, depth[next]);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep> checkEliminations(const BoardData& board,
                                                                    const CandidateGrid& candidates,
                                                                    const Position& start, const Position& end,
                                                                    int val_a, int val_b, int chain_length) {
        std::vector<Elimination> eliminations;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                Position pos{.row = row, .col = col};
                // Skip the endpoints themselves
                if ((pos.row == start.row && pos.col == start.col) || (pos.row == end.row && pos.col == end.col)) {
                    continue;
                }
                // Must see both endpoints
                if (!sees(pos, start) || !sees(pos, end)) {
                    continue;
                }
                // Eliminate A and B
                if (candidates.isAllowed(row, col, val_a)) {
                    eliminations.push_back(Elimination{.position = pos, .value = val_a});
                }
                if (candidates.isAllowed(row, col, val_b)) {
                    eliminations.push_back(Elimination{.position = pos, .value = val_b});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation =
            fmt::format("Remote Pairs: chain of {{{},{}}} cells from {} to {} (length {}) — eliminates {},{} from "
                        "cells seeing both endpoints",
                        val_a, val_b, formatPosition(start), formatPosition(end), chain_length, val_a, val_b);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::RemotePairs,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::RemotePairs),
                         .explanation_data = {.positions = {start, end},
                                              .values = {val_a, val_b, chain_length},
                                              .position_roles = {CellRole::ChainA, CellRole::ChainB}}};
    }
};

}  // namespace sudoku::core
