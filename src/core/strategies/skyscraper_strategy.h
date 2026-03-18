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

#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Skyscraper patterns
/// A Skyscraper occurs when two conjugate pairs (rows/cols where a candidate appears
/// in exactly 2 cells) share one endpoint column/row. The two non-shared endpoints
/// can eliminate the candidate from any cell that sees both of them.
class SkyscraperStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        auto row_result = findRowBasedSkyscraper(board, candidates);
        if (row_result.has_value()) {
            return row_result;
        }
        return findColBasedSkyscraper(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::Skyscraper;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Skyscraper";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::Skyscraper);
    }

private:
    struct ConjugatePair {
        size_t region;  // row or col index
        Position pos1;  // first endpoint
        Position pos2;  // second endpoint
    };

    /// Row-based: find rows with exactly 2 candidate positions, sharing one column
    [[nodiscard]] static std::optional<SolveStep> findRowBasedSkyscraper(const BoardData& board,
                                                                         const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            std::vector<ConjugatePair> pairs;
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                std::vector<size_t> cols;
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                        cols.push_back(col);
                    }
                }
                if (cols.size() == 2) {
                    pairs.push_back(ConjugatePair{.region = row,
                                                  .pos1 = Position{.row = row, .col = cols[0]},
                                                  .pos2 = Position{.row = row, .col = cols[1]}});
                }
            }

            auto result = findSkyscraperFromPairs(board, candidates, pairs, value, RegionType::Row);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Column-based: find columns with exactly 2 candidate positions, sharing one row
    [[nodiscard]] static std::optional<SolveStep> findColBasedSkyscraper(const BoardData& board,
                                                                         const CandidateGrid& candidates) {
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            std::vector<ConjugatePair> pairs;
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                std::vector<size_t> rows;
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                        rows.push_back(row);
                    }
                }
                if (rows.size() == 2) {
                    pairs.push_back(ConjugatePair{.region = col,
                                                  .pos1 = Position{.row = rows[0], .col = col},
                                                  .pos2 = Position{.row = rows[1], .col = col}});
                }
            }

            auto result = findSkyscraperFromPairs(board, candidates, pairs, value, RegionType::Col);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Given conjugate pairs, find two that share one endpoint and produce eliminations.
    [[nodiscard]] static std::optional<SolveStep> findSkyscraperFromPairs(const BoardData& board,
                                                                          const CandidateGrid& candidates,
                                                                          const std::vector<ConjugatePair>& pairs,
                                                                          int value, RegionType region_type) {
        for (size_t i = 0; i < pairs.size(); ++i) {
            for (size_t j = i + 1; j < pairs.size(); ++j) {
                const auto& pair_a = pairs[i];
                const auto& pair_b = pairs[j];

                // Try all 4 endpoint matchings to find a shared endpoint
                auto result = tryEndpointMatch(board, candidates, pair_a, pair_b, pair_a.pos1, pair_a.pos2, pair_b.pos1,
                                               pair_b.pos2, value, region_type);
                if (result.has_value()) {
                    return result;
                }
                result = tryEndpointMatch(board, candidates, pair_a, pair_b, pair_a.pos1, pair_a.pos2, pair_b.pos2,
                                          pair_b.pos1, value, region_type);
                if (result.has_value()) {
                    return result;
                }
                result = tryEndpointMatch(board, candidates, pair_a, pair_b, pair_a.pos2, pair_a.pos1, pair_b.pos1,
                                          pair_b.pos2, value, region_type);
                if (result.has_value()) {
                    return result;
                }
                result = tryEndpointMatch(board, candidates, pair_a, pair_b, pair_a.pos2, pair_a.pos1, pair_b.pos2,
                                          pair_b.pos1, value, region_type);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    /// Check if shared_a and shared_b share a row/col (the "base"), and if so,
    /// eliminate value from cells that see both non_shared_a and non_shared_b.
    [[nodiscard]] static std::optional<SolveStep>
    tryEndpointMatch(const BoardData& board, const CandidateGrid& candidates, const ConjugatePair& pair_a,
                     const ConjugatePair& pair_b, const Position& shared_a, const Position& non_shared_a,
                     const Position& shared_b, const Position& non_shared_b, int value, RegionType region_type) {
        // For row-based: shared endpoints must be in the same column
        // For col-based: shared endpoints must be in the same row
        bool endpoints_share =
            (region_type == RegionType::Row) ? (shared_a.col == shared_b.col) : (shared_a.row == shared_b.row);
        if (!endpoints_share) {
            return std::nullopt;
        }

        // The non-shared endpoints must NOT share the same col/row (otherwise it's an X-Wing)
        bool non_shared_share = (region_type == RegionType::Row) ? (non_shared_a.col == non_shared_b.col)
                                                                 : (non_shared_a.row == non_shared_b.row);
        if (non_shared_share) {
            return std::nullopt;
        }

        // Eliminate value from cells that see both non-shared endpoints
        std::vector<Elimination> eliminations;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] != EMPTY_CELL) {
                    continue;
                }
                Position pos{.row = row, .col = col};
                if (pos == shared_a || pos == shared_b || pos == non_shared_a || pos == non_shared_b) {
                    continue;
                }
                if (sees(pos, non_shared_a) && sees(pos, non_shared_b) && candidates.isAllowed(row, col, value)) {
                    eliminations.push_back(Elimination{.position = pos, .value = value});
                }
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation =
            fmt::format("Skyscraper on value {}: conjugate pairs in {}{} and {}{} share endpoint {}"
                        " \xe2\x80\x94 eliminates {} from cells seeing both {} and {}",
                        value, region_type == RegionType::Row ? "Row " : "Column ", pair_a.region + 1,
                        region_type == RegionType::Row ? "Row " : "Column ", pair_b.region + 1,
                        formatPosition(shared_a), value, formatPosition(non_shared_a), formatPosition(non_shared_b));

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::Skyscraper,
                         .position = non_shared_a,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::Skyscraper),
                         .explanation_data = {.positions = {shared_a, non_shared_a, shared_b, non_shared_b},
                                              .values = {value},
                                              .region_type = region_type,
                                              .region_index = pair_a.region,
                                              .secondary_region_type = region_type,
                                              .secondary_region_index = pair_b.region,
                                              .position_roles = {CellRole::LinkEndpoint, CellRole::ChainA,
                                                                 CellRole::LinkEndpoint, CellRole::ChainB}}};
    }
};

}  // namespace sudoku::core
