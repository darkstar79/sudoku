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

#include "../../src/core/technique_descriptions.h"

#include <set>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {}  // namespace

// Derived from the enum's own sentinel — see SolvingTechnique::kLastLogical.
// Backtracking = 255 sits outside the contiguous logical range and is covered separately.
constexpr int kMaxLogicalTechnique = static_cast<int>(SolvingTechnique::kLastLogical);

TEST_CASE("TechniqueDescription - All techniques have descriptions", "[technique_descriptions]") {
    SECTION("Every logical SolvingTechnique has non-empty title") {
        for (int i = 0; i <= kMaxLogicalTechnique; ++i) {
            auto technique = static_cast<SolvingTechnique>(i);
            auto desc = getTechniqueDescription(technique);

            INFO("Technique enum value: " << i << " (" << getTechniqueName(technique) << ")");
            REQUIRE_FALSE(desc.title.empty());
        }
    }

    SECTION("Every logical SolvingTechnique has non-empty what_it_is") {
        for (int i = 0; i <= kMaxLogicalTechnique; ++i) {
            auto technique = static_cast<SolvingTechnique>(i);
            auto desc = getTechniqueDescription(technique);

            INFO("Technique enum value: " << i);
            REQUIRE_FALSE(desc.what_it_is.empty());
        }
    }

    SECTION("Every logical SolvingTechnique has non-empty what_to_look_for") {
        for (int i = 0; i <= kMaxLogicalTechnique; ++i) {
            auto technique = static_cast<SolvingTechnique>(i);
            auto desc = getTechniqueDescription(technique);

            INFO("Technique enum value: " << i);
            REQUIRE_FALSE(desc.what_to_look_for.empty());
        }
    }

    SECTION("Backtracking has a description") {
        auto desc = getTechniqueDescription(SolvingTechnique::Backtracking);

        REQUIRE_FALSE(desc.title.empty());
        REQUIRE_FALSE(desc.what_it_is.empty());
    }
}

TEST_CASE("TechniqueDescription - No duplicate titles", "[technique_descriptions]") {
    SECTION("All logical techniques + Backtracking have unique titles") {
        std::set<std::string> titles;
        for (int i = 0; i <= kMaxLogicalTechnique; ++i) {
            auto desc = getTechniqueDescription(static_cast<SolvingTechnique>(i));
            titles.emplace(desc.title);
        }
        titles.emplace(getTechniqueDescription(SolvingTechnique::Backtracking).title);

        REQUIRE(titles.size() == static_cast<size_t>(kMaxLogicalTechnique + 2));
    }
}

TEST_CASE("TechniqueDescription - Specific descriptions present", "[technique_descriptions]") {
    SECTION("ForcingChain description mentions propagation") {
        auto desc = getTechniqueDescription(SolvingTechnique::ForcingChain);

        REQUIRE_FALSE(desc.title.empty());
        REQUIRE(desc.what_it_is.find("propagate") != std::string_view::npos);
    }

    SECTION("NiceLoop description mentions alternating") {
        auto desc = getTechniqueDescription(SolvingTechnique::NiceLoop);

        REQUIRE_FALSE(desc.title.empty());
        REQUIRE(desc.what_it_is.find("alternating") != std::string_view::npos);
    }

    SECTION("NakedSingle description present") {
        auto desc = getTechniqueDescription(SolvingTechnique::NakedSingle);

        REQUIRE_FALSE(desc.title.empty());
        REQUIRE_FALSE(desc.what_it_is.empty());
    }
}
