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

#include "../../src/core/localized_explanations.h"
#include "../../src/core/solving_technique.h"

#include <filesystem>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {

/// Helper to build a minimal SolveStep for testing
[[nodiscard]] SolveStep makeStep(SolvingTechnique technique, ExplanationData data,
                                 std::string fallback = "fallback explanation") {
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
// Helper function tests
// ============================================================================

TEST_CASE("localizedPosition formats position with English locale", "[localized_explanations]") {
    SECTION("Top-left corner") {
        REQUIRE(localizedPosition({.row = 0, .col = 0}) == "R1C1");
    }

    SECTION("Middle cell") {
        REQUIRE(localizedPosition({.row = 4, .col = 4}) == "R5C5");
    }

    SECTION("Bottom-right corner") {
        REQUIRE(localizedPosition({.row = 8, .col = 8}) == "R9C9");
    }
}

TEST_CASE("localizedRegion formats region name with English locale", "[localized_explanations]") {
    SECTION("Row") {
        REQUIRE(localizedRegion(RegionType::Row, 2) == "Row 3");
    }

    SECTION("Column") {
        REQUIRE(localizedRegion(RegionType::Col, 5) == "Column 6");
    }

    SECTION("Box") {
        REQUIRE(localizedRegion(RegionType::Box, 0) == "Box 1");
    }
}

TEST_CASE("formatValueList formats comma-separated values", "[localized_explanations]") {
    SECTION("Single value") {
        REQUIRE(formatValueList({5}) == "5");
    }

    SECTION("Two values") {
        REQUIRE(formatValueList({3, 7}) == "3, 7");
    }

    SECTION("Four values") {
        REQUIRE(formatValueList({1, 2, 3, 4}) == "1, 2, 3, 4");
    }
}

TEST_CASE("formatPositionList formats comma-separated positions", "[localized_explanations]") {
    SECTION("Single position") {
        REQUIRE(formatPositionList({{.row = 2, .col = 3}}) == "R3C4");
    }

    SECTION("Two positions") {
        REQUIRE(formatPositionList({{.row = 0, .col = 0}, {.row = 8, .col = 8}}) == "R1C1, R9C9");
    }
}

// ============================================================================
// Technique explanation tests (with real English locale)
// ============================================================================

TEST_CASE("getLocalizedExplanation - NakedSingle", "[localized_explanations]") {
    auto step = makeStep(SolvingTechnique::NakedSingle, {.positions = {{.row = 2, .col = 4}}, .values = {7}});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result == "Naked Single at R3C5: only value 7 is possible");
}

TEST_CASE("getLocalizedExplanation - HiddenSingle", "[localized_explanations]") {
    auto step = makeStep(SolvingTechnique::HiddenSingle, {.positions = {{.row = 0, .col = 0}}, .values = {3}});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result == "Hidden Single at R1C1: value 3 can only appear in this cell within its region");
}

TEST_CASE("getLocalizedExplanation - NakedPair", "[localized_explanations]") {
    auto step = makeStep(SolvingTechnique::NakedPair, {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}},
                                                       .values = {2, 5},
                                                       .region_type = RegionType::Row,
                                                       .region_index = 0});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result == "Naked Pair [2, 5] at R1C1, R1C4 in Row 1 eliminates candidates from other cells");
}

TEST_CASE("getLocalizedExplanation - NakedTriple", "[localized_explanations]") {
    auto step = makeStep(SolvingTechnique::NakedTriple,
                         {.positions = {{.row = 1, .col = 0}, {.row = 1, .col = 3}, {.row = 1, .col = 6}},
                          .values = {1, 4, 9},
                          .region_type = RegionType::Row,
                          .region_index = 1});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result == "Naked Triple [1, 4, 9] at R2C1, R2C4, R2C7 in Row 2 eliminates candidates from other cells");
}

TEST_CASE("getLocalizedExplanation - HiddenPair", "[localized_explanations]") {
    auto step = makeStep(SolvingTechnique::HiddenPair, {.positions = {{.row = 3, .col = 1}, {.row = 3, .col = 5}},
                                                        .values = {6, 8},
                                                        .region_type = RegionType::Row,
                                                        .region_index = 3});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result == "Hidden Pair [6, 8] at R4C2, R4C6 in Row 4 eliminates other candidates from these cells");
}

TEST_CASE("getLocalizedExplanation - HiddenTriple", "[localized_explanations]") {
    auto step = makeStep(SolvingTechnique::HiddenTriple,
                         {.positions = {{.row = 5, .col = 0}, {.row = 5, .col = 2}, {.row = 5, .col = 7}},
                          .values = {2, 4, 7},
                          .region_type = RegionType::Row,
                          .region_index = 5});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result ==
            "Hidden Triple [2, 4, 7] at R6C1, R6C3, R6C8 in Row 6 eliminates other candidates from these cells");
}

TEST_CASE("getLocalizedExplanation - PointingPair", "[localized_explanations]") {
    SECTION("Confined to row") {
        auto step = makeStep(SolvingTechnique::PointingPair, {.positions = {},
                                                              .values = {3},
                                                              .region_type = RegionType::Box,
                                                              .region_index = 0,
                                                              .secondary_region_type = RegionType::Row,
                                                              .secondary_region_index = 1});

        auto result = getLocalizedExplanation(step);
        REQUIRE(result == "Pointing Pair: 3 in Box 1 confined to Row 2 eliminates 3 from other cells in Row 2");
    }

    SECTION("Confined to column") {
        auto step = makeStep(SolvingTechnique::PointingPair, {.positions = {},
                                                              .values = {5},
                                                              .region_type = RegionType::Box,
                                                              .region_index = 4,
                                                              .secondary_region_type = RegionType::Col,
                                                              .secondary_region_index = 3});

        auto result = getLocalizedExplanation(step);
        REQUIRE(result == "Pointing Pair: 5 in Box 5 confined to Column 4 eliminates 5 from other cells in Column 4");
    }
}

