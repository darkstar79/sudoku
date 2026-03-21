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
#include "als_helpers.h"

#include <algorithm>
#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding generalized ALS Chain patterns (4-6 ALSs).
/// A chain of N ALSs linked by N-1 distinct restricted commons (RCs):
///   A1 -RC1- A2 -RC2- A3 -RC3- ... -RCn-1- An
/// Value Z common to A1 and An (Z not any RC) can be eliminated from cells
/// that see all Z-cells in both A1 and An.
class ALSChainStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        return findALSChain(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::ALSChain;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "ALS Chain";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::ALSChain);
    }

private:
    /// Maximum chain length (number of ALSs)
    static constexpr size_t kMaxChainLength = 6;
    /// Minimum chain length (4+ extends beyond ALS-XY-Wing which handles 3)
    static constexpr size_t kMinChainLength = 4;

    /// Adjacency entry: which ALS index is connected, and which RC values are available
    struct Adjacency {
        size_t als_index;
        std::vector<int> rc_values;  ///< Restricted common values between the two ALSs
    };

    /// Build adjacency list: for each ALS, list non-overlapping ALSs with at least one RC
    [[nodiscard]] static std::vector<std::vector<Adjacency>> buildAdjacency(const std::vector<ALS>& als_list,
                                                                            const CandidateGrid& candidates) {
        std::vector<std::vector<Adjacency>> adj(als_list.size());

        for (size_t i = 0; i < als_list.size(); ++i) {
            for (size_t j = i + 1; j < als_list.size(); ++j) {
                if (ALSHelpers::sharesCells(als_list[i], als_list[j])) {
                    continue;
                }

                uint16_t common = als_list[i].cand_mask & als_list[j].cand_mask;
                if (common == 0) {
                    continue;
                }

                std::vector<int> rcs;
                for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                    if ((common & (1U << v)) == 0) {
                        continue;
                    }
                    auto i_cells = ALSHelpers::cellsWithValue(candidates, als_list[i], v);
                    auto j_cells = ALSHelpers::cellsWithValue(candidates, als_list[j], v);
                    if (!i_cells.empty() && !j_cells.empty() && ALSHelpers::allSee(i_cells, j_cells)) {
                        rcs.push_back(v);
                    }
                }

                if (!rcs.empty()) {
                    adj[i].push_back(Adjacency{.als_index = j, .rc_values = rcs});
                    adj[j].push_back(Adjacency{.als_index = i, .rc_values = rcs});
                }
            }
        }

        return adj;
    }

    /// DFS state for chain building
    struct ChainNode {
        size_t als_index;
        int rc_from_prev;  ///< RC used to reach this node from previous (-1 for start)
    };

    /// Main search: enumerate ALSs, build adjacency, DFS for chains of length 4-6
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — DFS chain search with RC distinctness and elimination checks; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findALSChain(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        auto als_list = ALSHelpers::enumerateALSs(board, candidates);
        if (als_list.size() < kMinChainLength) {
            return std::nullopt;
        }

        auto adj = buildAdjacency(als_list, candidates);

        // DFS from each ALS as chain start
        for (size_t start = 0; start < als_list.size(); ++start) {
            std::vector<ChainNode> chain;
            chain.push_back(ChainNode{.als_index = start, .rc_from_prev = -1});

            uint16_t used_rcs = 0;
            std::vector<bool> visited(als_list.size(), false);
            visited[start] = true;

            auto result = dfsSearch(board, candidates, als_list, adj, chain, used_rcs, visited);
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    /// Check if an ALS shares any cells with any ALS already in the chain
    [[nodiscard]] static bool sharesAnyCellWithChain(const std::vector<ALS>& als_list,
                                                     const std::vector<ChainNode>& chain, size_t candidate_idx) {
        const auto& candidate = als_list[candidate_idx];
        return std::ranges::any_of(
            chain, [&](const ChainNode& node) { return ALSHelpers::sharesCells(als_list[node.als_index], candidate); });
    }

    /// Recursive DFS to extend the chain
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — recursive DFS with pruning; nesting is inherent to backtracking search
    [[nodiscard]] static std::optional<SolveStep> dfsSearch(const BoardData& board, const CandidateGrid& candidates,
                                                            const std::vector<ALS>& als_list,
                                                            const std::vector<std::vector<Adjacency>>& adj,
                                                            std::vector<ChainNode>& chain, uint16_t used_rcs,
                                                            std::vector<bool>& visited) {
        size_t current_idx = chain.back().als_index;

        // If chain length >= kMinChainLength, try to find eliminations
        if (chain.size() >= kMinChainLength) {
            auto result = tryChainElimination(board, candidates, als_list, chain, used_rcs);
            if (result.has_value()) {
                return result;
            }
        }

        // Don't extend beyond max length
        if (chain.size() >= kMaxChainLength) {
            return std::nullopt;
        }

        // Extend chain via adjacency
        for (const auto& neighbor : adj[current_idx]) {
            if (visited[neighbor.als_index]) {
                continue;
            }

            // Ensure no cell overlap with any ALS already in the chain
            if (sharesAnyCellWithChain(als_list, chain, neighbor.als_index)) {
                continue;
            }

            for (int rc : neighbor.rc_values) {
                // RC must be distinct from all previously used RCs
                if ((used_rcs & (1U << rc)) != 0) {
                    continue;
                }

                // Extend
                chain.push_back(ChainNode{.als_index = neighbor.als_index, .rc_from_prev = rc});
                uint16_t new_used_rcs = used_rcs | static_cast<uint16_t>(1U << rc);
                visited[neighbor.als_index] = true;

                auto result = dfsSearch(board, candidates, als_list, adj, chain, new_used_rcs, visited);
                if (result.has_value()) {
                    return result;
                }

                // Backtrack
                chain.pop_back();
                visited[neighbor.als_index] = false;
            }
        }

        return std::nullopt;
    }

    /// Check if the current chain produces any eliminations
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — elimination search across board; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    tryChainElimination(const BoardData& board, const CandidateGrid& candidates, const std::vector<ALS>& als_list,
                        const std::vector<ChainNode>& chain, uint16_t used_rcs) {
        const auto& first_als = als_list[chain.front().als_index];
        const auto& last_als = als_list[chain.back().als_index];

        // Find values Z common to first and last ALS, excluding all RCs
        uint16_t common = first_als.cand_mask & last_als.cand_mask;
        uint16_t z_mask = common & ~used_rcs;

        if (z_mask == 0) {
            return std::nullopt;
        }

        // Collect all cells in the chain (for excluding from eliminations)
        std::vector<Position> all_chain_cells;
        for (const auto& node : chain) {
            for (const auto& cell : als_list[node.als_index].cells) {
                all_chain_cells.push_back(cell);
            }
        }

        for (int z = MIN_VALUE; z <= MAX_VALUE; ++z) {
            if ((z_mask & (1U << z)) == 0) {
                continue;
            }

            auto first_z_cells = ALSHelpers::cellsWithValue(candidates, first_als, z);
            auto last_z_cells = ALSHelpers::cellsWithValue(candidates, last_als, z);
            if (first_z_cells.empty() || last_z_cells.empty()) {
                continue;
            }

            // Find elimination targets
            std::vector<Elimination> eliminations;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] != EMPTY_CELL) {
                        continue;
                    }
                    if (!candidates.isAllowed(row, col, z)) {
                        continue;
                    }
                    Position pos{.row = row, .col = col};

                    // Skip cells in any ALS in the chain
                    bool in_chain =
                        std::ranges::any_of(all_chain_cells, [&pos](const Position& c) { return c == pos; });
                    if (in_chain) {
                        continue;
                    }

                    // Must see all Z-cells in first ALS
                    bool sees_all_first =
                        std::ranges::all_of(first_z_cells, [&pos](const Position& c) { return sees(pos, c); });
                    if (!sees_all_first) {
                        continue;
                    }

                    // Must see all Z-cells in last ALS
                    bool sees_all_last =
                        std::ranges::all_of(last_z_cells, [&pos](const Position& c) { return sees(pos, c); });
                    if (sees_all_last) {
                        eliminations.push_back(Elimination{.position = pos, .value = z});
                    }
                }
            }

            if (!eliminations.empty()) {
                return buildStep(als_list, chain, used_rcs, z, eliminations);
            }
        }

        return std::nullopt;
    }

    /// Build the SolveStep result from a successful chain
    [[nodiscard]] static SolveStep buildStep(const std::vector<ALS>& als_list, const std::vector<ChainNode>& chain,
                                             uint16_t /*used_rcs*/, int val_z,
                                             const std::vector<Elimination>& eliminations) {
        std::vector<Position> positions;
        std::vector<CellRole> roles;
        std::vector<int> values;

        // Alternate between SetA, SetB, SetC roles cyclically
        static constexpr std::array<CellRole, 3> kRoles = {CellRole::SetA, CellRole::SetB, CellRole::SetC};

        for (size_t i = 0; i < chain.size(); ++i) {
            auto role = kRoles[i % kRoles.size()];
            for (const auto& cell : als_list[chain[i].als_index].cells) {
                positions.push_back(cell);
                roles.push_back(role);
            }
        }

        // Collect RC values in chain order
        std::vector<int> rc_list;
        for (size_t i = 1; i < chain.size(); ++i) {
            rc_list.push_back(chain[i].rc_from_prev);
        }

        // Format RC chain description
        std::string rc_desc;
        for (size_t i = 0; i < rc_list.size(); ++i) {
            if (i > 0) {
                rc_desc += ", ";
            }
            rc_desc += fmt::format("RC{}={}", i + 1, rc_list[i]);
        }

        auto explanation = fmt::format("ALS Chain ({} ALSs): linked by {} — eliminates {} from cells seeing "
                                       "Z-cells in first and last ALS",
                                       chain.size(), rc_desc, val_z);

        // values: [rc1, rc2, ..., rcN-1, z, chain_length]
        values = rc_list;
        values.push_back(val_z);
        values.push_back(static_cast<int>(chain.size()));

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::ALSChain,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::ALSChain),
                         .explanation_data = {
                             .positions = positions, .values = std::move(values), .position_roles = std::move(roles)}};
    }
};

}  // namespace sudoku::core
