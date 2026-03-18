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

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Hidden Unique Rectangle patterns.
/// A Hidden UR has 4 cells forming a rectangle across 2 boxes, all containing
/// candidates {P,Q}. When one corner has conjugate pairs on P (or Q) in both
/// its row and column within the rectangle, the other value can be eliminated
/// from that corner to avoid a deadly pattern.
class HiddenUniqueRectangleStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        for (int p = MIN_VALUE; p <= MAX_VALUE; ++p) {
            for (int q = p + 1; q <= MAX_VALUE; ++q) {
                auto result = findHiddenUR(board, candidates, p, q);
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::HiddenUniqueRectangle;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Hidden Unique Rectangle";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::HiddenUniqueRectangle);
    }

private:
    /// Search for Hidden UR pattern for a specific value pair {p, q}.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — rectangle enumeration with value pairs and corner checks; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findHiddenUR(const BoardData& board, const CandidateGrid& candidates,
                                                               int val_p, int val_q) {
        // Collect columns containing both p and q, grouped by row
        std::array<std::vector<size_t>, BOARD_SIZE> cols_by_row;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, val_p) &&
                    candidates.isAllowed(row, col, val_q)) {
                    cols_by_row[row].push_back(col);
                }
            }
        }

        // Find rectangles: pick two rows, each with at least 2 {p,q}-cells
        for (size_t r1 = 0; r1 < BOARD_SIZE; ++r1) {
            if (cols_by_row[r1].size() < 2) {
                continue;
            }
            for (size_t r2 = r1 + 1; r2 < BOARD_SIZE; ++r2) {
                if (cols_by_row[r2].size() < 2) {
                    continue;
                }

                // Pick column pairs from row r1, check if both exist in row r2
                for (size_t ci = 0; ci < cols_by_row[r1].size(); ++ci) {
                    size_t c1 = cols_by_row[r1][ci];
                    for (size_t cj = ci + 1; cj < cols_by_row[r1].size(); ++cj) {
                        size_t c2 = cols_by_row[r1][cj];

                        // Check c1 and c2 both exist in row r2
                        bool c1_in_r2 = std::ranges::find(cols_by_row[r2], c1) != cols_by_row[r2].end();
                        bool c2_in_r2 = std::ranges::find(cols_by_row[r2], c2) != cols_by_row[r2].end();
                        if (!c1_in_r2 || !c2_in_r2) {
                            continue;
                        }

                        Position a{.row = r1, .col = c1};
                        Position b{.row = r1, .col = c2};
                        Position c{.row = r2, .col = c1};
                        Position d{.row = r2, .col = c2};

                        if (countUniqueBoxes(a, b, c, d) != 2) {
                            continue;
                        }

                        auto result = checkAllCorners(board, candidates, a, b, c, d, val_p, val_q);
                        if (result.has_value()) {
                            return result;
                        }
                    }
                }
            }
        }

        return std::nullopt;
    }

    /// Try each corner of the rectangle as a Hidden UR target.
    [[nodiscard]] static std::optional<SolveStep> checkAllCorners(const BoardData& board,
                                                                  const CandidateGrid& candidates, const Position& a,
                                                                  const Position& b, const Position& c,
                                                                  const Position& d, int val_p, int val_q) {
        // a=(r1,c1), b=(r1,c2), c=(r2,c1), d=(r2,c2)
        // Each corner's row-partner shares its row, col-partner shares its column, diag is opposite
        struct CornerCheck {
            Position target;
            Position row_partner;
            Position col_partner;
            Position diag_partner;
            size_t index = 0;
        };
        std::array<CornerCheck, 4> corners = {{
            {.target = a, .row_partner = b, .col_partner = c, .diag_partner = d, .index = 0},
            {.target = b, .row_partner = a, .col_partner = d, .diag_partner = c, .index = 1},
            {.target = c, .row_partner = d, .col_partner = a, .diag_partner = b, .index = 2},
            {.target = d, .row_partner = c, .col_partner = b, .diag_partner = a, .index = 3},
        }};

        for (const auto& [target, row_partner, col_partner, diag_partner, idx] : corners) {
            auto result = checkSingleCorner(board, candidates, a, b, c, d, target, row_partner, col_partner,
                                            diag_partner, idx, val_p, val_q);
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    /// Check if a specific corner qualifies as a Hidden UR target.
    /// The corner needs conjugate pairs on one value (strong_val) in both its row and column,
    /// AND the diagonal partner must be forced to take elim_val (to complete the deadly pattern).
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — multiple conjugate pair checks with diagonal partner validation; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    checkSingleCorner(const BoardData& board, const CandidateGrid& candidates, const Position& a, const Position& b,
                      const Position& c, const Position& d, const Position& target, const Position& row_partner,
                      const Position& col_partner, const Position& diag_partner, size_t target_idx, int val_p,
                      int val_q) {
        for (int strong_val : {val_p, val_q}) {
            int elim_val = (strong_val == val_p) ? val_q : val_p;

            if (!candidates.isAllowed(target.row, target.col, elim_val)) {
                continue;
            }

            // strong_val must be conjugate pair in the row between target and row_partner
            // (exactly 2 cells in the row have it, and row_partner is one of them)
            if (!isConjugatePairInUnit(board, candidates, target.row, true, strong_val)) {
                continue;
            }
            if (!candidates.isAllowed(row_partner.row, row_partner.col, strong_val)) {
                continue;
            }

            // strong_val must be conjugate pair in the column between target and col_partner
            if (!isConjugatePairInUnit(board, candidates, target.col, false, strong_val)) {
                continue;
            }
            if (!candidates.isAllowed(col_partner.row, col_partner.col, strong_val)) {
                continue;
            }

            // The diagonal partner must be forced to take elim_val for the deadly pattern.
            // If target gets elim_val, strong_val is forced to row_partner and col_partner,
            // blocking strong_val from diag_partner. But diag_partner having OTHER candidates
            // besides {strong_val, elim_val} could break the deadly pattern argument.
            // Require one of:
            //   (a) diag_partner is bivalue {val_p, val_q}
            //   (b) elim_val is conjugate in diag_partner's row (between diag and col_partner)
            //   (c) elim_val is conjugate in diag_partner's column (between diag and row_partner)
            if (!isDiagPartnerForced(board, candidates, diag_partner, row_partner, col_partner, elim_val, val_p,
                                     val_q)) {
                continue;
            }

            // Hidden UR confirmed: strong_val must go in target → eliminate elim_val
            std::vector<Elimination> eliminations;
            eliminations.push_back(Elimination{.position = target, .value = elim_val});

            auto explanation = fmt::format("Hidden Unique Rectangle: cells {}, {}, {}, {} with values {{{},{}}} — "
                                           "eliminates {} from {} to avoid deadly pattern",
                                           formatPosition(a), formatPosition(b), formatPosition(c), formatPosition(d),
                                           val_p, val_q, elim_val, formatPosition(target));

            std::vector<Position> all_positions = {a, b, c, d};
            std::vector<CellRole> roles(4, CellRole::Roof);
            roles[target_idx] = CellRole::Floor;

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::HiddenUniqueRectangle,
                             .position = target,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::HiddenUniqueRectangle),
                             .explanation_data = {.positions = all_positions,
                                                  .values = {val_p, val_q, elim_val},
                                                  .position_roles = std::move(roles)}};
        }

        return std::nullopt;
    }

    /// Checks if val appears in exactly 2 empty cells in the given row or column.
    /// @param idx Row index (if is_row) or column index (if !is_row)
    /// @param is_row true for row check, false for column check
    [[nodiscard]] static bool isConjugatePairInUnit(const BoardData& board, const CandidateGrid& candidates, size_t idx,
                                                    bool is_row, int val) {
        int count = 0;
        for (size_t i = 0; i < BOARD_SIZE; ++i) {
            size_t row = is_row ? idx : i;
            size_t col = is_row ? i : idx;
            if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, val)) {
                ++count;
                if (count > 2) {
                    return false;
                }
            }
        }
        return count == 2;
    }

    /// Checks if a cell has exactly 2 candidates: val_p and val_q.
    [[nodiscard]] static bool isBivalue(const BoardData& board, const CandidateGrid& candidates, const Position& pos,
                                        int val_p, int val_q) {
        if (board[pos.row][pos.col] != EMPTY_CELL) {
            return false;
        }
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if (v == val_p || v == val_q) {
                if (!candidates.isAllowed(pos.row, pos.col, v)) {
                    return false;
                }
            } else {
                if (candidates.isAllowed(pos.row, pos.col, v)) {
                    return false;
                }
            }
        }
        return true;
    }

    /// Checks if the diagonal partner would be forced to take elim_val,
    /// completing the deadly pattern. Requires one of:
    ///   (a) diag_partner is bivalue {val_p, val_q}
    ///   (b) elim_val is conjugate in diag_partner's row (between diag and col_partner)
    ///   (c) elim_val is conjugate in diag_partner's column (between diag and row_partner)
    [[nodiscard]] static bool isDiagPartnerForced(const BoardData& board, const CandidateGrid& candidates,
                                                  const Position& diag_partner, const Position& row_partner,
                                                  const Position& col_partner, int elim_val, int val_p, int val_q) {
        // (a) diag_partner is bivalue {val_p, val_q}
        if (isBivalue(board, candidates, diag_partner, val_p, val_q)) {
            return true;
        }

        // (b) elim_val is conjugate in diag_partner's row, and col_partner is the partner
        if (isConjugatePairInUnit(board, candidates, diag_partner.row, true, elim_val) &&
            candidates.isAllowed(col_partner.row, col_partner.col, elim_val)) {
            return true;
        }

        // (c) elim_val is conjugate in diag_partner's column, and row_partner is the partner
        if (isConjugatePairInUnit(board, candidates, diag_partner.col, false, elim_val) &&
            candidates.isAllowed(row_partner.row, row_partner.col, elim_val)) {
            return true;
        }

        return false;
    }
};

}  // namespace sudoku::core
