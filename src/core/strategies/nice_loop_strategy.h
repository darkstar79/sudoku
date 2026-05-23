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

#include "../candidate_grid.h"
#include "../i_solving_strategy.h"
#include "../solve_step.h"
#include "../solving_technique.h"
#include "../strategy_base.h"

#include <array>
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Alternating Inference Chains (AICs / Nice Loops).
/// Builds a graph of (cell, digit) nodes connected by strong and weak links,
/// then searches for alternating chains that force eliminations or placements.
///
/// Strong link: exactly 2 positions for a digit in a unit, or bivalue cell.
/// Weak link: same digit in same unit (3+ positions), or two candidates in same cell.
///
/// Discontinuous AIC Type 1: chain starts strong, ends strong at same node → node is true.
/// Discontinuous AIC Type 2: endpoints share a digit → eliminate from cells seeing both endpoints.
class NiceLoopStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        Graph graph;
        buildGraph(board, candidates, graph);
        return searchChains(board, candidates, graph);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::NiceLoop;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Nice Loop";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::NiceLoop);
    }

private:
    static constexpr int MAX_CHAIN_LENGTH = 12;
    static constexpr size_t NUM_NODES = TOTAL_CELLS * BOARD_SIZE;  // 81 cells * 9 digits

    /// Node index: (row * TOTAL_CELLS) + (col * BOARD_SIZE) + (digit - MIN_VALUE)
    [[nodiscard]] static size_t nodeIndex(size_t row, size_t col, int digit) {
        return (row * TOTAL_CELLS) + (col * BOARD_SIZE) + static_cast<size_t>(digit - MIN_VALUE);
    }

    [[nodiscard]] static size_t nodeRow(size_t node) {
        return node / TOTAL_CELLS;
    }

    [[nodiscard]] static size_t nodeCol(size_t node) {
        return (node % TOTAL_CELLS) / BOARD_SIZE;
    }

    [[nodiscard]] static int nodeDigit(size_t node) {
        return static_cast<int>(node % BOARD_SIZE) + MIN_VALUE;
    }

    [[nodiscard]] static Position nodePosition(size_t node) {
        return Position{.row = nodeRow(node), .col = nodeCol(node)};
    }

    struct Graph {
        std::array<std::vector<uint16_t>, NUM_NODES> strong;
        std::array<std::vector<uint16_t>, NUM_NODES> weak;
    };

    /// Build strong and weak link graph
    static void buildGraph(const BoardData& board, const CandidateGrid& candidates, Graph& graph) {
        // Strong links from conjugate pairs in units
        buildUnitLinks(board, candidates, graph);
        // Strong/weak links from bivalue cells
        buildCellLinks(board, candidates, graph);
    }

    /// Build links from units (rows, columns, boxes)
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — builds strong/weak unit links across rows/cols/boxes; nesting is inherent
    static void buildUnitLinks(const BoardData& board, const CandidateGrid& candidates, Graph& graph) {
        // Rows
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
                std::vector<size_t> positions;
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, digit)) {
                        positions.push_back(col);
                    }
                }
                addUnitLinks(graph, row, digit, positions, true);
            }
        }

        // Columns
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
                std::vector<size_t> positions;
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, digit)) {
                        positions.push_back(row);
                    }
                }
                addUnitLinksCol(graph, col, digit, positions);
            }
        }

        // Boxes
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            size_t box_r = (box / BOX_SIZE) * BOX_SIZE;
            size_t box_c = (box % BOX_SIZE) * BOX_SIZE;
            for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
                std::vector<std::pair<size_t, size_t>> positions;
                for (size_t br = 0; br < BOX_SIZE; ++br) {
                    for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                        size_t row = box_r + br;
                        size_t col = box_c + bc;
                        if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, digit)) {
                            positions.emplace_back(row, col);
                        }
                    }
                }
                addUnitLinksBox(graph, digit, positions);
            }
        }
    }

    /// Add links for a digit within a row
    static void addUnitLinks(Graph& graph, size_t row, int digit, const std::vector<size_t>& cols, bool /*is_row*/) {
        if (cols.size() == 2) {
            // Strong link (conjugate pair)
            auto n1 = static_cast<uint16_t>(nodeIndex(row, cols[0], digit));
            auto n2 = static_cast<uint16_t>(nodeIndex(row, cols[1], digit));
            graph.strong[n1].push_back(n2);
            graph.strong[n2].push_back(n1);
        }
        if (cols.size() >= 2) {
            // Weak links between all pairs
            for (size_t i = 0; i < cols.size(); ++i) {
                for (size_t j = i + 1; j < cols.size(); ++j) {
                    auto n1 = static_cast<uint16_t>(nodeIndex(row, cols[i], digit));
                    auto n2 = static_cast<uint16_t>(nodeIndex(row, cols[j], digit));
                    graph.weak[n1].push_back(n2);
                    graph.weak[n2].push_back(n1);
                }
            }
        }
    }

    /// Add links for a digit within a column
    static void addUnitLinksCol(Graph& graph, size_t col, int digit, const std::vector<size_t>& rows) {
        if (rows.size() == 2) {
            auto n1 = static_cast<uint16_t>(nodeIndex(rows[0], col, digit));
            auto n2 = static_cast<uint16_t>(nodeIndex(rows[1], col, digit));
            graph.strong[n1].push_back(n2);
            graph.strong[n2].push_back(n1);
        }
        if (rows.size() >= 2) {
            for (size_t i = 0; i < rows.size(); ++i) {
                for (size_t j = i + 1; j < rows.size(); ++j) {
                    auto n1 = static_cast<uint16_t>(nodeIndex(rows[i], col, digit));
                    auto n2 = static_cast<uint16_t>(nodeIndex(rows[j], col, digit));
                    graph.weak[n1].push_back(n2);
                    graph.weak[n2].push_back(n1);
                }
            }
        }
    }

    /// Add links for a digit within a box
    static void addUnitLinksBox(Graph& graph, int digit, const std::vector<std::pair<size_t, size_t>>& positions) {
        if (positions.size() == 2) {
            auto n1 = static_cast<uint16_t>(nodeIndex(positions[0].first, positions[0].second, digit));
            auto n2 = static_cast<uint16_t>(nodeIndex(positions[1].first, positions[1].second, digit));
            graph.strong[n1].push_back(n2);
            graph.strong[n2].push_back(n1);
        }
        if (positions.size() >= 2) {
            for (size_t i = 0; i < positions.size(); ++i) {
                for (size_t j = i + 1; j < positions.size(); ++j) {
                    auto n1 = static_cast<uint16_t>(nodeIndex(positions[i].first, positions[i].second, digit));
                    auto n2 = static_cast<uint16_t>(nodeIndex(positions[j].first, positions[j].second, digit));
                    graph.weak[n1].push_back(n2);
                    graph.weak[n2].push_back(n1);
                }
            }
        }
    }

    /// Build links from cell candidates (bivalue cells = strong, all cells = weak)
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — builds strong/weak cell links across all candidate pairs; nesting is inherent
    static void buildCellLinks(const BoardData& board, const CandidateGrid& candidates, Graph& graph) {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }

                uint16_t mask = candidates.getPossibleValuesMask(row, col);
                int count = candidates.countPossibleValues(row, col);

                // Collect candidate digits
                std::vector<int> cands;
                for (int d = MIN_VALUE; d <= MAX_VALUE; ++d) {
                    if ((mask & valueToBit(d)) != 0) {
                        cands.push_back(d);
                    }
                }

                if (count == 2) {
                    // Bivalue cell: strong link between the two candidates
                    auto n1 = static_cast<uint16_t>(nodeIndex(row, col, cands[0]));
                    auto n2 = static_cast<uint16_t>(nodeIndex(row, col, cands[1]));
                    graph.strong[n1].push_back(n2);
                    graph.strong[n2].push_back(n1);
                }

                // Weak links between all candidates in the cell
                if (count >= 2) {
                    for (size_t i = 0; i < cands.size(); ++i) {
                        for (size_t j = i + 1; j < cands.size(); ++j) {
                            auto n1 = static_cast<uint16_t>(nodeIndex(row, col, cands[i]));
                            auto n2 = static_cast<uint16_t>(nodeIndex(row, col, cands[j]));
                            graph.weak[n1].push_back(n2);
                            graph.weak[n2].push_back(n1);
                        }
                    }
                }
            }
        }
    }

    /// Search for alternating inference chains that produce eliminations
    [[nodiscard]] static std::optional<SolveStep> searchChains(const BoardData& board, const CandidateGrid& candidates,
                                                               const Graph& graph) {
        // Try each active node as chain start with a strong link
        for (size_t start = 0; start < NUM_NODES; ++start) {
            size_t row = nodeRow(start);
            size_t col = nodeCol(start);
            int digit = nodeDigit(start);

            if (board[row][col] != EMPTY_CELL || !candidates.isAllowed(row, col, digit)) {
                continue;
            }

            if (graph.strong[start].empty()) {
                continue;
            }

            // DFS: start with strong link outward
            std::vector<uint16_t> chain;
            chain.push_back(static_cast<uint16_t>(start));
            std::array<bool, NUM_NODES> visited{};
            visited[start] = true;

            // Follow strong link from start
            for (uint16_t next_node : graph.strong[start]) {
                chain.push_back(next_node);
                visited[next_node] = true;

                // After strong link, next must be weak
                auto result = dfs(board, candidates, graph, chain, visited, false, static_cast<uint16_t>(start));
                if (result.has_value()) {
                    return result;
                }

                visited[next_node] = false;
                chain.pop_back();
            }
        }
        return std::nullopt;
    }

    /// DFS for alternating chains
    /// @param need_strong true if next link must be strong, false if weak
    /// @param chain_start the starting node (for checking discontinuous AIC)
    [[nodiscard]] static std::optional<SolveStep> dfs(const BoardData& board, const CandidateGrid& candidates,
                                                      const Graph& graph, std::vector<uint16_t>& chain,
                                                      std::array<bool, NUM_NODES>& visited, bool need_strong,
                                                      uint16_t chain_start) {
        if (chain.size() > static_cast<size_t>(MAX_CHAIN_LENGTH)) {
            return std::nullopt;
        }

        uint16_t current = chain.back();
        const auto& links = need_strong ? graph.strong[current] : graph.weak[current];

        for (uint16_t next_node : links) {
            if (visited[next_node]) {
                continue;
            }

            // Check for discontinuous AIC Type 2:
            // Current chain ends after a weak link. If we can follow a strong link back
            // to form a valid AIC ending, check for eliminations.
            // AIC: strong-weak-...-weak-strong. The chain currently ends at a node reached by
            // weak link (need_strong=true means we just followed a weak link).
            // If need_strong and next_node has a strong link, and we can find eliminations
            // between chain_start and next_node for the same digit:
            if (need_strong && chain.size() >= 3) {
                // We're about to follow a strong link to next_node.
                // Check: does next_node connect via strong to chain_start's digit?
                // Type 2: start and end are same digit, different cells
                int start_digit = nodeDigit(chain_start);
                int end_digit = nodeDigit(next_node);

                if (start_digit == end_digit) {
                    Position start_pos = nodePosition(chain_start);
                    Position end_pos = nodePosition(next_node);

                    if (!(start_pos == end_pos)) {
                        auto result = findAICEliminations(board, candidates, chain_start, next_node, chain, next_node);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }

            chain.push_back(next_node);
            visited[next_node] = true;

            auto result = dfs(board, candidates, graph, chain, visited, !need_strong, chain_start);
            if (result.has_value()) {
                return result;
            }

            visited[next_node] = false;
            chain.pop_back();
        }

        return std::nullopt;
    }

    /// Find eliminations from a discontinuous AIC (Type 2)
    /// Both endpoints assert the same digit → eliminate from cells seeing both
    [[nodiscard]] static std::optional<SolveStep>
    findAICEliminations(const BoardData& board, const CandidateGrid& candidates, uint16_t start_node, uint16_t end_node,
                        const std::vector<uint16_t>& chain, uint16_t final_node) {
        Position start_pos = nodePosition(start_node);
        Position end_pos = nodePosition(end_node);
        int digit = nodeDigit(start_node);

        std::vector<Elimination> eliminations;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                Position pos{.row = row, .col = col};
                if (pos == start_pos || pos == end_pos) {
                    continue;
                }
                if (sees(pos, start_pos) && sees(pos, end_pos) && candidates.isAllowed(row, col, digit)) {
                    eliminations.push_back(Elimination{.position = pos, .value = digit});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        // Build chain description
        auto explanation = fmt::format(
            "Nice Loop: AIC of length {} on digit {} from {} to {} — eliminates {} from cells seeing both endpoints",
            chain.size() + 1, digit, formatPosition(start_pos), formatPosition(end_pos), digit);

        std::vector<Position> chain_positions;
        chain_positions.reserve(chain.size() + 1);
        for (uint16_t node : chain) {
            chain_positions.push_back(nodePosition(node));
        }
        chain_positions.push_back(nodePosition(final_node));

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::NiceLoop,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::NiceLoop),
                         .explanation_data = {.positions = {start_pos, end_pos},
                                              .values = {digit, static_cast<int>(chain.size() + 1)},
                                              .position_roles = {CellRole::ChainA, CellRole::ChainB}}};
    }
};

}  // namespace sudoku::core
