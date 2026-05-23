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

/// Strategy for finding XY-Chain patterns.
/// Generalizes XY-Wing to arbitrary-length chains of bivalue cells.
/// Each consecutive pair shares one candidate. If the first and last cells
/// of a chain of length >= 4 share an elimination candidate X, then X can be
/// eliminated from cells seeing both endpoints.
class XYChainStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // CPD-OFF — bivalue cell collection pattern shared with remote_pairs
        // Collect all bivalue cells
        std::vector<Position> bivalue_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.countPossibleValues(row, col) == 2) {
                    bivalue_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }

        // CPD-ON

        if (bivalue_cells.size() < 4) {
            return std::nullopt;
        }

        // Try each cell as chain start
        for (const auto& start : bivalue_cells) {
            auto start_cands = getCandidates(candidates, start.row, start.col);
            // Try each candidate as the elimination value X
            for (int x_idx = 0; x_idx < 2; ++x_idx) {
                int elim_x = start_cands[x_idx];
                int outgoing = start_cands[1 - x_idx];

                std::vector<bool> visited(TOTAL_CELLS, false);
                visited[(start.row * BOARD_SIZE) + start.col] = true;

                auto result = dfs(board, candidates, bivalue_cells, start, start, elim_x, outgoing, visited, 1);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::XYChain;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "XY-Chain";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::XYChain);
    }

private:
    static constexpr int MAX_DEPTH = 8;

    [[nodiscard]] static std::optional<SolveStep> dfs(const BoardData& board, const CandidateGrid& candidates,
                                                      const std::vector<Position>& bivalue_cells, const Position& start,
                                                      const Position& current, int elim_x, int outgoing,
                                                      std::vector<bool>& visited, int depth) {
        if (depth > MAX_DEPTH) {
            return std::nullopt;
        }

        for (const auto& next : bivalue_cells) {
            size_t next_idx = (next.row * BOARD_SIZE) + next.col;
            if (visited[next_idx]) {
                continue;
            }
            if (!sees(current, next)) {
                continue;
            }

            auto next_cands = getCandidates(candidates, next.row, next.col);
            // Next cell must contain the outgoing value
            bool has_outgoing = (next_cands[0] == outgoing || next_cands[1] == outgoing);
            if (!has_outgoing) {
                continue;
            }

            int new_outgoing = (next_cands[0] == outgoing) ? next_cands[1] : next_cands[0];

            // If new_outgoing == elim_x and chain length >= 4 (depth >= 3), we found a valid chain
            if (new_outgoing == elim_x && depth >= 3) {
                auto result = findEliminations(board, candidates, start, next, elim_x, depth + 1);
                if (result.has_value()) {
                    return result;
                }
            }

            // Continue DFS
            visited[next_idx] = true;
            auto result = dfs(board, candidates, bivalue_cells, start, next, elim_x, new_outgoing, visited, depth + 1);
            if (result.has_value()) {
                return result;
            }
            visited[next_idx] = false;
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep> findEliminations(const BoardData& board,
                                                                   const CandidateGrid& candidates,
                                                                   const Position& start, const Position& end,
                                                                   int elim_x, int chain_length) {
        std::vector<Elimination> eliminations;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                Position pos{.row = row, .col = col};
                if (pos == start || pos == end) {
                    continue;
                }
                if (sees(pos, start) && sees(pos, end) && candidates.isAllowed(row, col, elim_x)) {
                    eliminations.push_back(Elimination{.position = pos, .value = elim_x});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation = fmt::format(
            "XY-Chain: chain of {} bivalue cells from {} to {} — eliminates {} from cells seeing both endpoints",
            chain_length, formatPosition(start), formatPosition(end), elim_x);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::XYChain,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::XYChain),
                         .explanation_data = {.positions = {start, end},
                                              .values = {elim_x, chain_length},
                                              .position_roles = {CellRole::ChainA, CellRole::ChainB}}};
    }
};

}  // namespace sudoku::core
