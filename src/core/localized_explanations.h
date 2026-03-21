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

#include "i_localization_manager.h"
#include "solve_step.h"
#include "string_keys.h"

#include <string>

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace sudoku::core {

/// Format a position using the localized template (e.g., "R3C5" in English)
[[nodiscard]] inline std::string localizedPosition(const ILocalizationManager& loc, const Position& pos) {
    return fmt::format(fmt::runtime(loc.getString(StringKeys::PositionFmt)), pos.row + 1, pos.col + 1);
}

/// Format a region name with 1-indexed number (e.g., "Row 3" in English)
[[nodiscard]] inline std::string localizedRegion(const ILocalizationManager& loc, RegionType type, size_t idx) {
    std::string_view name;
    switch (type) {
        case RegionType::Row:
            name = loc.getString(StringKeys::RegionRow);
            break;
        case RegionType::Col:
            name = loc.getString(StringKeys::RegionColumn);
            break;
        case RegionType::Box:
            name = loc.getString(StringKeys::RegionBox);
            break;
        default:
            name = loc.getString(StringKeys::RegionUnknown);
            break;
    }
    return fmt::format("{} {}", name, idx + 1);
}

/// Format a comma-separated list of values (e.g., "1, 3, 5")
[[nodiscard]] inline std::string formatValueList(const std::vector<int>& values) {
    return fmt::format("{}", fmt::join(values, ", "));
}

/// Format a comma-separated list of positions using localized format
[[nodiscard]] inline std::string formatPositionList(const ILocalizationManager& loc,
                                                    const std::vector<Position>& positions) {
    std::vector<std::string> strs;
    strs.reserve(positions.size());
    for (const auto& pos : positions) {
        strs.push_back(localizedPosition(loc, pos));
    }
    return fmt::format("{}", fmt::join(strs, ", "));
}

/// Build a localized explanation string from a SolveStep.
/// Uses the localization manager to look up templates, then fills in
/// dynamic data (positions, values, regions) from the step's explanation_data.
/// Falls back to the raw English explanation if data is insufficient.
// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size) — dispatch table over SolvingTechnique enum; each case is independent; switch-over-enum complexity is not meaningful here
[[nodiscard]] inline std::string getLocalizedExplanation(const ILocalizationManager& loc, const SolveStep& step) {
    const auto& data = step.explanation_data;

    switch (step.technique) {
        case SolvingTechnique::NakedSingle: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainNakedSingle)),
                               localizedPosition(loc, data.positions[0]), data.values[0]);
        }
        case SolvingTechnique::HiddenSingle: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainHiddenSingle)),
                               localizedPosition(loc, data.positions[0]), data.values[0]);
        }
        case SolvingTechnique::NakedPair: {
            if (data.positions.size() < 2 || data.values.size() < 2 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainNakedPair)), formatValueList(data.values),
                               formatPositionList(loc, data.positions),
                               localizedRegion(loc, data.region_type, data.region_index));
        }
        case SolvingTechnique::NakedTriple: {
            if (data.positions.size() < 3 || data.values.size() < 3 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainNakedTriple)),
                               formatValueList(data.values), formatPositionList(loc, data.positions),
                               localizedRegion(loc, data.region_type, data.region_index));
        }
        case SolvingTechnique::HiddenPair: {
            if (data.positions.size() < 2 || data.values.size() < 2 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainHiddenPair)), formatValueList(data.values),
                               formatPositionList(loc, data.positions),
                               localizedRegion(loc, data.region_type, data.region_index));
        }
        case SolvingTechnique::HiddenTriple: {
            if (data.positions.size() < 3 || data.values.size() < 3 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainHiddenTriple)),
                               formatValueList(data.values), formatPositionList(loc, data.positions),
                               localizedRegion(loc, data.region_type, data.region_index));
        }
        case SolvingTechnique::PointingPair: {
            if (data.values.empty() || data.region_type == RegionType::None ||
                data.secondary_region_type == RegionType::None) {
                return step.explanation;
            }
            // Template: "Pointing Pair: {0} in Box {1} confined to {2} {3} eliminates {0} from other cells in {2} {3}"
            std::string_view sec_region_name;
            switch (data.secondary_region_type) {
                case RegionType::Row:
                    sec_region_name = loc.getString(StringKeys::RegionRow);
                    break;
                case RegionType::Col:
                    sec_region_name = loc.getString(StringKeys::RegionColumn);
                    break;
                default:
                    sec_region_name = loc.getString(StringKeys::RegionUnknown);
                    break;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainPointingPair)), data.values[0],
                               data.region_index + 1, sec_region_name, data.secondary_region_index + 1);
        }
        case SolvingTechnique::BoxLineReduction: {
            if (data.values.empty() || data.region_type == RegionType::None ||
                data.secondary_region_type == RegionType::None) {
                return step.explanation;
            }
            // Template: "Box/Line Reduction: {0} in {1} {2} confined to Box {3} eliminates {0} from other cells in Box
            // {3}"
            std::string_view region_name;
            switch (data.region_type) {
                case RegionType::Row:
                    region_name = loc.getString(StringKeys::RegionRow);
                    break;
                case RegionType::Col:
                    region_name = loc.getString(StringKeys::RegionColumn);
                    break;
                default:
                    region_name = loc.getString(StringKeys::RegionUnknown);
                    break;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainBoxLineReduction)), data.values[0],
                               region_name, data.region_index + 1, data.secondary_region_index + 1);
        }
        case SolvingTechnique::NakedQuad: {
            if (data.positions.size() < 4 || data.values.size() < 4 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainNakedQuad)), formatValueList(data.values),
                               formatPositionList(loc, data.positions),
                               localizedRegion(loc, data.region_type, data.region_index));
        }
        case SolvingTechnique::HiddenQuad: {
            if (data.positions.size() < 4 || data.values.size() < 4 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainHiddenQuad)), formatValueList(data.values),
                               formatPositionList(loc, data.positions),
                               localizedRegion(loc, data.region_type, data.region_index));
        }
        case SolvingTechnique::XWing: {
            if (data.values.empty() || data.region_type == RegionType::None) {
                return step.explanation;
            }
            // Row-based: region_type=Row, Col-based: region_type=Col
            if (data.region_type == RegionType::Row) {
                // Rows {1} and {2}, Columns {3} and {4}
                // Positions: [r1c1, r1c2, r2c1, r2c2]
                if (data.positions.size() < 4) {
                    return step.explanation;
                }
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainXWingRow)), data.values[0],
                                   data.positions[0].row + 1, data.positions[2].row + 1, data.positions[0].col + 1,
                                   data.positions[1].col + 1);
            }
            // Col-based
            if (data.positions.size() < 4) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainXWingCol)), data.values[0],
                               data.positions[0].col + 1, data.positions[1].col + 1, data.positions[0].row + 1,
                               data.positions[2].row + 1);
        }
        case SolvingTechnique::XYWing: {
            if (data.positions.size() < 3 || data.values.size() < 3) {
                return step.explanation;
            }
            // Template: "XY-Wing: pivot {0} {{{1},{2}}}, wing {3} {{{1},{4}}}, wing {5} {{{2},{4}}} eliminates {4} from
            // cells seeing both wings"
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainXYWing)),
                               localizedPosition(loc, data.positions[0]), data.values[0], data.values[1],
                               localizedPosition(loc, data.positions[1]), data.values[2],
                               localizedPosition(loc, data.positions[2]));
        }
        case SolvingTechnique::Swordfish: {
            // values = {candidate, r1, r2, r3, c1, c2, c3} (1-indexed from strategy)
            if (data.values.size() < 7 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            if (data.region_type == RegionType::Row) {
                // Row-based: rows then cols
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSwordfishRow)), data.values[0],
                                   data.values[1], data.values[2], data.values[3], data.values[4], data.values[5],
                                   data.values[6]);
            }
            // Col-based: cols then rows
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSwordfishCol)), data.values[0],
                               data.values[1], data.values[2], data.values[3], data.values[4], data.values[5],
                               data.values[6]);
        }
        case SolvingTechnique::Skyscraper: {
            if (data.positions.size() < 4 || data.values.empty()) {
                return step.explanation;
            }
            // positions: [pair1_shared, pair1_non_shared, pair2_shared, pair2_non_shared]
            // Template: "Skyscraper on value {0}: conjugate pairs in {1} and {2} share endpoint {3}
            //            — eliminates {0} from cells seeing both {4} and {5}"
            std::string region1 = localizedRegion(loc, data.region_type, data.region_index);
            std::string region2 = localizedRegion(loc, data.secondary_region_type, data.secondary_region_index);
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSkyscraper)), data.values[0], region1,
                               region2, localizedPosition(loc, data.positions[0]),
                               localizedPosition(loc, data.positions[1]), localizedPosition(loc, data.positions[3]));
        }
        case SolvingTechnique::TwoStringKite: {
            if (data.positions.size() < 4 || data.values.empty()) {
                return step.explanation;
            }
            // positions: [row_end1, row_end2, col_end1, col_end2]
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainTwoStringKite)), data.values[0],
                               localizedPosition(loc, data.positions[0]), localizedPosition(loc, data.positions[1]),
                               localizedPosition(loc, data.positions[2]), localizedPosition(loc, data.positions[3]));
        }
        case SolvingTechnique::XYZWing: {
            if (data.positions.size() < 3 || data.values.size() < 3) {
                return step.explanation;
            }
            // Template: "XYZ-Wing: pivot {0} {{{1},{2},{3}}}, wing {4} and wing {5} eliminate {3} from cells seeing
            // all three"
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainXYZWing)),
                               localizedPosition(loc, data.positions[0]), data.values[0], data.values[1],
                               data.values[2], localizedPosition(loc, data.positions[1]),
                               localizedPosition(loc, data.positions[2]));
        }
        case SolvingTechnique::UniqueRectangle: {
            if (data.positions.size() < 4 || data.values.size() < 2) {
                return step.explanation;
            }
            // technique_subtype distinguishes UR sub-types: 0=Type1, 1=Type2, 2=Type3, 3=Type4
            if (data.technique_subtype == 1 && data.values.size() >= 3) {
                // Type 2: extra candidate {3} eliminated from shared {4}
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainUniqueRectangleType2)),
                                   formatPositionList(loc, data.positions), data.values[0], data.values[1],
                                   data.values[2],
                                   localizedRegion(loc, data.secondary_region_type, data.secondary_region_index));
            }
            if (data.technique_subtype == 2 && data.values.size() >= 2) {
                // Type 3: floor extras form naked subset in {3}
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainUniqueRectangleType3)),
                                   formatPositionList(loc, data.positions), data.values[0], data.values[1],
                                   localizedRegion(loc, data.secondary_region_type, data.secondary_region_index));
            }
            if (data.technique_subtype == 3 && data.values.size() >= 4) {
                // Type 4: strong link on {3} in {4} eliminates {5}
                return fmt::format(
                    fmt::runtime(loc.getString(StringKeys::ExplainUniqueRectangleType4)),
                    formatPositionList(loc, data.positions), data.values[0], data.values[1], data.values[2],
                    localizedRegion(loc, data.secondary_region_type, data.secondary_region_index), data.values[3]);
            }
            if (data.technique_subtype == 5 && data.values.size() >= 3) {
                // Type 6: digit {3} conjugate in both parallel lines → eliminates extras
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainUniqueRectangleType6)),
                                   formatPositionList(loc, data.positions), data.values[0], data.values[1],
                                   data.values[2]);
            }
            // Type 1 (default): eliminates {1},{2} from {3}
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainUniqueRectangle)),
                               formatPositionList(loc, data.positions), data.values[0], data.values[1],
                               localizedPosition(loc, data.positions[3]));
        }
        case SolvingTechnique::WWing: {
            if (data.positions.size() < 4 || data.values.size() < 2) {
                return step.explanation;
            }
            // Template: "W-Wing: cells {0} and {1} {{{2},{3}}} connected by strong link on {2} — eliminates {3} from
            // cells seeing both"
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainWWing)),
                               localizedPosition(loc, data.positions[0]), localizedPosition(loc, data.positions[1]),
                               data.values[0], data.values[1]);
        }
        case SolvingTechnique::SimpleColoring: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // technique_subtype: 0 = contradiction, 1 = exclusion
            if (data.technique_subtype == 0) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSimpleColoringContradiction)),
                                   data.values[0]);
            }
            if (data.positions.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSimpleColoringExclusion)), data.values[0],
                               localizedPosition(loc, data.positions[0]));
        }
        case SolvingTechnique::FinnedXWing: {
            if (data.values.empty() || data.region_type == RegionType::None) {
                return step.explanation;
            }
            // values = {candidate, row/col1, row/col2, col/row1, col/row2}, positions includes fin
            if (data.positions.empty() || data.values.size() < 5) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(loc, data.positions.back());
            if (data.region_type == RegionType::Row) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainFinnedXWingRow)), data.values[0],
                                   data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainFinnedXWingCol)), data.values[0],
                               data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
        }
        case SolvingTechnique::RemotePairs: {
            if (data.positions.size() < 2 || data.values.size() < 3) {
                return step.explanation;
            }
            // values = {A, B, chain_length}
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainRemotePairs)), data.values[0],
                               data.values[1], localizedPosition(loc, data.positions[0]),
                               localizedPosition(loc, data.positions[1]), data.values[2]);
        }
        case SolvingTechnique::BUG: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainBUG)),
                               localizedPosition(loc, data.positions[0]), data.values[0]);
        }
        case SolvingTechnique::Jellyfish: {
            // values = {candidate, r1, r2, r3, r4, c1, c2, c3, c4} (1-indexed)
            if (data.values.size() < 9 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            if (data.region_type == RegionType::Row) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainJellyfishRow)), data.values[0],
                                   data.values[1], data.values[2], data.values[3], data.values[4], data.values[5],
                                   data.values[6], data.values[7], data.values[8]);
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainJellyfishCol)), data.values[0],
                               data.values[1], data.values[2], data.values[3], data.values[4], data.values[5],
                               data.values[6], data.values[7], data.values[8]);
        }
        case SolvingTechnique::FinnedSwordfish: {
            // values = {candidate, row/col1, row/col2, row/col3}, positions includes fin at back
            if (data.positions.empty() || data.values.size() < 4 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(loc, data.positions.back());
            if (data.region_type == RegionType::Row) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainFinnedSwordfishRow)), data.values[0],
                                   data.values[1], data.values[2], data.values[3], fin_pos);
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainFinnedSwordfishCol)), data.values[0],
                               data.values[1], data.values[2], data.values[3], fin_pos);
        }
        case SolvingTechnique::EmptyRectangle: {
            if (data.positions.empty() || data.values.size() < 2 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            // values = {candidate, box+1}, positions.back() = elimination target
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainEmptyRectangle)), data.values[0],
                               data.values[1], localizedRegion(loc, data.region_type, data.region_index),
                               localizedPosition(loc, data.positions.back()));
        }
        case SolvingTechnique::WXYZWing: {
            if (data.positions.size() < 4 || data.values.empty()) {
                return step.explanation;
            }
            // positions: [pivot, w1, w2, w3], values: [Z]
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainWXYZWing)),
                               localizedPosition(loc, data.positions[0]), localizedPosition(loc, data.positions[1]),
                               localizedPosition(loc, data.positions[2]), localizedPosition(loc, data.positions[3]),
                               data.values[0]);
        }
        case SolvingTechnique::FinnedJellyfish: {
            // values = {candidate, row/col1..4}, positions includes fin at back
            if (data.positions.empty() || data.values.size() < 5 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(loc, data.positions.back());
            if (data.region_type == RegionType::Row) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainFinnedJellyfishRow)), data.values[0],
                                   data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainFinnedJellyfishCol)), data.values[0],
                               data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
        }
        case SolvingTechnique::XYChain: {
            if (data.positions.size() < 2 || data.values.size() < 2) {
                return step.explanation;
            }
            // values = {X, chain_length}
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainXYChain)), data.values[1],
                               localizedPosition(loc, data.positions[0]), localizedPosition(loc, data.positions[1]),
                               data.values[0]);
        }
        case SolvingTechnique::MultiColoring: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // technique_subtype: 0 = wrap, 1 = trap
            if (data.technique_subtype == 0) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainMultiColoringWrap)), data.values[0]);
            }
            if (data.positions.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainMultiColoringTrap)), data.values[0],
                               localizedPosition(loc, data.positions[0]));
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
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainALSxZ)),
                               formatPositionList(loc, als_a_pos), formatPositionList(loc, als_b_pos), data.values[0],
                               data.values[1]);
        }
        case SolvingTechnique::SueDeCoq: {
            if (data.values.empty() || data.region_type == RegionType::None) {
                return step.explanation;
            }
            // region_type: Row/Col; region_index = line index; secondary_region_index = box index
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSueDeCoq)),
                               localizedRegion(loc, data.region_type, data.region_index),
                               data.secondary_region_index + 1);
        }
        case SolvingTechnique::ForcingChain: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainForcingChain)),
                               localizedPosition(loc, data.positions[0]), data.values[0]);
        }
        case SolvingTechnique::NiceLoop: {
            if (data.positions.size() < 2 || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainNiceLoop)),
                               localizedPosition(loc, data.positions[0]), localizedPosition(loc, data.positions[1]),
                               data.values[0]);
        }
        case SolvingTechnique::XCycles: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // technique_subtype: 0=Type1, 1=Type2, 2=Type3
            if (data.technique_subtype == 1 && !data.positions.empty()) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainXCyclesType2)), data.values[0],
                                   localizedPosition(loc, data.positions[0]));
            }
            if (data.technique_subtype == 2 && !data.positions.empty()) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainXCyclesType3)), data.values[0],
                                   localizedPosition(loc, data.positions[0]));
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainXCyclesType1)), data.values[0]);
        }
        case SolvingTechnique::ThreeDMedusa: {
            if (data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainThreeDMedusa)), data.values[0]);
        }
        case SolvingTechnique::HiddenUniqueRectangle: {
            if (data.positions.size() < 4 || data.values.size() < 4) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainHiddenUniqueRectangle)),
                               formatPositionList(loc, data.positions), data.values[0], data.values[1], data.values[2],
                               localizedPosition(loc, data.positions[static_cast<size_t>(data.values[3])]));
        }
        case SolvingTechnique::AvoidableRectangle: {
            if (data.positions.size() < 4 || data.values.size() < 4) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainAvoidableRectangle)),
                               formatPositionList(loc, data.positions), data.values[0], data.values[1], data.values[2],
                               localizedPosition(loc, data.positions[static_cast<size_t>(data.values[3])]));
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
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainALSXYWing)),
                               formatPositionList(loc, als_a), formatPositionList(loc, als_b),
                               formatPositionList(loc, als_c), data.values[0], data.values[1], data.values[2]);
        }
        case SolvingTechnique::DeathBlossom: {
            if (data.positions.empty() || data.values.empty()) {
                return step.explanation;
            }
            // positions[0] = stem, values[0] = Z description
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainDeathBlossom)),
                               localizedPosition(loc, data.positions[0]), data.values[0], data.values[1]);
        }
        case SolvingTechnique::VWXYZWing: {
            if (data.positions.size() < 5 || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainVWXYZWing)),
                               localizedPosition(loc, data.positions[0]), localizedPosition(loc, data.positions[1]),
                               localizedPosition(loc, data.positions[2]), localizedPosition(loc, data.positions[3]),
                               localizedPosition(loc, data.positions[4]), data.values[0]);
        }
        case SolvingTechnique::FrankenFish: {
            if (data.values.size() < 2) {
                return step.explanation;
            }
            // values = {fish_name_placeholder, digit, ...}
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainFrankenFish)), data.values[0],
                               data.values[1], data.positions.empty() ? "" : formatPositionList(loc, data.positions),
                               data.values.size() >= 3 ? std::to_string(data.values[2]) : "");
        }
        case SolvingTechnique::MutantFish: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // values = {digit}, positions = base pattern cells
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainMutantFish)), data.values[0],
                               data.positions.empty() ? "" : formatPositionList(loc, data.positions),
                               "",  // cover description not stored in explanation_data
                               static_cast<int>(step.eliminations.size()));
        }
        case SolvingTechnique::GroupedXCycles: {
            if (data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainGroupedXCycles)), data.values[0],
                               data.values.size() >= 2 ? std::to_string(data.values[1]) : "");
        }
        case SolvingTechnique::SashimiXWing: {
            if (data.values.empty() || data.positions.empty() || data.region_type == RegionType::None) {
                return step.explanation;
            }
            // values = {candidate, row/col1, row/col2}, fin is last position
            if (data.values.size() < 3) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(loc, data.positions.back());
            if (data.region_type == RegionType::Row) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSashimiXWingRow)), data.values[0],
                                   data.values[1], data.values[2], fin_pos);
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSashimiXWingCol)), data.values[0],
                               data.values[1], data.values[2], fin_pos);
        }
        case SolvingTechnique::SashimiSwordfish: {
            if (data.positions.empty() || data.values.size() < 4 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(loc, data.positions.back());
            if (data.region_type == RegionType::Row) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSashimiSwordfishRow)), data.values[0],
                                   data.values[1], data.values[2], data.values[3], fin_pos);
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSashimiSwordfishCol)), data.values[0],
                               data.values[1], data.values[2], data.values[3], fin_pos);
        }
        case SolvingTechnique::SashimiJellyfish: {
            if (data.positions.empty() || data.values.size() < 5 || data.region_type == RegionType::None) {
                return step.explanation;
            }
            auto fin_pos = localizedPosition(loc, data.positions.back());
            if (data.region_type == RegionType::Row) {
                return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSashimiJellyfishRow)), data.values[0],
                                   data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainSashimiJellyfishCol)), data.values[0],
                               data.values[1], data.values[2], data.values[3], data.values[4], fin_pos);
        }
        case SolvingTechnique::KrakenFish: {
            if (data.values.empty() || data.positions.empty()) {
                return step.explanation;
            }
            // Template: "Kraken Fish on value {0}: finned fish with chain-verified eliminations from {1}"
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainKrakenFish)), data.values[0],
                               formatPositionList(loc, data.positions));
        }
        case SolvingTechnique::ALSChain: {
            if (data.values.empty() || data.positions.empty()) {
                return step.explanation;
            }
            // values = [rc1, rc2, ..., z, chain_length]; positions = all ALS cells
            auto chain_len = data.values.back();
            auto val_z = data.values.size() >= 2 ? data.values[static_cast<size_t>(data.values.size() - 2)] : 0;
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainALSChain)), chain_len, val_z,
                               formatPositionList(loc, data.positions));
        }
        case SolvingTechnique::JuniorExocet: {
            if (data.positions.size() < 4 || data.values.empty()) {
                return step.explanation;
            }
            // positions = {base1, base2, target1, target2}, values = base candidates
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainJuniorExocet)),
                               localizedPosition(loc, data.positions[0]), localizedPosition(loc, data.positions[1]),
                               formatValueList(data.values), localizedPosition(loc, data.positions[2]),
                               localizedPosition(loc, data.positions[3]));
        }
        case SolvingTechnique::UniqueLoop: {
            if (data.positions.size() < 4 || data.values.size() < 2) {
                return step.explanation;
            }
            // Template: "Unique Loop: cells {0} with values {{{1},{2}}} — eliminates {1},{2} from {3}"
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainUniqueLoop)),
                               formatPositionList(loc, data.positions), data.values[0], data.values[1],
                               localizedPosition(loc, data.positions.back()));
        }
        case SolvingTechnique::ContinuousNiceLoop: {
            if (data.values.empty()) {
                return step.explanation;
            }
            // Template: "Continuous Nice Loop: loop of {0} nodes — eliminates {1} candidate(s)"
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainContinuousNiceLoop)), data.values[0],
                               data.values.size() >= 2 ? data.values[1] : 0);
        }
        case SolvingTechnique::GroupedNiceLoop: {
            if (data.positions.size() < 2 || data.values.empty()) {
                return step.explanation;
            }
            return fmt::format(fmt::runtime(loc.getString(StringKeys::ExplainGroupedNiceLoop)),
                               localizedPosition(loc, data.positions[0]), localizedPosition(loc, data.positions[1]),
                               data.values[0]);
        }
        case SolvingTechnique::UnitForcingChain:
        case SolvingTechnique::RegionForcingChain:
        case SolvingTechnique::Backtracking:
            return step.explanation;
    }

    return step.explanation;
}

}  // namespace sudoku::core
