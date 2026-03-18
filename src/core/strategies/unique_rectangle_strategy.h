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
#include <optional>
#include <vector>

#include <fmt/format.h>

namespace sudoku::core {

/// Strategy for finding Unique Rectangle patterns (Types 1-4).
/// A Unique Rectangle has 4 cells forming a rectangle across exactly 2 boxes.
/// Types eliminate candidates to avoid the "deadly pattern" that would imply multiple solutions.
class UniqueRectangleStrategy : public ISolvingStrategy, protected StrategyBase {
public:
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — dispatches to UR type 1-4 checks via bivalue pair enumeration; nesting is inherent
    [[nodiscard]] std::optional<SolveStep> findStep(const BoardData& board,
                                                    const CandidateGrid& candidates) const override {
        // Find all bivalue cells
        std::vector<Position> bivalue_cells;
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (board[row][col] == EMPTY_CELL && candidates.countPossibleValues(row, col) == 2) {
                    bivalue_cells.push_back(Position{.row = row, .col = col});
                }
            }
        }

        // Try pairs of bivalue cells in the same row with same candidates
        for (size_t i = 0; i < bivalue_cells.size(); ++i) {
            for (size_t j = i + 1; j < bivalue_cells.size(); ++j) {
                const auto& c1 = bivalue_cells[i];
                const auto& c2 = bivalue_cells[j];

                if (c1.row != c2.row) {
                    continue;
                }
                if (sameBox(c1, c2)) {
                    continue;
                }

                auto cands1 = getCandidates(candidates, c1.row, c1.col);
                auto cands2 = getCandidates(candidates, c2.row, c2.col);

                if (cands1[0] != cands2[0] || cands1[1] != cands2[1]) {
                    continue;
                }

                int val_a = cands1[0];
                int val_b = cands1[1];

                auto result = findRectangleCompletion(board, candidates, c1, c2, val_a, val_b);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        // Also try pairs in the same column
        for (size_t i = 0; i < bivalue_cells.size(); ++i) {
            for (size_t j = i + 1; j < bivalue_cells.size(); ++j) {
                const auto& c1 = bivalue_cells[i];
                const auto& c2 = bivalue_cells[j];

                if (c1.col != c2.col) {
                    continue;
                }
                if (sameBox(c1, c2)) {
                    continue;
                }

                auto cands1 = getCandidates(candidates, c1.row, c1.col);
                auto cands2 = getCandidates(candidates, c2.row, c2.col);

                if (cands1[0] != cands2[0] || cands1[1] != cands2[1]) {
                    continue;
                }

                int val_a = cands1[0];
                int val_b = cands1[1];

                auto result = findRectangleCompletionCol(board, candidates, c1, c2, val_a, val_b);
                if (result.has_value()) {
                    return result;
                }
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] SolvingTechnique getTechnique() const override {
        return SolvingTechnique::UniqueRectangle;
    }

    [[nodiscard]] std::string_view getName() const override {
        return "Unique Rectangle";
    }

    [[nodiscard]] double getDifficultyRating() const override {
        return getTechniqueRating(SolvingTechnique::UniqueRectangle);
    }

private:
    // CPD-OFF — UR type 1-4 share structural patterns (floor analysis, bivalue matching) that differ in elimination
    // logic
    /// Given two bivalue cells {A,B} in the same row, find a row that completes a UR.
    [[nodiscard]] static std::optional<SolveStep> findRectangleCompletion(const BoardData& board,
                                                                          const CandidateGrid& candidates,
                                                                          const Position& c1, const Position& c2,
                                                                          int val_a, int val_b) {
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            if (row == c1.row) {
                continue;
            }

            Position c3{.row = row, .col = c1.col};
            Position c4{.row = row, .col = c2.col};

            auto result = checkRectangle(board, candidates, c1, c2, c3, c4, val_a, val_b);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Column-based: two bivalue cells in same column, find column that completes rectangle.
    [[nodiscard]] static std::optional<SolveStep> findRectangleCompletionCol(const BoardData& board,
                                                                             const CandidateGrid& candidates,
                                                                             const Position& c1, const Position& c2,
                                                                             int val_a, int val_b) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            if (col == c1.col) {
                continue;
            }

            Position c3{.row = c1.row, .col = col};
            Position c4{.row = c2.row, .col = col};

            auto result = checkRectangle(board, candidates, c1, c2, c3, c4, val_a, val_b);
            if (result.has_value()) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// Validates that c3 and c4 are empty, contain A and B, and the 4 cells span exactly 2 boxes.
    [[nodiscard]] static bool isValidRectangle(const BoardData& board, const CandidateGrid& candidates,
                                               const Position& c1, const Position& c2, const Position& c3,
                                               const Position& c4, int val_a, int val_b) {
        if (board[c3.row][c3.col] != EMPTY_CELL || board[c4.row][c4.col] != EMPTY_CELL) {
            return false;
        }
        if (!candidates.isAllowed(c3.row, c3.col, val_a) || !candidates.isAllowed(c3.row, c3.col, val_b)) {
            return false;
        }
        if (!candidates.isAllowed(c4.row, c4.col, val_a) || !candidates.isAllowed(c4.row, c4.col, val_b)) {
            return false;
        }
        return countUniqueBoxes(c1, c2, c3, c4) == 2;
    }

    /// Checks if a cell is bivalue with exactly {A,B}.
    [[nodiscard]] static bool isBivalueAB(const CandidateGrid& candidates, const Position& cell, int val_a, int val_b) {
        if (candidates.countPossibleValues(cell.row, cell.col) != 2) {
            return false;
        }
        return candidates.isAllowed(cell.row, cell.col, val_a) && candidates.isAllowed(cell.row, cell.col, val_b);
    }

    /// Gets extra candidates beyond {A,B} as a bitmask.
    [[nodiscard]] static uint16_t getExtrasMask(const CandidateGrid& candidates, const Position& cell, int val_a,
                                                int val_b) {
        uint16_t mask = candidates.getPossibleValuesMask(cell.row, cell.col);
        mask &= ~valueToBit(val_a);
        mask &= ~valueToBit(val_b);
        return mask;
    }

    /// Identifies roof (bivalue {A,B}) and floor (extra candidates) cells from the 4 UR cells.
    /// Returns {roof_cells, floor_cells}.
    [[nodiscard]] static std::pair<std::vector<Position>, std::vector<Position>>
    classifyCells(const CandidateGrid& candidates, const Position& c1, const Position& c2, const Position& c3,
                  const Position& c4, int val_a, int val_b) {
        std::vector<Position> roof;
        std::vector<Position> floor;
        for (const auto& cell : {c1, c2, c3, c4}) {
            if (isBivalueAB(candidates, cell, val_a, val_b)) {
                roof.push_back(cell);
            } else {
                floor.push_back(cell);
            }
        }
        return {roof, floor};
    }

    /// Determines the shared unit type between two cells.
    [[nodiscard]] static RegionType sharedUnitType(const Position& a, const Position& b) {
        if (a.row == b.row) {
            return RegionType::Row;
        }
        if (a.col == b.col) {
            return RegionType::Col;
        }
        if (sameBox(a, b)) {
            return RegionType::Box;
        }
        return RegionType::None;
    }

    /// Try all UR types for the given 4 cells. c1 and c2 are bivalue {A,B}.
    [[nodiscard]] static std::optional<SolveStep> checkRectangle(const BoardData& board,
                                                                 const CandidateGrid& candidates, const Position& c1,
                                                                 const Position& c2, const Position& c3,
                                                                 const Position& c4, int val_a, int val_b) {
        auto result = checkType1Rectangle(board, candidates, c1, c2, c3, c4, val_a, val_b);
        if (result.has_value()) {
            return result;
        }
        result = checkType2Rectangle(board, candidates, c1, c2, c3, c4, val_a, val_b);
        if (result.has_value()) {
            return result;
        }
        result = checkType4Rectangle(board, candidates, c1, c2, c3, c4, val_a, val_b);
        if (result.has_value()) {
            return result;
        }
        return checkType3Rectangle(board, candidates, c1, c2, c3, c4, val_a, val_b);
    }

    /// Type 1: exactly 3 cells are bivalue {A,B}, the 4th (floor) has extras → eliminate A,B from floor.
    [[nodiscard]] static std::optional<SolveStep>
    checkType1Rectangle(const BoardData& board, const CandidateGrid& candidates, const Position& c1, const Position& c2,
                        const Position& c3, const Position& c4, int val_a, int val_b) {
        if (!isValidRectangle(board, candidates, c1, c2, c3, c4, val_a, val_b)) {
            return std::nullopt;
        }

        auto [roof, floor] = classifyCells(candidates, c1, c2, c3, c4, val_a, val_b);
        if (roof.size() != 3 || floor.size() != 1) {
            return std::nullopt;
        }

        const auto& floor_cell = floor[0];
        std::vector<Elimination> eliminations;
        eliminations.push_back(Elimination{.position = floor_cell, .value = val_a});
        eliminations.push_back(Elimination{.position = floor_cell, .value = val_b});

        auto explanation = fmt::format("Unique Rectangle: cells {}, {}, {}, {} with values {{{},{}}} — eliminates "
                                       "{},{} from {} to avoid deadly pattern",
                                       formatPosition(c1), formatPosition(c2), formatPosition(c3), formatPosition(c4),
                                       val_a, val_b, val_a, val_b, formatPosition(floor_cell));

        std::vector<Position> all_positions = {c1, c2, c3, c4};
        std::vector<CellRole> ur_roles = {isBivalueAB(candidates, c1, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                                          isBivalueAB(candidates, c2, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                                          isBivalueAB(candidates, c3, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                                          isBivalueAB(candidates, c4, val_a, val_b) ? CellRole::Roof : CellRole::Floor};

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::UniqueRectangle,
                         .position = floor_cell,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::UniqueRectangle),
                         .explanation_data = {.positions = all_positions,
                                              .values = {val_a, val_b},
                                              .position_roles = std::move(ur_roles)}};
    }

    /// Type 2: 2 roof cells bivalue {A,B}, 2 floor cells have {A,B,C} (same extra C).
    /// Floor cells share a unit → eliminate C from other cells in that unit.
    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity) — UR type 2 checks floor cell extras across shared units; nesting is inherent
    checkType2Rectangle(const BoardData& board, const CandidateGrid& candidates, const Position& c1, const Position& c2,
                        const Position& c3, const Position& c4, int val_a, int val_b) {
        if (!isValidRectangle(board, candidates, c1, c2, c3, c4, val_a, val_b)) {
            return std::nullopt;
        }

        auto [roof, floor] = classifyCells(candidates, c1, c2, c3, c4, val_a, val_b);
        if (roof.size() != 2 || floor.size() != 2) {
            return std::nullopt;
        }

        // Both floor cells must have exactly 3 candidates: {A, B, C}
        if (candidates.countPossibleValues(floor[0].row, floor[0].col) != 3 ||
            candidates.countPossibleValues(floor[1].row, floor[1].col) != 3) {
            return std::nullopt;
        }

        // Both must have the same single extra candidate C
        uint16_t extras0 = getExtrasMask(candidates, floor[0], val_a, val_b);
        uint16_t extras1 = getExtrasMask(candidates, floor[1], val_a, val_b);
        if (extras0 != extras1 || extras0 == 0) {
            return std::nullopt;
        }

        // Extract the single extra value C
        int val_c = 0;
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((extras0 & valueToBit(v)) != 0) {
                val_c = v;
                break;
            }
        }

        // Floor cells must share a unit
        RegionType unit_type = sharedUnitType(floor[0], floor[1]);
        if (unit_type == RegionType::None) {
            return std::nullopt;
        }

        // Find cells in the shared unit that have C as candidate (excluding floor cells)
        size_t unit_index = getUnitIndex(unit_type, floor[0]);
        auto unit_cells = getEmptyCellsInUnit(board, unit_type, unit_index);

        std::vector<Elimination> eliminations;
        for (const auto& cell : unit_cells) {
            if ((cell.row == floor[0].row && cell.col == floor[0].col) ||
                (cell.row == floor[1].row && cell.col == floor[1].col)) {
                continue;
            }
            if (candidates.isAllowed(cell.row, cell.col, val_c)) {
                eliminations.push_back(Elimination{.position = cell, .value = val_c});
            }
        }

        if (eliminations.empty()) {
            return std::nullopt;
        }

        auto explanation = fmt::format("Unique Rectangle Type 2: cells {}, {}, {}, {} with values {{{},{}}} — "
                                       "eliminates {} from cells seeing both floor cells in shared {}",
                                       formatPosition(c1), formatPosition(c2), formatPosition(c3), formatPosition(c4),
                                       val_a, val_b, val_c, formatRegion(unit_type, unit_index));

        std::vector<Position> all_positions = {c1, c2, c3, c4};
        std::vector<CellRole> ur_roles = {isBivalueAB(candidates, c1, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                                          isBivalueAB(candidates, c2, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                                          isBivalueAB(candidates, c3, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                                          isBivalueAB(candidates, c4, val_a, val_b) ? CellRole::Roof : CellRole::Floor};

        return SolveStep{.type = SolveStepType::Elimination,
                         .technique = SolvingTechnique::UniqueRectangle,
                         .position = eliminations[0].position,
                         .value = 0,
                         .eliminations = eliminations,
                         .explanation = explanation,
                         .rating = getTechniqueRating(SolvingTechnique::UniqueRectangle),
                         .explanation_data = {.positions = all_positions,
                                              .values = {val_a, val_b, val_c},
                                              .secondary_region_type = unit_type,
                                              .secondary_region_index = unit_index,
                                              .technique_subtype = 1,  // Type 2
                                              .position_roles = std::move(ur_roles)}};
    }

    /// Type 3: 2 roof cells bivalue {A,B}, 2 floor cells have extras beyond {A,B}.
    /// Extras from both floor cells form a naked pair with another cell in the shared unit.
    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — UR type 3 enumerates naked pair candidates across floor units; nesting is inherent
    checkType3Rectangle(const BoardData& board, const CandidateGrid& candidates, const Position& c1, const Position& c2,
                        const Position& c3, const Position& c4, int val_a, int val_b) {
        if (!isValidRectangle(board, candidates, c1, c2, c3, c4, val_a, val_b)) {
            return std::nullopt;
        }

        auto [roof, floor] = classifyCells(candidates, c1, c2, c3, c4, val_a, val_b);
        if (roof.size() != 2 || floor.size() != 2) {
            return std::nullopt;
        }

        // Collect the union of extras from both floor cells
        uint16_t extras_union =
            getExtrasMask(candidates, floor[0], val_a, val_b) | getExtrasMask(candidates, floor[1], val_a, val_b);
        if (extras_union == 0) {
            return std::nullopt;
        }

        // Count extras — for naked pair, need exactly 2 extras
        int extras_count = 0;
        for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
            if ((extras_union & valueToBit(v)) != 0) {
                ++extras_count;
            }
        }
        if (extras_count > 2) {
            return std::nullopt;  // Only implement naked-pair subset case
        }

        // Floor cells must share a unit
        RegionType unit_type = sharedUnitType(floor[0], floor[1]);
        if (unit_type == RegionType::None) {
            return std::nullopt;
        }

        size_t unit_index = getUnitIndex(unit_type, floor[0]);
        auto unit_cells = getEmptyCellsInUnit(board, unit_type, unit_index);

        // Find a cell in the unit (not a floor cell) whose candidates are a subset of extras_union
        // This cell + the two floor cells' extras form a naked pair/triple
        for (const auto& partner : unit_cells) {
            if ((partner.row == floor[0].row && partner.col == floor[0].col) ||
                (partner.row == floor[1].row && partner.col == floor[1].col)) {
                continue;
            }

            uint16_t partner_mask = candidates.getPossibleValuesMask(partner.row, partner.col);
            // Partner candidates must be a non-empty subset of extras_union
            if (partner_mask == 0 || (partner_mask & ~extras_union) != 0) {
                continue;
            }

            // Found a naked subset partner — eliminate extras from other cells in the unit
            std::vector<Elimination> eliminations;
            for (const auto& cell : unit_cells) {
                if ((cell.row == floor[0].row && cell.col == floor[0].col) ||
                    (cell.row == floor[1].row && cell.col == floor[1].col) ||
                    (cell.row == partner.row && cell.col == partner.col)) {
                    continue;
                }
                for (int v = MIN_VALUE; v <= MAX_VALUE; ++v) {
                    if ((extras_union & valueToBit(v)) != 0 && candidates.isAllowed(cell.row, cell.col, v)) {
                        eliminations.push_back(Elimination{.position = cell, .value = v});
                    }
                }
            }

            if (eliminations.empty()) {
                continue;
            }

            auto explanation = fmt::format("Unique Rectangle Type 3: cells {}, {}, {}, {} with values {{{},{}}} — "
                                           "floor extras form naked subset in {}",
                                           formatPosition(c1), formatPosition(c2), formatPosition(c3),
                                           formatPosition(c4), val_a, val_b, formatRegion(unit_type, unit_index));

            std::vector<Position> all_positions = {c1, c2, c3, c4};
            std::vector<CellRole> ur_roles = {
                isBivalueAB(candidates, c1, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                isBivalueAB(candidates, c2, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                isBivalueAB(candidates, c3, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                isBivalueAB(candidates, c4, val_a, val_b) ? CellRole::Roof : CellRole::Floor};

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::UniqueRectangle,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::UniqueRectangle),
                             .explanation_data = {.positions = all_positions,
                                                  .values = {val_a, val_b},
                                                  .secondary_region_type = unit_type,
                                                  .secondary_region_index = unit_index,
                                                  .technique_subtype = 2,  // Type 3
                                                  .position_roles = std::move(ur_roles)}};
        }

        return std::nullopt;
    }

    /// Type 4: 2 roof cells bivalue {A,B}, 2 floor cells contain {A,B,+extras}.
    /// Floor cells share a unit where A (or B) forms a strong link (only in those 2 cells).
    /// → Eliminate the OTHER value (B or A) from both floor cells.
    [[nodiscard]] static std::optional<SolveStep>
    // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — UR type 4 checks strong link across floor units; nesting is inherent
    checkType4Rectangle(const BoardData& board, const CandidateGrid& candidates, const Position& c1, const Position& c2,
                        const Position& c3, const Position& c4, int val_a, int val_b) {
        if (!isValidRectangle(board, candidates, c1, c2, c3, c4, val_a, val_b)) {
            return std::nullopt;
        }

        auto [roof, floor] = classifyCells(candidates, c1, c2, c3, c4, val_a, val_b);
        if (roof.size() != 2 || floor.size() != 2) {
            return std::nullopt;
        }

        // Floor cells must share a unit
        RegionType unit_type = sharedUnitType(floor[0], floor[1]);
        if (unit_type == RegionType::None) {
            return std::nullopt;
        }

        size_t unit_index = getUnitIndex(unit_type, floor[0]);
        auto unit_cells = getEmptyCellsInUnit(board, unit_type, unit_index);

        // Check if A or B forms a strong link in this unit (only appears in the 2 floor cells)
        for (int strong_val : {val_a, val_b}) {
            int other_val = (strong_val == val_a) ? val_b : val_a;

            // Count how many cells in the unit have strong_val as candidate
            int count = 0;
            for (const auto& cell : unit_cells) {
                if (candidates.isAllowed(cell.row, cell.col, strong_val)) {
                    ++count;
                }
            }

            // Strong link: strong_val appears in exactly 2 cells in the unit (the floor cells)
            if (count != 2) {
                continue;
            }

            // Verify it's exactly the floor cells
            bool f0_has = candidates.isAllowed(floor[0].row, floor[0].col, strong_val);
            bool f1_has = candidates.isAllowed(floor[1].row, floor[1].col, strong_val);
            if (!f0_has || !f1_has) {
                continue;
            }

            // Eliminate other_val from both floor cells
            std::vector<Elimination> eliminations;
            if (candidates.isAllowed(floor[0].row, floor[0].col, other_val)) {
                eliminations.push_back(Elimination{.position = floor[0], .value = other_val});
            }
            if (candidates.isAllowed(floor[1].row, floor[1].col, other_val)) {
                eliminations.push_back(Elimination{.position = floor[1], .value = other_val});
            }

            if (eliminations.empty()) {
                continue;
            }

            auto explanation =
                fmt::format("Unique Rectangle Type 4: cells {}, {}, {}, {} with values {{{},{}}} — strong link on {} "
                            "in {} eliminates {} from floor cells",
                            formatPosition(c1), formatPosition(c2), formatPosition(c3), formatPosition(c4), val_a,
                            val_b, strong_val, formatRegion(unit_type, unit_index), other_val);

            std::vector<Position> all_positions = {c1, c2, c3, c4};
            std::vector<CellRole> ur_roles = {
                isBivalueAB(candidates, c1, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                isBivalueAB(candidates, c2, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                isBivalueAB(candidates, c3, val_a, val_b) ? CellRole::Roof : CellRole::Floor,
                isBivalueAB(candidates, c4, val_a, val_b) ? CellRole::Roof : CellRole::Floor};

            return SolveStep{.type = SolveStepType::Elimination,
                             .technique = SolvingTechnique::UniqueRectangle,
                             .position = eliminations[0].position,
                             .value = 0,
                             .eliminations = eliminations,
                             .explanation = explanation,
                             .rating = getTechniqueRating(SolvingTechnique::UniqueRectangle),
                             .explanation_data = {.positions = all_positions,
                                                  .values = {val_a, val_b, strong_val, other_val},
                                                  .secondary_region_type = unit_type,
                                                  .secondary_region_index = unit_index,
                                                  .technique_subtype = 3,  // Type 4
                                                  .position_roles = std::move(ur_roles)}};
        }

        return std::nullopt;
    }
    // CPD-ON
};

}  // namespace sudoku::core
