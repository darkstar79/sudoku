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

#include "core/i_localization_manager.h"

#include <string>
#include <unordered_map>

namespace sudoku::core {

/// Lightweight mock for testing code that depends on ILocalizationManager.
/// Returns English format templates for keys used with fmt::runtime(),
/// and the key itself for all other keys (allowing tests to verify lookups).
class MockLocalizationManager final : public ILocalizationManager {
public:
    MockLocalizationManager() {
        // Keys that are used as fmt format templates need English values
        // so fmt::runtime() doesn't crash on the raw key string.
        // clang-format off
        format_templates_ = {
            // Position & region formatting
            {"position.fmt", "R{0}C{1}"},
            {"region.row", "row"},
            {"region.column", "column"},
            {"region.box", "box"},
            {"region.unknown", "unknown region"},
            // Rating format
            {"rating.format", "SE {0}"},
            // Coaching feedback messages
            {"coaching.check_correct", "Correct! You found all {0}/{1}."},
            {"coaching.check_partial", "{0}/{1} correct, {2} missed."},
            {"coaching.check_wrong", "Some actions were incorrect. {0}/{1} correct, {2} wrong."},
            {"coaching.check_zero", "0/{0} correct — try making some changes first."},
            {"coaching.level_header", "Level {0}/3"},
            // Hint suggestion format
            {"hint.suggestion_place", "Suggestion: Place {0} at R{1}C{2}"},
            // Training hints — Singles
            {"hint.singles.l1", "Look at cell {0}."},
            {"hint.singles.l2_region", "Focus on {0} — count the candidates."},
            {"hint.singles.l2_no_region", "Count the candidates in cell {0}."},
            {"hint.singles.l3", "The value is {0}."},
            // Training hints — Subsets
            {"hint.subsets.l1_region", "Focus on {0}."},
            {"hint.subsets.l1_no_region", "Look for cells that share the same candidates in a unit."},
            {"hint.subsets.l2_values", "These cells form a [{0}] subset. Values in the subset can only go in these cells — eliminate them from other cells in the region."},
            {"hint.subsets.l2_no_values", "These cells form the subset. Values in the subset can only go in these cells — eliminate them from other cells in the region."},
            {"hint.subsets.l3", "Eliminate candidates from cells that see all subset cells."},
            // Training hints — Intersections
            {"hint.intersections.l1_value", "Look for value {0} confined to an intersection."},
            {"hint.intersections.l1_no_value", "Look for a candidate confined to the intersection of a box and a line."},
            {"hint.intersections.l2", "The intersection cells. The candidate is confined to these cells — eliminate it from other cells in the line or box outside this intersection."},
            {"hint.intersections.l3", "Eliminate the candidate from cells outside the intersection."},
            // Training hints — Fish
            {"hint.fish.l1_value", "Look for a fish pattern on value {0}."},
            {"hint.fish.l1_no_value", "Look for a fish pattern (rows/columns with restricted candidate positions)."},
            {"hint.fish.l2", "Base and cover sets. Blue cells are the base set (rows/columns where the candidate is restricted). Green cells are the cover set. Eliminate the candidate from cover set cells that aren't in the base set."},
            {"hint.fish.l3", "Eliminate the candidate from cover set cells outside the base set."},
            // Training hints — Wings
            {"hint.wings.l1", "Find the pivot cell at {0}."},
            {"hint.wings.l2", "Pivot and wing cells. The orange pivot connects to the green wings. Candidates shared by both wings can be eliminated from cells that see all wing endpoints."},
            {"hint.wings.l3", "Eliminate the shared candidate from cells that see all wing endpoints."},
            // Training hints — SingleDigit
            {"hint.single_digit.l1_value", "Look for conjugate pairs on value {0}."},
            {"hint.single_digit.l1_no_value", "Look for conjugate pairs (cells where a digit appears exactly twice in a unit)."},
            {"hint.single_digit.l2", "The chain cells. These cells form conjugate pairs (a digit appears exactly twice in a unit). Follow the alternating pattern to find eliminations."},
            {"hint.single_digit.l3", "Cells that see both endpoints of the pattern can be eliminated."},
            // Training hints — Coloring
            {"hint.coloring.l1_value", "Build a coloring chain on value {0}."},
            {"hint.coloring.l1_no_value", "Start coloring conjugate pairs with two alternating colors."},
            {"hint.coloring.l2", "The coloring chain. Blue and green are two alternating colors — one must be true, one false. Cells that see both colors can have the candidate eliminated."},
            {"hint.coloring.l3", "One color must be false — eliminate from cells that see both colors."},
            // Training hints — UniqueRect
            {"hint.unique_rect.l1", "Look for a deadly pattern — four cells forming a rectangle across two boxes."},
            {"hint.unique_rect.l2", "The rectangle corners. These four cells across two boxes form a potential deadly pattern. To keep the puzzle unique, eliminate the candidate that would complete the rectangle."},
            {"hint.unique_rect.l3", "To avoid the deadly pattern, eliminate the candidate that would complete it."},
            // Training hints — Chains
            {"hint.chains.l1_pos", "Start the chain from cell {0}."},
            {"hint.chains.l1_no_pos", "Look for a chain of linked cells with alternating strong/weak links."},
            {"hint.chains.l2", "The chain path. Follow the alternating strong (blue) and weak (green) links. The chain's logic forces a conclusion at the endpoints."},
            {"hint.chains.l3_placement", "All chains lead to value {0} at {1}."},
            {"hint.chains.l3_elimination", "Eliminate candidates that contradict the chain logic."},
            // Training hints — SetLogic
            {"hint.set_logic.l1", "Look for an Almost Locked Set (a group of N cells with N+1 candidates)."},
            {"hint.set_logic.l2", "The ALS cells and restricted common. An ALS is N cells with N+1 candidates. The restricted common candidate links the sets — eliminations apply to cells that see all relevant ALS members."},
            {"hint.set_logic.l3", "Eliminate candidates from cells that see all relevant ALS members."},
            // Training hints — Special
            {"hint.special.l1", "Look for the cell with three candidates (the only non-bivalue cell)."},
            {"hint.special.l2", "The key cell is {0}."},
            // Technique descriptions (selected keys needed by tests)
            {"tech.desc.naked_single.what_it_is", "A cell has only one possible candidate left. All other values are eliminated by row, column, and box constraints."},
            {"tech.desc.naked_single.what_to_look_for", "Look for cells where 8 of the 9 values are already present in the cell's row, column, or box."},
            {"tech.desc.forcing_chain.what_it_is", "For a cell with 2-3 candidates, assume each candidate is true and propagate the consequences. If all assumptions lead to the same conclusion (a placement or elimination), that conclusion must be true."},
            {"tech.desc.forcing_chain.what_to_look_for", "Pick a cell with few candidates. Try each value and propagate. Look for common outcomes."},
            {"tech.desc.nice_loop.what_it_is", "Build a chain of alternating strong and weak links between (cell, digit) pairs. If the chain forms a loop or its endpoints share a digit, eliminations can be derived from the alternating inference chain rules."},
            {"tech.desc.nice_loop.what_to_look_for", "Trace alternating strong/weak links. Check if endpoints share a digit for eliminations."},
            {"tech.desc.backtracking.what_it_is", "A brute-force trial-and-error method. Not a logical technique — used as a fallback when no logical strategy can make progress."},
            {"tech.desc.backtracking.what_to_look_for", "This technique is not used in training exercises."},
        };
        // clang-format on
    }

    [[nodiscard]] std::string_view getString(std::string_view key) const override {
        // Check format templates first (needed for fmt::runtime() calls)
        auto fmt_it = format_templates_.find(std::string(key));
        if (fmt_it != format_templates_.end()) {
            return fmt_it->second;
        }
        // Fall back to returning the key itself
        auto [it, _] = key_cache_.try_emplace(std::string(key), std::string(key));
        return it->second;
    }

    [[nodiscard]] std::expected<void, std::string> setLocale(std::string_view /*locale_code*/) override {
        return {};
    }

    [[nodiscard]] std::string_view getCurrentLocale() const override {
        return "en";
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> getAvailableLocales() const override {
        return {{"en", "English"}};
    }

private:
    std::unordered_map<std::string, std::string> format_templates_;
    mutable std::unordered_map<std::string, std::string> key_cache_;
};

}  // namespace sudoku::core
