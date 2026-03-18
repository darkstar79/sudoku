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

/// Tests for getLocalizedExplanation — advanced techniques not covered in
/// test_localized_explanations.cpp: Swordfish, Skyscraper, TwoStringKite,
/// XYZWing, UniqueRectangle (Types 1-4), WWing, SimpleColoring, FinnedXWing,
/// RemotePairs, BUG, Jellyfish, FinnedSwordfish, EmptyRectangle, WXYZWing,
/// FinnedJellyfish, XYChain, MultiColoring, ALSxZ, SueDeCoq, ForcingChain,
/// NiceLoop.

#include "../../src/core/localization_manager.h"
#include "../../src/core/localized_explanations.h"
#include "../../src/core/solving_technique.h"

#include <filesystem>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

[[nodiscard]] std::filesystem::path projectRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

[[nodiscard]] std::shared_ptr<LocalizationManager> makeEnglishLoc() {
    auto loc = std::make_shared<LocalizationManager>(projectRoot() / "resources" / "locales");
    [[maybe_unused]] auto r = loc->setLocale("en");
    return loc;
}

[[nodiscard]] SolveStep makeStep(SolvingTechnique technique, ExplanationData data, std::string fallback = "fallback") {
    return SolveStep{.type = SolveStepType::Elimination,
                     .technique = technique,
                     .position = data.positions.empty() ? Position{.row = 0, .col = 0} : data.positions[0],
                     .value = data.values.empty() ? 0 : data.values[0],
                     .eliminations = {},
                     .explanation = std::move(fallback),
                     .rating = getTechniqueRating(technique),
                     .explanation_data = std::move(data)};
}

}  // namespace

// ============================================================================
// Swordfish
// ============================================================================

TEST_CASE("getLocalizedExplanation - Swordfish row-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {candidate, r1, r2, r3, c1, c2, c3}
    auto step = makeStep(SolvingTechnique::Swordfish,
                         {.positions = {}, .values = {3, 1, 4, 7, 2, 5, 8}, .region_type = RegionType::Row});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result ==
            "Swordfish on value 3 in Rows 1, 4, 7 and Columns 2, 5, 8 eliminates 3 from other cells in those columns");
}

TEST_CASE("getLocalizedExplanation - Swordfish col-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::Swordfish,
                         {.positions = {}, .values = {5, 2, 6, 9, 3, 7, 1}, .region_type = RegionType::Col});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result ==
            "Swordfish on value 5 in Columns 2, 6, 9 and Rows 3, 7, 1 eliminates 5 from other cells in those rows");
}

TEST_CASE("getLocalizedExplanation - Swordfish fallback on insufficient values", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::Swordfish,
                         {.positions = {}, .values = {3, 1, 4}, .region_type = RegionType::Row}, "raw swordfish");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw swordfish");
}

// ============================================================================
// Skyscraper
// ============================================================================

TEST_CASE("getLocalizedExplanation - Skyscraper", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // positions: [shared1, non-shared1, shared2, non-shared2]
    auto step =
        makeStep(SolvingTechnique::Skyscraper,
                 {.positions = {{.row = 0, .col = 2}, {.row = 0, .col = 6}, {.row = 4, .col = 2}, {.row = 4, .col = 6}},
                  .values = {7},
                  .region_type = RegionType::Row,
                  .region_index = 0,
                  .secondary_region_type = RegionType::Row,
                  .secondary_region_index = 4});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Skyscraper on value 7: conjugate pairs in Row 1 and Row 5 share endpoint R1C3 — eliminates 7 "
                      "from cells seeing both R1C7 and R5C7");
}

TEST_CASE("getLocalizedExplanation - Skyscraper fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::Skyscraper, {.positions = {{.row = 0, .col = 0}}, .values = {7}}, "raw skyscraper");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw skyscraper");
}

// ============================================================================
// TwoStringKite
// ============================================================================

TEST_CASE("getLocalizedExplanation - TwoStringKite", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // positions: [row_end1, row_end2, col_end1, col_end2]
    auto step =
        makeStep(SolvingTechnique::TwoStringKite,
                 {.positions = {{.row = 0, .col = 1}, {.row = 0, .col = 4}, {.row = 2, .col = 1}, {.row = 6, .col = 4}},
                  .values = {5}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "2-String Kite on value 5: row pair R1C2,R1C5 and column pair R3C2,R7C5 connected through shared "
                      "box — eliminates 5 from cells seeing both endpoints");
}

TEST_CASE("getLocalizedExplanation - TwoStringKite fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::TwoStringKite, {.positions = {}, .values = {5}}, "raw kite");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw kite");
}

