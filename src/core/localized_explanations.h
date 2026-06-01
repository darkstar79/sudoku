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

#include "core/i18n_helpers.h"
#include "solve_step.h"

#include <string>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace sudoku::core {

/// Format a position using the localized template (e.g., "R3C5" in English)
[[nodiscard]] inline std::string localizedPosition(const Position& pos) {
    return fmt::format(fmt::runtime(core::loc("Sudoku", "R{0}C{1}")), pos.row + 1, pos.col + 1);
}

/// Format a region name with 1-indexed number (e.g., "Row 3" in English)
[[nodiscard]] inline std::string localizedRegion(RegionType type, size_t idx) {
    std::string name;
    switch (type) {
        case RegionType::Row:
            name = core::loc("Sudoku", "Row");
            break;
        case RegionType::Col:
            name = core::loc("Sudoku", "Column");
            break;
        case RegionType::Box:
            name = core::loc("Sudoku", "Box");
            break;
        default:
            name = core::loc("Sudoku", "Unknown Region");
            break;
    }
    return fmt::format("{} {}", name, idx + 1);
}

/// Format a comma-separated list of values (e.g., "1, 3, 5")
[[nodiscard]] inline std::string formatValueList(const std::vector<int>& values) {
    return fmt::format("{}", fmt::join(values, ", "));
}

/// Format a comma-separated list of positions using localized format
[[nodiscard]] inline std::string formatPositionList(const std::vector<Position>& positions) {
    std::vector<std::string> strs;
    strs.reserve(positions.size());
    for (const auto& pos : positions) {
        strs.push_back(localizedPosition(pos));
    }
    return fmt::format("{}", fmt::join(strs, ", "));
}

