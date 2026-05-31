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

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Grouped X-Cycles patterns.
/// Extends X-Cycles by allowing grouped nodes: 2-3 cells in the same box on the
/// same row or column that all share a candidate digit. A cell "sees" a group if
/// it sees ALL cells in the group. Strong/weak links between cells, groups, and
/// cell-group pairs enable longer alternating chains.
/// Same Type 1/2/3 rules as X-Cycles.
class GroupedXCyclesStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
            auto result = findGroupedXCycle(board, candidates, digit);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::GroupedXCycles;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Grouped X-Cycles";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::GroupedXCycles);
    }

private:
    static constexpr size_t MAX_CHAIN_LEN = 8;

    /// A node is either a single cell or a group of cells.
    struct GNode {
        std::vector<size_t> cells;  // flat indices (row*9+col)

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

    /// Check if a cell sees all cells in a node (for weak links and elimination checks).
    [[nodiscard]] static bool cellSeesNode(size_t cell, const GNode& node) {
        Position p = indexToPos(cell);
        return std::ranges::all_of(node.cells, [&](size_t c) { return c != cell && sees(p, indexToPos(c)); });
    }

    /// Check if all cells of node A see all cells of node B.
    [[nodiscard]] static bool nodesAllSee(const GNode& a, const GNode& b) {
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
    [[nodiscard]] static bool nodesOverlap(const GNode& a, const GNode& b) {
        return std::ranges::any_of(a.cells, [&b](size_t ca) { return b.containsCell(ca); });
    }

    /// Build nodes and links, then search for cycles.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — node enumeration + link building + DFS; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findGroupedXCycle(const BoardData& board,
                                                                    const CandidateGrid& candidates, int digit) {
        // Collect candidate cells for this digit
        std::vector<size_t> cand_cells;
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                if (board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, digit)) {
                    cand_cells.push_back(cellIndex(r, c));
                }
            }
        }

        if (cand_cells.size() < 4) {
            return std::nullopt;
        }

        // Build nodes: single cells + groups
        std::vector<GNode> nodes;
        nodes.reserve(cand_cells.size() + (BOARD_SIZE * BOX_SIZE * 2));

        // Single cell nodes
        for (size_t cell : cand_cells) {
            nodes.push_back(GNode{.cells = {cell}});
        }

        // Group nodes: 2-3 cells in the same box on the same row or column
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
                    nodes.push_back(GNode{.cells = row_cells});
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
                    nodes.push_back(GNode{.cells = col_cells});
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

        // Strong links: two nodes that together cover ALL candidates in a unit,
        // and are non-overlapping
        buildLinks(board, candidates, digit, nodes, cand_cells, strong_adj, weak_adj);

        // DFS from each node that has strong links
        for (size_t start = 0; start < n; ++start) {
            if (strong_adj[start].empty()) {
                continue;
            }

            // Start with strong link
            for (size_t neighbor : strong_adj[start]) {
                std::vector<size_t> chain = {start, neighbor};
                std::vector<bool> visited(n, false);
                visited[start] = true;
                visited[neighbor] = true;
                auto result =
                    dfs(board, candidates, digit, nodes, start, true, chain, visited, true, strong_adj, weak_adj);
                if (result.has_value()) {
                    return result;
                }
            }

            // Start with weak link
            for (size_t neighbor : weak_adj[start]) {
                std::vector<size_t> chain = {start, neighbor};
                std::vector<bool> visited(n, false);
                visited[start] = true;
                visited[neighbor] = true;
                auto result =
                    dfs(board, candidates, digit, nodes, start, false, chain, visited, false, strong_adj, weak_adj);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    /// Build strong and weak links between nodes.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — unit-based link enumeration; nesting is inherent
    static void buildLinks(const BoardData& /*board*/, const CandidateGrid& /*candidates*/, int /*digit*/,
                           const std::vector<GNode>& nodes, const std::vector<size_t>& cand_cells,
                           std::vector<std::vector<size_t>>& strong_adj, std::vector<std::vector<size_t>>& weak_adj) {
        size_t n = nodes.size();

        // For strong links: find pairs of non-overlapping nodes that together
        // cover all candidates in at least one shared unit (row, col, or box).
        // For each unit, find the candidate cells, then check if any pair of nodes covers them exactly.

        // Precompute which cells are in each unit for this digit
        std::array<std::vector<size_t>, 27> unit_cells{};  // 9 rows + 9 cols + 9 boxes
        for (size_t cell : cand_cells) {
            size_t row = cell / BOARD_SIZE;
            size_t col = cell % BOARD_SIZE;
            unit_cells[row].push_back(cell);
            unit_cells[9 + col].push_back(cell);
            unit_cells[18 + getBoxIndex(row, col)].push_back(cell);
        }

        // Precompute which cells each node covers, as a sorted vector
        std::vector<std::vector<size_t>> node_sorted_cells(n);
        for (size_t i = 0; i < n; ++i) {
            node_sorted_cells[i] = nodes[i].cells;
            std::ranges::sort(node_sorted_cells[i]);
        }

        // For each unit, check all node pairs
        for (size_t u = 0; u < 27; ++u) {
            if (unit_cells[u].size() < 2) {
                continue;
            }

            auto sorted_unit = unit_cells[u];
            std::ranges::sort(sorted_unit);

            // Find which nodes are subsets of this unit
            std::vector<size_t> relevant_nodes;
            for (size_t i = 0; i < n; ++i) {
                if (std::ranges::includes(sorted_unit, node_sorted_cells[i])) {
                    relevant_nodes.push_back(i);
                }
            }

            // Check pairs: if two non-overlapping nodes together cover exactly the unit → strong
            for (size_t ri = 0; ri < relevant_nodes.size(); ++ri) {
                for (size_t rj = ri + 1; rj < relevant_nodes.size(); ++rj) {
                    size_t ni = relevant_nodes[ri];
                    size_t nj = relevant_nodes[rj];

                    if (nodesOverlap(nodes[ni], nodes[nj])) {
                        continue;
                    }

                    // Union of their cells
                    std::vector<size_t> combined;
                    combined.reserve(node_sorted_cells[ni].size() + node_sorted_cells[nj].size());
                    std::ranges::merge(node_sorted_cells[ni], node_sorted_cells[nj], std::back_inserter(combined));
                    auto dup = std::ranges::unique(combined);
                    combined.erase(dup.begin(), dup.end());

                    if (combined == sorted_unit) {
                        // Strong link: they cover the entire unit
                        addUniqueEdge(strong_adj, ni, nj);
                    }
                }
            }
        }

        // Weak links: two non-overlapping nodes where all cells see each other
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (nodesOverlap(nodes[i], nodes[j])) {
                    continue;
                }
                if (nodesAllSee(nodes[i], nodes[j])) {
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

    /// Recursive DFS with alternating links.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — recursive DFS with cycle detection; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> dfs(const BoardData& board, const CandidateGrid& candidates,
                                                      int digit, const std::vector<GNode>& nodes, size_t start,
                                                      bool first_link_strong, std::vector<size_t>& chain,
                                                      std::vector<bool>& visited, bool last_was_strong,
                                                      const std::vector<std::vector<size_t>>& strong_adj,
                                                      const std::vector<std::vector<size_t>>& weak_adj) {
        size_t current = chain.back();
        bool next_strong = !last_was_strong;

        // Try to close cycle
        if (chain.size() >= 4) {
            const auto& closing_adj = next_strong ? strong_adj[current] : weak_adj[current];
            bool can_close = std::ranges::find(closing_adj, start) != closing_adj.end();

            if (can_close) {
                if (first_link_strong != next_strong) {
                    auto result = buildType1Step(board, candidates, digit, nodes, chain, first_link_strong);
                    if (result.has_value()) {
                        return result;
                    }
                } else if (!first_link_strong) {
                    auto result = buildType3Step(candidates, digit, nodes, chain);
                    if (result.has_value()) {
                        return result;
                    }
                } else if (first_link_strong) {
                    auto result = buildType2Step(candidates, digit, nodes, chain);
                    if (result.has_value()) {
                        return result;
                    }
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
            auto result = dfs(board, candidates, digit, nodes, start, first_link_strong, chain, visited, next_strong,
                              strong_adj, weak_adj);
            if (result.has_value()) {
                return result;
            }
            visited[neighbor] = false;
            chain.pop_back();
        }

        return std::nullopt;
    }

    /// Type 1: Continuous nice loop — eliminate from cells seeing weak link endpoints.
    [[nodiscard]] static std::optional<SolveStep>
    buildType1Step(const BoardData& board, const CandidateGrid& candidates, int digit, const std::vector<GNode>& nodes,
                   const std::vector<size_t>& chain, bool first_link_strong) {
        size_t cn = chain.size();

        // Identify weak links
        std::vector<std::pair<size_t, size_t>> weak_link_nodes;
        for (size_t i = 0; i < cn; ++i) {
            bool is_strong = ((i % 2) == 0) == first_link_strong;
            if (!is_strong) {
                weak_link_nodes.emplace_back(chain[i], chain[(i + 1) % cn]);
            }
        }

        // Cells in the chain
        std::array<bool, TOTAL_CELLS> in_chain{};
        for (size_t ni : chain) {
            for (size_t cell : nodes[ni].cells) {
                in_chain[cell] = true;
            }
        }

        std::vector<Elimination> eliminations;
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            for (size_t c = 0; c < BOARD_SIZE; ++c) {
                size_t cell = cellIndex(r, c);
                if (board[r][c] != EMPTY_CELL || in_chain[cell] || !candidates.isAllowed(r, c, digit)) {
                    continue;
                }
                for (const auto& [from_ni, to_ni] : weak_link_nodes) {
                    if (cellSeesNode(cell, nodes[from_ni]) && cellSeesNode(cell, nodes[to_ni])) {
                        eliminations.push_back(Elimination{.position = Position{.row = r, .col = c}, .value = digit});
                        break;
                    }
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto positions = collectPositions(nodes, chain);
        auto explanation =
            fmt::format("Grouped X-Cycles on {}: continuous loop — eliminates {} from cells seeing weak link endpoints",
                        digit, digit);

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::GroupedXCycles,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::GroupedXCycles),
            .explanation_data = {.positions = positions,
                                 .values = {digit},
                                 .technique_subtype = 0,
                                 .position_roles = std::vector<CellRole>(positions.size(), CellRole::ChainA)}};
    }

    /// Type 2: Strong-strong discontinuity — place digit at the start node cell.
    [[nodiscard]] static std::optional<SolveStep> buildType2Step(const CandidateGrid& candidates, int digit,
                                                                 const std::vector<GNode>& nodes,
                                                                 const std::vector<size_t>& chain) {
        const auto& start_node = nodes[chain[0]];
        if (!start_node.isSingle()) {
            return std::nullopt;  // A group cannot be placed
        }

        size_t cell = start_node.cells[0];
        Position target = indexToPos(cell);
        if (!candidates.isAllowed(target.row, target.col, digit)) {
            return std::nullopt;
        }

        auto positions = collectPositions(nodes, chain);
        auto explanation = fmt::format("Grouped X-Cycles on {}: strong-strong discontinuity at {} — places {}", digit,
                                       formatPosition(target), digit);

        return SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::GroupedXCycles,
            .position = target,
            .value = digit,
            .eliminations = {},
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::GroupedXCycles),
            .explanation_data = {.positions = positions,
                                 .values = {digit},
                                 .technique_subtype = 1,
                                 .position_roles = std::vector<CellRole>(positions.size(), CellRole::ChainA)}};
    }

    /// Type 3: Weak-weak discontinuity — eliminate digit from start node cells.
    [[nodiscard]] static std::optional<SolveStep> buildType3Step(const CandidateGrid& candidates, int digit,
                                                                 const std::vector<GNode>& nodes,
                                                                 const std::vector<size_t>& chain) {
        const auto& start_node = nodes[chain[0]];
        if (!start_node.isSingle()) {
            return std::nullopt;  // Type 3 only applies to single cells
        }

        size_t cell = start_node.cells[0];
        Position target = indexToPos(cell);
        if (!candidates.isAllowed(target.row, target.col, digit)) {
            return std::nullopt;
        }

        auto positions = collectPositions(nodes, chain);
        auto explanation = fmt::format("Grouped X-Cycles on {}: weak-weak discontinuity at {} — eliminates {}", digit,
                                       formatPosition(target), digit);

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::GroupedXCycles,
            .position = target,
            .value = 0,
            .eliminations = {Elimination{.position = target, .value = digit}},
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::GroupedXCycles),
            .explanation_data = {.positions = positions,
                                 .values = {digit},
                                 .technique_subtype = 2,
                                 .position_roles = std::vector<CellRole>(positions.size(), CellRole::ChainA)}};
    }

    /// Collect unique cell positions from chain nodes.
    [[nodiscard]] static std::vector<Position> collectPositions(const std::vector<GNode>& nodes,
                                                                const std::vector<size_t>& chain) {
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
        return positions;
    }
};

}  // namespace sudoku::core
