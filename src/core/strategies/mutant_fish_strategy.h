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

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <bit>
#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Mutant Fish patterns.
/// A Mutant Fish generalizes Franken Fish by allowing BOTH base and cover sets to
/// freely mix rows, columns, and boxes.  The distinguishing criterion is that both
/// the base set AND the cover set each contain units of at least 2 different types.
///   - Standard Fish: base = all rows (or all cols), cover = all cols (or all rows)
///   - Franken Fish: ONE side mixes unit types, the other is pure lines
///   - Mutant Fish: BOTH sides mix unit types
/// For each digit D and size N (2-4):
///   - Enumerate N base sets from all 27 units (rows, cols, boxes)
///   - Find N cover sets from the remaining units
///   - Reject if base or cover units overlap in candidate cells (degenerate fish)
///   - Require both base and cover to span >= 2 unit types
///   - All base cells must be covered; eliminate D from cover cells outside base
class MutantFishStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        return findMutantFish(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::MutantFish;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Mutant Fish";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::MutantFish);
    }

private:
    /// Unit type for base/cover sets
    enum class UnitType : uint8_t {
        Row,
        Col,
        Box
    };

    /// A unit with its type and index
    struct Unit {
        UnitType type;
        size_t index;
    };

    /// A unit with its precomputed candidate cells.
    struct UnitCells {
        Unit unit;
        std::vector<size_t> cells;  ///< Flat indices (row * 9 + col) of candidate cells
    };

    /// Get the candidate cells (flat indices) for a unit where digit D is a candidate.
    [[nodiscard]] static uint16_t getCellsInUnit(const BoardData& board, const CandidateGrid& candidates,
                                                 const Unit& unit, int digit, std::vector<size_t>& out_cells) {
        uint16_t count = 0;
        out_cells.clear();
        switch (unit.type) {
            case UnitType::Row:
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (board[unit.index][col] == EMPTY_CELL && candidates.isAllowed(unit.index, col, digit)) {
                        out_cells.push_back((unit.index * BOARD_SIZE) + col);
                        ++count;
                    }
                }
                break;
            case UnitType::Col:
                for (size_t row = 0; row < BOARD_SIZE; ++row) {
                    if (board[row][unit.index] == EMPTY_CELL && candidates.isAllowed(row, unit.index, digit)) {
                        out_cells.push_back((row * BOARD_SIZE) + unit.index);
                        ++count;
                    }
                }
                break;
            case UnitType::Box: {
                size_t start_row = (unit.index / BOX_SIZE) * BOX_SIZE;
                size_t start_col = (unit.index % BOX_SIZE) * BOX_SIZE;
                for (size_t r = start_row; r < start_row + BOX_SIZE; ++r) {
                    for (size_t c = start_col; c < start_col + BOX_SIZE; ++c) {
                        if (board[r][c] == EMPTY_CELL && candidates.isAllowed(r, c, digit)) {
                            out_cells.push_back((r * BOARD_SIZE) + c);
                            ++count;
                        }
                    }
                }
                break;
            }
        }
        return count;
    }

    /// Check if a cell (flat index) is covered by a unit.
    [[nodiscard]] static bool isCoveredBy(size_t cell_idx, const Unit& unit) {
        size_t row = cell_idx / BOARD_SIZE;
        size_t col = cell_idx % BOARD_SIZE;
        switch (unit.type) {
            case UnitType::Row:
                return row == unit.index;
            case UnitType::Col:
                return col == unit.index;
            case UnitType::Box:
                return getBoxIndex(row, col) == unit.index;
        }
        return false;
    }

    /// Check if a set of unit types spans at least 2 different types.
    [[nodiscard]] static bool hasMultipleUnitTypes(const std::vector<size_t>& chosen_indices,
                                                   const std::vector<UnitCells>& units) {
        if (chosen_indices.size() < 2) {
            return false;
        }
        auto first_type = units[chosen_indices[0]].unit.type;
        for (size_t i = 1; i < chosen_indices.size(); ++i) {
            if (units[chosen_indices[i]].unit.type != first_type) {
                return true;
            }
        }
        return false;
    }

    /// Main search function.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — digit x size x base/cover combination search; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findMutantFish(const BoardData& board,
                                                                 const CandidateGrid& candidates) {
        for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
            // Build all 27 units that have >= 2 candidate cells for this digit
            std::vector<UnitCells> all_units;
            all_units.reserve(27);

            for (size_t i = 0; i < BOARD_SIZE; ++i) {
                Unit u{.type = UnitType::Row, .index = i};
                std::vector<size_t> cells;
                if (getCellsInUnit(board, candidates, u, digit, cells) >= 2) {
                    all_units.push_back({.unit = u, .cells = std::move(cells)});
                }
            }
            for (size_t i = 0; i < BOARD_SIZE; ++i) {
                Unit u{.type = UnitType::Col, .index = i};
                std::vector<size_t> cells;
                if (getCellsInUnit(board, candidates, u, digit, cells) >= 2) {
                    all_units.push_back({.unit = u, .cells = std::move(cells)});
                }
            }
            for (size_t i = 0; i < BOARD_SIZE; ++i) {
                Unit u{.type = UnitType::Box, .index = i};
                std::vector<size_t> cells;
                if (getCellsInUnit(board, candidates, u, digit, cells) >= 2) {
                    all_units.push_back({.unit = u, .cells = std::move(cells)});
                }
            }

            // Also build cover-eligible units (>= 1 candidate cell) for cover enumeration.
            // Cover units only need >= 1 cell to cover base cells.
            std::vector<UnitCells> cover_eligible;
            cover_eligible.reserve(27);
            for (size_t i = 0; i < BOARD_SIZE; ++i) {
                Unit u{.type = UnitType::Row, .index = i};
                std::vector<size_t> cells;
                if (getCellsInUnit(board, candidates, u, digit, cells) >= 1) {
                    cover_eligible.push_back({.unit = u, .cells = std::move(cells)});
                }
            }
            for (size_t i = 0; i < BOARD_SIZE; ++i) {
                Unit u{.type = UnitType::Col, .index = i};
                std::vector<size_t> cells;
                if (getCellsInUnit(board, candidates, u, digit, cells) >= 1) {
                    cover_eligible.push_back({.unit = u, .cells = std::move(cells)});
                }
            }
            for (size_t i = 0; i < BOARD_SIZE; ++i) {
                Unit u{.type = UnitType::Box, .index = i};
                std::vector<size_t> cells;
                if (getCellsInUnit(board, candidates, u, digit, cells) >= 1) {
                    cover_eligible.push_back({.unit = u, .cells = std::move(cells)});
                }
            }

            // Try fish sizes 2, 3, 4
            for (size_t fish_size = 2; fish_size <= 4; ++fish_size) {
                if (all_units.size() < fish_size) {
                    continue;
                }
                auto result = enumerateBaseCombinations(board, candidates, digit, all_units, cover_eligible, fish_size,
                                                        0, {}, {});
                if (result.has_value()) {
                    return result;
                }
            }
        }
        return std::nullopt;
    }

    /// Recursively enumerate base combinations of given size.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — recursive combination enumeration with cover matching; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    enumerateBaseCombinations(const BoardData& board, const CandidateGrid& candidates, int digit,
                              const std::vector<UnitCells>& base_units, const std::vector<UnitCells>& cover_eligible,
                              size_t target_size, size_t start_idx, std::vector<size_t> chosen_bases,
                              std::vector<size_t> all_base_cells) {
        if (chosen_bases.size() == target_size) {
            // Check that base set spans at least 2 different unit types
            if (!hasMultipleUnitTypes(chosen_bases, base_units)) {
                return std::nullopt;
            }

            // Check for overlapping D-cells between base units (degenerate fish)
            size_t total_before_dedup = all_base_cells.size();
            std::ranges::sort(all_base_cells);
            auto last = std::ranges::unique(all_base_cells);
            all_base_cells.erase(last.begin(), last.end());
            if (all_base_cells.size() != total_before_dedup) {
                return std::nullopt;  // overlapping base units
            }

            // Build the set of base unit identifiers to exclude from cover
            return findCoverAndEliminate(board, candidates, digit, base_units, cover_eligible, chosen_bases,
                                         all_base_cells, target_size);
        }

        for (size_t i = start_idx; i < base_units.size(); ++i) {
            chosen_bases.push_back(i);
            auto extended_cells = all_base_cells;
            extended_cells.insert(extended_cells.end(), base_units[i].cells.begin(), base_units[i].cells.end());

            auto result = enumerateBaseCombinations(board, candidates, digit, base_units, cover_eligible, target_size,
                                                    i + 1, chosen_bases, extended_cells);
            if (result.has_value()) {
                return result;
            }
            chosen_bases.pop_back();
        }

        return std::nullopt;
    }

    /// Check if two units are the same (type + index).
    [[nodiscard]] static bool isSameUnit(const Unit& unit_a, const Unit& unit_b) {
        return unit_a.type == unit_b.type && unit_a.index == unit_b.index;
    }

    /// Find N cover units that cover all base cells, then eliminate.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — cover combination search with elimination; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    findCoverAndEliminate(const BoardData& board, const CandidateGrid& candidates, int digit,
                          const std::vector<UnitCells>& base_units, const std::vector<UnitCells>& cover_eligible,
                          const std::vector<size_t>& chosen_bases, const std::vector<size_t>& base_cells,
                          size_t target_size) {
        // Filter cover_eligible to exclude units used as base
        std::vector<size_t> cover_indices;
        cover_indices.reserve(cover_eligible.size());
        for (size_t ci = 0; ci < cover_eligible.size(); ++ci) {
            bool is_base = false;
            for (size_t bi : chosen_bases) {
                if (isSameUnit(cover_eligible[ci].unit, base_units[bi].unit)) {
                    is_base = true;
                    break;
                }
            }
            if (!is_base) {
                cover_indices.push_back(ci);
            }
        }

        if (cover_indices.size() < target_size) {
            return std::nullopt;
        }

        // For each base cell, compute which cover units (indexed into cover_indices) contain it
        // Use a 32-bit bitmask (cover_indices.size() <= 27, fits in uint32_t)
        std::vector<uint32_t> cell_cover_masks(base_cells.size(), 0);
        for (size_t ci_idx = 0; ci_idx < cover_indices.size(); ++ci_idx) {
            size_t ci = cover_indices[ci_idx];
            for (size_t bc_idx = 0; bc_idx < base_cells.size(); ++bc_idx) {
                if (isCoveredBy(base_cells[bc_idx], cover_eligible[ci].unit)) {
                    cell_cover_masks[bc_idx] |= (1U << ci_idx);
                }
            }
        }

        // Enumerate cover combinations of size target_size
        return enumerateCoverCombinations(board, candidates, digit, base_units, cover_eligible, chosen_bases,
                                          base_cells, cover_indices, cell_cover_masks, target_size, 0, 0, {});
    }

    /// Recursively enumerate cover combinations.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — recursive cover enumeration with elimination check; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    enumerateCoverCombinations(const BoardData& board, const CandidateGrid& candidates, int digit,
                               const std::vector<UnitCells>& base_units, const std::vector<UnitCells>& cover_eligible,
                               const std::vector<size_t>& chosen_bases, const std::vector<size_t>& base_cells,
                               const std::vector<size_t>& cover_indices, const std::vector<uint32_t>& cell_cover_masks,
                               size_t target_size, size_t start_idx, uint32_t cover_mask,
                               std::vector<size_t> chosen_cover_idx_positions) {
        if (chosen_cover_idx_positions.size() == target_size) {
            // Check all base cells are covered
            for (size_t ci = 0; ci < base_cells.size(); ++ci) {
                if ((cell_cover_masks[ci] & cover_mask) == 0) {
                    return std::nullopt;  // uncovered base cell
                }
            }

            // Build the actual cover unit indices (into cover_eligible)
            std::vector<size_t> chosen_covers;
            chosen_covers.reserve(target_size);
            for (size_t pos : chosen_cover_idx_positions) {
                chosen_covers.push_back(cover_indices[pos]);
            }

            // Check that cover set spans at least 2 different unit types
            if (!hasMultipleUnitTypes(chosen_covers, cover_eligible)) {
                return std::nullopt;
            }

            // Compute eliminations
            return computeEliminations(board, candidates, digit, base_units, cover_eligible, chosen_bases, base_cells,
                                       chosen_covers);
        }

        for (size_t i = start_idx; i < cover_indices.size(); ++i) {
            chosen_cover_idx_positions.push_back(i);
            uint32_t new_mask = cover_mask | (1U << i);

            auto result = enumerateCoverCombinations(board, candidates, digit, base_units, cover_eligible, chosen_bases,
                                                     base_cells, cover_indices, cell_cover_masks, target_size, i + 1,
                                                     new_mask, chosen_cover_idx_positions);
            if (result.has_value()) {
                return result;
            }
            chosen_cover_idx_positions.pop_back();
        }

        return std::nullopt;
    }

    /// Compute eliminations for a valid Mutant Fish.
    [[nodiscard]] static std::optional<SolveStep>
    computeEliminations(const BoardData& board, const CandidateGrid& candidates, int digit,
                        const std::vector<UnitCells>& base_units, const std::vector<UnitCells>& cover_eligible,
                        const std::vector<size_t>& chosen_bases, const std::vector<size_t>& base_cells,
                        const std::vector<size_t>& chosen_covers) {
        // Collect all cover cells
        std::vector<size_t> cover_cells;
        for (size_t ci : chosen_covers) {
            cover_cells.insert(cover_cells.end(), cover_eligible[ci].cells.begin(), cover_eligible[ci].cells.end());
        }
        std::ranges::sort(cover_cells);
        auto last = std::ranges::unique(cover_cells);
        cover_cells.erase(last.begin(), last.end());

        // Base cells set for quick lookup
        auto isBaseCell = [&base_cells](size_t cell) {
            return std::ranges::find(base_cells, cell) != base_cells.end();
        };

        std::vector<Elimination> eliminations;
        for (size_t cell : cover_cells) {
            if (isBaseCell(cell)) {
                continue;
            }
            size_t row = cell / BOARD_SIZE;
            size_t col = cell % BOARD_SIZE;
            if (board[row][col] == EMPTY_CELL && candidates.isAllowed(row, col, digit)) {
                eliminations.push_back(Elimination{.position = Position{.row = row, .col = col}, .value = digit});
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        // Build explanation
        std::string base_desc;
        for (size_t i = 0; i < chosen_bases.size(); ++i) {
            if (i > 0) {
                base_desc += ", ";
            }
            base_desc += formatUnit(base_units[chosen_bases[i]].unit);
        }

        std::string cover_desc;
        for (size_t i = 0; i < chosen_covers.size(); ++i) {
            if (i > 0) {
                cover_desc += ", ";
            }
            cover_desc += formatUnit(cover_eligible[chosen_covers[i]].unit);
        }

        auto explanation = fmt::format("Mutant Fish on {}: base {{{}}} / cover {{{}}} — eliminates {} from {} cell(s)",
                                       digit, base_desc, cover_desc, digit, eliminations.size());

        // Collect pattern positions
        std::vector<Position> positions;
        positions.reserve(base_cells.size());
        for (size_t cell : base_cells) {
            positions.push_back(Position{.row = cell / BOARD_SIZE, .col = cell % BOARD_SIZE});
        }

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::MutantFish,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::MutantFish),
            .explanation_data = {.positions = positions,
                                 .values = {digit},
                                 .position_roles = std::vector<CellRole>(positions.size(), CellRole::Pattern)}};
    }

    /// Format a unit for explanation text.
    [[nodiscard]] static std::string formatUnit(const Unit& unit) {
        switch (unit.type) {
            case UnitType::Row:
                return fmt::format("R{}", unit.index + 1);
            case UnitType::Col:
                return fmt::format("C{}", unit.index + 1);
            case UnitType::Box:
                return fmt::format("B{}", unit.index + 1);
        }
        return "?";
    }
};

}  // namespace sudoku::core