/// Build a localized explanation string from a SolveStep.
/// Uses the localization manager to look up templates, then fills in
/// dynamic data (positions, values, regions) from the step's explanation_data.
/// Falls back to the raw English explanation if data is insufficient.
// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — dispatch table over SolvingTechnique enum; each case is independent; switch-over-enum complexity is not meaningful here
[[nodiscard]] inline std::string getLocalizedExplanation(const SolveStep& step) {
    const auto& data = step.explanation_data;

    switch (step.technique) {
        case SolvingTechnique::NakedSingle: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(core::loc("Sudoku", "Naked Single at {0}: only value {1} is possible")),
                               localizedPosition(data.positions[0]), data.values[0]);
        }
        case SolvingTechnique::HiddenSingle: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku", "Hidden Single at {0}: value {1} can only appear in this cell within its region")),
                localizedPosition(data.positions[0]), data.values[0]);
        }
        case SolvingTechnique::NakedPair: {
            if (data.positions.size() < 2 || data.values.size() < 2 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(core::loc(
                                   "Sudoku", "Naked Pair [{0}] at {1} in {2} eliminates candidates from other cells")),
                               formatValueList(data.values), formatPositionList(data.positions),
                               localizedRegion(data.region_type, data.region_index));
        }
        case SolvingTechnique::NakedTriple: {
            if (data.positions.size() < 3 || data.values.size() < 3 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(
                    core::loc("Sudoku", "Naked Triple [{0}] at {1} in {2} eliminates candidates from other cells")),
                formatValueList(data.values), formatPositionList(data.positions),
                localizedRegion(data.region_type, data.region_index));
        }
        case SolvingTechnique::HiddenPair: {
            if (data.positions.size() < 2 || data.values.size() < 2 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku",
                                       "Hidden Pair [{0}] at {1} in {2} eliminates other candidates from these cells")),
                formatValueList(data.values), formatPositionList(data.positions),
                localizedRegion(data.region_type, data.region_index));
        }
        case SolvingTechnique::HiddenTriple: {
            if (data.positions.size() < 3 || data.values.size() < 3 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku", "Hidden Triple [{0}] at {1} in {2} eliminates other candidates from these cells")),
                formatValueList(data.values), formatPositionList(data.positions),
                localizedRegion(data.region_type, data.region_index));
        }
        case SolvingTechnique::PointingPair: {
            if (data.values.empty() || data.region_type == RegionType::None ||
                data.secondary_region_type == RegionType::None) {
                return step.explanation;
            }
            // Template: "Pointing Pair: {0} in Box {1} confined to {2} {3} eliminates {0} from other cells in {2} {3}"
            std::string sec_region_name;
            switch (data.secondary_region_type) {
                case RegionType::Row:
                    sec_region_name = core::loc("Sudoku", "Row");
                    break;
                case RegionType::Col:
                    sec_region_name = core::loc("Sudoku", "Column");
                    break;
                default:
                    sec_region_name = core::loc("Sudoku", "Unknown Region");
                    break;
            }
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "Pointing Pair: {0} in Box {1} confined to {2} {3} eliminates {0} from other cells in {2} {3}")),
                data.values[0], data.region_index + 1, sec_region_name, data.secondary_region_index + 1);
        }
        case SolvingTechnique::BoxLineReduction: {
            if (data.values.empty() || data.region_type == RegionType::None ||
                data.secondary_region_type == RegionType::None) {
                return step.explanation;
            }
            // Template: "Box/Line Reduction: {0} in {1} {2} confined to Box {3} eliminates {0} from other cells in Box
            // {3}"
            std::string region_name;
            switch (data.region_type) {
                case RegionType::Row:
                    region_name = core::loc("Sudoku", "Row");
                    break;
                case RegionType::Col:
                    region_name = core::loc("Sudoku", "Column");
                    break;
                default:
                    region_name = core::loc("Sudoku", "Unknown Region");
                    break;
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Box/Line Reduction: {0} in {1} {2} confined to Box {3} "
                                                 "eliminates {0} from other cells in Box {3}")),
                data.values[0], region_name, data.region_index + 1, data.secondary_region_index + 1);
        }
        case SolvingTechnique::NakedQuad: {
            if (data.positions.size() < 4 || data.values.size() < 4 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(core::loc(
                                   "Sudoku", "Naked Quad [{0}] at {1} in {2} eliminates candidates from other cells")),
                               formatValueList(data.values), formatPositionList(data.positions),
                               localizedRegion(data.region_type, data.region_index));
        }
        case SolvingTechnique::HiddenQuad: {
            if (data.positions.size() < 4 || data.values.size() < 4 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku",
                                       "Hidden Quad [{0}] at {1} in {2} eliminates other candidates from these cells")),
                formatValueList(data.values), formatPositionList(data.positions),
                localizedRegion(data.region_type, data.region_index));
        }
        case SolvingTechnique::XWing: {
            if (data.values.empty() || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            // Row-based: pattern_axis=Row, Col-based: pattern_axis=Col (gh#39)
            if (data.pattern_axis == RegionType::Row) {
                // Rows {1} and {2}, Columns {3} and {4}
                // Positions: [r1c1, r1c2, r2c1, r2c2]
                if (data.positions.size() < 4) {
                    return step.explanation;
                }
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku", "X-Wing on value {0} in Rows {1} and {2}, Columns {3} and "
                                                     "{4} eliminates {0} from other cells in those columns")),
                    data.values[0], data.positions[0].row + 1, data.positions[2].row + 1, data.positions[0].col + 1,
                    data.positions[1].col + 1);
            }
            // Col-based
            if (data.positions.size() < 4) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "X-Wing on value {0} in Columns {1} and {2}, Rows {3} and {4} "
                                                 "eliminates {0} from other cells in those rows")),
                data.values[0], data.positions[0].col + 1, data.positions[1].col + 1, data.positions[0].row + 1,
                data.positions[2].row + 1);
        }
        case SolvingTechnique::XYWing: {
            if (data.positions.size() < 3 || data.values.size() < 3) {
                return step.explanation;
            }
            // Template: "XY-Wing: pivot {0} {{{1},{2}}}, wing {3} {{{1},{4}}}, wing {5} {{{2},{4}}} eliminates {4} from
            // cells seeing both wings"
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "XY-Wing: pivot {0} {{{1},{2}}}, wing {3} {{{1},{4}}}, wing {5} "
                                                 "{{{2},{4}}} eliminates {4} from cells seeing both wings")),
                localizedPosition(data.positions[0]), data.values[0], data.values[1],
                localizedPosition(data.positions[1]), data.values[2], localizedPosition(data.positions[2]));
        }
        case SolvingTechnique::Swordfish: {
            // values = {candidate, r1, r2, r3, c1, c2, c3} (1-indexed from strategy)
            if (data.values.size() < 7 || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            if (data.pattern_axis == RegionType::Row) {
                // Row-based: rows then cols
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku",
                                           "Swordfish on value {0} in Rows {1}, {2}, {3} and Columns {4}, {5}, {6} "
                                           "eliminates {0} from other cells in those columns")),
                    data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], data.values[5],
                    data.values[6]);
            }
            // Col-based: cols then rows
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Swordfish on value {0} in Columns {1}, {2}, {3} and Rows {4}, "
                                                 "{5}, {6} eliminates {0} from other cells in those rows")),
                data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], data.values[5],
                data.values[6]);
        }
        case SolvingTechnique::Skyscraper: {
            if (data.positions.size() < 4 || data.values.empty()) {
                return step.explanation;
            }
            // positions: [pair1_shared, pair1_non_shared, pair2_shared, pair2_non_shared]
            // Template: "Skyscraper on value {0}: conjugate pairs in {1} and {2} share endpoint {3}
            //            — eliminates {0} from cells seeing both {4} and {5}"
            std::string region1 = localizedRegion(data.region_type, data.region_index);
            std::string region2 = localizedRegion(data.secondary_region_type, data.secondary_region_index);
            return fmt::format(
                fmt::runtime(core::loc("Sudoku",
                                       "Skyscraper on value {0}: conjugate pairs in {1} and {2} share endpoint {3} — "
                                       "eliminates {0} from cells seeing both {4} and {5}")),
                data.values[0], region1, region2, localizedPosition(data.positions[0]),
                localizedPosition(data.positions[1]), localizedPosition(data.positions[3]));
        }
        case SolvingTechnique::TwoStringKite: {
            if (data.positions.size() < 4 || data.values.empty()) {
                return step.explanation;
            }
            // positions: [row_end1, row_end2, col_end1, col_end2]
            return fmt::format(
                fmt::runtime(core::loc("Sudoku",
                                       "2-String Kite on value {0}: row pair {1},{2} and column pair {3},{4} connected "
                                       "through shared box — eliminates {0} from cells seeing both endpoints")),
                data.values[0], localizedPosition(data.positions[0]), localizedPosition(data.positions[1]),
                localizedPosition(data.positions[2]), localizedPosition(data.positions[3]));
        }
        case SolvingTechnique::XYZWing: {
            if (data.positions.size() < 3 || data.values.size() < 3) {
                return step.explanation;
            }
            // Template: "XYZ-Wing: pivot {0} {{{1},{2},{3}}}, wing {4} and wing {5} eliminate {3} from cells seeing
            // all three"
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "XYZ-Wing: pivot {0} {{{1},{2},{3}}}, wing {4} and wing {5} "
                                                 "eliminate {3} from cells seeing all three")),
                localizedPosition(data.positions[0]), data.values[0], data.values[1], data.values[2],
                localizedPosition(data.positions[1]), localizedPosition(data.positions[2]));
        }
        case SolvingTechnique::UniqueRectangle: {
            if (data.positions.size() < 4 || data.values.size() < 2) {
                return step.explanation;
            }
            // technique_subtype distinguishes UR sub-types: 0=Type1, 1=Type2, 2=Type3, 3=Type4
            if (data.technique_subtype == 1 && data.values.size() >= 3) {
                // Type 2: extra candidate {3} eliminated from shared {4}
                return fmt::format(
                    fmt::runtime(core::loc(
                        "Sudoku", "Unique Rectangle Type 2: cells {0} with values {{{1},{2}}} — extra candidate "
                                  "{3} eliminated from cells seeing both floor cells in shared {4}")),
                    formatPositionList(data.positions), data.values[0], data.values[1], data.values[2],
                    localizedRegion(data.secondary_region_type, data.secondary_region_index));
            }
            if (data.technique_subtype == 2 && data.values.size() >= 2) {
                // Type 3: floor extras form naked subset in {3}
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku",
                                           "Unique Rectangle Type 3: cells {0} with values {{{1},{2}}} — floor extras "
                                           "form naked subset in {3}, eliminating from other cells")),
                    formatPositionList(data.positions), data.values[0], data.values[1],
                    localizedRegion(data.secondary_region_type, data.secondary_region_index));
            }
            if (data.technique_subtype == 3 && data.values.size() >= 4) {
                // Type 4: strong link on {3} in {4} eliminates {5}
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku",
                                           "Unique Rectangle Type 4: cells {0} with values {{{1},{2}}} — strong link "
                                           "on {3} in {4} eliminates {5} from floor cells")),
                    formatPositionList(data.positions), data.values[0], data.values[1], data.values[2],
                    localizedRegion(data.secondary_region_type, data.secondary_region_index), data.values[3]);
            }
            if (data.technique_subtype == 5 && data.values.size() >= 3) {
                // Type 6: digit {3} conjugate in both parallel lines → eliminates extras
                return fmt::format(
                    fmt::runtime(core::loc(
                        "Sudoku",
                        "Unique Rectangle Type 6: cells {0} with values {{{1},{2}}} — {3} is conjugate in both "
                        "parallel lines of the rectangle, locking the pattern — eliminates extras from floor cells")),
                    formatPositionList(data.positions), data.values[0], data.values[1], data.values[2]);
            }
            // Type 1 (default): eliminates {1},{2} from {3}
            return fmt::format(fmt::runtime(core::loc("Sudoku", "Unique Rectangle: cells {0} with values {{{1},{2}}} — "
                                                                "eliminates {1},{2} from {3} to avoid deadly pattern")),
                               formatPositionList(data.positions), data.values[0], data.values[1],
                               localizedPosition(data.positions[3]));
        }
        case SolvingTechnique::WWing: {
            if (data.positions.size() < 4 || data.values.size() < 2) {
                return step.explanation;
            }
            // Template: "W-Wing: cells {0} and {1} {{{2},{3}}} connected by strong link on {2} — eliminates {3} from
            // cells seeing both"
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "W-Wing: cells {0} and {1} {{{2},{3}}} connected by strong link "
                                                 "on {2} — eliminates {3} from cells seeing both")),
                localizedPosition(data.positions[0]), localizedPosition(data.positions[1]), data.values[0],
                data.values[1]);
        }
        case SolvingTechnique::SimpleColoring: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // technique_subtype: 0 = contradiction, 1 = exclusion
            if (data.technique_subtype == 0) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku", "Simple Coloring on {0}: same-color cells see each other — "
                                                     "eliminates {0} from all cells of that color")),
                    data.values[0]);
            }
            if (data.positions.empty()) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(
                    core::loc("Sudoku", "Simple Coloring on {0}: cell {1} sees both colors — eliminates {0} from {1}")),
                data.values[0], localizedPosition(data.positions[0]));
        }
        case SolvingTechnique::FinnedXWing: {
            if (data.values.empty() || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            // values = {candidate, row/col1, row/col2, col/row1, col/row2}, positions includes fin
            if (data.positions.empty() || data.values.size() < 5) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(data.positions.back());
            if (data.pattern_axis == RegionType::Row) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku",
                                           "Finned X-Wing on value {0} in Rows {1} and {2}, Columns {3} and {4} with "
                                           "fin at {5} — eliminates {0} from cells in fin's box")),
                    data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Finned X-Wing on value {0} in Columns {1} and {2}, Rows {3} and "
                                                 "{4} with fin at {5} — eliminates {0} from cells in fin's box")),
                data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
        }
        case SolvingTechnique::RemotePairs: {
            if (data.positions.size() < 2 || data.values.size() < 3) {
                return step.explanation;
            }
            // values = {A, B, chain_length}
            return fmt::format(fmt::runtime(core::loc(
                                   "Sudoku", "Remote Pairs: chain of {{{0},{1}}} cells from {2} to {3} (length {4}) — "
                                             "eliminates {0},{1} from cells seeing both endpoints")),
                               data.values[0], data.values[1], localizedPosition(data.positions[0]),
                               localizedPosition(data.positions[1]), data.values[2]);
        }
        case SolvingTechnique::BUG: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku", "BUG: all cells bivalue except {0} — value {1} must be placed to avoid deadly pattern")),
                localizedPosition(data.positions[0]), data.values[0]);
        }
        case SolvingTechnique::Jellyfish: {
            // values = {candidate, r1, r2, r3, r4, c1, c2, c3, c4} (1-indexed)
            if (data.values.size() < 9 || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            if (data.pattern_axis == RegionType::Row) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku",
                                           "Jellyfish on value {0} in Rows {1}, {2}, {3}, {4} and Columns {5}, {6}, "
                                           "{7}, {8} eliminates {0} from other cells in those columns")),
                    data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], data.values[5],
                    data.values[6], data.values[7], data.values[8]);
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku",
                                       "Jellyfish on value {0} in Columns {1}, {2}, {3}, {4} and Rows {5}, {6}, {7}, "
                                       "{8} eliminates {0} from other cells in those rows")),
                data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], data.values[5],
                data.values[6], data.values[7], data.values[8]);
        }
        case SolvingTechnique::FinnedSwordfish: {
            // values = {candidate, row/col1, row/col2, row/col3}, positions includes fin at back
            if (data.positions.empty() || data.values.size() < 4 || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(data.positions.back());
            if (data.pattern_axis == RegionType::Row) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku", "Finned Swordfish on value {0} in Rows {1}, {2}, {3} with "
                                                     "fin at {4} — eliminates {0} from cells in fin's box")),
                    data.values[0], data.values[1], data.values[2], data.values[3], fin_pos);
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Finned Swordfish on value {0} in Columns {1}, {2}, {3} with fin "
                                                 "at {4} — eliminates {0} from cells in fin's box")),
                data.values[0], data.values[1], data.values[2], data.values[3], fin_pos);
        }
        case SolvingTechnique::EmptyRectangle: {
            if (data.positions.empty() || data.values.size() < 2 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            // values = {candidate, box+1}, positions.back() = elimination target
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Empty Rectangle on value {0}: ER in Box {1} with conjugate pair "
                                                 "in {2} — eliminates {0} from {3}")),
                data.values[0], data.values[1], localizedRegion(data.region_type, data.region_index),
                localizedPosition(data.positions.back()));
        }
        case SolvingTechnique::WXYZWing: {
            if (data.positions.size() < 4 || data.values.empty()) {
                return step.explanation;
            }
            // positions: [pivot, w1, w2, w3], values: [Z]
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "WXYZ-Wing: pivot {0} with wings {1}, {2}, {3} — eliminates {4} from cells seeing all four")),
                localizedPosition(data.positions[0]), localizedPosition(data.positions[1]),
                localizedPosition(data.positions[2]), localizedPosition(data.positions[3]), data.values[0]);
        }
        case SolvingTechnique::FinnedJellyfish: {
            // values = {candidate, row/col1..4}, positions includes fin at back
            if (data.positions.empty() || data.values.size() < 5 || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(data.positions.back());
            if (data.pattern_axis == RegionType::Row) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku", "Finned Jellyfish on value {0} in Rows {1}, {2}, {3}, {4} "
                                                     "with fin at {5} — eliminates {0} from cells in fin's box")),
                    data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Finned Jellyfish on value {0} in Columns {1}, {2}, {3}, {4} "
                                                 "with fin at {5} — eliminates {0} from cells in fin's box")),
                data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
        }
        case SolvingTechnique::XYChain: {
            if (data.positions.size() < 2 || data.values.size() < 2) {
                return step.explanation;
            }
            // values = {X, chain_length}
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "XY-Chain: chain of {0} bivalue cells from {1} to {2} — "
                                                 "eliminates {3} from cells seeing both endpoints")),
                data.values[1], localizedPosition(data.positions[0]), localizedPosition(data.positions[1]),
                data.values[0]);
        }
        case SolvingTechnique::MultiColoring: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // technique_subtype: 0 = wrap, 1 = trap
            if (data.technique_subtype == 0) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku", "Multi-Coloring on {0}: color sees both colors of another "
                                                     "cluster — eliminates {0} from all cells of that color")),
                    data.values[0]);
            }
            if (data.positions.empty()) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "Multi-Coloring on {0}: cell {1} sees complementary colors from two clusters — eliminates {0}")),
                data.values[0], localizedPosition(data.positions[0]));
        }
        case SolvingTechnique::ALSxZ: {
            if (data.values.size() < 4 || data.positions.empty()) {
                return step.explanation;
            }
            // values = {X, Z, als_a_size, als_b_size}
            // positions = {als_a cells..., als_b cells...}
            auto als_a_size = static_cast<size_t>(data.values[2]);
            auto als_b_start = als_a_size;
            std::vector<Position> als_a_pos(data.positions.begin(),
                                            data.positions.begin() + static_cast<ptrdiff_t>(als_a_size));
            std::vector<Position> als_b_pos(data.positions.begin() + static_cast<ptrdiff_t>(als_b_start),
                                            data.positions.end());
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "ALS-XZ: ALS {0} and ALS {1} linked by restricted common {2} — "
                                                 "eliminates {3} from cells seeing both ALSs")),
                formatPositionList(als_a_pos), formatPositionList(als_b_pos), data.values[0], data.values[1]);
        }
        case SolvingTechnique::SueDeCoq: {
            if (data.values.empty() || data.region_type == RegionType::None) {
                return step.explanation;
            }
            // region_type: Row/Col; region_index = line index; secondary_region_index = box index
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "Sue de Coq: intersection of {0} and Box {1} — eliminates candidates from rest of line and box")),
                localizedRegion(data.region_type, data.region_index), data.secondary_region_index + 1);
        }
        case SolvingTechnique::ForcingChain: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku", "Forcing Chain: assuming each candidate in {0} leads to the same conclusion — {1}")),
                localizedPosition(data.positions[0]), data.values[0]);
        }
        case SolvingTechnique::NiceLoop: {
            if (data.positions.size() < 2 || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(
                    core::loc("Sudoku", "Nice Loop: alternating inference chain from {0} to {1} — eliminates {2}")),
                localizedPosition(data.positions[0]), localizedPosition(data.positions[1]), data.values[0]);
        }
        case SolvingTechnique::XCycles: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // technique_subtype: 0=Type1, 1=Type2, 2=Type3
            if (data.technique_subtype == 1 && !data.positions.empty()) {
                return fmt::format(
                    fmt::runtime(
                        core::loc("Sudoku", "X-Cycles on value {0}: strong-strong discontinuity at {1} — places {0}")),
                    data.values[0], localizedPosition(data.positions[0]));
            }
            if (data.technique_subtype == 2 && !data.positions.empty()) {
                return fmt::format(
                    fmt::runtime(core::loc(
                        "Sudoku", "X-Cycles on value {0}: weak-weak discontinuity at {1} — eliminates {0} from {1}")),
                    data.values[0], localizedPosition(data.positions[0]));
            }
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "X-Cycles on value {0}: continuous loop — eliminates {0} from cells seeing weak link endpoints")),
                data.values[0]);
        }
        case SolvingTechnique::ThreeDMedusa: {
            if (data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(core::loc("Sudoku", "3D Medusa: multi-digit coloring — {0}")),
                               data.values[0]);
        }
        case SolvingTechnique::HiddenUniqueRectangle: {
            if (data.positions.size() < 4 || data.values.size() < 4) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Hidden Unique Rectangle: cells {0} with values {{{1},{2}}} — "
                                                 "eliminates {3} from {4} to avoid deadly pattern")),
                formatPositionList(data.positions), data.values[0], data.values[1], data.values[2],
                localizedPosition(data.positions[static_cast<size_t>(data.values[3])]));
        }
        case SolvingTechnique::AvoidableRectangle: {
            if (data.positions.size() < 4 || data.values.size() < 4) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Avoidable Rectangle: cells {0} with solved values {{{1},{2}}} — "
                                                 "eliminates {3} from {4} to avoid deadly pattern")),
                formatPositionList(data.positions), data.values[0], data.values[1], data.values[2],
                localizedPosition(data.positions[static_cast<size_t>(data.values[3])]));
        }
        case SolvingTechnique::ALSXYWing: {
            if (data.values.size() < 3 || data.positions.empty()) {
                return step.explanation;
            }
            // values = {X, Y, Z, als_a_size, als_b_size, als_c_size}
            // positions = {als_a cells..., als_b cells..., als_c cells...}
            if (data.values.size() < 6) {
                return step.explanation;
            }
            auto a_sz = static_cast<size_t>(data.values[3]);
            auto b_sz = static_cast<size_t>(data.values[4]);
            auto b_start = a_sz;
            auto c_start = a_sz + b_sz;
            std::vector<Position> als_a(data.positions.begin(), data.positions.begin() + static_cast<ptrdiff_t>(a_sz));
            std::vector<Position> als_b(data.positions.begin() + static_cast<ptrdiff_t>(b_start),
                                        data.positions.begin() + static_cast<ptrdiff_t>(c_start));
            std::vector<Position> als_c(data.positions.begin() + static_cast<ptrdiff_t>(c_start), data.positions.end());
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "ALS-XY-Wing: ALS {0}, ALS {1}, ALS {2} linked by X={3} and "
                                                 "Y={4} — eliminates {5} from cells seeing Z-cells in A and C")),
                formatPositionList(als_a), formatPositionList(als_b), formatPositionList(als_c), data.values[0],
                data.values[1], data.values[2]);
        }
        case SolvingTechnique::DeathBlossom: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            // positions[0] = stem, values[0] = Z description
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "Death Blossom: stem {0} with petals {1} — eliminates {2} from cells seeing all petal Z-cells")),
                localizedPosition(data.positions[0]), data.values[0], data.values[1]);
        }
        case SolvingTechnique::VWXYZWing: {
            if (data.positions.size() < 5 || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(core::loc("Sudoku", "VWXYZ-Wing: pivot {0} with wings {1}, {2}, {3}, {4} — "
                                                                "eliminates {5} from cells seeing all Z-cells")),
                               localizedPosition(data.positions[0]), localizedPosition(data.positions[1]),
                               localizedPosition(data.positions[2]), localizedPosition(data.positions[3]),
                               localizedPosition(data.positions[4]), data.values[0]);
        }
        case SolvingTechnique::FrankenFish: {
            if (data.values.size() < 2) {
                return step.explanation;
            }
            // values = {fish_name_placeholder, digit, ...}
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "Franken {0} on value {1}: base {2}, cover {3} — eliminates {1} from cover cells outside base")),
                data.values[0], data.values[1], data.positions.empty() ? "" : formatPositionList(data.positions),
                data.values.size() >= 3 ? std::to_string(data.values[2]) : "");
        }
        case SolvingTechnique::MutantFish: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // values = {digit}, positions = base pattern cells
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Mutant Fish on value {0}: base {1}, cover {2} — eliminates {0} "
                                                 "from {3} cover cell(s) outside base")),
                data.values[0], data.positions.empty() ? "" : formatPositionList(data.positions),
                "",  // cover description not stored in explanation_data
                static_cast<int>(step.eliminations.size()));
        }
        case SolvingTechnique::GroupedXCycles: {
            if (data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Grouped X-Cycles on value {0}: chain with grouped nodes — {1}")),
                data.values[0], data.values.size() >= 2 ? std::to_string(data.values[1]) : "");
        }
        case SolvingTechnique::SashimiXWing: {
            if (data.values.empty() || data.positions.empty() || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            // values = {candidate, row/col1, row/col2}, fin is last position
            if (data.values.size() < 3) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(data.positions.back());
            if (data.pattern_axis == RegionType::Row) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku",
                                           "Sashimi X-Wing on value {0} in Rows {1} and {2}, Columns {3} and {4} with "
                                           "fin at {5} — eliminates {0} from cells in fin's box")),
                    data.values[0], data.values[1], data.values[2], fin_pos);
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku",
                                       "Sashimi X-Wing on value {0} in Columns {1} and {2}, Rows {3} and {4} with fin "
                                       "at {5} — eliminates {0} from cells in fin's box")),
                data.values[0], data.values[1], data.values[2], fin_pos);
        }
        case SolvingTechnique::SashimiSwordfish: {
            if (data.positions.empty() || data.values.size() < 4 || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(data.positions.back());
            if (data.pattern_axis == RegionType::Row) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku", "Sashimi Swordfish on value {0} in Rows {1}, {2}, {3} with "
                                                     "fin at {4} — eliminates {0} from cells in fin's box")),
                    data.values[0], data.values[1], data.values[2], data.values[3], fin_pos);
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Sashimi Swordfish on value {0} in Columns {1}, {2}, {3} with "
                                                 "fin at {4} — eliminates {0} from cells in fin's box")),
                data.values[0], data.values[1], data.values[2], data.values[3], fin_pos);
        }
        case SolvingTechnique::SashimiJellyfish: {
            if (data.positions.empty() || data.values.size() < 5 || data.pattern_axis == RegionType::None) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(data.positions.back());
            if (data.pattern_axis == RegionType::Row) {
                return fmt::format(
                    fmt::runtime(core::loc("Sudoku", "Sashimi Jellyfish on value {0} in Rows {1}, {2}, {3}, {4} "
                                                     "with fin at {5} — eliminates {0} from cells in fin's box")),
                    data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
            }
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Sashimi Jellyfish on value {0} in Columns {1}, {2}, {3}, {4} "
                                                 "with fin at {5} — eliminates {0} from cells in fin's box")),
                data.values[0], data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
        }
        case SolvingTechnique::KrakenFish: {
            if (data.values.empty() || data.positions.empty()) {
                return step.explanation;
            }
            // Template: "Kraken Fish on value {0}: finned fish with chain-verified eliminations from {1}"
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku", "Kraken Fish on value {0}: finned fish with chain-verified eliminations from {1}")),
                data.values[0], formatPositionList(data.positions));
        }
        case SolvingTechnique::ALSChain: {
            if (data.values.empty() || data.positions.empty()) {
                return step.explanation;
            }
            // values = [rc1, rc2, ..., z, chain_length]; positions = all ALS cells
            auto chain_len = data.values.back();
            auto val_z = data.values.size() >= 2 ? data.values[static_cast<size_t>(data.values.size() - 2)] : 0;
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "ALS Chain ({0} ALSs): eliminates {1} from cells seeing Z-cells in first and last ALS at {2}")),
                chain_len, val_z, formatPositionList(data.positions));
        }
        case SolvingTechnique::JuniorExocet: {
            if (data.positions.size() < 4 || data.values.empty()) {
                return step.explanation;
            }
            // positions = {base1, base2, target1, target2}, values = base candidates
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Junior Exocet: base cells {0} and {1} with candidates {{{2}}} — "
                                                 "targets {3} and {4} can only contain base candidates")),
                localizedPosition(data.positions[0]), localizedPosition(data.positions[1]),
                formatValueList(data.values), localizedPosition(data.positions[2]),
                localizedPosition(data.positions[3]));
        }
        case SolvingTechnique::UniqueLoop: {
            if (data.positions.size() < 4 || data.values.size() < 2) {
                return step.explanation;
            }
            // Template: "Unique Loop: cells {0} with values {{{1},{2}}} — eliminates {1},{2} from {3}"
            return fmt::format(
                fmt::runtime(core::loc("Sudoku", "Unique Loop: cells {0} with values {{{1},{2}}} — eliminates "
                                                 "{1},{2} from {3} to avoid deadly pattern")),
                formatPositionList(data.positions), data.values[0], data.values[1],
                localizedPosition(data.positions.back()));
        }
        case SolvingTechnique::ContinuousNiceLoop: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // Template: "Continuous Nice Loop: loop of {0} nodes — eliminates {1} candidate(s)"
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku",
                    "Continuous Nice Loop: loop of {0} nodes — eliminates {1} candidate(s) via weak link logic")),
                data.values[0], data.values.size() >= 2 ? data.values[1] : 0);
        }
        case SolvingTechnique::GroupedNiceLoop: {
            if (data.positions.size() < 2 || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(
                fmt::runtime(core::loc(
                    "Sudoku", "Grouped Nice Loop: alternating inference chain from {0} to {1} — eliminates {2}")),
                localizedPosition(data.positions[0]), localizedPosition(data.positions[1]), data.values[0]);
        }
        case SolvingTechnique::UnitForcingChain:
        case SolvingTechnique::RegionForcingChain:
        case SolvingTechnique::Backtracking:
            return step.explanation;
    }

    return step.explanation;
}

}  // namespace sudoku::core