// ============================================================================
// XYZWing
// ============================================================================

TEST_CASE("getLocalizedExplanation - XYZWing", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(
        SolvingTechnique::XYZWing,
        {.positions = {{.row = 3, .col = 3}, {.row = 3, .col = 7}, {.row = 6, .col = 3}}, .values = {2, 5, 8}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "XYZ-Wing: pivot R4C4 {2,5,8}, wing R4C8 and wing R7C4 eliminate 8 from cells seeing all three");
}

TEST_CASE("getLocalizedExplanation - XYZWing fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::XYZWing, {.positions = {{.row = 0, .col = 0}}, .values = {2, 5}}, "raw xyz");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw xyz");
}

// ============================================================================
// UniqueRectangle
// ============================================================================

TEST_CASE("getLocalizedExplanation - UniqueRectangle Type1", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::UniqueRectangle,
                 {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 4}, {.row = 4, .col = 0}, {.row = 4, .col = 4}},
                  .values = {2, 8},
                  .technique_subtype = 0});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Unique Rectangle: cells R1C1, R1C5, R5C1, R5C5 with values {2,8} — eliminates 2,8 from R5C5 to "
                      "avoid deadly pattern");
}

TEST_CASE("getLocalizedExplanation - UniqueRectangle Type2", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::UniqueRectangle,
                 {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 4}, {.row = 4, .col = 0}, {.row = 4, .col = 4}},
                  .values = {2, 8, 5},
                  .secondary_region_type = RegionType::Row,
                  .secondary_region_index = 3,
                  .technique_subtype = 1});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Unique Rectangle Type 2: cells R1C1, R1C5, R5C1, R5C5 with values {2,8} — extra candidate 5 "
                      "eliminated from cells seeing both floor cells in shared Row 4");
}

TEST_CASE("getLocalizedExplanation - UniqueRectangle Type3", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::UniqueRectangle,
                 {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 4}, {.row = 4, .col = 0}, {.row = 4, .col = 4}},
                  .values = {2, 8},
                  .secondary_region_type = RegionType::Col,
                  .secondary_region_index = 4,
                  .technique_subtype = 2});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Unique Rectangle Type 3: cells R1C1, R1C5, R5C1, R5C5 with values {2,8} — floor extras form "
                      "naked subset in Column 5, eliminating from other cells");
}

TEST_CASE("getLocalizedExplanation - UniqueRectangle Type4", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::UniqueRectangle,
                 {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 4}, {.row = 4, .col = 0}, {.row = 4, .col = 4}},
                  .values = {2, 8, 5, 3},
                  .secondary_region_type = RegionType::Box,
                  .secondary_region_index = 0,
                  .technique_subtype = 3});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Unique Rectangle Type 4: cells R1C1, R1C5, R5C1, R5C5 with values {2,8} — strong link on 5 in "
                      "Box 1 eliminates 3 from floor cells");
}

TEST_CASE("getLocalizedExplanation - UniqueRectangle fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::UniqueRectangle, {.positions = {{.row = 0, .col = 0}}, .values = {2, 8}}, "raw ur");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw ur");
}

// ============================================================================
// WWing
// ============================================================================

TEST_CASE("getLocalizedExplanation - WWing", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::WWing,
                 {.positions = {{.row = 2, .col = 1}, {.row = 6, .col = 7}, {.row = 2, .col = 4}, {.row = 6, .col = 4}},
                  .values = {4, 9}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result ==
            "W-Wing: cells R3C2 and R7C8 {4,9} connected by strong link on 4 — eliminates 9 from cells seeing both");
}

TEST_CASE("getLocalizedExplanation - WWing fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::WWing, {.positions = {}, .values = {4, 9}}, "raw wwing");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw wwing");
}

// ============================================================================
// SimpleColoring
// ============================================================================

TEST_CASE("getLocalizedExplanation - SimpleColoring contradiction", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::SimpleColoring, {.positions = {}, .values = {6}, .technique_subtype = 0});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result ==
            "Simple Coloring on 6: same-color cells see each other — eliminates 6 from all cells of that color");
}

