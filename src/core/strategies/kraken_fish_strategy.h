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
#include "fish_helpers.h"
#include "forcing_chain_helpers.h"

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Kraken Fish patterns.
/// Extends finned fish by using chain propagation to verify eliminations
/// outside the fin's box. For each finned fish pattern, standard finned
/// eliminations (restricted to fin's box) are "free". Extended eliminations
/// (outside fin's box) are verified: if placing the digit at the fin cell
/// propagates to also eliminate the digit from the target cell, the
/// elimination is valid regardless of whether the fin is true or false.
class KrakenFishStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            // Row-based fish
            auto result = findOrientation(board, candidates, value, true);
            if (result.has_value()) {
                return result;
            }
            // Col-based fish
            result = findOrientation(board, candidates, value, false);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::KrakenFish;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Kraken Fish";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::KrakenFish);
    }

private:
    /// Fish order names for explanation strings
    [[nodiscard]] static std::string_view fishOrderName(size_t order) {
        switch (order) {
            case 2:
                return "X-Wing";
            case 3:
                return "Swordfish";
            case 4:
                return "Jellyfish";
            default:
                return "Fish";
        }
    }

    /// Try all fish orders (2, 3, 4) for a given orientation
    [[nodiscard]] static std::optional<SolveStep>
    findOrientation(const BoardData& board, const CandidateGrid& candidates, int value, bool by_row) {
        auto positions_map = FishHelpers::collectCandidatePositions(board, candidates, value, by_row);

        // Collect eligible base lines (2-5 positions per line, allowing finned patterns)
        std::vector<size_t> eligible;
        for (size_t i = 0; i < BOARD_SIZE; ++i) {
            if (positions_map[i].size() >= 2 && positions_map[i].size() <= 5) {
                eligible.push_back(i);
            }
        }

        // Try each fish order N = 2, 3, 4
        for (size_t order = 2; order <= 4; ++order) {
            auto result = tryOrder(board, candidates, value, by_row, positions_map, eligible, order);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Enumerate base line combinations of size `order` and look for finned patterns
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — fish enumeration with chain verification; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryOrder(const BoardData& board, const CandidateGrid& candidates,
                                                           int value, bool by_row,
                                                           const std::vector<std::vector<size_t>>& positions_map,
                                                           const std::vector<size_t>& eligible, size_t order) {
        if (eligible.size() < order) {
            return std::nullopt;
        }

        // Use iterative combination enumeration
        std::vector<size_t> combo(order);
        return enumerateCombinations(board, candidates, value, by_row, positions_map, eligible, order, combo, 0, 0);
    }

    /// Recursive combination generator for base line selection
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — recursive combination enumeration
    [[nodiscard]] static std::optional<SolveStep>
    enumerateCombinations(const BoardData& board, const CandidateGrid& candidates, int value, bool by_row,
                          const std::vector<std::vector<size_t>>& positions_map, const std::vector<size_t>& eligible,
                          size_t order, std::vector<size_t>& combo, size_t depth, size_t start) {
        if (depth == order) {
            return tryFinnedPattern(board, candidates, value, by_row, positions_map, combo);
        }

        for (size_t i = start; i < eligible.size(); ++i) {
            combo[depth] = eligible[i];
            auto result = enumerateCombinations(board, candidates, value, by_row, positions_map, eligible, order, combo,
                                                depth + 1, i + 1);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Given a set of base lines, check for finned pattern and compute Kraken eliminations
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — fin validation + chain propagation per elimination target
    [[nodiscard]] static std::optional<SolveStep>
    tryFinnedPattern(const BoardData& board, const CandidateGrid& candidates, int value, bool by_row,
                     const std::vector<std::vector<size_t>>& positions_map, const std::vector<size_t>& base_primaries) {
        size_t order = base_primaries.size();

        // Compute union of cover positions
        std::vector<size_t> union_secondaries;
        for (size_t primary : base_primaries) {
            union_secondaries = (union_secondaries.empty())
                                    ? positions_map[primary]
                                    : FishHelpers::indexUnion(union_secondaries, positions_map[primary]);
        }

        // For finned fish: union size must be order + 1
        if (union_secondaries.size() != order + 1) {
            return std::nullopt;
        }

        // Try each potential fin position
        for (size_t fi = 0; fi < union_secondaries.size(); ++fi) {
            auto info = FishHelpers::validateFinnedFish(positions_map, base_primaries, union_secondaries, fi);
            if (!info) {
                continue;
            }

            Position fin_pos = by_row ? Position{.row = info->fin_primary, .col = info->fin_secondary}
                                      : Position{.row = info->fin_secondary, .col = info->fin_primary};
            size_t fin_box = getBoxIndex(fin_pos.row, fin_pos.col);

            // Compute standard finned eliminations (restricted to fin's box)
            auto free_elims = FishHelpers::buildFinnedEliminations(board, candidates, value, base_primaries,
                                                                   info->base_secondaries, fin_box, by_row);

            // Compute extended eliminations (outside fin's box) verified by chain propagation
            auto kraken_elims = findKrakenEliminations(board, candidates, value, by_row, base_primaries,
                                                       info->base_secondaries, fin_pos, fin_box);

            // Combine free + kraken eliminations
            std::vector<Elimination> all_elims;
            all_elims.reserve(free_elims.size() + kraken_elims.size());
            all_elims.insert(all_elims.end(), free_elims.begin(), free_elims.end());
            all_elims.insert(all_elims.end(), kraken_elims.begin(), kraken_elims.end());

            // Only return if we found Kraken eliminations (otherwise a normal finned fish would have found it)
            if (kraken_elims.empty()) {
                continue;
            }

            auto [positions, roles] =
                FishHelpers::buildFinnedPositionsAndRoles(positions_map, base_primaries, fin_pos, by_row);

            std::string orientation = by_row ? "Rows" : "Columns";
            std::string base_list;
            for (size_t i = 0; i < base_primaries.size(); ++i) {
                if (i > 0) {
                    base_list += ", ";
                }
                base_list += std::to_string(base_primaries[i] + 1);
            }

            auto explanation =
                fmt::format("Kraken {} on value {} in {} {} with fin at {} — eliminates {} via chain verification",
                            fishOrderName(order), value, orientation, base_list, formatPosition(fin_pos), value);

            // Build values for explanation_data: [value, base_line1, base_line2, ...]
            std::vector<int> explain_values;
            explain_values.push_back(value);
            for (size_t bp : base_primaries) {
                explain_values.push_back(static_cast<int>(bp + 1));
            }

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::KrakenFish,
                             .position = all_elims[0].position,
                             .value = 0,
                             .eliminations = all_elims,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::KrakenFish),
                             .explanation_data = {.positions = positions,
                                                  .values = std::move(explain_values),
                                                  .region_type = by_row ? RegionType::Row : RegionType::Col,
                                                  .position_roles = std::move(roles)}};
        }
        return std::nullopt;
    }

    /// Find Kraken eliminations: cells in cover lines, outside base lines and outside fin's box,
    /// where chain propagation from the fin cell confirms the elimination.
    [[nodiscard]] static std::vector<Elimination>
    findKrakenEliminations(const BoardData& board, const CandidateGrid& candidates, int value, bool by_row,
                           std::span<const size_t> base_primaries, const std::vector<size_t>& base_secondaries,
                           const Position& fin_pos, size_t fin_box) {
        std::vector<Elimination> kraken_elims;

        // Collect candidate target cells: in cover lines (base_secondaries), NOT in base lines,
        // that have value as candidate, but are OUTSIDE the fin's box
        for (size_t secondary = 0; secondary < BOARD_SIZE; ++secondary) {
            if (std::ranges::contains(base_primaries, secondary)) {
                continue;  // Skip base lines
            }
            for (size_t base_sec : base_secondaries) {
                size_t row = by_row ? secondary : base_sec;
                size_t col = by_row ? base_sec : secondary;

                if (board[row][col] != EMPTY_CELL || !candidates.isAllowed(row, col, value)) {
                    continue;
                }

                // Skip cells in fin's box (those are handled by standard finned eliminations)
                if (getBoxIndex(row, col) == fin_box) {
                    continue;
                }

                // Chain verification: place value at fin cell and propagate
                if (verifyKrakenElimination(board, candidates, value, fin_pos, row, col)) {
                    kraken_elims.push_back(Elimination{.position = Position{.row = row, .col = col}, .value = value});
                }
            }
        }
        return kraken_elims;
    }

    /// Verify a Kraken elimination via chain propagation.
    /// Place value at fin_pos and propagate. If target cell loses value as candidate
    /// (either filled with another value, or value removed from candidates), return true.
    [[nodiscard]] static bool verifyKrakenElimination(const BoardData& board, const CandidateGrid& candidates,
                                                      int value, const Position& fin_pos, size_t target_row,
                                                      size_t target_col) {
        auto state = ForcingChainHelpers::initState(board, candidates);

        size_t fin_idx = (fin_pos.row * BOARD_SIZE) + fin_pos.col;
        ForcingChainHelpers::placeInState(state, fin_idx, value);
        if (state.contradiction) {
            // Contradiction means fin can't be true, so the elimination trivially holds
            // (but this shouldn't normally happen in a valid puzzle state)
            return true;
        }

        ForcingChainHelpers::propagate(state);
        if (state.contradiction) {
            // Propagation led to contradiction — fin assumption is impossible,
            // so the elimination holds vacuously
            return true;
        }

        size_t target_idx = (target_row * BOARD_SIZE) + target_col;

        // Check if target cell got filled with a different value
        if (state.board[target_idx] != EMPTY_CELL && state.board[target_idx] != value) {
            return true;
        }

        // Check if value was eliminated from target cell's candidates
        auto bit = valueToBit(value);
        return (state.masks[target_idx] & bit) == 0;
    }
};

}  // namespace sudoku::core
