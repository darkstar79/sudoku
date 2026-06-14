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

/// Strategy for finding Franken Fish patterns.
/// A Franken Fish generalizes standard fish (X-Wing, Swordfish, Jellyfish) by allowing
/// base and cover sets to be a mix of rows/columns and boxes.
/// At least one base or cover set must be a box (otherwise it's a standard fish).
/// For each digit D and size N (2-4):
///   - Enumerate N base sets from {rows ∪ boxes} or {cols ∪ boxes}
///   - Find N cover sets from the complementary type
///   - All base cells must be covered; eliminate D from cover cells outside base
class FrankenFishStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        return findFrankenFish(board, candidates);
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::FrankenFish;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Franken Fish";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::FrankenFish);
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

    /// Get the cell mask (bitmask over 81 cells) for a unit where digit D is a candidate.
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

    /// Main search function.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — digit×size×base/cover combination search; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep> findFrankenFish(const BoardData& board,
                                                                  const CandidateGrid& candidates) {
        for (int digit = MIN_VALUE; digit <= MAX_VALUE; ++digit) {
            // Try rows+boxes as base, cols as cover
            auto result = tryFrankenFishOrientation(board, candidates, digit, true);
            if (result.has_value()) {
                return result;
            }
            // Try cols+boxes as base, rows as cover
            result = tryFrankenFishOrientation(board, candidates, digit, false);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// A unit with its precomputed candidate cells.
    struct UnitCells {
        Unit unit;
        std::vector<size_t> cells;
    };

    /// Try Franken Fish with one orientation.
    /// @param row_base If true: base = {rows ∪ boxes}, cover = {cols}
    ///                 If false: base = {cols ∪ boxes}, cover = {rows}
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — base/cover enumeration; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    tryFrankenFishOrientation(const BoardData& board, const CandidateGrid& candidates, int digit, bool row_base) {
        // Cover/base disjointness invariant (Issue #21): in this orientation the cover units are
        // ALWAYS the complementary line type only (cols when base = rows+boxes, rows when base =
        // cols+boxes) — boxes are never cover-eligible. Hence:
        //   * a cover unit can never EQUAL a chosen base unit (different UnitType), and
        //   * cover units are mutually parallel lines, so two covers can never share a cell.
        // Both Franken degeneracies the review worried about are therefore impossible by
        // construction; no explicit rejection is needed (the symmetric Mutant guard lives in
        // mutant_fish_strategy.h::findCoverAndEliminate).
        //
        // Base candidates: lines + boxes
        std::vector<UnitCells> base_units;
        for (size_t i = 0; i < BOARD_SIZE; ++i) {
            Unit u{.type = row_base ? UnitType::Row : UnitType::Col, .index = i};
            std::vector<size_t> cells;
            if (getCellsInUnit(board, candidates, u, digit, cells) >= 2) {
                base_units.push_back({.unit = u, .cells = std::move(cells)});
            }
        }
        for (size_t i = 0; i < BOARD_SIZE; ++i) {
            Unit u{.type = UnitType::Box, .index = i};
            std::vector<size_t> cells;
            if (getCellsInUnit(board, candidates, u, digit, cells) >= 2) {
                base_units.push_back({.unit = u, .cells = std::move(cells)});
            }
        }

        // Cover candidates: complementary lines
        std::vector<UnitCells> cover_units;
        for (size_t i = 0; i < BOARD_SIZE; ++i) {
            Unit u{.type = row_base ? UnitType::Col : UnitType::Row, .index = i};
            std::vector<size_t> cells;
            if (getCellsInUnit(board, candidates, u, digit, cells) >= 1) {
                cover_units.push_back({.unit = u, .cells = std::move(cells)});
            }
        }

        // Try fish sizes 2, 3, 4
        for (size_t fish_size = 2; fish_size <= 4; ++fish_size) {
            if (base_units.size() < fish_size || cover_units.size() < fish_size) {
                continue;
            }

            // Enumerate base combinations
            auto result =
                enumerateBaseCombinations(board, candidates, digit, base_units, cover_units, fish_size, 0, {}, {});
            if (result.has_value()) {
                return result;
            }
        }

        return std::nullopt;
    }

    /// Recursively enumerate base combinations of given size.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — recursive combination enumeration with cover matching; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    enumerateBaseCombinations(const BoardData& board, const CandidateGrid& candidates, int digit,
                              const std::vector<struct UnitCells>& base_units,
                              const std::vector<struct UnitCells>& cover_units, size_t target_size, size_t start_idx,
                              std::vector<size_t> chosen_bases, std::vector<size_t> all_base_cells) {
        if (chosen_bases.size() == target_size) {
            // Check that at least one base is a box (otherwise it's a standard fish)
            bool has_box = false;
            for (size_t idx : chosen_bases) {
                if (base_units[idx].unit.type == UnitType::Box) {
                    has_box = true;
                    break;
                }
            }
            if (!has_box) {
                return std::nullopt;
            }

            // Check for overlapping D-cells between base units.
            // If two base units share a D-candidate cell, one D-position could
            // satisfy both units, invalidating the fish argument.
            size_t total_before_dedup = all_base_cells.size();
            std::ranges::sort(all_base_cells);
            auto last = std::ranges::unique(all_base_cells);
            all_base_cells.erase(last.begin(), last.end());
            if (all_base_cells.size() != total_before_dedup) {
                return std::nullopt;  // overlapping base units — degenerate fish
            }

            // Find minimal cover: for each base cell, which cover units contain it?
            // We need exactly target_size cover units that cover ALL base cells.
            return findCoverAndEliminate(board, candidates, digit, base_units, cover_units, chosen_bases,
                                         all_base_cells, target_size);
        }

        for (size_t i = start_idx; i < base_units.size(); ++i) {
            chosen_bases.push_back(i);
            auto extended_cells = all_base_cells;
            extended_cells.insert(extended_cells.end(), base_units[i].cells.begin(), base_units[i].cells.end());

            auto result = enumerateBaseCombinations(board, candidates, digit, base_units, cover_units, target_size,
                                                    i + 1, chosen_bases, extended_cells);
            if (result.has_value()) {
                return result;
            }
            chosen_bases.pop_back();
        }

        return std::nullopt;
    }

    /// Find N cover units that cover all base cells, then eliminate.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — cover combination search with elimination; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    findCoverAndEliminate(const BoardData& board, const CandidateGrid& candidates, int digit,
                          const std::vector<struct UnitCells>& base_units,
                          const std::vector<struct UnitCells>& cover_units, const std::vector<size_t>& chosen_bases,
                          const std::vector<size_t>& base_cells, size_t target_size) {
        // For each base cell, compute which cover units contain it (as bitmask).
        // Bitmask width = number of cover units. Franken covers are the complementary LINE type
        // only (at most BOARD_SIZE = 9 of them), so one bit each fits a uint16_t. (Cf.
        // mutant_fish_strategy.h, where covers may be any of the 27 units and a uint32_t is
        // needed.) (Issue #21 MED)
        static_assert(BOARD_SIZE <= 16, "cover bitmask (uint16_t) must hold one bit per cover line");
        std::vector<uint16_t> cell_cover_masks(base_cells.size(), 0);
        for (size_t ci = 0; ci < base_cells.size(); ++ci) {
            for (size_t ui = 0; ui < cover_units.size(); ++ui) {
                if (isCoveredBy(base_cells[ci], cover_units[ui].unit)) {
                    cell_cover_masks[ci] |= static_cast<uint16_t>(1U << ui);
                }
            }
        }

        // Enumerate cover combinations of size target_size
        return enumerateCoverCombinations(board, candidates, digit, base_units, cover_units, chosen_bases, base_cells,
                                          cell_cover_masks, target_size, 0, 0, {});
    }

    /// Recursively enumerate cover combinations.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — recursive cover enumeration with elimination check; nesting is inherent
    [[nodiscard]] static std::optional<SolveStep>
    enumerateCoverCombinations(const BoardData& board, const CandidateGrid& candidates, int digit,
                               const std::vector<struct UnitCells>& base_units,
                               const std::vector<struct UnitCells>& cover_units,
                               const std::vector<size_t>& chosen_bases, const std::vector<size_t>& base_cells,
                               const std::vector<uint16_t>& cell_cover_masks, size_t target_size, size_t start_idx,
                               uint16_t cover_mask, std::vector<size_t> chosen_covers) {
        if (chosen_covers.size() == target_size) {
            // Check all base cells are covered
            for (size_t ci = 0; ci < base_cells.size(); ++ci) {
                if ((cell_cover_masks[ci] & cover_mask) == 0) {
                    return std::nullopt;  // uncovered base cell
                }
            }

            // Compute eliminations: cells in cover units but NOT in base units
            return computeEliminations(board, candidates, digit, base_units, cover_units, chosen_bases, base_cells,
                                       chosen_covers);
        }

        for (size_t i = start_idx; i < cover_units.size(); ++i) {
            chosen_covers.push_back(i);
            uint16_t new_mask = cover_mask | static_cast<uint16_t>(1U << i);

            auto result =
                enumerateCoverCombinations(board, candidates, digit, base_units, cover_units, chosen_bases, base_cells,
                                           cell_cover_masks, target_size, i + 1, new_mask, chosen_covers);
            if (result.has_value()) {
                return result;
            }
            chosen_covers.pop_back();
        }

        return std::nullopt;
    }

    /// Compute eliminations for a valid Franken Fish.
    [[nodiscard]] static std::optional<SolveStep>
    computeEliminations(const BoardData& board, const CandidateGrid& candidates, int digit,
                        const std::vector<struct UnitCells>& base_units,
                        const std::vector<struct UnitCells>& cover_units, const std::vector<size_t>& chosen_bases,
                        const std::vector<size_t>& base_cells, const std::vector<size_t>& chosen_covers) {
        // Collect all cover cells
        std::vector<size_t> cover_cells;
        for (size_t ci : chosen_covers) {
            cover_cells.insert(cover_cells.end(), cover_units[ci].cells.begin(), cover_units[ci].cells.end());
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
            cover_desc += formatUnit(cover_units[chosen_covers[i]].unit);
        }

        auto explanation = fmt::format("Franken Fish on {}: base {{{}}} / cover {{{}}} — eliminates {} from {} cell(s)",
                                       digit, base_desc, cover_desc, digit, eliminations.size());

        // Collect pattern positions
        std::vector<Position> positions;
        positions.reserve(base_cells.size());
        for (size_t cell : base_cells) {
            positions.push_back(Position{.row = cell / BOARD_SIZE, .col = cell % BOARD_SIZE});
        }

        return SolveStep{
            .type = SolveStepType::Elimination,
            .technique = SolvingTechnique::FrankenFish,
            .position = eliminations[0].position,
            .value = 0,
            .eliminations = eliminations,
            .explanation = explanation,
            .rating = getTechniqueRating(SolvingTechnique::FrankenFish,
                                         RatingContext{.size_or_length = static_cast<int>(chosen_bases.size())}),
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