TEST_CASE("getLocalizedExplanation - SimpleColoring exclusion", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::SimpleColoring,
                         {.positions = {{.row = 3, .col = 4}}, .values = {6}, .technique_subtype = 1});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Simple Coloring on 6: cell R4C5 sees both colors — eliminates 6 from R4C5");
}

TEST_CASE("getLocalizedExplanation - SimpleColoring fallback on no values", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::SimpleColoring, {}, "raw sc");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw sc");
}

TEST_CASE("getLocalizedExplanation - SimpleColoring exclusion fallback on empty positions",
          "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // technique_subtype != 0 but positions empty → second fallback
    auto step =
        makeStep(SolvingTechnique::SimpleColoring, {.positions = {}, .values = {6}, .technique_subtype = 1}, "raw sc2");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw sc2");
}

// ============================================================================
// FinnedXWing
// ============================================================================

TEST_CASE("getLocalizedExplanation - FinnedXWing row-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {candidate, row1, row2, col1, col2}; positions.back() = fin
    auto step =
        makeStep(SolvingTechnique::FinnedXWing,
                 {.positions = {{.row = 4, .col = 2}}, .values = {7, 2, 5, 3, 8}, .region_type = RegionType::Row});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Finned X-Wing on value 7 in Rows 2 and 5, Columns 3 and 8 with fin at R5C3 — eliminates 7 from "
                      "cells in fin's box");
}

TEST_CASE("getLocalizedExplanation - FinnedXWing col-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::FinnedXWing,
                 {.positions = {{.row = 0, .col = 6}}, .values = {4, 3, 7, 1, 6}, .region_type = RegionType::Col});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Finned X-Wing on value 4 in Columns 3 and 7, Rows 1 and 6 with fin at R1C7 — eliminates 4 from "
                      "cells in fin's box");
}

TEST_CASE("getLocalizedExplanation - FinnedXWing fallback on no values", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::FinnedXWing, {.positions = {}, .values = {}, .region_type = RegionType::Row},
                         "raw finned x");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw finned x");
}

// ============================================================================
// RemotePairs
// ============================================================================

TEST_CASE("getLocalizedExplanation - RemotePairs", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {A, B, chain_length}
    auto step = makeStep(SolvingTechnique::RemotePairs,
                         {.positions = {{.row = 0, .col = 0}, {.row = 7, .col = 7}}, .values = {3, 7, 6}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Remote Pairs: chain of {3,7} cells from R1C1 to R8C8 (length 6) — eliminates 3,7 from cells "
                      "seeing both endpoints");
}

TEST_CASE("getLocalizedExplanation - RemotePairs fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::RemotePairs, {.positions = {}, .values = {3, 7, 6}}, "raw rp");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw rp");
}

// ============================================================================
// BUG
// ============================================================================

TEST_CASE("getLocalizedExplanation - BUG", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::BUG, {.positions = {{.row = 4, .col = 4}}, .values = {7}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "BUG: all cells bivalue except R5C5 — value 7 must be placed to avoid deadly pattern");
}

TEST_CASE("getLocalizedExplanation - BUG fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::BUG, {}, "raw bug");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw bug");
}

// ============================================================================
// Jellyfish
// ============================================================================

TEST_CASE("getLocalizedExplanation - Jellyfish row-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {candidate, r1, r2, r3, r4, c1, c2, c3, c4} (1-indexed)
    auto step = makeStep(SolvingTechnique::Jellyfish,
                         {.positions = {}, .values = {2, 1, 3, 6, 8, 2, 4, 7, 9}, .region_type = RegionType::Row});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Jellyfish on value 2 in Rows 1, 3, 6, 8 and Columns 2, 4, 7, 9 eliminates 2 from other cells in "
                      "those columns");
}

TEST_CASE("getLocalizedExplanation - Jellyfish col-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::Jellyfish,
                         {.positions = {}, .values = {5, 2, 4, 7, 9, 1, 3, 6, 8}, .region_type = RegionType::Col});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(
        result ==
        "Jellyfish on value 5 in Columns 2, 4, 7, 9 and Rows 1, 3, 6, 8 eliminates 5 from other cells in those rows");
}

