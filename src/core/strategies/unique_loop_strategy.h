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
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Unique Loop patterns (Type 1).
/// A Unique Loop is a deadly pattern where 4-6 cells form a loop, each consecutive
/// pair sharing a unit (row/col/box), and all cells contain candidates {A,B}.
/// If all cells had only {A,B}, two solutions would exist.
/// Type 1: exactly one cell has extras beyond {A,B} -> eliminate A,B from that cell.
class UniqueLoopStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — cycle-finding DFS over value pairs with adjacency checks; nesting is inherent
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Try each value pair (A, B) where A < B
        for (int val_a = MIN_VALUE; val_a <= MAX_VALUE; ++val_a) {
            for (int val_b = val_a + 1; val_b <= MAX_VALUE; ++val_b) {
                // Collect all empty cells that have both A and B as candidates
                std::vector<Position> ab_cells;
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    for (size_t col = 0; col < BOARD_SIZE; ++col) {
                        if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, val_a) &&
                            candidates.isAllowed(row, col, val_b)) {
                            ab_cells.push_back(Position{.row = row, .col = col});
                        }
                    }
                }

                // Need at least 4 cells to form a cycle
                if (ab_cells.size() < 4) {
                    continue;
                }

                auto result = findCycleElimination(board, candidates, ab_cells, val_a, val_b);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::UniqueLoop;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Unique Loop";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::UniqueLoop);
    }

