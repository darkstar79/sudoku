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

#include <array>
#include <optional>
#include <set>
#include <vector>

#include <bit>
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace sudoku::core {

/// Strategy for finding Junior Exocet (JE) patterns.
///
/// Sound minimum form of David P. Bird's Junior Exocet (row-based and column-based).
/// For the row-based pattern:
///
///   - Base pair B1, B2 in a single box, in the same row ("mini-row"). The union of
///     their candidates ("base digits") has cardinality 3 or 4.
///   - Target pair T1, T2 in the same band as the base box ("JE band"), each in one
///     of the two non-base boxes of the band, in different rows and different boxes,
///     neither in the base row, with **all** base digits appearing as candidates in
///     each target (this is what makes the elimination sound).
///   - S-cells: cells outside the JE band that are neither peers of B1/B2 nor peers
///     of any of the four "companion" cells (the two non-target cells of each
///     target's mini-row within the target's box).
///   - **Cover-house invariant** (S-cell check): for every base digit `d`, the
///     S-cells where `d` is a candidate or already placed must be confined to at
///     most two rows. If a base digit's S-cells span three or more rows, the JE
///     proof fails and the pattern is rejected.
///
/// The column-based variant is the row/column transpose of the above.
///
/// On a valid pattern, every non-base candidate in T1 and T2 is eliminated (Bird's
/// JE elimination rule 1 — the only rule implemented here; rules 2-9 require
/// additional analysis and are deferred).
class JuniorExocetStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        if (auto step = findRowBased(board, candidates); step.has_value()) {
            return step;
        }
        return findColBased(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::JuniorExocet;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Junior Exocet";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::JuniorExocet);
    }

private:
    /// Bird's definition admits 3- or 4-digit base sets. Two-digit bases collapse to
    /// a Naked Pair (handled earlier) and 5+ leaves no room for the cover-house
    /// invariant to bite.
    static constexpr int kMinBaseCandidates = 3;
    static constexpr int kMaxBaseCandidates = 4;

    // ============================================================================
    // Row-based JE
    // ============================================================================
    // CPD-OFF — row/col symmetry; the column variant below is the transpose.

    [[nodiscard]] static std::optional<SolveStep> findRowBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (size_t base_box = 0; base_box < BOARD_SIZE; ++base_box) {
            size_t band_start_row = (base_box / BOX_SIZE) * BOX_SIZE;
            size_t box_start_col = (base_box % BOX_SIZE) * BOX_SIZE;

            for (size_t br = 0; br < BOX_SIZE; ++br) {
                size_t base_row = band_start_row + br;

                std::vector<Position> mini_row;
                for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                    size_t col = box_start_col + bc;
                    if (board[base_row][col] == EMPTY_CELL) {
                        mini_row.push_back(Position{.row = base_row, .col = col});
                    }
                }
                if (mini_row.size() < 2) {
                    continue;
                }

                for (size_t i = 0; i < mini_row.size(); ++i) {
                    for (size_t j = i + 1; j < mini_row.size(); ++j) {
                        auto step =
                            tryRowBasePair(board, candidates, mini_row[i], mini_row[j], base_box, band_start_row);
                        if (step.has_value()) {
                            return step;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — pattern enumeration; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryRowBasePair(const BoardData& board,
                                                                 const CandidateGrid& candidates, const Position& base1,
                                                                 const Position& base2, size_t base_box,
                                                                 size_t band_start_row) {
        uint16_t base_mask = candidates.getPossibleValuesMask(base1.row, base1.col) |
                             candidates.getPossibleValuesMask(base2.row, base2.col);

        int base_count = std::popcount(base_mask);
        if (base_count < kMinBaseCandidates || base_count > kMaxBaseCandidates) {
            return std::nullopt;
        }

        std::vector<Position> targets;
        for (size_t r = band_start_row; r < band_start_row + BOX_SIZE; ++r) {
            if (r == base1.row) {
                continue;
            }
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (getBoxIndex(r, col) == base_box) {
                    continue;
                }
                if (board[r][col] != EMPTY_CELL) {
                    continue;
                }
                if (col == base1.col || col == base2.col) {
                    continue;
                }
                uint16_t t_mask = candidates.getPossibleValuesMask(r, col);
                if ((t_mask & base_mask) != base_mask) {
                    continue;
                }
                targets.push_back(Position{.row = r, .col = col});
            }
        }

        if (targets.size() < 2) {
            return std::nullopt;
        }

        for (size_t i = 0; i < targets.size(); ++i) {
            for (size_t j = i + 1; j < targets.size(); ++j) {
                const auto& t1 = targets[i];
                const auto& t2 = targets[j];

                if (t1.row == t2.row) {
                    continue;
                }
                if (getBoxIndex(t1.row, t1.col) == getBoxIndex(t2.row, t2.col)) {
                    continue;
                }

                if (!checkRowBasedCover(board, candidates, base1, base2, t1, t2, base_mask, band_start_row)) {
                    continue;
                }

                auto step = buildStep(candidates, base1, base2, t1, t2, base_mask, /*row_based=*/true);
                if (step.has_value()) {
                    return step;
                }
            }
        }
        return std::nullopt;
    }

    /// Collect the column indices that the S-cell region excludes (for row-based JE).
    /// S-cells are the cells outside the JE band whose column is neither a base
    /// column nor a column shared with a target's companion cells.
    [[nodiscard]] static std::set<size_t> rowBasedExcludedCols(const Position& base1, const Position& base2,
                                                               const Position& t1, const Position& t2) {
        std::set<size_t> excluded{base1.col, base2.col};
        size_t t1_box_col_start = (t1.col / BOX_SIZE) * BOX_SIZE;
        for (size_t c = t1_box_col_start; c < t1_box_col_start + BOX_SIZE; ++c) {
            if (c != t1.col) {
                excluded.insert(c);
            }
        }
        size_t t2_box_col_start = (t2.col / BOX_SIZE) * BOX_SIZE;
        for (size_t c = t2_box_col_start; c < t2_box_col_start + BOX_SIZE; ++c) {
            if (c != t2.col) {
                excluded.insert(c);
            }
        }
        return excluded;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    [[nodiscard]] static bool checkRowBasedCover(const BoardData& board, const CandidateGrid& candidates,
                                                 const Position& base1, const Position& base2, const Position& t1,
                                                 const Position& t2, uint16_t base_mask, size_t band_start_row) {
        auto excluded_cols = rowBasedExcludedCols(base1, base2, t1, t2);

        std::array<std::set<size_t>, MAX_VALUE + 1> digit_rows;  // index by digit 1..9
        for (size_t r = 0; r < BOARD_SIZE; ++r) {
            if (r >= band_start_row && r < band_start_row + BOX_SIZE) {
                continue;
            }
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (excluded_cols.contains(col)) {
                    continue;
                }
                for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                    if ((base_mask & valueToBit(v)) == 0) {
                        continue;
                    }
                    bool has = (board[r][col] == EMPTY_CELL) ? candidates.isAllowed(r, col, v) : (board[r][col] == v);
                    if (has) {
                        digit_rows[static_cast<size_t>(v)].insert(r);
                    }
                }
            }
        }

        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((base_mask & valueToBit(v)) == 0) {
                continue;
            }
            if (digit_rows[static_cast<size_t>(v)].size() > 2) {
                return false;
            }
        }
        return true;
    }
    // CPD-ON

    // ============================================================================
    // Column-based JE — row/col transpose of the above
    // ============================================================================
    // CPD-OFF

    [[nodiscard]] static std::optional<SolveStep> findColBased(const BoardData& board,
                                                               const CandidateGrid& candidates) {
        for (size_t base_box = 0; base_box < BOARD_SIZE; ++base_box) {
            size_t box_start_row = (base_box / BOX_SIZE) * BOX_SIZE;
            size_t stack_start_col = (base_box % BOX_SIZE) * BOX_SIZE;

            for (size_t bc = 0; bc < BOX_SIZE; ++bc) {
                size_t base_col = stack_start_col + bc;

                std::vector<Position> mini_col;
                for (size_t br = 0; br < BOX_SIZE; ++br) {
                    size_t row = box_start_row + br;
                    if (board[row][base_col] == EMPTY_CELL) {
                        mini_col.push_back(Position{.row = row, .col = base_col});
                    }
                }
                if (mini_col.size() < 2) {
                    continue;
                }

                for (size_t i = 0; i < mini_col.size(); ++i) {
                    for (size_t j = i + 1; j < mini_col.size(); ++j) {
                        auto step =
                            tryColBasePair(board, candidates, mini_col[i], mini_col[j], base_box, stack_start_col);
                        if (step.has_value()) {
                            return step;
                        }
                    }
                }
            }
        }
        return std::nullopt;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — pattern enumeration; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> tryColBasePair(const BoardData& board,
                                                                 const CandidateGrid& candidates, const Position& base1,
                                                                 const Position& base2, size_t base_box,
                                                                 size_t stack_start_col) {
        uint16_t base_mask = candidates.getPossibleValuesMask(base1.row, base1.col) |
                             candidates.getPossibleValuesMask(base2.row, base2.col);

        int base_count = std::popcount(base_mask);
        if (base_count < kMinBaseCandidates || base_count > kMaxBaseCandidates) {
            return std::nullopt;
        }

        std::vector<Position> targets;
        for (size_t col = stack_start_col; col < stack_start_col + BOX_SIZE; ++col) {
            if (col == base1.col) {
                continue;
            }
            for (size_t r = 0; r < BOARD_SIZE; ++r) {
                if (getBoxIndex(r, col) == base_box) {
                    continue;
                }
                if (board[r][col] != EMPTY_CELL) {
                    continue;
                }
                if (r == base1.row || r == base2.row) {
                    continue;
                }
                uint16_t t_mask = candidates.getPossibleValuesMask(r, col);
                if ((t_mask & base_mask) != base_mask) {
                    continue;
                }
                targets.push_back(Position{.row = r, .col = col});
            }
        }

        if (targets.size() < 2) {
            return std::nullopt;
        }

        for (size_t i = 0; i < targets.size(); ++i) {
            for (size_t j = i + 1; j < targets.size(); ++j) {
                const auto& t1 = targets[i];
                const auto& t2 = targets[j];

                if (t1.col == t2.col) {
                    continue;
                }
                if (getBoxIndex(t1.row, t1.col) == getBoxIndex(t2.row, t2.col)) {
                    continue;
                }

                if (!checkColBasedCover(board, candidates, base1, base2, t1, t2, base_mask, stack_start_col)) {
                    continue;
                }

                auto step = buildStep(candidates, base1, base2, t1, t2, base_mask, /*row_based=*/false);
                if (step.has_value()) {
                    return step;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::set<size_t> colBasedExcludedRows(const Position& base1, const Position& base2,
                                                               const Position& t1, const Position& t2) {
        std::set<size_t> excluded{base1.row, base2.row};
        size_t t1_box_row_start = (t1.row / BOX_SIZE) * BOX_SIZE;
        for (size_t r = t1_box_row_start; r < t1_box_row_start + BOX_SIZE; ++r) {
            if (r != t1.row) {
                excluded.insert(r);
            }
        }
        size_t t2_box_row_start = (t2.row / BOX_SIZE) * BOX_SIZE;
        for (size_t r = t2_box_row_start; r < t2_box_row_start + BOX_SIZE; ++r) {
            if (r != t2.row) {
                excluded.insert(r);
            }
        }
        return excluded;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    [[nodiscard]] static bool checkColBasedCover(const BoardData& board, const CandidateGrid& candidates,
                                                 const Position& base1, const Position& base2, const Position& t1,
                                                 const Position& t2, uint16_t base_mask, size_t stack_start_col) {
        auto excluded_rows = colBasedExcludedRows(base1, base2, t1, t2);

        std::array<std::set<size_t>, MAX_VALUE + 1> digit_cols;
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (col >= stack_start_col && col < stack_start_col + BOX_SIZE) {
                continue;
            }
            for (size_t r = 0; r < BOARD_SIZE; ++r) {
                if (excluded_rows.contains(r)) {
                    continue;
                }
                for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                    if ((base_mask & valueToBit(v)) == 0) {
                        continue;
                    }
                    bool has = (board[r][col] == EMPTY_CELL) ? candidates.isAllowed(r, col, v) : (board[r][col] == v);
                    if (has) {
                        digit_cols[static_cast<size_t>(v)].insert(col);
                    }
                }
            }
        }

        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((base_mask & valueToBit(v)) == 0) {
                continue;
            }
            if (digit_cols[static_cast<size_t>(v)].size() > 2) {
                return false;
            }
        }
        return true;
    }
    // CPD-ON

    // ============================================================================
    // Common: emit a SolveStep from a verified JE pattern
    // ============================================================================

    [[nodiscard]] static std::optional<SolveStep> buildStep(const CandidateGrid& candidates, const Position& base1,
                                                            const Position& base2, const Position& t1,
                                                            const Position& t2, uint16_t base_mask, bool row_based) {
        std::vector<Elimination> eliminations;

        uint16_t t1_mask = candidates.getPossibleValuesMask(t1.row, t1.col);
        uint16_t t1_non_base = t1_mask & ~base_mask;
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            if ((t1_non_base & valueToBit(value)) != 0) {
                eliminations.push_back(Elimination{.position = t1, .value = value});
            }
        }
        uint16_t t2_mask = candidates.getPossibleValuesMask(t2.row, t2.col);
        uint16_t t2_non_base = t2_mask & ~base_mask;
        for (int value = MIN_VALUE; value <= MAX_VALUE; ++value) {
            if ((t2_non_base & valueToBit(value)) != 0) {
                eliminations.push_back(Elimination{.position = t2, .value = value});
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        std::vector<int> base_values;
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((base_mask & valueToBit(v)) != 0) {
                base_values.push_back(v);
            }
        }

        auto explanation = fmt::format("Junior Exocet ({} based): base cells {} and {} with candidates {{{}}} "
                                       "— targets {} and {} can only contain base candidates "
                                       "— eliminates non-base candidates",
                                       row_based ? "row" : "column", formatPosition(base1), formatPosition(base2),
                                       fmt::join(base_values, ","), formatPosition(t1), formatPosition(t2));

        ExplanationData data;
        data.positions = {base1, base2, t1, t2};
        data.position_roles = {CellRole::Pattern, CellRole::Pattern, CellRole::Fin, CellRole::Fin};
        for (int v : base_values) {
            data.values.push_back(v);
        }

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::JuniorExocet,
                         .position = base1,
                         .value = 0,
                         .eliminations = std::move(eliminations),
                         .explanation = std::move(explanation),
                         .rating = getTechniqueRating(SolvingTechnique::JuniorExocet),
                         .explanation_data = std::move(data)};
    }
};

}  // namespace sudoku::core
