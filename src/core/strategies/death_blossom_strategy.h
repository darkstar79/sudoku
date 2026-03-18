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

/// Strategy for finding Death Blossom patterns.
/// A stem cell S with 2-3 candidates has one ALS petal for each candidate:
///   - Each petal ALS contains that candidate as a restricted common with S
///     (S sees all cells in the ALS that have that candidate)
///   - All petals are distinct (no shared cells, and don't contain the stem)
///   - Value Z common to all petals (Z ∉ stem candidates) → eliminate Z from
///     cells seeing all Z-cells in all petals
class DeathBlossomStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        return findDeathBlossom(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::DeathBlossom;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Death Blossom";
    }

    [[nodiscard]] int getDifficultyPoints() const override {
        return getTechniquePoints(SolvingTechnique::DeathBlossom);
    }

private:
    /// Main search: find stem cells and matching petals.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — stem+petal enumeration; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findDeathBlossom(const BoardData& board,
                                                                   const CandidateGrid& candidates) {
        auto all_als = ALSHelpers::enumerateALSs(board, candidates);
        if (all_als.size() < 2) {
            return std::nullopt;
        }

        // Find stem cells (2-3 candidates)
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                uint16_t stem_mask = candidates.getPossibleValuesMask(row, col);
                int stem_count = std::popcount(stem_mask);
                if (stem_count < 2 || stem_count > 3) {
                    continue;
                }

                Position stem{.row = row, .col = col};
                auto result = tryStemmWithPetals(board, candidates, stem, stem_mask, all_als);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    /// Try to find petals for a given stem cell.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — petal combination search; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryStemmWithPetals(const BoardData& board,
                                                                     const CandidateGrid& candidates,
                                                                     const Position& stem, uint16_t stem_mask,
                                                                     const std::vector<ALS>& all_als) {
        // Get stem candidates
        std::vector<int> stem_cands;
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((stem_mask & (1U << v)) != 0) {
                stem_cands.push_back(v);
            }
        }

        // For each stem candidate, find valid petal ALSs
        // A petal for candidate V: ALS contains V, and stem sees all V-cells in the ALS
        std::vector<std::vector<size_t>> petals_by_cand(stem_cands.size());

        for (size_t ci = 0; ci < stem_cands.size(); ++ci) {
            int val = stem_cands[ci];
            for (size_t ai = 0; ai < all_als.size(); ++ai) {
                const auto& als = all_als[ai];
                if ((als.cand_mask & (1U << val)) == 0) {
                    continue;
                }

                // Stem must not be in the ALS
                bool stem_in_als = false;
                for (const auto& p : als.cells) {
                    if (p == stem) {
                        stem_in_als = true;
                        break;
                    }
                }
                if (stem_in_als) {
                    continue;
                }

                // Restricted common: stem sees all V-cells in ALS
                auto v_cells = ALSHelpers::cellsWithValue(candidates, als, val);
                bool restricted = true;
                for (const auto& vc : v_cells) {
                    if (!sees(stem, vc)) {
                        restricted = false;
                        break;
                    }
                }
                if (restricted) {
                    petals_by_cand[ci].push_back(ai);
                }
            }
        }

        // Check each candidate has at least one petal
        for (const auto& pc : petals_by_cand) {
            if (pc.empty()) {
                return std::nullopt;
            }
        }

        // Enumerate petal combinations (one per candidate, non-overlapping)
        if (stem_cands.size() == 2) {
            return tryPetalCombination2(board, candidates, stem, stem_mask, all_als, stem_cands, petals_by_cand);
        }
        return tryPetalCombination3(board, candidates, stem, stem_mask, all_als, stem_cands, petals_by_cand);
    }

    // CPD-OFF — different loop nesting depths
    /// Try 2-petal combinations.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — 2-petal combination search; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    tryPetalCombination2(const BoardData& board, const CandidateGrid& candidates, const Position& stem,
                         uint16_t stem_mask, const std::vector<ALS>& all_als, const std::vector<int>& stem_cands,
                         const std::vector<std::vector<size_t>>& petals_by_cand) {
        for (size_t p0 : petals_by_cand[0]) {
            for (size_t p1 : petals_by_cand[1]) {
                if (p0 == p1 || ALSHelpers::sharesCells(all_als[p0], all_als[p1])) {
                    continue;
                }
                std::vector<const ALS*> petals = {&all_als[p0], &all_als[p1]};
                auto result = checkCommonElimination(board, candidates, stem, stem_mask, petals, stem_cands);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    /// Try 3-petal combinations.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — 3-petal combination search; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    tryPetalCombination3(const BoardData& board, const CandidateGrid& candidates, const Position& stem,
                         uint16_t stem_mask, const std::vector<ALS>& all_als, const std::vector<int>& stem_cands,
                         const std::vector<std::vector<size_t>>& petals_by_cand) {
        for (size_t p0 : petals_by_cand[0]) {
            for (size_t p1 : petals_by_cand[1]) {
                if (p0 == p1 || ALSHelpers::sharesCells(all_als[p0], all_als[p1])) {
                    continue;
                }
                for (size_t p2 : petals_by_cand[2]) {
                    if (p2 == p0 || p2 == p1 || ALSHelpers::sharesCells(all_als[p0], all_als[p2]) ||
                        ALSHelpers::sharesCells(all_als[p1], all_als[p2])) {
                        continue;
                    }
                    std::vector<const ALS*> petals = {&all_als[p0], &all_als[p1], &all_als[p2]};
                    auto result = checkCommonElimination(board, candidates, stem, stem_mask, petals, stem_cands);
                    if (result.has_value()) {
                        return result;
                    }
                }
            }
        }
        return std::nullopt;
    }
    // CPD-ON

    /// Check for common value Z across all petals and compute eliminations.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — Z-value search + elimination check; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> checkCommonElimination(const BoardData& board,
                                                                         const CandidateGrid& candidates,
                                                                         const Position& stem, uint16_t stem_mask,
                                                                         const std::vector<const ALS*>& petals,
                                                                         const std::vector<int>& stem_cands) {
        // Common candidate mask across all petals
        uint16_t common = petals[0]->cand_mask;
        for (size_t i = 1; i < petals.size(); ++i) {
            common &= petals[i]->cand_mask;
        }
        // Z must not be a stem candidate
        uint16_t z_candidates = common & ~stem_mask;
        if (z_candidates == 0) {
            return std::nullopt;
        }

        for (int z = MIN_VALUE; z <= MAX_VALUE; ++z) {
            if ((z_candidates & (1U << z)) == 0) {
                continue;
            }

            // Collect all Z-cells from all petals
            std::vector<Position> all_z_cells;
            for (const auto* petal : petals) {
                auto z_cells = ALSHelpers::cellsWithValue(candidates, *petal, z);
                all_z_cells.insert(all_z_cells.end(), z_cells.begin(), z_cells.end());
            }

            if (all_z_cells.empty()) {
                continue;
            }

            // Collect all petal cells for exclusion
            auto isInPetalOrStem = [&](size_t row, size_t col) {
                Position p{.row = row, .col = col};
                if (p == stem) {
                    return true;
                }
                for (const auto* petal : petals) {
                    for (const auto& c : petal->cells) {
                        if (c == p) {
                            return true;
                        }
                    }
                }
                return false;
            };

            std::vector<Elimination> eliminations;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] != EMPTY_CELL || isInPetalOrStem(row, col)) {
                        continue;
                    }
                    if (!candidates.isAllowed(row, col, z)) {
                        continue;
                    }

                    Position p{.row = row, .col = col};
                    bool sees_all = true;
                    for (const auto& zc : all_z_cells) {
                        if (!sees(p, zc)) {
                            sees_all = false;
                            break;
                        }
                    }
                    if (sees_all) {
                        eliminations.push_back(Elimination{.position = p, .value = z});
                    }
                }
            }

            if (!eliminations.empty()) {
                return buildStep(stem, petals, stem_cands, z, eliminations);
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] static SolveStep buildStep(const Position& stem, const std::vector<const ALS*>& petals,
                                             const std::vector<int>& stem_cands, int val_z,
                                             const std::vector<Elimination>& eliminations) {
        std::vector<Position> positions;
        std::vector<CellRole> roles;

        positions.push_back(stem);
        roles.push_back(CellRole::Pivot);

        std::array<CellRole, 3> petal_roles = {CellRole::SetA, CellRole::SetB, CellRole::SetC};
        for (size_t i = 0; i < petals.size(); ++i) {
            for (const auto& p : petals[i]->cells) {
                positions.push_back(p);
                roles.push_back(petal_roles[i]);
            }
        }

        std::string stem_str;
        for (size_t i = 0; i < stem_cands.size(); ++i) {
            if (i > 0) {
                stem_str += ",";
            }
            stem_str += std::to_string(stem_cands[i]);
        }

        auto explanation = fmt::format("Death Blossom: stem {} {{{}}} with {} petal(s) — eliminates {} from {} cell(s)",
                                       formatPosition(stem), stem_str, petals.size(), val_z, eliminations.size());

        std::vector<int> values(stem_cands.begin(), stem_cands.end());
        values.push_back(val_z);

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::DeathBlossom,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .points = getTechniquePoints(SolvingTechnique::DeathBlossom),
            .explanation_data = {.positions = positions, .values = values, .position_roles = std::move(roles)}};
    }
};

}  // namespace sudoku::core