TEST_CASE("getLocalizedExplanation - BoxLineReduction", "[localized_explanations]") {
    SECTION("Row confined to box") {
        auto step = makeStep(SolvingTechnique::BoxLineReduction, {.positions = {},
                                                                  .values = {7},
                                                                  .region_type = RegionType::Row,
                                                                  .region_index = 2,
                                                                  .secondary_region_type = RegionType::Box,
                                                                  .secondary_region_index = 0});

        auto result = getLocalizedExplanation(step);
        REQUIRE(result == "Box/Line Reduction: 7 in Row 3 confined to Box 1 eliminates 7 from other cells in Box 1");
    }

    SECTION("Column confined to box") {
        auto step = makeStep(SolvingTechnique::BoxLineReduction, {.positions = {},
                                                                  .values = {4},
                                                                  .region_type = RegionType::Col,
                                                                  .region_index = 5,
                                                                  .secondary_region_type = RegionType::Box,
                                                                  .secondary_region_index = 7});

        auto result = getLocalizedExplanation(step);
        REQUIRE(result == "Box/Line Reduction: 4 in Column 6 confined to Box 8 eliminates 4 from other cells in Box 8");
    }
}

TEST_CASE("getLocalizedExplanation - NakedQuad", "[localized_explanations]") {
    auto step =
        makeStep(SolvingTechnique::NakedQuad,
                 {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 0, .col = 2}, {.row = 0, .col = 3}},
                  .values = {1, 2, 3, 4},
                  .region_type = RegionType::Row,
                  .region_index = 0});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result ==
            "Naked Quad [1, 2, 3, 4] at R1C1, R1C2, R1C3, R1C4 in Row 1 eliminates candidates from other cells");
}

TEST_CASE("getLocalizedExplanation - HiddenQuad", "[localized_explanations]") {
    auto step =
        makeStep(SolvingTechnique::HiddenQuad,
                 {.positions = {{.row = 6, .col = 0}, {.row = 6, .col = 2}, {.row = 6, .col = 5}, {.row = 6, .col = 8}},
                  .values = {1, 3, 5, 9},
                  .region_type = RegionType::Row,
                  .region_index = 6});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result ==
            "Hidden Quad [1, 3, 5, 9] at R7C1, R7C3, R7C6, R7C9 in Row 7 eliminates other candidates from these cells");
}

TEST_CASE("getLocalizedExplanation - XWing row-based", "[localized_explanations]") {
    // Positions: [r1c1, r1c2, r2c1, r2c2] = corners of the X-Wing
    auto step =
        makeStep(SolvingTechnique::XWing,
                 {.positions = {{.row = 1, .col = 2}, {.row = 1, .col = 7}, {.row = 5, .col = 2}, {.row = 5, .col = 7}},
                  .values = {6},
                  .region_type = RegionType::Row});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result ==
            "X-Wing on value 6 in Rows 2 and 6, Columns 3 and 8 eliminates 6 from other cells in those columns");
}

TEST_CASE("getLocalizedExplanation - XWing col-based", "[localized_explanations]") {
    auto step =
        makeStep(SolvingTechnique::XWing,
                 {.positions = {{.row = 0, .col = 3}, {.row = 4, .col = 3}, {.row = 0, .col = 7}, {.row = 4, .col = 7}},
                  .values = {2},
                  .region_type = RegionType::Col});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result == "X-Wing on value 2 in Columns 4 and 4, Rows 1 and 1 eliminates 2 from other cells in those rows");
}

TEST_CASE("getLocalizedExplanation - XYWing", "[localized_explanations]") {
    // pivot has {a,b}, wing1 has {a,c}, wing2 has {b,c}
    auto step = makeStep(
        SolvingTechnique::XYWing,
        {.positions = {{.row = 3, .col = 3}, {.row = 3, .col = 7}, {.row = 6, .col = 3}}, .values = {2, 5, 8}});

    auto result = getLocalizedExplanation(step);
    REQUIRE(result ==
            "XY-Wing: pivot R4C4 {2,5}, wing R4C8 {2,8}, wing R7C4 {5,8} eliminates 8 from cells seeing both wings");
}

TEST_CASE("getLocalizedExplanation - Backtracking returns raw explanation", "[localized_explanations]") {
    auto step = makeStep(SolvingTechnique::Backtracking, {}, "Solved by backtracking");

    auto result = getLocalizedExplanation(step);
    REQUIRE(result == "Solved by backtracking");
}

// ============================================================================
// Fallback tests (insufficient data falls back to raw explanation)
// ============================================================================

