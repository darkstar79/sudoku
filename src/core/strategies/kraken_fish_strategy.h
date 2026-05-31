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

    /// Rating for a fin-exclusion deduction (vacuous Kraken case): the difficulty of the
    /// underlying finned fish of the same order, NOT the full Kraken rating. When the fin
    /// placement self-contradicts the pattern is just "a finned fish whose fin is dead", so
    /// rating it as a Kraken (8.5) would let the puzzle rater double-count (issue #40).
    [[nodiscard]] static double finnedFishRatingForOrder(size_t order) {
        switch (order) {
            case 3:
                return getTechniqueRating(SolvingTechnique::FinnedSwordfish);
            case 4:
                return getTechniqueRating(SolvingTechnique::FinnedJellyfish);
            default:
                return getTechniqueRating(SolvingTechnique::FinnedXWing);
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

            // Compute extended eliminations (outside fin's box) verified by chain propagation.
            // In-fin-box finned-fish eliminations are intentionally omitted from this step: they
            // are already discoverable by FinnedXWing/Swordfish/Jellyfish (registered earlier),
            // so including them here would misattribute their difficulty rating to Kraken Fish
            // and let the step's `position` point at a non-Kraken cell.
            auto outcome = findKrakenOutcome(board, candidates, value, by_row, base_primaries, info->base_secondaries,
                                             fin_pos, fin_box);

            if (outcome.eliminations.empty()) {
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

            // Vacuous fin contradiction: placing `value` at the fin is itself impossible, so the
            // chain proved nothing about the cover cells — but it proved the strictly simpler fact
            // that the fin cannot hold `value`. Report that single, honest deduction (rated at the
            // underlying finned-fish level) instead of a batch of vacuously "verified" cover
            // eliminations rated as a full Kraken. See issue #40.
            std::string explanation =
                outcome.fin_excluded
                    ? fmt::format("Kraken {} on value {} in {} {} with fin at {} — placing {} at the fin is "
                                  "impossible, so {} is eliminated from the fin cell",
                                  fishOrderName(order), value, orientation, base_list, formatPosition(fin_pos), value,
                                  value)
                    : fmt::format("Kraken {} on value {} in {} {} with fin at {} — eliminates {} via chain "
                                  "verification",
                                  fishOrderName(order), value, orientation, base_list, formatPosition(fin_pos), value);

            double rating = outcome.fin_excluded ? finnedFishRatingForOrder(order)
                                                 : getTechniqueRating(SolvingTechnique::KrakenFish);

            // Build values for explanation_data: [value, base_line1, base_line2, ...]
            std::vector<int> explain_values;
            explain_values.push_back(value);
            for (size_t bp : base_primaries) {
                explain_values.push_back(static_cast<int>(bp + 1));
            }

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::KrakenFish,
                             .position = outcome.eliminations[0].position,
                             .value = 0,
                             .eliminations = std::move(outcome.eliminations),
                             .explanation = explanation,
                             .rating = rating,
                             .explanation_data = {.positions = positions,
                                                  .values = std::move(explain_values),
                                                  // Convention across the fish family ([localized_explanations.h:204]
                                                  // for XWing, mirrored by Swordfish/Jellyfish/Finned*/Sashimi*):
                                                  // `region_type` encodes the *base-axis orientation* of the pattern
                                                  // (Row when bases are rows, Col when bases are columns) — it is not
                                                  // the axis where eliminations land. See gh#39 for the open question
                                                  // of unifying this semantic across all techniques.
                                                  .region_type = by_row ? RegionType::Row : RegionType::Col,
                                                  .position_roles = std::move(roles)}};
        }
        return std::nullopt;
    }

    /// Outcome of analysing a finned pattern's fin placement.
    struct KrakenOutcome {
        /// True when placing `value` at the fin self-contradicts: the fin cannot hold `value`.
        /// `eliminations` then holds the single fin-cell exclusion. Otherwise `eliminations`
        /// holds the chain-verified cover-line eliminations (possibly empty).
        bool fin_excluded{false};
        std::vector<Elimination> eliminations;
    };

    /// Analyse a finned pattern by placing `value` at the fin and propagating ONCE.
    ///
    /// - If the fin placement self-contradicts, the fin cannot hold `value`: return a single
    ///   fin-cell exclusion (`fin_excluded = true`). This is the strictly simpler, honest
    ///   deduction that the old vacuous-truth path concealed by confirming every cover
    ///   elimination instead (issue #40).
    /// - Otherwise sweep the cover-line target cells (in cover lines, NOT in base lines,
    ///   OUTSIDE the fin's box) and keep those the propagated chain eliminates `value` from.
    ///
    /// Propagation is target-independent, so it is hoisted out of the per-target loop (the old
    /// code re-ran it once per target). `primary` iterates the base-axis index of the candidate
    /// cell (row for by_row, col otherwise); base-line indices are skipped so the inner sweep
    /// covers the cover lines.
    [[nodiscard]] static KrakenOutcome findKrakenOutcome(const BoardData& board, const CandidateGrid& candidates,
                                                         int value, bool by_row, std::span<const size_t> base_primaries,
                                                         const std::vector<size_t>& base_secondaries,
                                                         const Position& fin_pos, size_t fin_box) {
        auto state = ForcingChainHelpers::initState(board, candidates);
        size_t fin_idx = (fin_pos.row * BOARD_SIZE) + fin_pos.col;
        ForcingChainHelpers::placeInState(state, fin_idx, value);
        if (!state.contradiction) {
            ForcingChainHelpers::propagate(state);
        }
        if (state.contradiction) {
            return KrakenOutcome{.fin_excluded = true,
                                 .eliminations = {Elimination{.position = fin_pos, .value = value}}};
        }

        auto bit = valueToBit(value);
        std::vector<Elimination> kraken_elims;
        for (size_t primary = 0; primary < BOARD_SIZE; ++primary) {
            if (std::ranges::contains(base_primaries, primary)) {
                continue;
            }
            for (size_t base_sec : base_secondaries) {
                size_t row = by_row ? primary : base_sec;
                size_t col = by_row ? base_sec : primary;

                if (board[row][col] != EMPTY_CELL || !candidates.isAllowed(row, col, value)) {
                    continue;
                }
                // Skip cells in fin's box (those are handled by standard finned eliminations)
                if (getBoxIndex(row, col) == fin_box) {
                    continue;
                }

                // Target loses `value` if it got filled with another value, or `value` was
                // eliminated from its candidates by the (already propagated) fin-chain.
                size_t target_idx = (row * BOARD_SIZE) + col;
                bool eliminated = (state.board[target_idx] != EMPTY_CELL && state.board[target_idx] != value) ||
                                  (state.masks[target_idx] & bit) == 0;
                if (eliminated) {
                    kraken_elims.push_back(Elimination{.position = Position{.row = row, .col = col}, .value = value});
                }
            }
        }
        return KrakenOutcome{.fin_excluded = false, .eliminations = std::move(kraken_elims)};
    }
};

}  // namespace sudoku::core