private:
    /// Maximum cycle length to search for
    static constexpr size_t kMaxCycleLength = 6;

    /// Checks if two positions share at least one unit (row, column, or box)
    [[nodiscard]] static bool shareUnit(const Position& a, const Position& b) {
        return a.row == b.row || a.col == b.col || sameBox(a, b);
    }

    /// Validates that a cycle forms a valid deadly pattern.
    /// In a deadly pattern, cells alternate between two coloring groups (even/odd indices).
    /// If two same-parity cells share a house, both A/B colorings conflict → not deadly.
    [[nodiscard]] static bool isValidDeadlyPattern(const std::vector<Position>& loop) {
        for (size_t i = 0; i < loop.size(); i += 2) {
            for (size_t j = i + 2; j < loop.size(); j += 2) {
                if (shareUnit(loop[i], loop[j])) {
                    return false;
                }
            }
        }
        for (size_t i = 1; i < loop.size(); i += 2) {
            for (size_t j = i + 2; j < loop.size(); j += 2) {
                if (shareUnit(loop[i], loop[j])) {
                    return false;
                }
            }
        }
        return true;
    }

    /// Validates that every row, column, and box containing loop cells has exactly 2.
    /// A unit with 1 loop cell would see a value conflict after swapping A↔B
    /// (the non-loop cell with the swapped-to value creates a duplicate).
    [[nodiscard]] static bool hasValidUnitPairing(const std::vector<Position>& loop) {
        std::array<int, BOARD_SIZE> row_count{};
        std::array<int, BOARD_SIZE> col_count{};
        std::array<int, BOARD_SIZE> box_count{};
        for (const auto& pos : loop) {
            row_count[pos.row]++;
            col_count[pos.col]++;
            box_count[getBoxIndex(pos.row, pos.col)]++;
        }
        for (size_t i = 0; i < BOARD_SIZE; ++i) {
            if (row_count[i] != 0 && row_count[i] != 2) {
                return false;
            }
            if (col_count[i] != 0 && col_count[i] != 2) {
                return false;
            }
            if (box_count[i] != 0 && box_count[i] != 2) {
                return false;
            }
        }
        return true;
    }

    /// Counts the number of unique boxes spanned by a set of positions
    [[nodiscard]] static size_t countUniqueBoxesInLoop(const std::vector<Position>& loop) {
        std::vector<size_t> boxes;
        boxes.reserve(loop.size());
        for (const auto& pos : loop) {
            boxes.push_back(getBoxIndex(pos.row, pos.col));
        }
        std::ranges::sort(boxes);
        auto last = std::ranges::unique(boxes);
        return static_cast<size_t>(std::ranges::distance(boxes.begin(), last.begin()));
    }

    /// Checks if a cell is bivalue with exactly {A,B}
    [[nodiscard]] static bool isBivalueAB(const CandidateGrid& candidates, const Position& cell, int val_a, int val_b) {
        if (candidates.countPossibleValues(cell.row, cell.col) != 2) {
            return false;
        }
        return candidates.isAllowed(cell.row, cell.col, val_a) && candidates.isAllowed(cell.row, cell.col, val_b);
    }

    /// Find cycles in the adjacency graph of ab_cells and check for Type 1 eliminations
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — DFS cycle search with backtracking; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findCycleElimination(const BoardData& /*board*/,
                                                                       const CandidateGrid& candidates,
                                                                       const std::vector<Position>& ab_cells, int val_a,
                                                                       int val_b) {
        const size_t n = ab_cells.size();

        // Build adjacency list
        std::vector<std::vector<size_t>> adj(n);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (shareUnit(ab_cells[i], ab_cells[j])) {
                    adj[i].push_back(j);
                    adj[j].push_back(i);
                }
            }
        }

        // DFS from each starting node to find simple cycles of length 4-6
        std::vector<size_t> path;
        std::vector<bool> in_path(n, false);

        for (size_t start = 0; start < n; ++start) {
            path.clear();
            std::ranges::fill(in_path, false);
            path.push_back(start);
            in_path[start] = true;

            auto result = dfs(candidates, ab_cells, adj, path, in_path, start, val_a, val_b);
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    /// Recursive DFS to find simple cycles returning to start_node
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — recursive DFS with cycle validation; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> dfs(const CandidateGrid& candidates,
                                                      const std::vector<Position>& ab_cells,
                                                      const std::vector<std::vector<size_t>>& adj,
                                                      std::vector<size_t>& path, std::vector<bool>& in_path,
                                                      size_t start_node, int val_a, int val_b) {
        size_t current = path.back();

        for (size_t neighbor : adj[current]) {
            // If we've found a cycle back to start with sufficient length
            if (neighbor == start_node && path.size() >= 4) {
                // Validate the cycle
                auto result = validateAndBuildStep(candidates, ab_cells, path, val_a, val_b);
                if (result.has_value()) {
                    return result;
                }
                continue;
            }

            // Only visit nodes with index > start to avoid duplicate cycles
            if (neighbor <= start_node) {
                continue;
            }

            // Don't revisit nodes already in the path
            if (in_path[neighbor]) {
                continue;
            }

            // Don't exceed max cycle length
            if (path.size() >= kMaxCycleLength) {
                continue;
            }

            path.push_back(neighbor);
            in_path[neighbor] = true;

            auto result = dfs(candidates, ab_cells, adj, path, in_path, start_node, val_a, val_b);
            if (result.has_value()) {
                return result;
            }

            path.pop_back();
            in_path[neighbor] = false;
        }

        return std::nullopt;
    }

    /// Validate a cycle and build a SolveStep if Type 1 applies
    [[nodiscard]] static std::optional<SolveStep> validateAndBuildStep(const CandidateGrid& candidates,
                                                                       const std::vector<Position>& ab_cells,
                                                                       const std::vector<size_t>& path, int val_a,
                                                                       int val_b) {
        // Extract positions from path indices
        std::vector<Position> loop_positions;
        loop_positions.reserve(path.size());
        for (size_t idx : path) {
            loop_positions.push_back(ab_cells[idx]);
        }

        // Odd-length cycles can't alternate A/B consistently — not a deadly pattern
        if (loop_positions.size() % 2 != 0) {
            return std::nullopt;
        }

        // Every unit containing loop cells must have exactly 2 (one per color group)
        if (!hasValidUnitPairing(loop_positions)) {
            return std::nullopt;
        }

        // Same-parity cells sharing a house means both colorings conflict — not deadly
        if (!isValidDeadlyPattern(loop_positions)) {
            return std::nullopt;
        }

        // Must span at least 2 distinct boxes
        if (countUniqueBoxesInLoop(loop_positions) < 2) {
            return std::nullopt;
        }

        // Classify cells: roof (bivalue {A,B}) vs floor (extras beyond {A,B})
        std::vector<Position> roof;
        std::vector<Position> floor;
        for (const auto& cell : loop_positions) {
            if (isBivalueAB(candidates, cell, val_a, val_b)) {
                roof.push_back(cell);
            } else {
                floor.push_back(cell);
            }
        }

        // Type 1: exactly one floor cell -> eliminate A and B from it
        if (floor.size() != 1) {
            return std::nullopt;
        }

        const auto& floor_cell = floor[0];
        std::vector<Elimination> eliminations;
        if (candidates.isAllowed(floor_cell.row, floor_cell.col, val_a)) {
            eliminations.push_back(Elimination{.position = floor_cell, .value = val_a});
        }
        if (candidates.isAllowed(floor_cell.row, floor_cell.col, val_b)) {
            eliminations.push_back(Elimination{.position = floor_cell, .value = val_b});
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        // Build explanation
        std::string positions_str;
        for (size_t i = 0; i < loop_positions.size(); ++i) {
            if (i > 0) {
                positions_str += ", ";
            }
            positions_str += formatPosition(loop_positions[i]);
        }

        auto explanation = fmt::format(
            "Unique Loop: cells {} with values {{{},{}}} — eliminates {},{} from {} to avoid deadly pattern",
            positions_str, val_a, val_b, val_a, val_b, formatPosition(floor_cell));

        // Build position roles
        std::vector<CellRole> roles;
        roles.reserve(loop_positions.size());
        for (const auto& cell : loop_positions) {
            roles.push_back(isBivalueAB(candidates, cell, val_a, val_b) ? CellRole::Roof : CellRole::Floor);
        }

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::UniqueLoop,
                         .position = floor_cell,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::UniqueLoop),
                         .explanation_data = {.positions = loop_positions,
                                              .values = {val_a, val_b},
                                              .position_roles = std::move(roles)}};
    }
};

}  // namespace sudoku::core
