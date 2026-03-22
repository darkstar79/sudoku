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

#include <array>
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for detecting BUG (Bivalue Universal Grave) + N (N = 1, 2, or 3).
/// If all empty cells have exactly 2 candidates EXCEPT N cells with 3 candidates,
/// the puzzle is in a BUG+N state. Each trivalue cell's "odd" candidate (the one that
/// appears an odd number of times in its row, column, or box) must be placed.
/// This produces a Placement, not an Elimination.
class BUGStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    // BUG+N restricted to N=1. Parity analysis for N>1 can produce wrong placements
    // on non-genuine BUG states where the bivalue pattern already has a unique solution.
    // Verified: 0 false positives for N=1 vs 18 for N>1 across 15000 test puzzles.
    static constexpr int MAX_TRIVALUE_CELLS = 1;

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — BUG+N scans all cells, builds parity table, and brute-forces placement combinations; nesting is inherent
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        std::vector<Position> trivalue_cells;

        // Phase 1: Scan cells and build global parity table simultaneously.
        // parity[unit_type][unit_index][value] — 0=even, 1=odd candidate count
        // unit_type: 0=row, 1=col, 2=box
        std::array<std::array<std::array<uint8_t, 10>, BOARD_SIZE>, 3> parity{};

        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                int count = candidates.countPossibleValues(row, col);
                if (count == 2) {
                    // expected bivalue — accumulate parity below
                } else if (count == 3) {
                    trivalue_cells.push_back(Position{.row = row, .col = col});
                    if (static_cast<int>(trivalue_cells.size()) > MAX_TRIVALUE_CELLS) {
                        return std::nullopt;
                    }
                } else {
                    return std::nullopt;  // not a BUG state
                }

                size_t box = getBoxIndex(row, col);
                uint16_t mask = candidates.getPossibleValuesMask(row, col);
                for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                    if ((mask & valueToBit(v)) != 0) {
                        parity[0][row][v] ^= 1;
                        parity[1][col][v] ^= 1;
                        parity[2][box][v] ^= 1;
                    }
                }
            }
        }

        if (trivalue_cells.empty() || static_cast<int>(trivalue_cells.size()) > MAX_TRIVALUE_CELLS) {
            return std::nullopt;
        }

        // Phase 2: Find valid extra-value assignment via global parity brute force.
        auto placements = findGlobalPlacements(candidates, trivalue_cells, parity);
        if (!placements.has_value()) {
            return std::nullopt;
        }

        // Phase 3: Build SolveStep from first placement.
        const auto& first = (*placements)[0];
        int n = static_cast<int>(trivalue_cells.size());

        std::string explanation;
        if (n == 1) {
            explanation =
                fmt::format("BUG: all cells bivalue except {} — value {} must be placed to avoid deadly pattern",
                            formatPosition(first.cell), first.value);
        } else {
            std::string cells_str;
            for (size_t i = 0; i < trivalue_cells.size(); ++i) {
                if (i > 0) {
                    cells_str += ", ";
                }
                cells_str += formatPosition(trivalue_cells[i]);
            }
            explanation = fmt::format(
                "BUG+{}: all cells bivalue except {} — value {} must be placed at {} to avoid deadly pattern", n,
                cells_str, first.value, formatPosition(first.cell));
        }

        return SolveStep{.type = SolveStepType::Placement,
                         .technique = SolvingTechnique::BUG,
                         .position = first.cell,
                         .value = first.value,
                         .eliminations = {},
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::BUG),
                         .explanation_data = {.positions = {first.cell}, .values = {first.value}}};
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::BUG;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "BUG";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::BUG);
    }

private:
    struct BugPlacement {
        Position cell;
        int value;
    };

    /// Find valid extra-value assignments for all trivalue cells using global parity analysis.
    /// For each combination of "extras" (one per trivalue cell), checks that removing them
    /// restores even parity in all 27 units. Returns the placements if a valid combo exists.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — brute-force parity check over 3^N combinations with nested validation loops; nesting is inherent
    [[nodiscard]] static std::optional<std::vector<BugPlacement>>
    findGlobalPlacements(const CandidateGrid& candidates, const std::vector<Position>& trivalue_cells,
                         const std::array<std::array<std::array<uint8_t, 10>, BOARD_SIZE>, 3>& parity) {
        const int n = static_cast<int>(trivalue_cells.size());

        // Get candidate values for each trivalue cell
        std::array<std::array<int, 3>, 3> cell_cands{};
        for (int i = 0; i < n; ++i) {
            auto cands = getCandidates(candidates, trivalue_cells[i].row, trivalue_cells[i].col);
            if (static_cast<int>(cands.size()) != 3) {
                return std::nullopt;
            }
            cell_cands[i] = {cands[0], cands[1], cands[2]};
        }

        // Brute force: 3^N combinations (3 for N=1, 9 for N=2, 27 for N=3)
        static constexpr std::array<int, 4> kPow3 = {1, 3, 9, 27};
        const int combos = kPow3[n];

        for (int c = 0; c < combos; ++c) {
            std::array<int, 3> choice = {c % 3, (c / 3) % 3, (c / 9) % 3};

            // Build placements from current choice
            std::vector<BugPlacement> placements;
            placements.reserve(n);
            for (int i = 0; i < n; ++i) {
                placements.push_back({trivalue_cells[i], cell_cands[i][choice[i]]});
            }

            // Check conflicts: no two placements with same value in same unit
            bool conflict = false;
            for (int i = 0; i < n && !conflict; ++i) {
                for (int j = i + 1; j < n && !conflict; ++j) {
                    if (placements[i].value == placements[j].value &&
                        (placements[i].cell.row == placements[j].cell.row ||
                         placements[i].cell.col == placements[j].cell.col ||
                         sameBox(placements[i].cell, placements[j].cell))) {
                        conflict = true;
                    }
                }
            }
            if (conflict) {
                continue;
            }

            // Check parity: after flipping each extra's parity, all must be even
            auto test_parity = parity;
            for (int i = 0; i < n; ++i) {
                int v = placements[i].value;
                size_t r = placements[i].cell.row;
                size_t col = placements[i].cell.col;
                size_t b = getBoxIndex(r, col);
                test_parity[0][r][v] ^= 1;
                test_parity[1][col][v] ^= 1;
                test_parity[2][b][v] ^= 1;
            }

            bool all_even = true;
            for (int ut = 0; ut < 3 && all_even; ++ut) {
                for (size_t ui = 0; ui < BOARD_SIZE && all_even; ++ui) {
                    for (int v = MIN_VALUE; v <= MAX_VALUE && all_even; ++v) {
                        if (test_parity[ut][ui][v] != 0) {
                            all_even = false;
                        }
                    }
                }
            }

            if (all_even) {
                return placements;
            }
        }

        return std::nullopt;
    }
};

}  // namespace sudoku::core
