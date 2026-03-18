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

/// Strategy for finding Cell Forcing Chains.
/// For each bivalue/trivalue cell, assume each candidate is true and propagate
/// implications (naked singles + hidden singles). If ALL branches agree on a
/// placement or elimination, that deduction is forced.
///
/// Type 1 (Placement): All branches place the same value in the same cell.
/// Type 2 (Elimination): All branches eliminate the same candidate from the same cell.
class ForcingChainStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Try bivalue cells first (most common), then trivalue
        for (int target_count = 2; target_count <= 3; ++target_count) {
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] != EMPTY_CELL) {
                        continue;
                    }
                    if (candidates.countPossibleValues(row, col) != target_count) {
                        continue;
                    }

                    auto cands = getCandidates(candidates, row, col);
                    auto result = tryForcingCell(board, candidates, Position{.row = row, .col = col}, cands);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::ForcingChain;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Forcing Chain";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::ForcingChain);
    }

private:
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
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — propagates hidden singles across rows/cols/boxes; nesting over units is inherent
    [[nodiscard]] static bool propagateHiddenSingles(PropagationState& state) {
        // Map cell index → value for hidden singles. Use 0 = no placement, -1 = conflict.
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

    /// Try forcing chains from a specific cell with given candidates
    [[nodiscard]] static std::optional<SolveStep> tryForcingCell(const BoardData& board,
                                                                 const CandidateGrid& candidates, Position source,
                                                                 const std::vector<int>& cands) {
        size_t source_idx = (source.row * BOARD_SIZE) + source.col;

        // Run propagation for each candidate assumption
        std::vector<PropagationState> branch_states;
        branch_states.reserve(cands.size());

        int contradiction_idx = -1;
        for (size_t ci = 0; ci < cands.size(); ++ci) {
            auto state = initState(board, candidates);
            placeInState(state, source_idx, cands[ci]);
            propagate(state);

            if (state.contradiction || !isStateConsistent(state)) {
                contradiction_idx = static_cast<int>(ci);
                continue;
            }

            branch_states.push_back(state);
        }

        // Contradiction-based deduction: exactly one branch contradicts, others don't.
        // Only trust this if exactly one out of two branches contradicted.
        // Validate: the forced value must still be a candidate in the actual grid.
        if (contradiction_idx >= 0 && cands.size() == 2 && branch_states.size() == 1) {
            int forced_value = cands[1 - static_cast<size_t>(contradiction_idx)];
            if (candidates.isAllowed(source.row, source.col, forced_value)) {
                // Extra validation: run propagation with the forced value and verify no contradiction
                auto verify = initState(board, candidates);
                placeInState(verify, source_idx, forced_value);
                propagate(verify);
                if (!verify.contradiction && isStateConsistent(verify)) {
                    return buildPlacementStep(source, forced_value, source, cands, "contradiction");
                }
            }
        }

        // Need all branches to agree (and at least 2 branches)
        if (branch_states.size() < 2) {
            return std::nullopt;
        }

        // Type 1: All branches place the same value in the same cell
        auto placement = findCommonPlacement(board, branch_states, source);
        if (placement.has_value()) {
            auto [target_pos, value] = *placement;
            // Validate: value must be a candidate at the target position
            if (candidates.isAllowed(target_pos.row, target_pos.col, value)) {
                return buildPlacementStep(target_pos, value, source, cands, "placement");
            }
        }

        // Type 2: All branches eliminate the same candidate from the same cell
        auto elimination = findCommonElimination(board, candidates, branch_states, source);
        if (elimination.has_value()) {
            return elimination;
        }

        return std::nullopt;
    }

    /// Find a cell+value that all branches agree on placing
    [[nodiscard]] static std::optional<std::pair<Position, int>>
    findCommonPlacement(const BoardData& board, const std::vector<PropagationState>& branches, Position source) {
        for (size_t idx = 0; idx < TOTAL_CELLS; ++idx) {
            size_t row = idx / BOARD_SIZE;
            size_t col = idx % BOARD_SIZE;
            if (board[row][col] != EMPTY_CELL) {
                continue;
            }
            if (row == source.row && col == source.col) {
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

    /// Find a candidate that all branches eliminate from a cell
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — scans all cells for common eliminations across branches; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findCommonElimination(const BoardData& board,
                                                                        const CandidateGrid& candidates,
                                                                        const std::vector<PropagationState>& branches,
                                                                        Position source) {
        std::vector<Elimination> eliminations;

        for (size_t idx = 0; idx < TOTAL_CELLS; ++idx) {
            size_t row = idx / BOARD_SIZE;
            size_t col = idx % BOARD_SIZE;
            if (board[row][col] != EMPTY_CELL) {
                continue;
            }
            if (row == source.row && col == source.col) {
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
                        // Cell was filled with a different value — val implicitly eliminated
                        continue;
                    }
                    if (branch.board[idx] == val) {
                        // Cell was filled with this exact value — not eliminated, it was placed!
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

        auto cands = getCandidates(candidates, source.row, source.col);

        auto explanation = fmt::format("Forcing Chain: assuming each candidate {{{}}} in {} — eliminates {} from {}",
                                       fmt::join(cands, ","), formatPosition(source), eliminations[0].value,
                                       formatPosition(eliminations[0].position));

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::ForcingChain,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .points = getTechniquePoints(SolvingTechnique::ForcingChain),
            .explanation_data = {.positions = {source, eliminations[0].position}, .values = {eliminations[0].value}}};
    }

    /// Build a placement step from a forcing chain result
    [[nodiscard]] static SolveStep buildPlacementStep(Position target, int value, Position source,
                                                      const std::vector<int>& cands,
                                                      [[maybe_unused]] const char* reason) {
        auto explanation = fmt::format("Forcing Chain: assuming each candidate {{{}}} in {} — places {} in {}",
                                       fmt::join(cands, ","), formatPosition(source), value, formatPosition(target));

        return SolveStep{.type = SolveStepType::Placement,
                         .technique = SolvingTechnique::ForcingChain,
                         .position = target,
                         .value = value,
                         .eliminations = {},
                         .explanation = explanation,
                         .points = getTechniquePoints(SolvingTechnique::ForcingChain),
                         .explanation_data = {.positions = {source, target}, .values = {value}}};
    }
};

}  // namespace sudoku::core