TEST_CASE("getLocalizedExplanation falls back when data is insufficient", "[localized_explanations]") {
    SECTION("NakedSingle with no positions") {
        auto step = makeStep(SolvingTechnique::NakedSingle, {.positions = {}, .values = {5}}, "raw naked single");
        REQUIRE(getLocalizedExplanation(step) == "raw naked single");
    }

    SECTION("NakedSingle with no values") {
        auto step = makeStep(SolvingTechnique::NakedSingle, {.positions = {{.row = 0, .col = 0}}, .values = {}},
                             "raw naked single");
        REQUIRE(getLocalizedExplanation(step) == "raw naked single");
    }

    SECTION("NakedPair with insufficient positions") {
        auto step = makeStep(SolvingTechnique::NakedPair,
                             {.positions = {{.row = 0, .col = 0}}, .values = {1, 2}, .region_type = RegionType::Row},
                             "raw naked pair");
        REQUIRE(getLocalizedExplanation(step) == "raw naked pair");
    }

    SECTION("XWing with no region_type") {
        auto step = makeStep(
            SolvingTechnique::XWing,
            {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 1, .col = 0}, {.row = 1, .col = 1}},
             .values = {3}},
            "raw x-wing");
        REQUIRE(getLocalizedExplanation(step) == "raw x-wing");
    }
}

// ============================================================================
// localizedRegion default branch
// ============================================================================

TEST_CASE("localizedRegion with None type returns unknown region string", "[localized_explanations]") {
    // RegionType::None triggers the default case in localizedRegion
    auto result = localizedRegion(RegionType::None, 0);
    // Just verify it doesn't crash and returns something
    REQUIRE(!result.empty());
}

// ============================================================================
// Sub-condition fallbacks for multi-condition guards
// ============================================================================

