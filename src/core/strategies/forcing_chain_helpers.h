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

#include <array>
#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace sudoku::core {

/// Shared infrastructure for forcing chain strategies (Cell FC, Unit FC, Region FC).
/// Provides propagation state management, naked/hidden single propagation, and
/// common placement/elimination detection across branches.
/// Inherits StrategyBase for cellIndex(), BOARD_SIZE, etc.
class ForcingChainHelpers : protected StrategyBase {
public:
    static constexpr int MAX_PROPAGATION_ITERATIONS = 20;

    /// Local state for propagation: flat board + flat candidate masks
    struct PropagationState {
        std::array<int, TOTAL_CELLS> board{};
        std::array<uint16_t, TOTAL_CELLS> masks{};
        std::array<uint16_t, TOTAL_CELLS>
            initial_masks{};  ///< Snapshot before propagation (to detect new contradictions)
        bool contradiction{false};
    };

    /// Initialize propagation state from current board + candidates
    [[nodiscard]] static PropagationState initState(const BoardData& board, const CandidateGrid& candidates) {
        PropagationState state;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                size_t idx = (row * BOARD_SIZE) + col;
                state.board[idx] = board[row][col];
                state.masks[idx] = (board[row][col] == EMPTY_CELL) ? candidates.getPossibleValuesMask(row, col)
                                                                   : static_cast<uint16_t>(0);
            }
        }
        state.initial_masks = state.masks;
        return state;
    }

    /// Place a value in the propagation state and eliminate from peers.
    /// Sets contradiction flag if a peer already has this value placed.
    static void placeInState(PropagationState& state, size_t idx, int value) {
        if (state.contradiction) {
            return;
        }

        size_t row = idx / BOARD_SIZE;
        size_t col = idx % BOARD_SIZE;

        // Check for conflict with existing placed values in peers
        for (size_t cc = 0; cc < BOARD_SIZE; ++cc) {
            if (cc != col && state.board[(row * BOARD_SIZE) + cc] == value) {
                state.contradiction = true;
                return;
            }
        }
        for (size_t rr = 0; rr < BOARD_SIZE; ++rr) {
            if (rr != row && state.board[(rr * BOARD_SIZE) + col] == value) {
                state.contradiction = true;
                return;
            }
        }
        size_t box_r = (row / BOX_SIZE) * BOX_SIZE;
        size_t box_c = (col / BOX_SIZE) * BOX_SIZE;
        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t peer = ((box_r + br) * BOARD_SIZE) + (box_c + bc);
                if (peer != idx && state.board[peer] == value) {
                    state.contradiction = true;
                    return;
                }
            }
        }

        state.board[idx] = value;
        state.masks[idx] = 0;

        auto bit = valueToBit(value);

        // Eliminate from same row
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            state.masks[(row * BOARD_SIZE) + c] &= ~bit;
        }
        // Eliminate from same column
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            state.masks[(r * BOARD_SIZE) + col] &= ~bit;
        }
        // Eliminate from same box
        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                state.masks[((box_r + br) * BOARD_SIZE) + (box_c + bc)] &= ~bit;
            }
        }
    }

    /// Propagate naked singles and hidden singles until no more progress
    static void propagate(PropagationState& state) {
        for (int iter = 0; iter < MAX_PROPAGATION_ITERATIONS; ++iter) {
            bool progress = false;

            // Naked singles: cell with exactly 1 candidate
            for (size_t idx = 0; idx < TOTAL_CELLS; ++idx) {
                if (state.board[idx] != EMPTY_CELL) {
                    continue;
                }
                int count = std::popcount(state.masks[idx]);
                if (count == 0) {
                    // Only a real contradiction if this cell originally HAD candidates
                    if (state.initial_masks[idx] != 0) {
                        state.contradiction = true;
                        return;
                    }
                    continue;
                }
                if (count == 1) {
                    int value = std::countr_zero(state.masks[idx]);
                    placeInState(state, idx, value);
                    if (state.contradiction) {
                        return;
                    }
                    progress = true;
                }
            }

            // Hidden singles: value with exactly 1 position in a unit
            progress = propagateHiddenSingles(state) || progress;

            if (state.contradiction || !progress) {
                return;
            }
        }
    }

    /// Find and place hidden singles in rows, columns, and boxes.
    /// Collects placements first, then applies them, to avoid mid-iteration mutation.
    /// If multiple units disagree on what value goes in a cell, skip that cell
    /// (the conflict will be resolved on a later iteration after more propagation).
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) -- propagates hidden singles across rows/cols/boxes; nesting over units is inherent
    [[nodiscard]] static bool propagateHiddenSingles(PropagationState& state) {
        // Map cell index -> value for hidden singles. Use 0 = no placement, -1 = conflict.
        std::array<int, TOTAL_CELLS> cell_placements{};

        auto recordPlacement = [&cell_placements](size_t idx, int val) {
            if (cell_placements[idx] == 0) {
                cell_placements[idx] = val;
            } else if (cell_placements[idx] != val) {
                cell_placements[idx] = -1;  // Conflict: different units want different values
            }
        };

        // Rows
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (int val = MIN_VALUE; val <= MAX_VALUE; ++val) {
                if (isValueInRow(state, row, val)) {
                    continue;
                }
                auto bit = valueToBit(val);
                size_t found_idx = TOTAL_CELLS;
                int count = 0;
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    size_t idx = (row * BOARD_SIZE) + col;
                    if (state.board[idx] == EMPTY_CELL && (state.masks[idx] & bit) != 0) {
                        found_idx = idx;
                        count++;
                        if (count > 1) {
                            break;
                        }
                    }
                }
                if (count == 1) {
                    recordPlacement(found_idx, val);
                }
            }
        }

        // Columns
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            for (int val = MIN_VALUE; val <= MAX_VALUE; ++val) {
                if (isValueInCol(state, col, val)) {
                    continue;
                }
                auto bit = valueToBit(val);
                size_t found_idx = TOTAL_CELLS;
                int count = 0;
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    size_t idx = (row * BOARD_SIZE) + col;
                    if (state.board[idx] == EMPTY_CELL && (state.masks[idx] & bit) != 0) {
                        found_idx = idx;
                        count++;
                        if (count > 1) {
                            break;
                        }
                    }
                }
                if (count == 1) {
                    recordPlacement(found_idx, val);
                }
            }
        }

        // Boxes
        for (size_t box = 0; box < BOARD_SIZE; ++box) {
            size_t box_r = (box / BOX_SIZE) * BOX_SIZE;
            size_t box_c = (box % BOX_SIZE) * BOX_SIZE;
            for (int val = MIN_VALUE; val <= MAX_VALUE; ++val) {
                if (isValueInBox(state, box, val)) {
                    continue;
                }
                auto bit = valueToBit(val);
                size_t found_idx = TOTAL_CELLS;
                int count = 0;
                for (size_t br = 0; br < BOX_SIZE; ++br) {
                    for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                        size_t idx = ((box_r + br) * BOARD_SIZE) + (box_c + bc);
                        if (state.board[idx] == EMPTY_CELL && (state.masks[idx] & bit) != 0) {
                            found_idx = idx;
                            count++;
                            if (count > 1) {
                                break;
                            }
                        }
                    }
                    if (count > 1) {
                        break;
                    }
                }
                if (count == 1) {
                    recordPlacement(found_idx, val);
                }
            }
        }

        // Apply non-conflicting placements (stop if contradiction detected)
        bool any_placed = false;
        for (size_t idx = 0; idx < TOTAL_CELLS; ++idx) {
            if (state.contradiction) {
                break;
            }
            int val = cell_placements[idx];
            if (val <= 0) {
                continue;  // 0 = no placement, -1 = conflict (skip)
            }
            if (state.board[idx] == EMPTY_CELL && (state.masks[idx] & valueToBit(val)) != 0) {
                placeInState(state, idx, val);
                any_placed = true;
            }
        }

        return any_placed;
    }

    /// Check if a value is already placed in a row
    [[nodiscard]] static bool isValueInRow(const PropagationState& state, size_t row, int val) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (state.board[(row * BOARD_SIZE) + col] == val) {
                return true;
            }
        }
        return false;
    }

    /// Check if a value is already placed in a column
    [[nodiscard]] static bool isValueInCol(const PropagationState& state, size_t col, int val) {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            if (state.board[(row * BOARD_SIZE) + col] == val) {
                return true;
            }
        }
        return false;
    }

    /// Check if a value is already placed in a box
    [[nodiscard]] static bool isValueInBox(const PropagationState& state, size_t box, int val) {
        size_t box_r = (box / BOX_SIZE) * BOX_SIZE;
        size_t box_c = (box % BOX_SIZE) * BOX_SIZE;
        for (size_t br = 0; br < BOX_SIZE; ++br) {
            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                if (state.board[((box_r + br) * BOARD_SIZE) + (box_c + bc)] == val) {
                    return true;
                }
            }
        }
        return false;
    }

    /// Validate a propagation state is consistent (no empty cells that should have candidates).
    /// Returns false if the state has latent contradictions not caught during propagation.
    [[nodiscard]] static bool isStateConsistent(const PropagationState& state) {
        for (size_t idx = 0; idx < TOTAL_CELLS; ++idx) {
            if (state.board[idx] == EMPTY_CELL && state.initial_masks[idx] != 0 && state.masks[idx] == 0) {
                return false;  // Cell lost all candidates during propagation
            }
        }
        return true;
    }

    /// Find a cell+value that all branches agree on placing
    [[nodiscard]] static std::optional<std::pair<Position, int>>
    findCommonPlacement(const BoardData& board, const std::vector<PropagationState>& branches,
                        const std::vector<size_t>& skip_indices = {}) {
        for (size_t idx = 0; idx < TOTAL_CELLS; ++idx) {
            size_t row = idx / BOARD_SIZE;
            size_t col = idx % BOARD_SIZE;
            if (board[row][col] != EMPTY_CELL) {
                continue;
            }

            // Skip source cells
            bool should_skip = false;
            for (size_t skip_idx : skip_indices) {
                if (idx == skip_idx) {
                    should_skip = true;
                    break;
                }
            }
            if (should_skip) {
                continue;
            }

            // Check if all branches placed a value here
            int common_value = branches[0].board[idx];
            if (common_value == EMPTY_CELL) {
                continue;
            }

            bool all_agree = true;
            for (size_t b = 1; b < branches.size(); ++b) {
                if (branches[b].board[idx] != common_value) {
                    all_agree = false;
                    break;
                }
            }

            if (all_agree) {
                return std::make_pair(Position{.row = row, .col = col}, common_value);
            }
        }
        return std::nullopt;
    }

    /// Find a candidate that all branches eliminate from a cell.
    /// Returns eliminations with the given technique and source description.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) -- scans all cells for common eliminations across branches; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findCommonElimination(
        const BoardData& board, const CandidateGrid& candidates, const std::vector<PropagationState>& branches,
        SolvingTechnique technique, const std::string& explanation_prefix,
        const std::vector<Position>& explanation_positions = {}, const std::vector<size_t>& skip_indices = {}) {
        std::vector<Elimination> eliminations;

        for (size_t idx = 0; idx < TOTAL_CELLS; ++idx) {
            size_t row = idx / BOARD_SIZE;
            size_t col = idx % BOARD_SIZE;
            if (board[row][col] != EMPTY_CELL) {
                continue;
            }

            // Skip source cells
            bool should_skip = false;
            for (size_t skip_idx : skip_indices) {
                if (idx == skip_idx) {
                    should_skip = true;
                    break;
                }
            }
            if (should_skip) {
                continue;
            }

            uint16_t original_mask = candidates.getPossibleValuesMask(row, col);

            for (int val = MIN_VALUE; val <= MAX_VALUE; ++val) {
                if ((original_mask & valueToBit(val)) == 0) {
                    continue;
                }

                // Check if ALL branches eliminate this candidate
                bool all_eliminate = true;
                for (const auto& branch : branches) {
                    if (branch.board[idx] != 0 && branch.board[idx] != val) {
                        // Cell was filled with a different value -- val implicitly eliminated
                        continue;
                    }
                    if (branch.board[idx] == val) {
                        // Cell was filled with this exact value -- not eliminated, it was placed!
                        all_eliminate = false;
                        break;
                    }
                    if ((branch.masks[idx] & valueToBit(val)) != 0) {
                        all_eliminate = false;
                        break;
                    }
                }

                if (all_eliminate) {
                    // Validate: elimination must not leave the cell with 0 candidates
                    uint16_t remaining = original_mask & ~valueToBit(val);
                    if (remaining != 0) {
                        eliminations.push_back(Elimination{.position = Position{.row = row, .col = col}, .value = val});
                    }
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation = fmt::format("{} eliminates {} from {}", explanation_prefix, eliminations[0].value,
                                       formatPosition(eliminations[0].position));

        std::vector<Position> all_positions = explanation_positions;
        all_positions.push_back(eliminations[0].position);

        std::vector<int> values = {eliminations[0].value};

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = technique,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(technique),
                         .explanation_data = {.positions = all_positions, .values = values}};
    }
};

}  // namespace sudoku::core
