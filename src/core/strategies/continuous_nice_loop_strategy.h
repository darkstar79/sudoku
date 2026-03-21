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

/// Strategy for finding Continuous Nice Loops (AIC Type 1).
/// A continuous loop is an alternating inference chain that returns to the
/// starting node with correct link parity. In a continuous loop, every weak
/// link produces eliminations:
///   - Same digit in different cells: eliminate from cells seeing both endpoints
///   - Different digits in same cell: eliminate all other candidates from that cell
///
/// Uses the same (cell, digit) graph as NiceLoopStrategy with identical strong
/// and weak link construction.
class ContinuousNiceLoopStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        Graph graph;
        buildGraph(board, candidates, graph);
        return searchContinuousLoops(board, candidates, graph);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::ContinuousNiceLoop;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Continuous Nice Loop";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::ContinuousNiceLoop);
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
        buildUnitLinks(board, candidates, graph);
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
                addRowLinks(graph, row, digit, positions);
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
                addColLinks(graph, col, digit, positions);
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
                addBoxLinks(graph, digit, positions);
            }
        }
    }

    /// Add links for a digit within a row
    static void addRowLinks(Graph& graph, size_t row, int digit, const std::vector<size_t>& cols) {
        if (cols.size() == 2) {
            auto n1 = static_cast<uint16_t>(nodeIndex(row, cols[0], digit));
            auto n2 = static_cast<uint16_t>(nodeIndex(row, cols[1], digit));
            graph.strong[n1].push_back(n2);
            graph.strong[n2].push_back(n1);
        }
        if (cols.size() >= 2) {
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
    static void addColLinks(Graph& graph, size_t col, int digit, const std::vector<size_t>& rows) {
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
    static void addBoxLinks(Graph& graph, int digit, const std::vector<std::pair<size_t, size_t>>& positions) {
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

                std::vector<int> cands;
                for (int d = MIN_VALUE; d <= MAX_VALUE; ++d) {
                    if ((mask & valueToBit(d)) != 0) {
                        cands.push_back(d);
                    }
                }

                if (count == 2) {
                    auto n1 = static_cast<uint16_t>(nodeIndex(row, col, cands[0]));
                    auto n2 = static_cast<uint16_t>(nodeIndex(row, col, cands[1]));
                    graph.strong[n1].push_back(n2);
                    graph.strong[n2].push_back(n1);
                }

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

    /// Search for continuous loops that produce eliminations
    [[nodiscard]] static std::optional<SolveStep>
    searchContinuousLoops(const BoardData& board, const CandidateGrid& candidates, const Graph& graph) {
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
            // link_types[i] = true if the link from chain[i] to chain[i+1] is strong
            std::vector<bool> link_types;
            std::array<bool, NUM_NODES> visited{};
            visited[start] = true;

            for (uint16_t next_node : graph.strong[start]) {
                chain.push_back(next_node);
                link_types.push_back(true);  // strong link from start to next_node
                visited[next_node] = true;

                // After strong link, next must be weak
                auto result =
                    dfsLoop(board, candidates, graph, chain, link_types, visited, false, static_cast<uint16_t>(start));
                if (result.has_value()) {
                    return result;
                }

                visited[next_node] = false;
                chain.pop_back();
                link_types.pop_back();
            }
        }
        return std::nullopt;
    }

    /// DFS for continuous alternating loops
    /// @param need_strong true if next link must be strong, false if weak
    /// @param chain_start the starting node (for checking loop closure)
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — DFS loop search with alternating link logic and loop closure checks
    [[nodiscard]] static std::optional<SolveStep> dfsLoop(const BoardData& board, const CandidateGrid& candidates,
                                                          const Graph& graph, std::vector<uint16_t>& chain,
                                                          std::vector<bool>& link_types,
                                                          std::array<bool, NUM_NODES>& visited, bool need_strong,
                                                          uint16_t chain_start) {
        if (chain.size() > static_cast<size_t>(MAX_CHAIN_LENGTH)) {
            return std::nullopt;
        }

        uint16_t current = chain.back();

        // Check if we can close the loop back to start with a weak link
        // Requirements: not looking for strong (i.e., we need a weak link next),
        // chain has at least 4 nodes, and chain_start is a weak neighbor of current
        if (!need_strong && chain.size() >= 4) {
            for (uint16_t weak_neighbor : graph.weak[current]) {
                if (weak_neighbor == chain_start) {
                    // Loop closes with a weak link from current back to start
                    // The full loop alternation: start =[strong]=> ... -[weak]-> start
                    auto result = collectLoopEliminations(board, candidates, chain, link_types);
                    if (result.has_value()) {
                        return result;
                    }
                    break;
                }
            }
        }

        const auto& links = need_strong ? graph.strong[current] : graph.weak[current];

        for (uint16_t next_node : links) {
            if (visited[next_node]) {
                continue;
            }

            chain.push_back(next_node);
            link_types.push_back(need_strong);
            visited[next_node] = true;

            auto result = dfsLoop(board, candidates, graph, chain, link_types, visited, !need_strong, chain_start);
            if (result.has_value()) {
                return result;
            }

            visited[next_node] = false;
            chain.pop_back();
            link_types.pop_back();
        }

        return std::nullopt;
    }

    /// Collect eliminations from all weak links in a continuous loop.
    /// The loop is: chain[0] -link_types[0]-> chain[1] -link_types[1]-> ... -[weak]-> chain[0]
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — elimination collection iterates over loop weak links with two distinct cases
    [[nodiscard]] static std::optional<SolveStep> collectLoopEliminations(const BoardData& board,
                                                                          const CandidateGrid& candidates,
                                                                          const std::vector<uint16_t>& chain,
                                                                          const std::vector<bool>& link_types) {
        std::vector<Elimination> eliminations;

        // Build set of loop cell positions for exclusion
        std::array<bool, TOTAL_CELLS> is_loop_cell{};
        for (uint16_t node : chain) {
            size_t row = nodeRow(node);
            size_t col = nodeCol(node);
            is_loop_cell[(row * BOARD_SIZE) + col] = true;
        }

        // Process each link in the loop (including the closing link back to start)
        size_t loop_size = chain.size();
        for (size_t i = 0; i < loop_size; ++i) {
            bool is_strong = (i < link_types.size()) ? link_types[i] : false;  // closing link is weak

            if (is_strong) {
                continue;  // Only weak links produce eliminations
            }

            uint16_t node_a = chain[i];
            uint16_t node_b = chain[(i + 1) % loop_size];

            Position pos_a = nodePosition(node_a);
            Position pos_b = nodePosition(node_b);
            int digit_a = nodeDigit(node_a);
            int digit_b = nodeDigit(node_b);

            if (digit_a == digit_b && !(pos_a == pos_b)) {
                // Same digit in different cells: eliminate from cells seeing both
                int digit = digit_a;
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    for (size_t col = 0; col < BOARD_SIZE; ++col) {
                        if (board[row][col] != EMPTY_CELL) {
                            continue;
                        }
                        if (is_loop_cell[(row * BOARD_SIZE) + col]) {
                            continue;
                        }
                        Position pos{.row = row, .col = col};
                        if (sees(pos, pos_a) && sees(pos, pos_b) && candidates.isAllowed(row, col, digit)) {
                            eliminations.push_back(Elimination{.position = pos, .value = digit});
                        }
                    }
                }
            } else if (pos_a == pos_b && digit_a != digit_b) {
                // Different digits in same cell: cell must contain one of these two,
                // so eliminate all OTHER candidates from this cell
                size_t row = pos_a.row;
                size_t col = pos_a.col;
                for (int d = MIN_VALUE; d <= MAX_VALUE; ++d) {
                    if (d == digit_a || d == digit_b) {
                        continue;
                    }
                    if (candidates.isAllowed(row, col, d)) {
                        eliminations.push_back(Elimination{.position = pos_a, .value = d});
                    }
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        // Deduplicate eliminations
        std::ranges::sort(eliminations, [](const Elimination& a, const Elimination& b) {
            if (a.position.row != b.position.row) {
                return a.position.row < b.position.row;
            }
            if (a.position.col != b.position.col) {
                return a.position.col < b.position.col;
            }
            return a.value < b.value;
        });
        auto last = std::ranges::unique(eliminations, [](const Elimination& a, const Elimination& b) {
            return a.position == b.position && a.value == b.value;
        });
        eliminations.erase(last.begin(), last.end());

        // Build explanation
        auto explanation = fmt::format(
            "Continuous Nice Loop: loop of {} nodes — eliminates {} candidate(s) from cells via weak link logic",
            chain.size(), eliminations.size());

        // Collect chain positions for explanation data
        std::vector<Position> chain_positions;
        chain_positions.reserve(chain.size());
        for (uint16_t node : chain) {
            chain_positions.push_back(nodePosition(node));
        }

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::ContinuousNiceLoop,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::ContinuousNiceLoop),
            .explanation_data = {.positions = chain_positions,
                                 .values = {static_cast<int>(chain.size()), static_cast<int>(eliminations.size())},
                                 .position_roles = {CellRole::ChainA, CellRole::ChainB}}};
    }
};

}  // namespace sudoku::core