TEST_CASE("getLocalizedExplanation sub-condition fallbacks", "[localized_explanations]") {
    SECTION("NakedPair: values too few → fallback") {
        auto step = makeStep(SolvingTechnique::NakedPair,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}},
                              .values = {1},  // only 1 value, need 2
                              .region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("NakedPair: region=None → fallback") {
        auto step = makeStep(SolvingTechnique::NakedPair,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}},
                              .values = {1, 2},
                              .region_type = RegionType::None},  // no region
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("NakedTriple: values too few → fallback") {
        auto step = makeStep(SolvingTechnique::NakedTriple,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}, {.row = 0, .col = 6}},
                              .values = {1, 2},  // only 2 values, need 3
                              .region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("NakedTriple: region=None → fallback") {
        auto step = makeStep(SolvingTechnique::NakedTriple,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}, {.row = 0, .col = 6}},
                              .values = {1, 2, 3},
                              .region_type = RegionType::None},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenPair: values too few → fallback") {
        auto step = makeStep(SolvingTechnique::HiddenPair,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}},
                              .values = {1},  // need 2
                              .region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenPair: region=None → fallback") {
        auto step = makeStep(SolvingTechnique::HiddenPair,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}},
                              .values = {1, 2},
                              .region_type = RegionType::None},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenTriple: values too few → fallback") {
        auto step = makeStep(SolvingTechnique::HiddenTriple,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}, {.row = 0, .col = 6}},
                              .values = {1, 2},  // need 3
                              .region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenTriple: region=None → fallback") {
        auto step = makeStep(SolvingTechnique::HiddenTriple,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}, {.row = 0, .col = 6}},
                              .values = {1, 2, 3},
                              .region_type = RegionType::None},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("NakedQuad: values too few → fallback") {
        auto step = makeStep(
            SolvingTechnique::NakedQuad,
            {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 0, .col = 2}, {.row = 0, .col = 3}},
             .values = {1, 2, 3},  // need 4
             .region_type = RegionType::Row},
            "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("NakedQuad: region=None → fallback") {
        auto step = makeStep(
            SolvingTechnique::NakedQuad,
            {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 0, .col = 2}, {.row = 0, .col = 3}},
             .values = {1, 2, 3, 4},
             .region_type = RegionType::None},
            "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenQuad: values too few → fallback") {
        auto step = makeStep(
            SolvingTechnique::HiddenQuad,
            {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 0, .col = 2}, {.row = 0, .col = 3}},
             .values = {1, 2, 3},  // need 4
             .region_type = RegionType::Row},
            "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenQuad: region=None → fallback") {
        auto step = makeStep(
            SolvingTechnique::HiddenQuad,
            {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 0, .col = 2}, {.row = 0, .col = 3}},
             .values = {1, 2, 3, 4},
             .region_type = RegionType::None},
            "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("PointingPair: values empty → fallback") {
        auto step = makeStep(SolvingTechnique::PointingPair,
                             {.positions = {},
                              .values = {},  // empty values
                              .region_type = RegionType::Box,
                              .secondary_region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("BoxLineReduction: region=None → fallback") {
        auto step = makeStep(SolvingTechnique::BoxLineReduction,
                             {.positions = {},
                              .values = {3},
                              .region_type = RegionType::None,  // no region
                              .secondary_region_type = RegionType::Box},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("BoxLineReduction: secondary=None → fallback") {
        auto step = makeStep(
            SolvingTechnique::BoxLineReduction,
            {.positions = {}, .values = {3}, .region_type = RegionType::Row, .secondary_region_type = RegionType::None},
            "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }
}

// ============================================================================
// Additional sub-condition fallback coverage
// ============================================================================

TEST_CASE("getLocalizedExplanation HiddenSingle fallback", "[localized_explanations]") {
    SECTION("No positions") {
        auto step = makeStep(SolvingTechnique::HiddenSingle, {.positions = {}, .values = {5}}, "raw hs");
        REQUIRE(getLocalizedExplanation(step) == "raw hs");
    }

    SECTION("No values") {
        auto step =
            makeStep(SolvingTechnique::HiddenSingle, {.positions = {{.row = 0, .col = 0}}, .values = {}}, "raw hs2");
        REQUIRE(getLocalizedExplanation(step) == "raw hs2");
    }
}

TEST_CASE("getLocalizedExplanation positions-too-few fallbacks", "[localized_explanations]") {
    SECTION("NakedTriple: positions<3 → fallback") {
        auto step = makeStep(SolvingTechnique::NakedTriple,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}},
                              .values = {1, 2, 3},
                              .region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenPair: positions<2 → fallback") {
        auto step = makeStep(SolvingTechnique::HiddenPair,
                             {.positions = {}, .values = {1, 2}, .region_type = RegionType::Row}, "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenTriple: positions<3 → fallback") {
        auto step = makeStep(SolvingTechnique::HiddenTriple,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 3}},
                              .values = {1, 2, 3},
                              .region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("NakedQuad: positions<4 → fallback") {
        auto step = makeStep(SolvingTechnique::NakedQuad,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 0, .col = 2}},
                              .values = {1, 2, 3, 4},
                              .region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("HiddenQuad: positions<4 → fallback") {
        auto step = makeStep(SolvingTechnique::HiddenQuad,
                             {.positions = {{.row = 0, .col = 0}, {.row = 0, .col = 1}, {.row = 0, .col = 2}},
                              .values = {1, 2, 3, 4},
                              .region_type = RegionType::Row},
                             "raw");
        REQUIRE(getLocalizedExplanation(step) == "raw");
    }

    SECTION("XWing Row branch: positions<4 → fallback") {
        // values non-empty, region_type=Row, but positions only 3 → inner fallback
        auto step = makeStep(SolvingTechnique::XWing,
                             {.positions = {{.row = 1, .col = 2}, {.row = 1, .col = 7}, {.row = 5, .col = 2}},
                              .values = {6},
                              .region_type = RegionType::Row},
                             "raw xw row");
        REQUIRE(getLocalizedExplanation(step) == "raw xw row");
    }

    SECTION("XWing Col branch: positions<4 → fallback") {
        auto step = makeStep(SolvingTechnique::XWing,
                             {.positions = {{.row = 0, .col = 3}, {.row = 4, .col = 3}, {.row = 0, .col = 7}},
                              .values = {2},
                              .region_type = RegionType::Col},
                             "raw xw col");
        REQUIRE(getLocalizedExplanation(step) == "raw xw col");
    }

    SECTION("XYWing: positions<3 → fallback") {
        auto step = makeStep(SolvingTechnique::XYWing, {.positions = {{.row = 3, .col = 3}}, .values = {2, 5, 8}},
                             "raw xywing");
        REQUIRE(getLocalizedExplanation(step) == "raw xywing");
    }
}

TEST_CASE("getLocalizedExplanation PointingPair additional sub-conditions", "[localized_explanations]") {
    SECTION("region_type=None → fallback") {
        auto step = makeStep(
            SolvingTechnique::PointingPair,
            {.positions = {}, .values = {3}, .region_type = RegionType::None, .secondary_region_type = RegionType::Row},
            "raw pp");
        REQUIRE(getLocalizedExplanation(step) == "raw pp");
    }

    SECTION("secondary_region_type=None → fallback") {
        auto step = makeStep(
            SolvingTechnique::PointingPair,
            {.positions = {}, .values = {3}, .region_type = RegionType::Box, .secondary_region_type = RegionType::None},
            "raw pp2");
        REQUIRE(getLocalizedExplanation(step) == "raw pp2");
    }

    SECTION("secondary_region_type=Box → inner default case") {
        // Passes the guard (secondary_region_type != None), but triggers default in inner switch
        auto step = makeStep(SolvingTechnique::PointingPair, {.positions = {},
                                                              .values = {7},
                                                              .region_type = RegionType::Box,
                                                              .region_index = 2,
                                                              .secondary_region_type = RegionType::Box,
                                                              .secondary_region_index = 2});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());  // returns something (uses RegionUnknown)
    }
}

TEST_CASE("getLocalizedExplanation BoxLineReduction additional sub-conditions", "[localized_explanations]") {
    SECTION("values empty → fallback") {
        auto step = makeStep(
            SolvingTechnique::BoxLineReduction,
            {.positions = {}, .values = {}, .region_type = RegionType::Row, .secondary_region_type = RegionType::Box},
            "raw blr");
        REQUIRE(getLocalizedExplanation(step) == "raw blr");
    }

    SECTION("region_type=Box → inner default case") {
        // region_type is Box (not Row or Col) and secondary_region_type is Row (not None)
        // Passes the guard, triggers default in inner switch
        auto step = makeStep(SolvingTechnique::BoxLineReduction, {.positions = {},
                                                                  .values = {5},
                                                                  .region_type = RegionType::Box,
                                                                  .region_index = 0,
                                                                  .secondary_region_type = RegionType::Row,
                                                                  .secondary_region_index = 0});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }
}

TEST_CASE("getLocalizedExplanation Backtracking case", "[localized_explanations]") {
    // Backtracking case just returns step.explanation directly
    auto step = makeStep(SolvingTechnique::Backtracking, {}, "backtracking raw explanation");
    auto result = getLocalizedExplanation(step);
    REQUIRE(result == "backtracking raw explanation");
}

TEST_CASE("getLocalizedExplanation localizedRegion None type", "[localized_explanations]") {
    // RegionType::None hits the default case in localizedRegion switch
    REQUIRE_FALSE(localizedRegion(RegionType::None, 0).empty());
}

TEST_CASE("getLocalizedExplanation formatPositionList with empty list", "[localized_explanations]") {
    // formatPositionList with empty positions should return ""
    std::vector<Position> empty;
    REQUIRE(formatPositionList(empty).empty());
}

// ============================================================================
// getLocalizedTechniqueName tests
// ============================================================================

TEST_CASE("getLocalizedTechniqueName returns English names", "[localized_explanations]") {
    using namespace std::string_view_literals;
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::NakedSingle) == "Naked Single"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::HiddenSingle) == "Hidden Single"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::NakedPair) == "Naked Pair"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::NakedTriple) == "Naked Triple"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::HiddenPair) == "Hidden Pair"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::HiddenTriple) == "Hidden Triple"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::PointingPair) == "Pointing Pair"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::BoxLineReduction) == "Box/Line Reduction"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::NakedQuad) == "Naked Quad"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::HiddenQuad) == "Hidden Quad"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::XWing) == "X-Wing"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::XYWing) == "XY-Wing"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::Backtracking) == "Backtracking"sv);
}

