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

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding X-Cycles (single-digit alternating inference chains).
/// Builds a graph of strong (conjugate pair) and weak (shared unit) links for each digit.
/// Searches for alternating cycles to find:
///   Type 1 (continuous loop): eliminates from cells seeing weak link endpoints
///   Type 2 (strong-strong discontinuity): places the digit
///   Type 3 (weak-weak discontinuity): eliminates from the discontinuity cell
class XCyclesStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
            auto result = findXCycleForDigit(board, candidates, digit);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::XCycles;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "X-Cycles";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::XCycles);
    }

private:
    static constexpr size_t MAX_CHAIN_LEN = 10;

    [[nodiscard]] static size_t cellIndex(size_t row, size_t col) {
        return (row * BOARD_SIZE) + col;
    }

    [[nodiscard]] static Position indexToPos(size_t idx) {
        return Position{.row = idx / BOARD_SIZE, .col = idx % BOARD_SIZE};
    }

    /// Build strong link adjacency list (conjugate pairs in each unit).
    static void buildStrongLinks(const BoardData& board, const CandidateGrid& candidates, int digit,
                                 std::array<std::vector<size_t>, TOTAL_CELLS>& strong_adj) {
        strong_adj = ColoringHelpers::buildConjugatePairGraph(board, candidates, digit);
    }

    /// Build weak link adjacency list (cells sharing a unit with candidate).
    static void buildWeakLinks(const std::vector<size_t>& cand_cells,
                               std::array<std::vector<size_t>, TOTAL_CELLS>& weak_adj) {
        for (size_t i = 0; i < cand_cells.size(); ++i) {
            for (size_t j = i + 1; j < cand_cells.size(); ++j) {
                if (sees(indexToPos(cand_cells[i]), indexToPos(cand_cells[j]))) {
                    weak_adj[cand_cells[i]].push_back(cand_cells[j]);
                    weak_adj[cand_cells[j]].push_back(cand_cells[i]);
                }
            }
        }
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — orchestrates graph building and DFS from each cell; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findXCycleForDigit(const BoardData& board,
                                                                     const CandidateGrid& candidates, int digit) {
        std::vector<size_t> cand_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, digit)) {
                    cand_cells.push_back(cellIndex(row, col));
                }
            }
        }

        if (cand_cells.size() < 4) {
            return std::nullopt;
        }

        std::array<std::vector<size_t>, TOTAL_CELLS> strong_adj{};
        std::array<std::vector<size_t>, TOTAL_CELLS> weak_adj{};
        buildStrongLinks(board, candidates, digit, strong_adj);
        buildWeakLinks(cand_cells, weak_adj);

        // DFS from each cell that has at least one strong link
        for (size_t start : cand_cells) {
            if (strong_adj[start].empty()) {
                continue;
            }

            // Start with strong link (finds Type 1 and Type 2)
            for (size_t neighbor : strong_adj[start]) {
                std::vector<size_t> chain = {start, neighbor};
                std::array<bool, TOTAL_CELLS> visited{};
                visited[start] = true;
                visited[neighbor] = true;
                auto result = dfs(board, candidates, digit, start, true, chain, visited, true, strong_adj, weak_adj);
                if (result.has_value()) {
                    return result;
                }
            }

            // Start with weak link (finds Type 3)
            for (size_t neighbor : weak_adj[start]) {
                std::vector<size_t> chain = {start, neighbor};
                std::array<bool, TOTAL_CELLS> visited{};
                visited[start] = true;
                visited[neighbor] = true;
                auto result = dfs(board, candidates, digit, start, false, chain, visited, false, strong_adj, weak_adj);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    /// Recursive DFS with alternating link constraint.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — recursive DFS with alternating link search; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> dfs(const BoardData& board, const CandidateGrid& candidates,
                                                      int digit, size_t start, bool first_link_strong,
                                                      std::vector<size_t>& chain,
                                                      std::array<bool, TOTAL_CELLS>& visited, bool last_was_strong,
                                                      const std::array<std::vector<size_t>, TOTAL_CELLS>& strong_adj,
                                                      const std::array<std::vector<size_t>, TOTAL_CELLS>& weak_adj) {
        size_t current = chain.back();
        bool next_strong = !last_was_strong;

        // Try to close the cycle back to start (minimum 4 nodes)
        if (chain.size() >= 4) {
            bool can_close = false;
            if (next_strong) {
                for (size_t n : strong_adj[current]) {
                    if (n == start) {
                        can_close = true;
                        break;
                    }
                }
            } else {
                for (size_t n : weak_adj[current]) {
                    if (n == start) {
                        can_close = true;
                        break;
                    }
                }
            }

            if (can_close) {
                if (first_link_strong != next_strong) {
                    // Type 1: continuous nice loop
                    auto result = buildType1Step(board, candidates, digit, chain, first_link_strong);
                    if (result.has_value()) {
                        return result;
                    }
                } else if (first_link_strong) {
                    // Type 2: strong-strong → place
                    auto result = buildType2Step(candidates, digit, chain);
                    if (result.has_value()) {
                        return result;
                    }
                } else {
                    // Type 3: weak-weak → eliminate
                    auto result = buildType3Step(candidates, digit, chain);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }

        if (chain.size() >= MAX_CHAIN_LEN) {
            return std::nullopt;
        }

        // Continue DFS with alternating link type
        const auto& neighbors = next_strong ? strong_adj[current] : weak_adj[current];
        for (size_t neighbor : neighbors) {
            if (visited[neighbor]) {
                continue;
            }
            chain.push_back(neighbor);
            visited[neighbor] = true;
            auto result = dfs(board, candidates, digit, start, first_link_strong, chain, visited, next_strong,
                              strong_adj, weak_adj);
            if (result.has_value()) {
                return result;
            }
            visited[neighbor] = false;
            chain.pop_back();
        }

        return std::nullopt;
    }

    /// Check if a cell sees both endpoints of any weak link.
    [[nodiscard]] static bool seesAnyWeakLink(const Position& pos,
                                              const std::vector<std::pair<size_t, size_t>>& weak_links) {
        return std::ranges::any_of(weak_links, [&pos](const auto& link) {
            return sees(pos, indexToPos(link.first)) && sees(pos, indexToPos(link.second));
        });
    }

    /// Type 1: Continuous nice loop — eliminate digit from external cells seeing
    /// both endpoints of any weak link in the loop.
    [[nodiscard]] static std::optional<SolveStep> buildType1Step(const BoardData& board,
                                                                 const CandidateGrid& candidates, int digit,
                                                                 const std::vector<size_t>& chain,
                                                                 bool first_link_strong) {
        size_t n = chain.size();

        std::vector<std::pair<size_t, size_t>> weak_links;
        for (size_t i = 0; i < n; ++i) {
            bool is_strong = ((i % 2) == 0) == first_link_strong;
            if (!is_strong) {
                weak_links.emplace_back(chain[i], chain[(i + 1) % n]);
            }
        }

        std::array<bool, TOTAL_CELLS> in_chain{};
        for (size_t idx : chain) {
            in_chain[idx] = true;
        }

        std::vector<Elimination> eliminations;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                size_t idx = cellIndex(row, col);
                if (board[row][col] != EMPTY_CELL || in_chain[idx]) {
                    continue;
                }
                if (!candidates.isAllowed(row, col, digit)) {
                    continue;
                }
                Position pos{.row = row, .col = col};
                if (seesAnyWeakLink(pos, weak_links)) {
                    eliminations.push_back(Elimination{.position = pos, .value = digit});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        std::vector<Position> positions;
        positions.reserve(chain.size());
        for (size_t idx : chain) {
            positions.push_back(indexToPos(idx));
        }

        auto explanation =
            fmt::format("X-Cycles on value {}: continuous loop — eliminates {} from cells seeing weak link endpoints",
                        digit, digit);

        std::vector<CellRole> roles;
        roles.reserve(chain.size());
        for (size_t i = 0; i < chain.size(); ++i) {
            roles.push_back((i % 2 == 0) ? CellRole::ChainA : CellRole::ChainB);
        }

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::XCycles,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .points = getTechniquePoints(SolvingTechnique::XCycles),
            .explanation_data = {
                .positions = positions, .values = {digit}, .technique_subtype = 0, .position_roles = std::move(roles)}};
    }

    /// Type 2: Strong-strong discontinuity at chain[0] → place digit there.
    [[nodiscard]] static std::optional<SolveStep> buildType2Step(const CandidateGrid& candidates, int digit,
                                                                 const std::vector<size_t>& chain) {
        Position target = indexToPos(chain[0]);
        if (!candidates.isAllowed(target.row, target.col, digit)) {
            return std::nullopt;
        }

        std::vector<Position> positions;
        positions.reserve(chain.size());
        for (size_t idx : chain) {
            positions.push_back(indexToPos(idx));
        }

        auto explanation = fmt::format("X-Cycles on value {}: strong-strong discontinuity at {} — places {}", digit,
                                       formatPosition(target), digit);

        std::vector<CellRole> roles;
        roles.reserve(chain.size());
        for (size_t i = 0; i < chain.size(); ++i) {
            roles.push_back((i % 2 == 0) ? CellRole::ChainA : CellRole::ChainB);
        }

        return SolveStep{
            .type = SolveStepType::Placement,
            .technique = SolvingTechnique::XCycles,
            .position = target,
            .value = digit,
            .eliminations = {},
            .explanation = explanation,
            .points = getTechniquePoints(SolvingTechnique::XCycles),
            .explanation_data = {
                .positions = positions, .values = {digit}, .technique_subtype = 1, .position_roles = std::move(roles)}};
    }

    /// Type 3: Weak-weak discontinuity at chain[0] → eliminate digit from there.
    [[nodiscard]] static std::optional<SolveStep> buildType3Step(const CandidateGrid& candidates, int digit,
                                                                 const std::vector<size_t>& chain) {
        Position target = indexToPos(chain[0]);
        if (!candidates.isAllowed(target.row, target.col, digit)) {
            return std::nullopt;
        }

        std::vector<Elimination> eliminations;
        eliminations.push_back(Elimination{.position = target, .value = digit});

        std::vector<Position> positions;
        positions.reserve(chain.size());
        for (size_t idx : chain) {
            positions.push_back(indexToPos(idx));
        }

        auto explanation = fmt::format("X-Cycles on value {}: weak-weak discontinuity at {} — eliminates {} from {}",
                                       digit, formatPosition(target), digit, formatPosition(target));

        std::vector<CellRole> roles;
        roles.reserve(chain.size());
        for (size_t i = 0; i < chain.size(); ++i) {
            roles.push_back((i % 2 == 0) ? CellRole::ChainA : CellRole::ChainB);
        }

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::XCycles,
            .position = target,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .points = getTechniquePoints(SolvingTechnique::XCycles),
            .explanation_data = {
                .positions = positions, .values = {digit}, .technique_subtype = 2, .position_roles = std::move(roles)}};
    }
};

}  // namespace sudoku::core
