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

#include "../../src/core/solving_technique.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("SolvingTechnique - Enum Values", "[solving_technique]") {
    SECTION("Underlying type is uint8_t") {
        // Verify enum uses correct underlying type for memory efficiency
        REQUIRE(sizeof(SolvingTechnique) == sizeof(uint8_t));
    }
}

TEST_CASE("SolvingTechnique - getTechniqueName()", "[solving_technique]") {
    SECTION("Returns correct names for all techniques") {
        REQUIRE(getTechniqueName(SolvingTechnique::NakedSingle) == "Naked Single");
        REQUIRE(getTechniqueName(SolvingTechnique::HiddenSingle) == "Hidden Single");
        REQUIRE(getTechniqueName(SolvingTechnique::NakedPair) == "Naked Pair");
        REQUIRE(getTechniqueName(SolvingTechnique::NakedTriple) == "Naked Triple");
        REQUIRE(getTechniqueName(SolvingTechnique::HiddenPair) == "Hidden Pair");
        REQUIRE(getTechniqueName(SolvingTechnique::HiddenTriple) == "Hidden Triple");
        REQUIRE(getTechniqueName(SolvingTechnique::PointingPair) == "Pointing Pair");
        REQUIRE(getTechniqueName(SolvingTechnique::BoxLineReduction) == "Box/Line Reduction");
        REQUIRE(getTechniqueName(SolvingTechnique::NakedQuad) == "Naked Quad");
        REQUIRE(getTechniqueName(SolvingTechnique::HiddenQuad) == "Hidden Quad");
        REQUIRE(getTechniqueName(SolvingTechnique::XWing) == "X-Wing");
        REQUIRE(getTechniqueName(SolvingTechnique::XYWing) == "XY-Wing");
        REQUIRE(getTechniqueName(SolvingTechnique::Backtracking) == "Backtracking");
    }

    SECTION("Names are non-empty") {
        // Test all 13 techniques consistently
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::NakedSingle).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::HiddenSingle).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::NakedPair).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::NakedTriple).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::HiddenPair).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::HiddenTriple).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::PointingPair).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::BoxLineReduction).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::NakedQuad).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::HiddenQuad).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::XWing).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::XYWing).empty());
        REQUIRE_FALSE(getTechniqueName(SolvingTechnique::Backtracking).empty());
    }
}

TEST_CASE("SolvingTechnique - getTechniqueRating()", "[solving_technique]") {
    SECTION("Returns correct Sudoku Explainer ratings") {
        // SE ratings (per Sudoku Explainer v1.2.1 scale)
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenSingle) == 1.5);
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedSingle) == 2.3);
        REQUIRE(getTechniqueRating(SolvingTechnique::PointingPair) == 2.6);
        REQUIRE(getTechniqueRating(SolvingTechnique::BoxLineReduction) == 2.8);
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedPair) == 3.0);
        REQUIRE(getTechniqueRating(SolvingTechnique::XWing) == 3.2);
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedTriple) == 3.6);
        REQUIRE(getTechniqueRating(SolvingTechnique::Swordfish) == 3.8);
        REQUIRE(getTechniqueRating(SolvingTechnique::XYWing) == 4.2);
        REQUIRE(getTechniqueRating(SolvingTechnique::XYZWing) == 4.4);
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedQuad) == 5.0);
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenQuad) == 5.4);
        REQUIRE(getTechniqueRating(SolvingTechnique::BUG) == 5.6);
        REQUIRE(getTechniqueRating(SolvingTechnique::ForcingChain) == 7.5);
    }

    SECTION("Backtracking is rated as harder than logical strategies") {
        REQUIRE(getTechniqueRating(SolvingTechnique::Backtracking) == 12.0);
    }

    SECTION("SE ratings follow expected ordering for basic techniques") {
        // SE ordering differs from the old points — HiddenSingle is easier than NakedSingle
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenSingle) < getTechniqueRating(SolvingTechnique::NakedSingle));
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedSingle) < getTechniqueRating(SolvingTechnique::PointingPair));
        REQUIRE(getTechniqueRating(SolvingTechnique::PointingPair) <
                getTechniqueRating(SolvingTechnique::BoxLineReduction));
        REQUIRE(getTechniqueRating(SolvingTechnique::BoxLineReduction) <
                getTechniqueRating(SolvingTechnique::NakedPair));
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedPair) < getTechniqueRating(SolvingTechnique::XWing));
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedQuad) < getTechniqueRating(SolvingTechnique::HiddenQuad));
        REQUIRE(getTechniqueRating(SolvingTechnique::XWing) < getTechniqueRating(SolvingTechnique::XYWing));
    }

    SECTION("All point values are non-negative") {
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedSingle) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenSingle) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedPair) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedTriple) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenPair) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenTriple) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::PointingPair) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::BoxLineReduction) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::NakedQuad) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::HiddenQuad) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::XWing) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::XYWing) >= 0);
        REQUIRE(getTechniqueRating(SolvingTechnique::Backtracking) >= 0);
    }
}

TEST_CASE("SolvingTechnique - Helper Functions are Constexpr", "[solving_technique]") {
    SECTION("getTechniqueName() is constexpr") {
        // Verify function can be evaluated at compile time
        constexpr auto name = getTechniqueName(SolvingTechnique::NakedSingle);
        REQUIRE(name == "Naked Single");
    }

    SECTION("getTechniqueRating() is constexpr") {
        // Verify function can be evaluated at compile time
        constexpr double rating = getTechniqueRating(SolvingTechnique::NakedSingle);
        REQUIRE(rating == 2.3);
    }
}