// ============================================================================
// Tests for techniques not yet covered: Swordfish, Skyscraper, TwoStringKite,
// XYZWing, UniqueRectangle, WWing, SimpleColoring, FinnedXWing, RemotePairs,
// BUG, Jellyfish, FinnedSwordfish, EmptyRectangle, WXYZWing, FinnedJellyfish,
// XYChain, MultiColoring, ALSxZ, SueDeCoq, ForcingChain, NiceLoop
// ============================================================================

TEST_CASE("getLocalizedExplanation - Swordfish", "[localized_explanations]") {
    // values = {candidate, r1, r2, r3, c1, c2, c3} (7 values)

    SECTION("Row-based Swordfish") {
        auto step =
            makeStep(SolvingTechnique::Swordfish, {.values = {5, 1, 2, 3, 4, 5, 6}, .region_type = RegionType::Row});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Col-based Swordfish") {
        auto step =
            makeStep(SolvingTechnique::Swordfish, {.values = {5, 1, 2, 3, 4, 5, 6}, .region_type = RegionType::Col});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values too few") {
        auto step = makeStep(SolvingTechnique::Swordfish, {.values = {5, 1}, .region_type = RegionType::Row},
                             "swordfish fallback");
        auto result = getLocalizedExplanation(step);
        REQUIRE(result == "swordfish fallback");
    }

    SECTION("Fallback: region_type None") {
        auto step = makeStep(SolvingTechnique::Swordfish,
                             {.values = {5, 1, 2, 3, 4, 5, 6}, .region_type = RegionType::None}, "swordfish fallback");
        auto result = getLocalizedExplanation(step);
        REQUIRE(result == "swordfish fallback");
    }
}

TEST_CASE("getLocalizedExplanation - Skyscraper", "[localized_explanations]") {
    const std::vector<Position> four_pos = {{0, 0}, {0, 4}, {2, 0}, {5, 4}};

    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::Skyscraper, {.positions = four_pos,
                                                            .values = {5},
                                                            .region_type = RegionType::Row,
                                                            .region_index = 0,
                                                            .secondary_region_type = RegionType::Col,
                                                            .secondary_region_index = 0});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions too few") {
        auto step =
            makeStep(SolvingTechnique::Skyscraper, {.positions = {{0, 0}}, .values = {5}}, "skyscraper fallback");
        REQUIRE(getLocalizedExplanation(step) == "skyscraper fallback");
    }

    SECTION("Fallback: values empty") {
        auto step = makeStep(SolvingTechnique::Skyscraper, {.positions = four_pos}, "skyscraper fallback");
        REQUIRE(getLocalizedExplanation(step) == "skyscraper fallback");
    }
}

TEST_CASE("getLocalizedExplanation - TwoStringKite", "[localized_explanations]") {
    const std::vector<Position> four_pos = {{0, 0}, {0, 4}, {3, 0}, {6, 4}};

    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::TwoStringKite, {.positions = four_pos, .values = {5}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions too few") {
        auto step = makeStep(SolvingTechnique::TwoStringKite, {.positions = {{0, 0}}, .values = {5}}, "kite fallback");
        REQUIRE(getLocalizedExplanation(step) == "kite fallback");
    }
}

TEST_CASE("getLocalizedExplanation - XYZWing", "[localized_explanations]") {
    const std::vector<Position> three_pos = {{0, 0}, {0, 4}, {3, 0}};

    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::XYZWing, {.positions = three_pos, .values = {1, 2, 3}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values too few") {
        auto step = makeStep(SolvingTechnique::XYZWing, {.positions = three_pos, .values = {1, 2}}, "xyzwing fallback");
        REQUIRE(getLocalizedExplanation(step) == "xyzwing fallback");
    }
}

