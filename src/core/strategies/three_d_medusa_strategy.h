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

#include <algorithm>
#include <array>
#include <optional>
#include <queue>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding 3D Medusa patterns (multi-digit coloring).
/// Extends Simple Coloring by building a graph on (cell, digit) nodes.
/// Strong links connect:
///   - Conjugate pairs: 2 cells with digit D in a unit → (cellA,D)↔(cellB,D)
///   - Bivalue cells: cell with exactly 2 candidates {A,B} → (cell,A)↔(cell,B)
/// BFS colors each component with 2 alternating colors, then applies 6 rules:
///   Rule 1: Same color twice in a cell → that color is false
///   Rule 2: Same color twice in a unit for same digit → that color is false
///   Rule 3: Uncolored candidate sees same digit in same color → eliminate
///   Rule 4: Uncolored candidate in cell with colored candidate, sees same color elsewhere → eliminate
///   Rule 5: Both colors in a cell → eliminate all uncolored candidates in that cell
///   Rule 6: Uncolored candidate sees both colors for same digit → eliminate
class ThreeDMedusaStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        return findMedusa(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::ThreeDMedusa;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "3D Medusa";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::ThreeDMedusa);
    }

private:
    static constexpr int8_t UNVISITED = -1;
    static constexpr int8_t COLOR_A = 0;
    static constexpr int8_t COLOR_B = 1;
    static constexpr size_t TOTAL_NODES = TOTAL_CELLS * MAX_VALUE;  // 81 * 9 = 729

    /// Node index: (row * 9 + col) * 9 + (digit - 1)
    [[nodiscard]] static size_t nodeIndex(size_t row, size_t col, int digit) {
        return (((row * BOARD_SIZE) + col) * MAX_VALUE) + static_cast<size_t>(digit - 1);
    }

    [[nodiscard]] static size_t nodeRow(size_t idx) {
        return idx / (BOARD_SIZE * MAX_VALUE);
    }

    [[nodiscard]] static size_t nodeCol(size_t idx) {
        return (idx / MAX_VALUE) % BOARD_SIZE;
    }

    [[nodiscard]] static int nodeDigit(size_t idx) {
        return static_cast<int>(idx % MAX_VALUE) + 1;
    }

    [[nodiscard]] static size_t cellOf(size_t idx) {
        return idx / MAX_VALUE;
    }

    /// Build the 3D Medusa graph and search for eliminations.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — multi-digit coloring graph + 6 rules; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findMedusa(const BoardData& board, const CandidateGrid& candidates) {
        // Build adjacency list on (cell, digit) nodes — strong links only
        std::array<std::vector<size_t>, TOTAL_NODES> adj{};

        auto addEdge = [&adj](size_t a, size_t b) {
            adj[a].push_back(b);
            adj[b].push_back(a);
        };

        // Conjugate pairs in rows
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
                std::vector<size_t> cols;
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, digit)) {
                        cols.push_back(col);
                    }
                }
                if (cols.size() == 2) {
                    addEdge(nodeIndex(row, cols[0], digit), nodeIndex(row, cols[1], digit));
                }
            }
        }

        // Conjugate pairs in columns
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
                std::vector<size_t> rows;
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, digit)) {
                        rows.push_back(row);
                    }
                }
                if (rows.size() == 2) {
                    addEdge(nodeIndex(rows[0], col, digit), nodeIndex(rows[1], col, digit));
                }
            }
        }

        // Conjugate pairs in boxes
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            size_t start_row = (box / BOX_SIZE) * BOX_SIZE;
            size_t start_col = (box % BOX_SIZE) * BOX_SIZE;
            for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
                std::vector<size_t> cells;
                for (size_t r = start_row; r < start_row + BOX_SIZE; ++r) {
                    for (size_t c = start_col; c < start_col + BOX_SIZE; ++c) {
                        if (board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, digit)) {
                            cells.push_back(nodeIndex(r, c, digit));
                        }
                    }
                }
                if (cells.size() == 2) {
                    addEdge(cells[0], cells[1]);
                }
            }
        }

        // Bivalue cell links: cell with exactly 2 candidates → strong link between them
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                std::vector<int> cands;
                for (int d = MIN_VALUE; d <= MAX_VALUE; ++d) {
                    if (candidates.isAllowed(row, col, d)) {
                        cands.push_back(d);
                    }
                }
                if (cands.size() == 2) {
                    addEdge(nodeIndex(row, col, cands[0]), nodeIndex(row, col, cands[1]));
                }
            }
        }

        // BFS coloring for each connected component
        std::array<int8_t, TOTAL_NODES> color{};
        color.fill(UNVISITED);

        for (size_t start = 0; start < TOTAL_NODES; ++start) {
            if (color[start] != UNVISITED || adj[start].empty()) {
                continue;
            }

            std::vector<size_t> color_a_nodes;
            std::vector<size_t> color_b_nodes;
            bool is_bipartite = true;

            std::queue<size_t> queue;
            color[start] = COLOR_A;
            queue.push(start);
            color_a_nodes.push_back(start);

            while (!queue.empty()) {
                size_t curr = queue.front();
                queue.pop();
                int8_t next_color = (color[curr] == COLOR_A) ? COLOR_B : COLOR_A;

                for (size_t neighbor : adj[curr]) {
                    if (color[neighbor] == UNVISITED) {
                        color[neighbor] = next_color;
                        queue.push(neighbor);
                        if (next_color == COLOR_A) {
                            color_a_nodes.push_back(neighbor);
                        } else {
                            color_b_nodes.push_back(neighbor);
                        }
                    } else if (color[neighbor] != next_color) {
                        // Odd cycle: graph is not bipartite — 2-coloring is invalid
                        is_bipartite = false;
                    }
                }
            }

            // Need at least 2 nodes
            if (color_a_nodes.size() + color_b_nodes.size() < 3) {
                continue;
            }

            // Try contradiction rules (1 & 2) — if a color is self-contradicting, eliminate it
            auto contradiction = checkContradiction(candidates, color_a_nodes, color_b_nodes);
            if (contradiction.has_value()) {
                return contradiction;
            }

            // Skip elimination rules for non-bipartite components — the 2-coloring is invalid
            if (!is_bipartite) {
                continue;
            }

            // Try elimination rules (3-6) — remove uncolored candidates
            auto elim = checkEliminations(board, candidates, color_a_nodes, color_b_nodes, color);
            if (elim.has_value()) {
                return elim;
            }
        }

        return std::nullopt;
    }

    /// Check contradiction rules: same color twice in a cell (Rule 1) or
    /// same color twice in a unit for same digit (Rule 2).
    [[nodiscard]] static std::optional<SolveStep> checkContradiction(const CandidateGrid& candidates,
                                                                     const std::vector<size_t>& color_a_nodes,
                                                                     const std::vector<size_t>& color_b_nodes) {
        auto result = checkColorContradiction(candidates, color_a_nodes);
        if (result.has_value()) {
            return result;
        }
        return checkColorContradiction(candidates, color_b_nodes);
    }

    /// Check if a single color has contradictions.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — checks same-cell + same-unit contradictions; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> checkColorContradiction(const CandidateGrid& candidates,
                                                                          const std::vector<size_t>& nodes) {
        // Rule 1: Two nodes of same color in the same cell (different digits)
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t j = i + 1; j < nodes.size(); ++j) {
                if (cellOf(nodes[i]) == cellOf(nodes[j])) {
                    return buildContradictionStep(candidates, nodes);
                }
            }
        }

        // Rule 2: Two nodes of same color with same digit that see each other
        for (size_t i = 0; i < nodes.size(); ++i) {
            for (size_t j = i + 1; j < nodes.size(); ++j) {
                if (nodeDigit(nodes[i]) != nodeDigit(nodes[j])) {
                    continue;
                }
                Position pi{.row = nodeRow(nodes[i]), .col = nodeCol(nodes[i])};
                Position pj{.row = nodeRow(nodes[j]), .col = nodeCol(nodes[j])};
                if (sees(pi, pj)) {
                    return buildContradictionStep(candidates, nodes);
                }
            }
        }

        return std::nullopt;
    }

    /// Build step eliminating all candidates of the false color.
    [[nodiscard]] static std::optional<SolveStep> buildContradictionStep(const CandidateGrid& candidates,
                                                                         const std::vector<size_t>& false_nodes) {
        std::vector<Elimination> eliminations;
        std::vector<Position> positions;

        for (size_t node : false_nodes) {
            size_t row = nodeRow(node);
            size_t col = nodeCol(node);
            int digit = nodeDigit(node);
            if (candidates.isAllowed(row, col, digit)) {
                Position pos{.row = row, .col = col};
                eliminations.push_back(Elimination{.position = pos, .value = digit});
                positions.push_back(pos);
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation = fmt::format("3D Medusa: color contradiction — eliminates candidates of false color");

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::ThreeDMedusa,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .points = getTechniquePoints(SolvingTechnique::ThreeDMedusa),
            .explanation_data = {.positions = positions,
                                 .values = {},
                                 .technique_subtype = 0,  // 0 = contradiction
                                 .position_roles = std::vector<CellRole>(positions.size(), CellRole::ChainA)}};
    }

    /// Check elimination rules 3-6 for uncolored candidates.
    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — rules 3-6 checking uncolored candidates; nesting is inherent
    checkEliminations(const BoardData& board, const CandidateGrid& candidates, const std::vector<size_t>& color_a_nodes,
                      const std::vector<size_t>& color_b_nodes, const std::array<int8_t, TOTAL_NODES>& color) {
        std::vector<Elimination> eliminations;

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
                    if (!candidates.isAllowed(row, col, digit)) {
                        continue;
                    }
                    size_t node = nodeIndex(row, col, digit);
                    if (color[node] != UNVISITED) {
                        continue;  // colored node, skip
                    }

                    Position pos{.row = row, .col = col};

                    // Rule 4: Uncolored candidate sees both colors for same digit
                    bool sees_a_same_digit = seesColorWithDigit(pos, digit, color_a_nodes);
                    bool sees_b_same_digit = seesColorWithDigit(pos, digit, color_b_nodes);
                    if (sees_a_same_digit && sees_b_same_digit) {
                        addUniqueElimination(eliminations, pos, digit);
                        continue;
                    }

                    bool cell_has_a = hasCellColorInComponent(row, col, color_a_nodes);
                    bool cell_has_b = hasCellColorInComponent(row, col, color_b_nodes);

                    // Rule 3: Cell has both colors → eliminate all uncolored candidates
                    // If A true: A-candidate fills cell → D eliminated
                    // If B true: B-candidate fills cell → D eliminated
                    if (cell_has_a && cell_has_b) {
                        addUniqueElimination(eliminations, pos, digit);
                        continue;
                    }

                    // Rule 5: Cell has color C, uncolored sees OPPOSITE color for same digit
                    // If C true: cell gets C-value → D eliminated
                    // If !C true: (other,D) with !C is true → D goes there → D eliminated from cell
                    if (cell_has_a && sees_b_same_digit) {
                        addUniqueElimination(eliminations, pos, digit);
                        continue;
                    }
                    if (cell_has_b && sees_a_same_digit) {
                        addUniqueElimination(eliminations, pos, digit);
                        continue;
                    }
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        // Collect chain positions
        std::vector<Position> chain_positions;
        chain_positions.reserve(color_a_nodes.size() + color_b_nodes.size());
        for (size_t node : color_a_nodes) {
            chain_positions.push_back(Position{.row = nodeRow(node), .col = nodeCol(node)});
        }
        for (size_t node : color_b_nodes) {
            chain_positions.push_back(Position{.row = nodeRow(node), .col = nodeCol(node)});
        }

        auto explanation = fmt::format("3D Medusa: eliminates {} from {} via coloring rules", eliminations[0].value,
                                       formatPosition(eliminations[0].position));

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::ThreeDMedusa,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .points = getTechniquePoints(SolvingTechnique::ThreeDMedusa),
                         .explanation_data = {.positions = {eliminations[0].position},
                                              .values = {eliminations[0].value},
                                              .technique_subtype = 1,  // 1 = elimination rule
                                              .position_roles = {CellRole::ChainA}}};
    }

    /// Check if an uncolored candidate sees a node with the given digit in the color set.
    [[nodiscard]] static bool seesColorWithDigit(const Position& pos, int digit,
                                                 const std::vector<size_t>& color_nodes) {
        return std::ranges::any_of(color_nodes, [&](size_t node) {
            return nodeDigit(node) == digit && sees(pos, Position{.row = nodeRow(node), .col = nodeCol(node)});
        });
    }

    /// Check if a cell contains any node in the given component color set.
    /// Only checks nodes from the CURRENT component (not from previous components).
    [[nodiscard]] static bool hasCellColorInComponent(size_t row, size_t col,
                                                      const std::vector<size_t>& component_nodes) {
        size_t cell = (row * BOARD_SIZE) + col;
        return std::ranges::any_of(component_nodes, [cell](size_t node) { return cellOf(node) == cell; });
    }

    /// Add elimination only if not already present.
    static void addUniqueElimination(std::vector<Elimination>& eliminations, const Position& pos, int digit) {
        for (const auto& e : eliminations) {
            if (e.position == pos && e.value == digit) {
                return;
            }
        }
        eliminations.push_back(Elimination{.position = pos, .value = digit});
    }
};

}  // namespace sudoku::core
