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
#include "../i_game_validator.h"
#include "../solve_step.h"
#include "../strategy_base.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace sudoku::core {

/// Shared utility functions for fish-based strategies (X-Wing, Swordfish, Jellyfish, and finned variants).
/// Inherits StrategyBase for cellIndex(), BOARD_SIZE, etc.
class FishHelpers : protected StrategyBase {
public:
    /// For each row (by_row=true) or column (by_row=false), collect the column/row indices
    /// where `value` is a candidate. Returns a vector of 9 vectors indexed by row/col.
    [[nodiscard]] static std::vector<std::vector<size_t>>
    collectCandidatePositions(const BoardData& board, const CandidateGrid& candidates, int value, bool by_row) {
        std::vector<std::vector<size_t>> positions(BOARD_SIZE);
        for (size_t primary = 0; primary < BOARD_SIZE; ++primary) {
            for (size_t secondary = 0; secondary < BOARD_SIZE; ++secondary) {
                size_t row = by_row ? primary : secondary;
                size_t col = by_row ? secondary : primary;
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, value)) {
                    positions[primary].push_back(secondary);
                }
            }
        }
        return positions;
    }

    /// Compute sorted union of an arbitrary number of index vectors.
    template <typename... Vecs>
    [[nodiscard]] static std::vector<size_t> indexUnion(const Vecs&... vecs) {
        std::vector<size_t> result;
        result.reserve(BOARD_SIZE);
        auto add = [&](const std::vector<size_t>& vec) {
            for (size_t idx : vec) {
                if (!std::ranges::contains(result, idx)) {
                    result.push_back(idx);
                }
            }
        };
        (add(vecs), ...);
        std::ranges::sort(result);
        return result;
    }
    /// Result of finned fish validation: identifies the fin position and base indices.
    struct FinnedFishInfo {
        size_t fin_primary;                    ///< Fin's base line (row if by_row, col otherwise)
        size_t fin_secondary;                  ///< Fin's cover index (col if by_row, row otherwise)
        std::vector<size_t> base_secondaries;  ///< Cover indices excluding fin
    };

    /// Validates a finned fish pattern for a specific fin candidate.
    /// @param positions_map Candidate positions indexed by primary (base) line
    /// @param base_primaries The base lines forming the fish
    /// @param union_secondaries The union of cover indices (size = base_primaries.size() + 1)
    /// @param fin_idx Index into union_secondaries to try as the fin
    /// @return FinnedFishInfo if valid, nullopt otherwise
    [[nodiscard]] static std::optional<FinnedFishInfo>
    validateFinnedFish(const std::vector<std::vector<size_t>>& positions_map, std::span<const size_t> base_primaries,
                       const std::vector<size_t>& union_secondaries, size_t fin_idx) {
        size_t fin_secondary = union_secondaries[fin_idx];
        std::vector<size_t> base_secondaries;
        for (size_t i = 0; i < union_secondaries.size(); ++i) {
            if (i != fin_idx) {
                base_secondaries.push_back(union_secondaries[i]);
            }
        }

        // Exactly one base line should contain the fin secondary
        size_t fin_primary = 0;
        int fin_count = 0;
        for (size_t primary : base_primaries) {
            if (std::ranges::contains(positions_map[primary], fin_secondary)) {
                fin_primary = primary;
                ++fin_count;
            }
        }
        if (fin_count != 1) {
            return std::nullopt;
        }

        // Non-finned lines' candidates must all be within base_secondaries
        for (size_t primary : base_primaries) {
            if (primary == fin_primary) {
                continue;
            }
            for (size_t sec : positions_map[primary]) {
                if (!std::ranges::contains(base_secondaries, sec)) {
                    return std::nullopt;
                }
            }
        }

        return FinnedFishInfo{.fin_primary = fin_primary,
                              .fin_secondary = fin_secondary,
                              .base_secondaries = std::move(base_secondaries)};
    }

    /// Builds eliminations for a finned fish pattern.
    /// Eliminates value from cells in base_secondaries × non-pattern primaries, restricted to fin's box.
    [[nodiscard]] static std::vector<Elimination> buildFinnedEliminations(const BoardData& board,
                                                                          const CandidateGrid& candidates, int value,
                                                                          std::span<const size_t> base_primaries,
                                                                          const std::vector<size_t>& base_secondaries,
                                                                          size_t fin_box, bool by_row) {
        std::vector<Elimination> eliminations;
        for (size_t secondary = 0; secondary < BOARD_SIZE; ++secondary) {
            if (std::ranges::contains(base_primaries, secondary)) {
                continue;
            }
            for (size_t base_sec : base_secondaries) {
                size_t row = by_row ? secondary : base_sec;
                size_t col = by_row ? base_sec : secondary;
                if (getBoxIndex(row, col) == fin_box && board[row][col] == EMPTY_CELL &&
                    candidates.isAllowed(row, col, value)) {
                    eliminations.push_back(Elimination{.position = Position{.row = row, .col = col}, .value = value});
                }
            }
        }
        return eliminations;
    }

    /// Collects all pattern positions and assigns cell roles (Pattern or Fin).
    [[nodiscard]] static std::pair<std::vector<Position>, std::vector<CellRole>>
    buildFinnedPositionsAndRoles(const std::vector<std::vector<size_t>>& positions_map,
                                 std::span<const size_t> base_primaries, const Position& fin_pos, bool by_row) {
        std::vector<Position> positions;
        for (size_t primary : base_primaries) {
            for (size_t secondary : positions_map[primary]) {
                size_t row = by_row ? primary : secondary;
                size_t col = by_row ? secondary : primary;
                positions.push_back(Position{.row = row, .col = col});
            }
        }

        std::vector<CellRole> roles;
        roles.reserve(positions.size());
        for (const auto& pos : positions) {
            roles.push_back(pos == fin_pos ? CellRole::Fin : CellRole::Pattern);
        }
        return {std::move(positions), std::move(roles)};
    }
};

}  // namespace sudoku::core