TEST_CASE("getLocalizedExplanation - UniqueRectangle", "[localized_explanations]") {
    const std::vector<Position> four_pos = {{0, 0}, {0, 3}, {3, 0}, {3, 3}};

    SECTION("Type 1 (subtype=0)") {
        auto step = makeStep(SolvingTechnique::UniqueRectangle, {.positions = four_pos,
                                                                 .values = {1, 2},
                                                                 .secondary_region_type = RegionType::Row,
                                                                 .secondary_region_index = 0,
                                                                 .technique_subtype = 0});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Type 2 (subtype=1)") {
        auto step = makeStep(SolvingTechnique::UniqueRectangle, {.positions = four_pos,
                                                                 .values = {1, 2, 3},
                                                                 .secondary_region_type = RegionType::Row,
                                                                 .secondary_region_index = 0,
                                                                 .technique_subtype = 1});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Type 3 (subtype=2)") {
        auto step = makeStep(SolvingTechnique::UniqueRectangle, {.positions = four_pos,
                                                                 .values = {1, 2},
                                                                 .secondary_region_type = RegionType::Row,
                                                                 .secondary_region_index = 0,
                                                                 .technique_subtype = 2});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Type 4 (subtype=3)") {
        auto step = makeStep(SolvingTechnique::UniqueRectangle, {.positions = four_pos,
                                                                 .values = {1, 2, 3, 4},
                                                                 .secondary_region_type = RegionType::Row,
                                                                 .secondary_region_index = 0,
                                                                 .technique_subtype = 3});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Type 6 (subtype=5)") {
        auto step = makeStep(SolvingTechnique::UniqueRectangle, {.positions = four_pos,
                                                                 .values = {1, 2, 3},
                                                                 .secondary_region_type = RegionType::Row,
                                                                 .secondary_region_index = 0,
                                                                 .technique_subtype = 5});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions too few") {
        auto step =
            makeStep(SolvingTechnique::UniqueRectangle, {.positions = {{0, 0}}, .values = {1, 2}}, "ur fallback");
        REQUIRE(getLocalizedExplanation(step) == "ur fallback");
    }
}

TEST_CASE("getLocalizedExplanation - WWing", "[localized_explanations]") {
    const std::vector<Position> four_pos = {
        {.row = 0, .col = 0}, {.row = 0, .col = 4}, {.row = 3, .col = 0}, {.row = 6, .col = 4}};

    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::WWing, {.positions = four_pos, .values = {1, 2}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions too few") {
        auto step = makeStep(SolvingTechnique::WWing, {.positions = {{0, 0}}, .values = {1, 2}}, "wwing fallback");
        REQUIRE(getLocalizedExplanation(step) == "wwing fallback");
    }

    SECTION("Fallback: values too few") {
        auto step = makeStep(SolvingTechnique::WWing, {.positions = four_pos, .values = {1}}, "wwing fallback");
        REQUIRE(getLocalizedExplanation(step) == "wwing fallback");
    }
}

TEST_CASE("getLocalizedExplanation - SimpleColoring", "[localized_explanations]") {
    SECTION("Contradiction (subtype=0)") {
        auto step = makeStep(SolvingTechnique::SimpleColoring, {.values = {5}, .technique_subtype = 0});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Exclusion (subtype=1) with position") {
        auto step =
            makeStep(SolvingTechnique::SimpleColoring, {.positions = {{2, 3}}, .values = {5}, .technique_subtype = 1});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values empty") {
        auto step = makeStep(SolvingTechnique::SimpleColoring, {}, "simple fallback");
        REQUIRE(getLocalizedExplanation(step) == "simple fallback");
    }

    SECTION("Fallback: subtype=1 but positions empty") {
        auto step =
            makeStep(SolvingTechnique::SimpleColoring, {.values = {5}, .technique_subtype = 1}, "simple fallback");
        REQUIRE(getLocalizedExplanation(step) == "simple fallback");
    }
}

TEST_CASE("getLocalizedExplanation - FinnedXWing", "[localized_explanations]") {
    // values[0..4] = {candidate, row1, row2, col1, col2}; positions.back() = fin
    const std::vector<int> vals = {5, 1, 2, 3, 4};
    const std::vector<Position> fin_pos = {{.row = 0, .col = 5}};

    SECTION("Row-based FinnedXWing") {
        auto step = makeStep(SolvingTechnique::FinnedXWing,
                             {.positions = fin_pos, .values = vals, .region_type = RegionType::Row});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Col-based FinnedXWing") {
        auto step = makeStep(SolvingTechnique::FinnedXWing,
                             {.positions = fin_pos, .values = vals, .region_type = RegionType::Col});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values empty") {
        auto step = makeStep(SolvingTechnique::FinnedXWing, {.positions = fin_pos, .region_type = RegionType::Row},
                             "finxwing fallback");
        REQUIRE(getLocalizedExplanation(step) == "finxwing fallback");
    }

    SECTION("Fallback: positions empty (second guard)") {
        auto step = makeStep(SolvingTechnique::FinnedXWing, {.values = vals, .region_type = RegionType::Row},
                             "finxwing fallback");
        REQUIRE(getLocalizedExplanation(step) == "finxwing fallback");
    }
}

TEST_CASE("getLocalizedExplanation - RemotePairs", "[localized_explanations]") {
    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::RemotePairs, {.positions = {{0, 0}, {5, 5}}, .values = {1, 2, 4}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values too few") {
        auto step =
            makeStep(SolvingTechnique::RemotePairs, {.positions = {{0, 0}, {5, 5}}, .values = {1, 2}}, "rp fallback");
        REQUIRE(getLocalizedExplanation(step) == "rp fallback");
    }
}

