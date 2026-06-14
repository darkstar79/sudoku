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

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Grouped Nice Loop (Grouped AIC) patterns.
/// Extends Nice Loop with grouped nodes (2-3 cells in the same box on the same
/// row/column sharing a candidate digit). Combines multi-digit cell-based links
/// (bivalue strong links, multi-candidate weak links) with single-digit grouped
/// unit links. Searches for discontinuous Type 2 AICs where start and end nodes
/// share a digit and all their cells see each other.
class GroupedNiceLoopStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        return findGroupedNiceLoop(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::GroupedNiceLoop;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Grouped Nice Loop";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::GroupedNiceLoop);
    }

private:
    static constexpr size_t MAX_CHAIN_LEN = 10;

    /// A node is either a single cell or a group of cells, associated with a digit.
    struct GAICNode {
        std::vector<size_t> cells;  // flat indices (row*9 + col)
        int digit;                  // the candidate digit for this node

        [[nodiscard]] bool isSingle() const {
            return cells.size() == 1;
        }

        [[nodiscard]] bool containsCell(size_t cell) const {
            return std::ranges::find(cells, cell) != cells.end();
        }
    };

    [[nodiscard]] static Position indexToPos(size_t idx) {
        return Position{.row = idx / BOARD_SIZE, .col = idx % BOARD_SIZE};
    }

    [[nodiscard]] static size_t cellIndex(size_t row, size_t col) {
        return (row * BOARD_SIZE) + col;
    }

    /// Check if a cell sees all cells in a node.
    [[nodiscard]] static bool cellSeesNode(size_t cell, const GAICNode& node) {
        Position p = indexToPos(cell);
        return std::ranges::all_of(node.cells, [&](size_t c) { return c != cell && sees(p, indexToPos(c)); });
    }

    /// Check if all cells of node A see all cells of node B.
    [[nodiscard]] static bool nodesAllSee(const GAICNode& a, const GAICNode& b) {
        for (size_t ca : a.cells) {
            for (size_t cb : b.cells) {
                if (ca == cb || !sees(indexToPos(ca), indexToPos(cb))) {
                    return false;
                }
            }
        }
        return true;
    }

    /// Check if nodes share any cells.
    [[nodiscard]] static bool nodesOverlap(const GAICNode& a, const GAICNode& b) {
        return std::ranges::any_of(a.cells, [&b](size_t ca) { return b.containsCell(ca); });
    }

    /// Check if nodes share cells AND have the same digit.
    [[nodiscard]] static bool nodesConflict(const GAICNode& a, const GAICNode& b) {
        return a.digit == b.digit && nodesOverlap(a, b);
    }

    /// Build all nodes and links, then search for chains.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — node enumeration + link building + DFS; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findGroupedNiceLoop(const BoardData& board,
                                                                      const CandidateGrid& candidates) {
        // Build nodes: for each digit, single cells + grouped nodes
        std::vector<GAICNode> nodes;
        nodes.reserve(512);

        for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
            // Single cell nodes
            for (size_t r = 0; r < BOARD_SIZE; ++r) {
                for (size_t c = 0; c < BOARD_SIZE; ++c) {
                    if (board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, digit)) {
                        nodes.push_back(GAICNode{.cells = {cellIndex(r, c)}, .digit = digit});
                    }
                }
            }

            // Group nodes: 2-3 cells in same box on same row or column
            for (size_t box = 0; box < BOARD_SIZE; ++box) {
                size_t sr = (box / BOX_SIZE) * BOX_SIZE;
                size_t sc = (box % BOX_SIZE) * BOX_SIZE;

                // Groups by row within box
                for (size_t r = sr; r < sr + BOX_SIZE; ++r) {
                    std::vector<size_t> row_cells;
                    for (size_t c = sc; c < sc + BOX_SIZE; ++c) {
                        if (board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, digit)) {
                            row_cells.push_back(cellIndex(r, c));
                        }
                    }
                    if (row_cells.size() >= 2 && row_cells.size() <= 3) {
                        nodes.push_back(GAICNode{.cells = row_cells, .digit = digit});
                    }
                }

                // Groups by column within box
                for (size_t c = sc; c < sc + BOX_SIZE; ++c) {
                    std::vector<size_t> col_cells;
                    for (size_t r = sr; r < sr + BOX_SIZE; ++r) {
                        if (board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, digit)) {
                            col_cells.push_back(cellIndex(r, c));
                        }
                    }
                    if (col_cells.size() >= 2 && col_cells.size() <= 3) {
                        nodes.push_back(GAICNode{.cells = col_cells, .digit = digit});
                    }
                }
            }
        }

        if (nodes.size() < 4) {
            return std::nullopt;
        }

        // Build strong and weak link adjacency lists
        size_t n = nodes.size();
        std::vector<std::vector<size_t>> strong_adj(n);
        std::vector<std::vector<size_t>> weak_adj(n);

        buildLinks(board, candidates, nodes, strong_adj, weak_adj);

        // DFS from each single-cell node that has strong links (start with strong link)
        for (size_t start = 0; start < n; ++start) {
            if (!nodes[start].isSingle()) {
                continue;  // Start only from single-cell nodes for Type 2
            }
            if (strong_adj[start].empty()) {
                continue;
            }

            for (size_t neighbor : strong_adj[start]) {
                std::vector<size_t> chain = {start, neighbor};
                std::vector<bool> visited(n, false);
                visited[start] = true;
                visited[neighbor] = true;
                auto result = dfs(board, candidates, nodes, start, chain, visited, true, strong_adj, weak_adj);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    /// Build strong and weak links between nodes.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — unit-based + cell-based link enumeration; nesting is inherent
    static void buildLinks(const BoardData& board, const CandidateGrid& candidates, const std::vector<GAICNode>& nodes,
                           std::vector<std::vector<size_t>>& strong_adj, std::vector<std::vector<size_t>>& weak_adj) {
        size_t n = nodes.size();

        // Precompute candidate cells per (digit, unit) for strong link detection
        // unit encoding: row=0..8, col=9..17, box=18..26
        std::array<std::vector<size_t>, size_t{27} * 9> unit_cand_cells{};  // [unit * 9 + (digit-1)]
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                if (board[r][c] != EMPTY_CELL) {
                    continue;
                }
                size_t cell = cellIndex(r, c);
                size_t box = StrategyBase::getBoxIndex(r, c);
                for (int d = MIN_VALUE; d <= MAX_VALUE; ++d) {
                    if (candidates.isAllowed(r, c, d)) {
                        auto di = static_cast<size_t>(d - MIN_VALUE);
                        unit_cand_cells[(r * 9) + di].push_back(cell);
                        unit_cand_cells[((9 + c) * 9) + di].push_back(cell);
                        unit_cand_cells[((18 + box) * 9) + di].push_back(cell);
                    }
                }
            }
        }

        // Precompute sorted cells per node
        std::vector<std::vector<size_t>> node_sorted(n);
        for (size_t i = 0; i < n; ++i) {
            node_sorted[i] = nodes[i].cells;
            std::ranges::sort(node_sorted[i]);
        }

        // Same-digit strong links: two non-overlapping same-digit nodes that cover
        // all candidates for that digit in a shared unit
        for (size_t u = 0; u < 27; ++u) {
            for (int d = MIN_VALUE; d <= MAX_VALUE; ++d) {
                auto di = static_cast<size_t>(d - MIN_VALUE);
                auto& uc = unit_cand_cells[(u * 9) + di];
                if (uc.size() < 2) {
                    continue;
                }

                auto sorted_unit = uc;
                std::ranges::sort(sorted_unit);

                // Find nodes that are subsets of this unit AND have matching digit
                std::vector<size_t> relevant;
                for (size_t i = 0; i < n; ++i) {
                    if (nodes[i].digit != d) {
                        continue;
                    }
                    if (std::ranges::includes(sorted_unit, node_sorted[i])) {
                        relevant.push_back(i);
                    }
                }

                // Check pairs
                for (size_t ri = 0; ri < relevant.size(); ++ri) {
                    for (size_t rj = ri + 1; rj < relevant.size(); ++rj) {
                        size_t ni = relevant[ri];
                        size_t nj = relevant[rj];
                        if (nodesOverlap(nodes[ni], nodes[nj])) {
                            continue;
                        }

                        // Check if their union covers the entire unit
                        std::vector<size_t> combined;
                        combined.reserve(node_sorted[ni].size() + node_sorted[nj].size());
                        std::ranges::merge(node_sorted[ni], node_sorted[nj], std::back_inserter(combined));
                        auto dup = std::ranges::unique(combined);
                        combined.erase(dup.begin(), dup.end());

                        if (combined == sorted_unit) {
                            addUniqueEdge(strong_adj, ni, nj);
                        }
                    }
                }
            }
        }

        // Cell-based strong links: bivalue cell {D1, D2} → strong link between (cell, D1) and (cell, D2)
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                if (board[r][c] != EMPTY_CELL) {
                    continue;
                }
                int count = candidates.countPossibleValues(r, c);
                if (count != 2) {
                    continue;
                }

                size_t cell = cellIndex(r, c);
                // Find the two single-cell nodes for this cell
                std::vector<size_t> cell_nodes;
                for (size_t i = 0; i < n; ++i) {
                    if (nodes[i].isSingle() && nodes[i].cells[0] == cell) {
                        cell_nodes.push_back(i);
                    }
                }
                if (cell_nodes.size() == 2) {
                    addUniqueEdge(strong_adj, cell_nodes[0], cell_nodes[1]);
                }
            }
        }

        // Same-digit weak links: two non-overlapping same-digit nodes where all cells see each other
        // Cell-based weak links: different digits in same cell (for cells with 3+ candidates)
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (nodesOverlap(nodes[i], nodes[j])) {
                    // Same-cell different-digit weak link
                    if (nodes[i].digit != nodes[j].digit && nodes[i].isSingle() && nodes[j].isSingle() &&
                        nodes[i].cells[0] == nodes[j].cells[0]) {
                        addUniqueEdge(weak_adj, i, j);
                    }
                    continue;
                }

                // Same-digit: all cells must see each other
                if (nodes[i].digit == nodes[j].digit && nodesAllSee(nodes[i], nodes[j])) {
                    addUniqueEdge(weak_adj, i, j);
                }
            }
        }
    }

    static void addUniqueEdge(std::vector<std::vector<size_t>>& adj, size_t a, size_t b) {
        if (std::ranges::find(adj[a], b) == adj[a].end()) {
            adj[a].push_back(b);
            adj[b].push_back(a);
        }
    }

    /// Recursive DFS with alternating strong/weak links.
    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — recursive DFS with cycle detection; nesting is inherent
    dfs(const BoardData& board, const CandidateGrid& candidates, const std::vector<GAICNode>& nodes, size_t start,
        std::vector<size_t>& chain, std::vector<bool>& visited, bool last_was_strong,
        const std::vector<std::vector<size_t>>& strong_adj, const std::vector<std::vector<size_t>>& weak_adj) {
        size_t current = chain.back();
        bool next_strong = !last_was_strong;

        // Try to detect Type 2 discontinuous AIC: chain needs strong link at the end
        // The chain started with a strong link (start node is TRUE).
        // After an even number of links (last_was_strong after odd index), we need
        // the current node to connect via strong link to something that forms
        // a Type 2 conclusion with the start node.
        if (next_strong && chain.size() >= 3) {
            // We need a strong link from current to a potential endpoint.
            // Type 2: start and endpoint have same digit, their cells all see each other.
            for (size_t endpoint : strong_adj[current]) {
                if (visited[endpoint] && endpoint != start) {
                    continue;
                }
                if (endpoint == start) {
                    continue;  // Same node — not a Type 2
                }

                const auto& start_node = nodes[start];
                const auto& end_node = nodes[endpoint];

                if (start_node.digit != end_node.digit) {
                    continue;
                }

                // Check that start and end don't share cells (not the same node)
                if (nodesConflict(start_node, end_node)) {
                    continue;
                }

                // All cells in start must see all cells in endpoint
                if (!nodesAllSee(start_node, end_node)) {
                    continue;
                }

                // Find eliminations: cells that see ALL cells in both endpoint nodes
                auto result = findType2Eliminations(board, candidates, nodes, chain, start, endpoint);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        if (chain.size() >= MAX_CHAIN_LEN) {
            return std::nullopt;
        }

        const auto& neighbors = next_strong ? strong_adj[current] : weak_adj[current];
        for (size_t neighbor : neighbors) {
            if (visited[neighbor]) {
                continue;
            }
            chain.push_back(neighbor);
            visited[neighbor] = true;
            auto result = dfs(board, candidates, nodes, start, chain, visited, next_strong, strong_adj, weak_adj);
            if (result.has_value()) {
                return result;
            }
            visited[neighbor] = false;
            chain.pop_back();
        }

        return std::nullopt;
    }

    /// Find Type 2 eliminations: both endpoints assert the same digit.
    /// Eliminate from cells that see ALL cells in both endpoint nodes.
    [[nodiscard]] static std::optional<SolveStep>
    findType2Eliminations(const BoardData& board, const CandidateGrid& candidates, const std::vector<GAICNode>& nodes,
                          const std::vector<size_t>& chain, size_t start_idx, size_t end_idx) {
        const auto& start_node = nodes[start_idx];
        const auto& end_node = nodes[end_idx];
        int digit = start_node.digit;

        // Collect all cells in the chain (to exclude from eliminations)
        std::array<bool, TOTAL_CELLS> in_chain{};
        for (size_t ni : chain) {
            for (size_t cell : nodes[ni].cells) {
                in_chain[cell] = true;
            }
        }
        for (size_t cell : end_node.cells) {
            in_chain[cell] = true;
        }

        std::vector<Elimination> eliminations;
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                size_t cell = cellIndex(r, c);
                if (board[r][c] != EMPTY_CELL || in_chain[cell] || !candidates.isAllowed(r, c, digit)) {
                    continue;
                }
                // Cell must see ALL cells in both start and end nodes
                if (cellSeesNode(cell, start_node) && cellSeesNode(cell, end_node)) {
                    eliminations.push_back(Elimination{.position = Position{.row = r, .col = c}, .value = digit});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto positions = collectPositions(nodes, chain, end_idx);
        Position start_pos = indexToPos(start_node.cells[0]);
        Position end_pos = indexToPos(end_node.cells[0]);

        auto explanation = fmt::format(
            "Grouped Nice Loop: AIC of length {} on digit {} from {} to {} — eliminates {} from cells seeing both "
            "endpoints",
            chain.size() + 1, digit, formatPosition(start_pos), formatPosition(end_pos), digit);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::GroupedNiceLoop,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating =
                             getTechniqueRating(SolvingTechnique::GroupedNiceLoop,
                                                RatingContext{.size_or_length = static_cast<int>(chain.size() + 1)}),
                         .explanation_data = {.positions = {start_pos, end_pos},
                                              .values = {digit, static_cast<int>(chain.size() + 1)},
                                              .position_roles = {CellRole::ChainA, CellRole::ChainB}}};
    }

    /// Collect unique cell positions from chain nodes plus the endpoint.
    [[nodiscard]] static std::vector<Position> collectPositions(const std::vector<GAICNode>& nodes,
                                                                const std::vector<size_t>& chain, size_t end_idx) {
        std::vector<Position> positions;
        std::array<bool, TOTAL_CELLS> seen{};
        for (size_t ni : chain) {
            for (size_t cell : nodes[ni].cells) {
                if (!seen[cell]) {
                    seen[cell] = true;
                    positions.push_back(indexToPos(cell));
                }
            }
        }
        for (size_t cell : nodes[end_idx].cells) {
            if (!seen[cell]) {
                seen[cell] = true;
                positions.push_back(indexToPos(cell));
            }
        }
        return positions;
    }
};

}  // namespace sudoku::core
