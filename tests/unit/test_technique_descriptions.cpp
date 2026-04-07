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

#include "../../src/core/technique_descriptions.h"
#include "../helpers/mock_localization_manager.h"

#include <set>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

namespace {
MockLocalizationManager mock_loc;
}  // namespace

TEST_CASE("TechniqueDescription - All techniques have descriptions", "[technique_descriptions]") {
    SECTION("Every SolvingTechnique (0-32) has non-empty title") {
        for (int i = 0; i <= 32; ++i) {
            auto technique = static_cast<SolvingTechnique>(i);
            auto desc = getTechniqueDescription(mock_loc, technique);

            INFO("Technique enum value: " << i << " (" << getTechniqueName(technique) << ")");
            REQUIRE_FALSE(desc.title.empty());
        }
    }

    SECTION("Every SolvingTechnique (0-32) has non-empty what_it_is") {
        for (int i = 0; i <= 32; ++i) {
            auto technique = static_cast<SolvingTechnique>(i);
            auto desc = getTechniqueDescription(mock_loc, technique);

            INFO("Technique enum value: " << i);
            REQUIRE_FALSE(desc.what_it_is.empty());
        }
    }

    SECTION("Every SolvingTechnique (0-32) has non-empty what_to_look_for") {
        for (int i = 0; i <= 32; ++i) {
            auto technique = static_cast<SolvingTechnique>(i);
            auto desc = getTechniqueDescription(mock_loc, technique);

            INFO("Technique enum value: " << i);
            REQUIRE_FALSE(desc.what_to_look_for.empty());
        }
    }

    SECTION("Backtracking has a description") {
        auto desc = getTechniqueDescription(mock_loc, SolvingTechnique::Backtracking);

        REQUIRE_FALSE(desc.title.empty());
        REQUIRE_FALSE(desc.what_it_is.empty());
    }
}

TEST_CASE("TechniqueDescription - No duplicate titles", "[technique_descriptions]") {
    SECTION("All 34 techniques have unique titles") {
        std::set<std::string> titles;
        // 0-32 + 255 (Backtracking)
        for (int i = 0; i <= 32; ++i) {
            auto desc = getTechniqueDescription(mock_loc, static_cast<SolvingTechnique>(i));
            titles.emplace(desc.title);
        }
        titles.emplace(getTechniqueDescription(mock_loc, SolvingTechnique::Backtracking).title);

        REQUIRE(titles.size() == 34);
    }
}

TEST_CASE("TechniqueDescription - Specific descriptions present", "[technique_descriptions]") {
    SECTION("ForcingChain description mentions propagation") {
        auto desc = getTechniqueDescription(mock_loc, SolvingTechnique::ForcingChain);

        REQUIRE_FALSE(desc.title.empty());
        REQUIRE(desc.what_it_is.find("propagate") != std::string_view::npos);
    }

    SECTION("NiceLoop description mentions alternating") {
        auto desc = getTechniqueDescription(mock_loc, SolvingTechnique::NiceLoop);

        REQUIRE_FALSE(desc.title.empty());
        REQUIRE(desc.what_it_is.find("alternating") != std::string_view::npos);
    }

    SECTION("NakedSingle description present") {
        auto desc = getTechniqueDescription(mock_loc, SolvingTechnique::NakedSingle);

        REQUIRE_FALSE(desc.title.empty());
        REQUIRE_FALSE(desc.what_it_is.empty());
    }
}