TEST_CASE("getLocalizedExplanation - BUG", "[localized_explanations]") {
    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::BUG, {.positions = {{3, 4}}, .values = {7}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions empty") {
        auto step = makeStep(SolvingTechnique::BUG, {.values = {7}}, "bug fallback");
        REQUIRE(getLocalizedExplanation(step) == "bug fallback");
    }

    SECTION("Fallback: values empty") {
        auto step = makeStep(SolvingTechnique::BUG, {.positions = {{3, 4}}}, "bug fallback");
        REQUIRE(getLocalizedExplanation(step) == "bug fallback");
    }
}

TEST_CASE("getLocalizedExplanation - Jellyfish", "[localized_explanations]") {
    // values = {candidate, r1, r2, r3, r4, c1, c2, c3, c4} (9 values)
    const std::vector<int> vals = {5, 1, 2, 3, 4, 5, 6, 7, 8};

    SECTION("Row-based Jellyfish") {
        auto step = makeStep(SolvingTechnique::Jellyfish, {.values = vals, .region_type = RegionType::Row});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Col-based Jellyfish") {
        auto step = makeStep(SolvingTechnique::Jellyfish, {.values = vals, .region_type = RegionType::Col});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values too few") {
        auto step =
            makeStep(SolvingTechnique::Jellyfish, {.values = {5, 1}, .region_type = RegionType::Row}, "jf fallback");
        REQUIRE(getLocalizedExplanation(step) == "jf fallback");
    }
}

TEST_CASE("getLocalizedExplanation - FinnedSwordfish", "[localized_explanations]") {
    // values = {candidate, row/col1..3}; positions.back() = fin
    const std::vector<int> vals = {5, 1, 2, 3};
    const std::vector<Position> fin_pos = {{.row = 0, .col = 5}};

    SECTION("Row-based FinnedSwordfish") {
        auto step = makeStep(SolvingTechnique::FinnedSwordfish,
                             {.positions = fin_pos, .values = vals, .region_type = RegionType::Row});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Col-based FinnedSwordfish") {
        auto step = makeStep(SolvingTechnique::FinnedSwordfish,
                             {.positions = fin_pos, .values = vals, .region_type = RegionType::Col});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions empty") {
        auto step = makeStep(SolvingTechnique::FinnedSwordfish, {.values = vals, .region_type = RegionType::Row},
                             "fsf fallback");
        REQUIRE(getLocalizedExplanation(step) == "fsf fallback");
    }
}

TEST_CASE("getLocalizedExplanation - EmptyRectangle", "[localized_explanations]") {
    SECTION("Happy path") {
        auto step = makeStep(
            SolvingTechnique::EmptyRectangle,
            {.positions = {{.row = 4, .col = 4}}, .values = {5, 3}, .region_type = RegionType::Row, .region_index = 2});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions empty") {
        auto step = makeStep(SolvingTechnique::EmptyRectangle, {.values = {5, 3}, .region_type = RegionType::Row},
                             "er fallback");
        REQUIRE(getLocalizedExplanation(step) == "er fallback");
    }

    SECTION("Fallback: values too few") {
        auto step = makeStep(SolvingTechnique::EmptyRectangle,
                             {.positions = {{4, 4}}, .values = {5}, .region_type = RegionType::Row}, "er fallback");
        REQUIRE(getLocalizedExplanation(step) == "er fallback");
    }
}

TEST_CASE("getLocalizedExplanation - WXYZWing", "[localized_explanations]") {
    const std::vector<Position> four_pos = {{0, 0}, {0, 4}, {3, 0}, {6, 4}};

    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::WXYZWing, {.positions = four_pos, .values = {5}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions too few") {
        auto step = makeStep(SolvingTechnique::WXYZWing, {.positions = {{0, 0}}, .values = {5}}, "wxyz fallback");
        REQUIRE(getLocalizedExplanation(step) == "wxyz fallback");
    }
}

TEST_CASE("getLocalizedExplanation - FinnedJellyfish", "[localized_explanations]") {
    // values = {candidate, row/col1..4}; positions.back() = fin
    const std::vector<int> vals = {5, 1, 2, 3, 4};
    const std::vector<Position> fin_pos = {{0, 5}};

    SECTION("Row-based FinnedJellyfish") {
        auto step = makeStep(SolvingTechnique::FinnedJellyfish,
                             {.positions = fin_pos, .values = vals, .region_type = RegionType::Row});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Col-based FinnedJellyfish") {
        auto step = makeStep(SolvingTechnique::FinnedJellyfish,
                             {.positions = fin_pos, .values = vals, .region_type = RegionType::Col});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values too few") {
        auto step = makeStep(SolvingTechnique::FinnedJellyfish,
                             {.positions = fin_pos, .values = {5}, .region_type = RegionType::Row}, "fjf fallback");
        REQUIRE(getLocalizedExplanation(step) == "fjf fallback");
    }
}

TEST_CASE("getLocalizedExplanation - XYChain", "[localized_explanations]") {
    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::XYChain, {.positions = {{0, 0}, {5, 5}}, .values = {5, 3}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions too few") {
        auto step = makeStep(SolvingTechnique::XYChain, {.positions = {{0, 0}}, .values = {5, 3}}, "xychain fallback");
        REQUIRE(getLocalizedExplanation(step) == "xychain fallback");
    }
}