TEST_CASE("getLocalizedExplanation - Jellyfish fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::Jellyfish,
                         {.positions = {}, .values = {2, 1, 3}, .region_type = RegionType::Row}, "raw jellyfish");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw jellyfish");
}

// ============================================================================
// FinnedSwordfish
// ============================================================================

TEST_CASE("getLocalizedExplanation - FinnedSwordfish row-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {candidate, row/col1, row/col2, row/col3}; positions.back() = fin
    auto step = makeStep(SolvingTechnique::FinnedSwordfish,
                         {.positions = {{.row = 4, .col = 3}}, .values = {6, 2, 5, 8}, .region_type = RegionType::Row});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result ==
            "Finned Swordfish on value 6 in Rows 2, 5, 8 with fin at R5C4 — eliminates 6 from cells in fin's box");
}

TEST_CASE("getLocalizedExplanation - FinnedSwordfish col-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::FinnedSwordfish,
                         {.positions = {{.row = 6, .col = 3}}, .values = {3, 1, 4, 7}, .region_type = RegionType::Col});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result ==
            "Finned Swordfish on value 3 in Columns 1, 4, 7 with fin at R7C4 — eliminates 3 from cells in fin's box");
}

TEST_CASE("getLocalizedExplanation - FinnedSwordfish fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::FinnedSwordfish,
                         {.positions = {}, .values = {6, 2, 5, 8}, .region_type = RegionType::Row}, "raw fs");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw fs");
}

// ============================================================================
// EmptyRectangle
// ============================================================================

TEST_CASE("getLocalizedExplanation - EmptyRectangle", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {candidate, box+1}; positions.back() = elimination target
    auto step = makeStep(
        SolvingTechnique::EmptyRectangle,
        {.positions = {{.row = 7, .col = 5}}, .values = {4, 5}, .region_type = RegionType::Row, .region_index = 2});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Empty Rectangle on value 4: ER in Box 5 with conjugate pair in Row 3 — eliminates 4 from R8C6");
}

TEST_CASE("getLocalizedExplanation - EmptyRectangle fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::EmptyRectangle, {.positions = {}, .values = {4, 5}}, "raw er");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw er");
}

// ============================================================================
// WXYZWing
// ============================================================================

TEST_CASE("getLocalizedExplanation - WXYZWing", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // positions: [pivot, w1, w2, w3]; values: [Z]
    auto step =
        makeStep(SolvingTechnique::WXYZWing,
                 {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 4}, {.row = 4, .col = 0}, {.row = 4, .col = 4}},
                  .values = {7}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "WXYZ-Wing: pivot R1C1 with wings R1C5, R5C1, R5C5 — eliminates 7 from cells seeing all four");
}

TEST_CASE("getLocalizedExplanation - WXYZWing fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::WXYZWing, {.positions = {}, .values = {7}}, "raw wxyz");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw wxyz");
}

// ============================================================================
// FinnedJellyfish
// ============================================================================

TEST_CASE("getLocalizedExplanation - FinnedJellyfish row-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {candidate, row/col1..4}; positions.back() = fin
    auto step =
        makeStep(SolvingTechnique::FinnedJellyfish,
                 {.positions = {{.row = 2, .col = 4}}, .values = {8, 1, 3, 5, 7}, .region_type = RegionType::Row});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result ==
            "Finned Jellyfish on value 8 in Rows 1, 3, 5, 7 with fin at R3C5 — eliminates 8 from cells in fin's box");
}

TEST_CASE("getLocalizedExplanation - FinnedJellyfish col-based", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step =
        makeStep(SolvingTechnique::FinnedJellyfish,
                 {.positions = {{.row = 2, .col = 4}}, .values = {8, 1, 3, 5, 7}, .region_type = RegionType::Col});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(
        result ==
        "Finned Jellyfish on value 8 in Columns 1, 3, 5, 7 with fin at R3C5 — eliminates 8 from cells in fin's box");
}

TEST_CASE("getLocalizedExplanation - FinnedJellyfish fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::FinnedJellyfish,
                         {.positions = {}, .values = {8, 1, 3, 5, 7}, .region_type = RegionType::Row}, "raw fj");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw fj");
}

// ============================================================================
// XYChain
// ============================================================================

