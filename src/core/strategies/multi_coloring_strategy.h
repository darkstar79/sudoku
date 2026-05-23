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
#include "coloring_helpers.h"

#include <array>
#include <optional>
#include <queue>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Multi-Coloring patterns (cross-cluster coloring).
/// Extends Simple Coloring by examining relationships between separate connected components.
/// Rule 4 (Wrap): If a color in one cluster sees both colors of another cluster, that color is false.
/// Rule 3 (Trap): If an uncolored cell sees complementary colors from two different clusters,
///                the value can be eliminated from that cell.
class MultiColoringStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto result = findMultiColoringForValue(board, candidates, value);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::MultiColoring;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Multi-Coloring";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::MultiColoring);
    }

private:
    static constexpr int8_t UNVISITED = -1;
    static constexpr int8_t COLOR_A = 0;
    static constexpr int8_t COLOR_B = 1;

    [[nodiscard]] static size_t cellIndex(size_t row, size_t col) {
        return (row * BOARD_SIZE) + col;
    }

    [[nodiscard]] static Position indexToPos(size_t idx) {
        return Position{.row = idx / BOARD_SIZE, .col = idx % BOARD_SIZE};
    }

    struct Component {
        std::vector<size_t> color_a;
        std::vector<size_t> color_b;
    };

    /// Check if any cell in group1 sees any cell in group2
    [[nodiscard]] static bool anySees(const std::vector<size_t>& group1, const std::vector<size_t>& group2) {
        for (size_t a : group1) {
            Position pa = indexToPos(a);
            for (size_t b : group2) {
                if (sees(pa, indexToPos(b))) {
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — BFS cluster building + inter-cluster rule checking; nesting is inherent
    findMultiColoringForValue(const BoardData& board, const CandidateGrid& candidates, int value) {
        // Build conjugate pair graph
        auto adj = ColoringHelpers::buildConjugatePairGraph(board, candidates, value);

        // BFS coloring to find all components
        std::array<int8_t, TOTAL_CELLS> color{};
        color.fill(UNVISITED);
        std::vector<Component> components;

        for (size_t start = 0; start < TOTAL_CELLS; ++start) {
            if (color[start] != UNVISITED || adj[start].empty()) {
                continue;
            }

            Component comp;
            std::queue<size_t> queue;
            color[start] = COLOR_A;
            queue.push(start);
            comp.color_a.push_back(start);

            while (!queue.empty()) {
                size_t curr = queue.front();
                queue.pop();
                int8_t next_color = (color[curr] == COLOR_A) ? COLOR_B : COLOR_A;

                for (size_t neighbor : adj[curr]) {
                    if (color[neighbor] == UNVISITED) {
                        color[neighbor] = next_color;
                        queue.push(neighbor);
                        if (next_color == COLOR_A) {
                            comp.color_a.push_back(neighbor);
                        } else {
                            comp.color_b.push_back(neighbor);
                        }
                    }
                }
            }

            if (comp.color_a.size() + comp.color_b.size() >= 2) {
                components.push_back(std::move(comp));
            }
        }

        if (components.size() < 2) {
            return std::nullopt;
        }

        // Rule 4 (Wrap): color sees both colors of another cluster
        auto wrap_result = checkWrap(candidates, value, components);
        if (wrap_result.has_value()) {
            return wrap_result;
        }

        // Rule 3 (Trap): uncolored cell sees complementary colors from two clusters
        auto trap_result = checkTrap(board, candidates, value, components, color);
        if (trap_result.has_value()) {
            return trap_result;
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::optional<SolveStep> checkWrap(const CandidateGrid& candidates, int value,
                                                            const std::vector<Component>& components) {
        for (size_t i = 0; i < components.size(); ++i) {
            for (size_t j = i + 1; j < components.size(); ++j) {
                bool iA_jA = anySees(components[i].color_a, components[j].color_a);
                bool iA_jB = anySees(components[i].color_a, components[j].color_b);
                bool iB_jA = anySees(components[i].color_b, components[j].color_a);
                bool iB_jB = anySees(components[i].color_b, components[j].color_b);

                // If color_X sees both colors of the other cluster, color_X is false
                if (iA_jA && iA_jB) {
                    return buildWrapStep(candidates, value, components[i].color_a);
                }
                if (iB_jA && iB_jB) {
                    return buildWrapStep(candidates, value, components[i].color_b);
                }
                if (iA_jA && iB_jA) {
                    return buildWrapStep(candidates, value, components[j].color_a);
                }
                if (iA_jB && iB_jB) {
                    return buildWrapStep(candidates, value, components[j].color_b);
                }
            }
        }
        return std::nullopt;
    }

    // CPD-OFF — graph coloring pattern shared with simple_coloring
    [[nodiscard]] static std::optional<SolveStep> buildWrapStep(const CandidateGrid& candidates, int value,
                                                                const std::vector<size_t>& false_color) {
        std::vector<Elimination> eliminations;
        std::vector<Position> positions;
        for (size_t idx : false_color) {
            Position pos = indexToPos(idx);
            if (candidates.isAllowed(pos.row, pos.col, value)) {
                eliminations.push_back(Elimination{.position = pos, .value = value});
            }
            positions.push_back(pos);
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation = fmt::format("Multi-Coloring on {}: color sees both colors of another cluster — eliminates "
                                       "{} from all cells of that color",
                                       value, value);

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::MultiColoring,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::MultiColoring),
            .explanation_data = {.positions = positions,
                                 .values = {value},
                                 .technique_subtype = 0,  // 0 = wrap
                                 .position_roles = std::vector<CellRole>(positions.size(), CellRole::ChainA)}};
    }
    // CPD-ON

    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — checks all uncolored cells against cluster color pairs; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> checkTrap(const BoardData& board, const CandidateGrid& candidates,
                                                            int value, const std::vector<Component>& components,
                                                            const std::array<int8_t, TOTAL_CELLS>& color) {
        // For each pair of clusters with a cross-cluster "sees" link:
        // If color_X (cluster i) sees color_Y (cluster j), they can't both be true.
        // So ~X or ~Y must be true. Uncolored cell seeing both ~X and ~Y → eliminate.
        std::vector<Elimination> eliminations;

        for (size_t i = 0; i < components.size(); ++i) {
            for (size_t j = i + 1; j < components.size(); ++j) {
                // Check all 4 cross-cluster pairs
                struct CrossPair {
                    const std::vector<size_t>& x_group;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
                    const std::vector<size_t>&
                        complement_x;                    // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members) ~X
                    const std::vector<size_t>& y_group;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
                    const std::vector<size_t>&
                        complement_y;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members) ~Y
                };

                std::array<CrossPair, 4> pairs = {{
                    {.x_group = components[i].color_a,
                     .complement_x = components[i].color_b,
                     .y_group = components[j].color_a,
                     .complement_y = components[j].color_b},
                    {.x_group = components[i].color_a,
                     .complement_x = components[i].color_b,
                     .y_group = components[j].color_b,
                     .complement_y = components[j].color_a},
                    {.x_group = components[i].color_b,
                     .complement_x = components[i].color_a,
                     .y_group = components[j].color_a,
                     .complement_y = components[j].color_b},
                    {.x_group = components[i].color_b,
                     .complement_x = components[i].color_a,
                     .y_group = components[j].color_b,
                     .complement_y = components[j].color_a},
                }};

                for (const auto& cp : pairs) {
                    if (!anySees(cp.x_group, cp.y_group)) {
                        continue;
                    }

                    // Uncolored cells seeing both ~X and ~Y
                    for (size_t row = 0; row < BOARD_SIZE; ++row) {
                        for (size_t col = 0; col < BOARD_SIZE; ++col) {
                            size_t idx = cellIndex(row, col);
                            if (board[row][col] != EMPTY_CELL || color[idx] != UNVISITED) {
                                continue;
                            }
                            if (!candidates.isAllowed(row, col, value)) {
                                continue;
                            }

                            Position pos{.row = row, .col = col};
                            bool sees_comp_x = false;
                            for (size_t cx : cp.complement_x) {
                                if (sees(pos, indexToPos(cx))) {
                                    sees_comp_x = true;
                                    break;
                                }
                            }
                            if (!sees_comp_x) {
                                continue;
                            }

                            bool sees_comp_y = false;
                            for (size_t cy : cp.complement_y) {
                                if (sees(pos, indexToPos(cy))) {
                                    sees_comp_y = true;
                                    break;
                                }
                            }
                            if (sees_comp_y) {
                                // Check not already added
                                bool already = false;
                                for (const auto& e : eliminations) {
                                    if (e.position == pos) {
                                        already = true;
                                        break;
                                    }
                                }
                                if (!already) {
                                    eliminations.push_back(Elimination{.position = pos, .value = value});
                                }
                            }
                        }
                    }
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation =
            fmt::format("Multi-Coloring on {}: cell {} sees complementary colors from two clusters — eliminates {}",
                        value, formatPosition(eliminations[0].position), value);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::MultiColoring,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::MultiColoring),
                         .explanation_data = {.positions = {eliminations[0].position},
                                              .values = {value},
                                              .technique_subtype = 1}};  // 1 = trap
    }
};

}  // namespace sudoku::core