TEST_CASE("getLocalizedExplanation - MultiColoring", "[localized_explanations]") {
    SECTION("Wrap contradiction (subtype=0)") {
        auto step = makeStep(SolvingTechnique::MultiColoring, {.values = {5}, .technique_subtype = 0});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Trap exclusion (subtype=1) with position") {
        auto step =
            makeStep(SolvingTechnique::MultiColoring, {.positions = {{3, 4}}, .values = {5}, .technique_subtype = 1});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values empty") {
        auto step = makeStep(SolvingTechnique::MultiColoring, {}, "mc fallback");
        REQUIRE(getLocalizedExplanation(step) == "mc fallback");
    }

    SECTION("Fallback: subtype=1 but positions empty") {
        auto step = makeStep(SolvingTechnique::MultiColoring, {.values = {5}, .technique_subtype = 1}, "mc fallback");
        REQUIRE(getLocalizedExplanation(step) == "mc fallback");
    }
}

TEST_CASE("getLocalizedExplanation - ALSxZ", "[localized_explanations]") {
    // values = {X, Z, als_a_size, als_b_size}; positions = als_a_cells + als_b_cells
    const std::vector<Position> positions = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};

    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::ALSxZ, {.positions = positions, .values = {1, 2, 2, 2}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values too few") {
        auto step = makeStep(SolvingTechnique::ALSxZ, {.positions = positions, .values = {1, 2}}, "als fallback");
        REQUIRE(getLocalizedExplanation(step) == "als fallback");
    }

    SECTION("Fallback: positions empty") {
        auto step = makeStep(SolvingTechnique::ALSxZ, {.values = {1, 2, 2, 2}}, "als fallback");
        REQUIRE(getLocalizedExplanation(step) == "als fallback");
    }
}

TEST_CASE("getLocalizedExplanation - SueDeCoq", "[localized_explanations]") {
    SECTION("Happy path") {
        auto step =
            makeStep(SolvingTechnique::SueDeCoq,
                     {.values = {5}, .region_type = RegionType::Row, .region_index = 2, .secondary_region_index = 1});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: values empty") {
        auto step = makeStep(SolvingTechnique::SueDeCoq, {.region_type = RegionType::Row}, "sdc fallback");
        REQUIRE(getLocalizedExplanation(step) == "sdc fallback");
    }

    SECTION("Fallback: region_type None") {
        auto step =
            makeStep(SolvingTechnique::SueDeCoq, {.values = {5}, .region_type = RegionType::None}, "sdc fallback");
        REQUIRE(getLocalizedExplanation(step) == "sdc fallback");
    }
}

TEST_CASE("getLocalizedExplanation - ForcingChain", "[localized_explanations]") {
    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::ForcingChain, {.positions = {{0, 0}}, .values = {5}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions empty") {
        auto step = makeStep(SolvingTechnique::ForcingChain, {.values = {5}}, "fc fallback");
        REQUIRE(getLocalizedExplanation(step) == "fc fallback");
    }

    SECTION("Fallback: values empty") {
        auto step = makeStep(SolvingTechnique::ForcingChain, {.positions = {{0, 0}}}, "fc fallback");
        REQUIRE(getLocalizedExplanation(step) == "fc fallback");
    }
}

TEST_CASE("getLocalizedExplanation - NiceLoop", "[localized_explanations]") {
    SECTION("Happy path") {
        auto step = makeStep(SolvingTechnique::NiceLoop, {.positions = {{0, 0}, {5, 5}}, .values = {5}});
        auto result = getLocalizedExplanation(step);
        REQUIRE(!result.empty());
    }

    SECTION("Fallback: positions too few") {
        auto step = makeStep(SolvingTechnique::NiceLoop, {.positions = {{0, 0}}, .values = {5}}, "nl fallback");
        REQUIRE(getLocalizedExplanation(step) == "nl fallback");
    }

    SECTION("Fallback: values empty") {
        auto step = makeStep(SolvingTechnique::NiceLoop, {.positions = {{0, 0}, {5, 5}}}, "nl fallback");
        REQUIRE(getLocalizedExplanation(step) == "nl fallback");
    }
}

// ============================================================================
// getLocalizedTechniqueName — covers the remaining 21 switch cases that were
// not exercised in the existing test (lines 225-266 in solving_technique.h)
// ============================================================================

TEST_CASE("getLocalizedTechniqueName - advanced techniques", "[localized_explanations]") {
    using namespace std::string_view_literals;

    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::Swordfish) == "Swordfish"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::Skyscraper) == "Skyscraper"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::TwoStringKite) == "2-String Kite"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::XYZWing) == "XYZ-Wing"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::UniqueRectangle) == "Unique Rectangle"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::WWing) == "W-Wing"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::SimpleColoring) == "Simple Coloring"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::FinnedXWing) == "Finned X-Wing"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::RemotePairs) == "Remote Pairs"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::BUG) == "BUG"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::Jellyfish) == "Jellyfish"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::FinnedSwordfish) == "Finned Swordfish"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::EmptyRectangle) == "Empty Rectangle"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::WXYZWing) == "WXYZ-Wing"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::FinnedJellyfish) == "Finned Jellyfish"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::XYChain) == "XY-Chain"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::MultiColoring) == "Multi-Coloring"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::ALSxZ) == "ALS-XZ"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::SueDeCoq) == "Sue de Coq"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::ForcingChain) == "Forcing Chain"sv);
    REQUIRE(getLocalizedTechniqueName(SolvingTechnique::NiceLoop) == "Nice Loop"sv);
}
