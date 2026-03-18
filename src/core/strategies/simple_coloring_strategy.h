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
#include "coloring_helpers.h"

#include <array>
#include <optional>
#include <queue>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Simple Coloring (single-digit coloring) patterns
/// For a single candidate value, build chains of conjugate pairs and assign
/// alternating colors. Two rules produce eliminations:
/// Rule 1 (Contradiction): If two same-color cells see each other, that color is
///         false — eliminate the value from all cells of that color.
/// Rule 2 (Exclusion): If an outside cell sees cells of both colors, eliminate
///         the value from that cell.
class SimpleColoringStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            auto result = findColoringForValue(board, candidates, value);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::SimpleColoring;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Simple Coloring";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::SimpleColoring);
    }

private:
    static constexpr int8_t UNVISITED = -1;
    static constexpr int8_t COLOR_A = 0;
    static constexpr int8_t COLOR_B = 1;

    /// Flat index for a 9x9 grid
    [[nodiscard]] static size_t cellIndex(size_t row, size_t col) {
        return (row * BOARD_SIZE) + col;
    }

    [[nodiscard]] static Position indexToPos(size_t idx) {
        return Position{.row = idx / BOARD_SIZE, .col = idx % BOARD_SIZE};
    }

    /// Build conjugate pair graph and try coloring for a single value
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — BFS coloring + contradiction/exclusion rule scanning; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findColoringForValue(const BoardData& board,
                                                                       const CandidateGrid& candidates, int value) {
        // Build adjacency list: edges between cells that form conjugate pairs
        auto adj = ColoringHelpers::buildConjugatePairGraph(board, candidates, value);

        // BFS coloring for each connected component
        std::array<int8_t, TOTAL_CELLS> color{};
        color.fill(UNVISITED);

        for (size_t start = 0; start < TOTAL_CELLS; ++start) {
            if (color[start] != UNVISITED || adj[start].empty()) {
                continue;
            }

            // BFS to color this component
            std::vector<size_t> color_a_cells;
            std::vector<size_t> color_b_cells;

            std::queue<size_t> queue;
            color[start] = COLOR_A;
            queue.push(start);
            color_a_cells.push_back(start);

            while (!queue.empty()) {
                size_t curr = queue.front();
                queue.pop();
                int8_t next_color = (color[curr] == COLOR_A) ? COLOR_B : COLOR_A;

                for (size_t neighbor : adj[curr]) {
                    if (color[neighbor] == UNVISITED) {
                        color[neighbor] = next_color;
                        queue.push(neighbor);
                        if (next_color == COLOR_A) {
                            color_a_cells.push_back(neighbor);
                        } else {
                            color_b_cells.push_back(neighbor);
                        }
                    }
                }
            }

            // Need at least 2 cells in the chain for meaningful coloring
            if (color_a_cells.size() + color_b_cells.size() < 4) {
                continue;
            }

            // Rule 1: Contradiction — two same-color cells see each other
            auto contradiction_result =
                checkContradiction(board, candidates, value, color_a_cells, color_b_cells, color);
            if (contradiction_result.has_value()) {
                return contradiction_result;
            }

            // Rule 2: Exclusion — outside cell sees both colors
            auto exclusion_result = checkExclusion(board, candidates, value, color_a_cells, color_b_cells, color);
            if (exclusion_result.has_value()) {
                return exclusion_result;
            }
        }

        return std::nullopt;
    }

    /// Rule 1: If two cells of the same color see each other, that color is false.
    /// Eliminate value from all cells of that color.
    [[nodiscard]] static std::optional<SolveStep> checkContradiction(const BoardData& /*board*/,
                                                                     const CandidateGrid& candidates, int value,
                                                                     const std::vector<size_t>& color_a_cells,
                                                                     const std::vector<size_t>& color_b_cells,
                                                                     const std::array<int8_t, TOTAL_CELLS>& /*color*/) {
        // Check color A for contradiction
        for (size_t i = 0; i < color_a_cells.size(); ++i) {
            for (size_t j = i + 1; j < color_a_cells.size(); ++j) {
                Position pi = indexToPos(color_a_cells[i]);
                Position pj = indexToPos(color_a_cells[j]);
                if (sees(pi, pj)) {
                    // Color A is false — eliminate value from all color A cells
                    return buildContradictionStep(candidates, value, color_a_cells);
                }
            }
        }

        // Check color B for contradiction
        for (size_t i = 0; i < color_b_cells.size(); ++i) {
            for (size_t j = i + 1; j < color_b_cells.size(); ++j) {
                Position pi = indexToPos(color_b_cells[i]);
                Position pj = indexToPos(color_b_cells[j]);
                if (sees(pi, pj)) {
                    return buildContradictionStep(candidates, value, color_b_cells);
                }
            }
        }

        return std::nullopt;
    }

    // CPD-OFF — graph coloring pattern shared with multi_coloring
    [[nodiscard]] static std::optional<SolveStep> buildContradictionStep(const CandidateGrid& candidates, int value,
                                                                         const std::vector<size_t>& false_color_cells) {
        std::vector<Elimination> eliminations;
        std::vector<Position> positions;
        for (size_t idx : false_color_cells) {
            Position pos = indexToPos(idx);
            if (candidates.isAllowed(pos.row, pos.col, value)) {
                eliminations.push_back(Elimination{.position = pos, .value = value});
            }
            positions.push_back(pos);
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation = fmt::format(
            "Simple Coloring on {}: same-color cells see each other — eliminates {} from all cells of that color",
            value, value);

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::SimpleColoring,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::SimpleColoring),
            .explanation_data = {.positions = positions,
                                 .values = {value},
                                 .technique_subtype = 0,  // 0 = contradiction
                                 .position_roles = std::vector<CellRole>(positions.size(), CellRole::ChainA)}};
    }
    // CPD-ON

    /// Rule 2: An outside cell that sees cells of both colors can have the value eliminated.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — scans all uncolored cells for color exclusion; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> checkExclusion(const BoardData& board,
                                                                 const CandidateGrid& candidates, int value,
                                                                 const std::vector<size_t>& color_a_cells,
                                                                 const std::vector<size_t>& color_b_cells,
                                                                 const std::array<int8_t, TOTAL_CELLS>& color) {
        std::vector<Elimination> eliminations;
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
                bool sees_a = false;
                bool sees_b = false;

                for (size_t a_idx : color_a_cells) {
                    if (sees(pos, indexToPos(a_idx))) {
                        sees_a = true;
                        break;
                    }
                }
                for (size_t b_idx : color_b_cells) {
                    if (sees(pos, indexToPos(b_idx))) {
                        sees_b = true;
                        break;
                    }
                }

                if (sees_a && sees_b) {
                    eliminations.push_back(Elimination{.position = pos, .value = value});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        // Collect all chain positions for explanation
        std::vector<Position> chain_positions;
        chain_positions.reserve(color_a_cells.size() + color_b_cells.size());
        for (size_t idx : color_a_cells) {
            chain_positions.push_back(indexToPos(idx));
        }
        for (size_t idx : color_b_cells) {
            chain_positions.push_back(indexToPos(idx));
        }

        auto explanation = fmt::format("Simple Coloring on {}: cell {} sees both colors — eliminates {}", value,
                                       formatPosition(eliminations[0].position), value);

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::SimpleColoring,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::SimpleColoring),
                         .explanation_data = {.positions = {eliminations[0].position},
                                              .values = {value},
                                              .technique_subtype = 1,  // 1 = exclusion
                                              .position_roles = {CellRole::ChainA}}};
    }
};

}  // namespace sudoku::core