TEST_CASE("getLocalizedExplanation - XYChain", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {X, chain_length}
    auto step = makeStep(SolvingTechnique::XYChain,
                         {.positions = {{.row = 0, .col = 0}, {.row = 7, .col = 7}}, .values = {9, 6}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result ==
            "XY-Chain: chain of 6 bivalue cells from R1C1 to R8C8 — eliminates 9 from cells seeing both endpoints");
}

TEST_CASE("getLocalizedExplanation - XYChain fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::XYChain, {.positions = {}, .values = {9}}, "raw xychain");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw xychain");
}

// ============================================================================
// MultiColoring
// ============================================================================

TEST_CASE("getLocalizedExplanation - MultiColoring wrap", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::MultiColoring, {.positions = {}, .values = {4}, .technique_subtype = 0});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(
        result ==
        "Multi-Coloring on 4: color sees both colors of another cluster — eliminates 4 from all cells of that color");
}

TEST_CASE("getLocalizedExplanation - MultiColoring trap", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::MultiColoring,
                         {.positions = {{.row = 5, .col = 3}}, .values = {4}, .technique_subtype = 1});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Multi-Coloring on 4: cell R6C4 sees complementary colors from two clusters — eliminates 4");
}

TEST_CASE("getLocalizedExplanation - MultiColoring fallback on empty values", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::MultiColoring, {}, "raw mc");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw mc");
}

TEST_CASE("getLocalizedExplanation - MultiColoring trap fallback on empty positions",
          "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // technique_subtype != 0, no positions → second fallback
    auto step =
        makeStep(SolvingTechnique::MultiColoring, {.positions = {}, .values = {4}, .technique_subtype = 1}, "raw mc2");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw mc2");
}

// ============================================================================
// ALSxZ
// ============================================================================

TEST_CASE("getLocalizedExplanation - ALSxZ", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    // values = {X, Z, als_a_size, als_b_size}
    // positions = als_a cells... + als_b cells...
    auto step =
        makeStep(SolvingTechnique::ALSxZ,
                 {.positions = {{.row = 0, .col = 1}, {.row = 0, .col = 3}, {.row = 2, .col = 1}, {.row = 2, .col = 3}},
                  .values = {5, 3, 2, 2}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "ALS-XZ: ALS R1C2, R1C4 and ALS R3C2, R3C4 linked by restricted common 5 — eliminates 3 from "
                      "cells seeing both ALSs");
}

TEST_CASE("getLocalizedExplanation - ALSxZ fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::ALSxZ, {.positions = {}, .values = {5, 3}}, "raw als");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw als");
}

// ============================================================================
// SueDeCoq
// ============================================================================

TEST_CASE("getLocalizedExplanation - SueDeCoq", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::SueDeCoq, {.positions = {},
                                                      .values = {3, 5},
                                                      .region_type = RegionType::Row,
                                                      .region_index = 2,
                                                      .secondary_region_index = 1});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Sue de Coq: intersection of Row 3 and Box 2 — eliminates candidates from rest of line and box");
}

TEST_CASE("getLocalizedExplanation - SueDeCoq fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::SueDeCoq, {.positions = {}, .values = {}}, "raw sdc");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw sdc");
}

// ============================================================================
// ForcingChain
// ============================================================================

TEST_CASE("getLocalizedExplanation - ForcingChain", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::ForcingChain, {.positions = {{.row = 4, .col = 4}}, .values = {7}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Forcing Chain: assuming each candidate in R5C5 leads to the same conclusion — 7");
}

TEST_CASE("getLocalizedExplanation - ForcingChain fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::ForcingChain, {}, "raw fc");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw fc");
}

// ============================================================================
// NiceLoop
// ============================================================================

TEST_CASE("getLocalizedExplanation - NiceLoop", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::NiceLoop,
                         {.positions = {{.row = 0, .col = 0}, {.row = 8, .col = 8}}, .values = {5}});
    auto result = getLocalizedExplanation(*loc, step);
    REQUIRE(result == "Nice Loop: alternating inference chain from R1C1 to R9C9 — eliminates 5");
}

TEST_CASE("getLocalizedExplanation - NiceLoop fallback", "[localized_explanations_advanced]") {
    auto loc = makeEnglishLoc();
    auto step = makeStep(SolvingTechnique::NiceLoop, {.positions = {{.row = 0, .col = 0}}, .values = {}}, "raw nl");
    REQUIRE(getLocalizedExplanation(*loc, step) == "raw nl");
}
