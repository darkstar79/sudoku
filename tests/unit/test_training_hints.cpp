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

#include "../../src/core/training_hints.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

// --- getTechniqueCategory ---

TEST_CASE("getTechniqueCategory — maps all techniques", "[training_hints]") {
    CHECK(getTechniqueCategory(SolvingTechnique::NakedSingle) == TechniqueCategory::Singles);
    CHECK(getTechniqueCategory(SolvingTechnique::HiddenSingle) == TechniqueCategory::Singles);
    CHECK(getTechniqueCategory(SolvingTechnique::NakedPair) == TechniqueCategory::Subsets);
    CHECK(getTechniqueCategory(SolvingTechnique::HiddenQuad) == TechniqueCategory::Subsets);
    CHECK(getTechniqueCategory(SolvingTechnique::PointingPair) == TechniqueCategory::Intersections);
    CHECK(getTechniqueCategory(SolvingTechnique::BoxLineReduction) == TechniqueCategory::Intersections);
    CHECK(getTechniqueCategory(SolvingTechnique::XWing) == TechniqueCategory::Fish);
    CHECK(getTechniqueCategory(SolvingTechnique::FrankenFish) == TechniqueCategory::Fish);
    CHECK(getTechniqueCategory(SolvingTechnique::XYWing) == TechniqueCategory::Wings);
    CHECK(getTechniqueCategory(SolvingTechnique::WWing) == TechniqueCategory::Wings);
    CHECK(getTechniqueCategory(SolvingTechnique::Skyscraper) == TechniqueCategory::SingleDigit);
    CHECK(getTechniqueCategory(SolvingTechnique::SimpleColoring) == TechniqueCategory::Coloring);
    CHECK(getTechniqueCategory(SolvingTechnique::UniqueRectangle) == TechniqueCategory::UniqueRect);
    CHECK(getTechniqueCategory(SolvingTechnique::XYChain) == TechniqueCategory::Chains);
    CHECK(getTechniqueCategory(SolvingTechnique::ForcingChain) == TechniqueCategory::Chains);
    CHECK(getTechniqueCategory(SolvingTechnique::ALSxZ) == TechniqueCategory::SetLogic);
    CHECK(getTechniqueCategory(SolvingTechnique::BUG) == TechniqueCategory::Special);
    CHECK(getTechniqueCategory(SolvingTechnique::Backtracking) == TechniqueCategory::Special);
}

// --- Helper to build a SolveStep for hint tests ---

namespace {

SolveStep makePlacementStep(SolvingTechnique tech, Position pos, int value) {
    return SolveStep{
        .type = SolveStepType::Placement,
        .technique = tech,
        .position = pos,
        .value = value,
        .eliminations = {},
        .explanation = "Test explanation",
        .rating = 10,
        .explanation_data = {.positions = {pos}, .values = {value}, .region_type = RegionType::Row, .region_index = 0},
    };
}

SolveStep makeEliminationStep(SolvingTechnique tech, std::vector<Elimination> elims) {
    ExplanationData data;
    data.values = std::vector<int>{1};
    data.region_type = RegionType::Box;
    data.region_index = 2;
    for (const auto& e : elims) {
        data.positions.push_back(e.position);
        data.position_roles.push_back(CellRole::Pattern);
    }
    return SolveStep{
        .type = SolveStepType::Elimination,
        .technique = tech,
        .position = elims.empty() ? Position{} : elims[0].position,
        .value = 0,
        .eliminations = elims,
        .explanation = "Test elimination",
        .rating = 50,
        .explanation_data = data,
    };
}

}  // namespace

// --- getTrainingHint per-category tests ---

TEST_CASE("getTrainingHint — Singles category", "[training_hints]") {
    auto step = makePlacementStep(SolvingTechnique::NakedSingle, Position{.row = 1, .col = 0}, 9);

    SECTION("Level 1: points to cell") {
        auto hint = getTrainingHint(SolvingTechnique::NakedSingle, 1, step);
        CHECK(hint.text.find("R2C1") != std::string::npos);
        CHECK(hint.highlights.size() == 1);
        CHECK(hint.highlights.size() == 1);
        CHECK(hint.highlights[0].role == CellRole::Pattern);
    }

    SECTION("Level 2: mentions region") {
        auto hint = getTrainingHint(SolvingTechnique::NakedSingle, 2, step);
        CHECK(hint.text.find("Row 1") != std::string::npos);
    }

    SECTION("Level 2: no region fallback") {
        step.explanation_data.region_type = RegionType::None;
        auto hint = getTrainingHint(SolvingTechnique::NakedSingle, 2, step);
        CHECK(hint.text.find("candidates") != std::string::npos);
    }

    SECTION("Level 3: reveals value") {
        auto hint = getTrainingHint(SolvingTechnique::NakedSingle, 3, step);
        CHECK(hint.text.find("9") != std::string::npos);
    }
}

TEST_CASE("getTrainingHint — Subsets category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 0, .col = 5}, 3}};
    auto step = makeEliminationStep(SolvingTechnique::NakedPair, elims);

    SECTION("Level 1: mentions region") {
        auto hint = getTrainingHint(SolvingTechnique::NakedPair, 1, step);
        CHECK(hint.text.find("Box 3") != std::string::npos);
    }

    SECTION("Level 1: no region fallback") {
        step.explanation_data.region_type = RegionType::None;
        auto hint = getTrainingHint(SolvingTechnique::NakedPair, 1, step);
        CHECK(hint.text.find("same candidates") != std::string::npos);
    }

    SECTION("Level 2: highlights subset cells and shows values") {
        // The step's explanation_data.values should appear in level 2 text
        step.explanation_data.values = {3, 7};
        auto hint = getTrainingHint(SolvingTechnique::NakedPair, 2, step);
        CHECK(hint.text.find("subset") != std::string::npos);
        CHECK(hint.text.find("3") != std::string::npos);
        CHECK(hint.text.find("7") != std::string::npos);
        CHECK_FALSE(hint.highlights.empty());
    }

    SECTION("Level 2: works without values") {
        step.explanation_data.values.clear();
        auto hint = getTrainingHint(SolvingTechnique::NakedPair, 2, step);
        CHECK(hint.text.find("subset") != std::string::npos);
        CHECK_FALSE(hint.highlights.empty());
    }

    SECTION("Level 3: highlights elimination targets") {
        auto hint = getTrainingHint(SolvingTechnique::NakedPair, 3, step);
        CHECK(hint.text.find("Eliminate") != std::string::npos);
        CHECK(hint.highlights.size() == 1);
        CHECK(hint.highlights[0].role == CellRole::Fin);
    }
}

TEST_CASE("getTrainingHint — Intersections category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 2, .col = 3}, 5}};
    auto step = makeEliminationStep(SolvingTechnique::PointingPair, elims);

    SECTION("Level 1 with value") {
        auto hint = getTrainingHint(SolvingTechnique::PointingPair, 1, step);
        CHECK(hint.text.find("1") != std::string::npos);  // data.values[0] = 1
    }

    SECTION("Level 1 without value") {
        step.explanation_data.values.clear();
        auto hint = getTrainingHint(SolvingTechnique::PointingPair, 1, step);
        CHECK(hint.text.find("intersection") != std::string::npos);
    }

    SECTION("Level 3: elimination targets") {
        auto hint = getTrainingHint(SolvingTechnique::PointingPair, 3, step);
        CHECK(hint.highlights[0].role == CellRole::Fin);
    }
}

TEST_CASE("getTrainingHint — Fish category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 4, .col = 7}, 2}};
    auto step = makeEliminationStep(SolvingTechnique::XWing, elims);

    SECTION("Level 1 with value") {
        auto hint = getTrainingHint(SolvingTechnique::XWing, 1, step);
        CHECK(hint.text.find("fish") != std::string::npos);
        CHECK(hint.text.find("1") != std::string::npos);
    }

    SECTION("Level 1 without value") {
        step.explanation_data.values.clear();
        auto hint = getTrainingHint(SolvingTechnique::XWing, 1, step);
        CHECK(hint.text.find("fish") != std::string::npos);
    }

    SECTION("Level 2: base/cover sets") {
        auto hint = getTrainingHint(SolvingTechnique::XWing, 2, step);
        CHECK(hint.text.find("Base") != std::string::npos);
    }

    SECTION("Level 3: elimination") {
        auto hint = getTrainingHint(SolvingTechnique::XWing, 3, step);
        CHECK(hint.highlights[0].role == CellRole::Fin);
    }
}

TEST_CASE("getTrainingHint — Wings category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 3, .col = 3}, 4}};
    auto step = makeEliminationStep(SolvingTechnique::XYWing, elims);
    // Add a Pivot role
    step.explanation_data.position_roles = {CellRole::Pivot, CellRole::Wing};

    SECTION("Level 1: finds pivot") {
        auto hint = getTrainingHint(SolvingTechnique::XYWing, 1, step);
        CHECK(hint.text.find("pivot") != std::string::npos);
        CHECK(hint.highlights[0].role == CellRole::Pivot);
    }

    SECTION("Level 2: pivot and wing cells") {
        auto hint = getTrainingHint(SolvingTechnique::XYWing, 2, step);
        CHECK(hint.text.find("Pivot") != std::string::npos);
    }

    SECTION("Level 3: elimination") {
        auto hint = getTrainingHint(SolvingTechnique::XYWing, 3, step);
        CHECK(hint.highlights[0].role == CellRole::Fin);
    }
}

TEST_CASE("getTrainingHint — SingleDigit category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 5, .col = 2}, 7}};
    auto step = makeEliminationStep(SolvingTechnique::Skyscraper, elims);

    SECTION("Level 1 with value") {
        auto hint = getTrainingHint(SolvingTechnique::Skyscraper, 1, step);
        CHECK(hint.text.find("conjugate") != std::string::npos);
    }

    SECTION("Level 1 without value") {
        step.explanation_data.values.clear();
        auto hint = getTrainingHint(SolvingTechnique::Skyscraper, 1, step);
        CHECK(hint.text.find("conjugate") != std::string::npos);
    }

    SECTION("Level 3: elimination") {
        auto hint = getTrainingHint(SolvingTechnique::Skyscraper, 3, step);
        CHECK(hint.text.find("endpoints") != std::string::npos);
    }
}

TEST_CASE("getTrainingHint — Coloring category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 6, .col = 1}, 3}};
    auto step = makeEliminationStep(SolvingTechnique::SimpleColoring, elims);

    SECTION("Level 1 with value") {
        auto hint = getTrainingHint(SolvingTechnique::SimpleColoring, 1, step);
        CHECK(hint.text.find("coloring") != std::string::npos);
    }

    SECTION("Level 1 without value") {
        step.explanation_data.values.clear();
        auto hint = getTrainingHint(SolvingTechnique::SimpleColoring, 1, step);
        CHECK(hint.text.find("coloring") != std::string::npos);
    }

    SECTION("Level 2: chain cells") {
        auto hint = getTrainingHint(SolvingTechnique::SimpleColoring, 2, step);
        CHECK(hint.text.find("chain") != std::string::npos);
    }

    SECTION("Level 3: elimination") {
        auto hint = getTrainingHint(SolvingTechnique::SimpleColoring, 3, step);
        CHECK(hint.text.find("color") != std::string::npos);
    }
}

TEST_CASE("getTrainingHint — UniqueRect category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 0, .col = 3}, 6}};
    auto step = makeEliminationStep(SolvingTechnique::UniqueRectangle, elims);

    SECTION("Level 1: deadly pattern") {
        auto hint = getTrainingHint(SolvingTechnique::UniqueRectangle, 1, step);
        CHECK(hint.text.find("deadly") != std::string::npos);
    }

    SECTION("Level 2: rectangle corners") {
        auto hint = getTrainingHint(SolvingTechnique::UniqueRectangle, 2, step);
        CHECK(hint.text.find("corners") != std::string::npos);
    }

    SECTION("Level 3: elimination") {
        auto hint = getTrainingHint(SolvingTechnique::UniqueRectangle, 3, step);
        CHECK(hint.text.find("deadly") != std::string::npos);
    }
}

TEST_CASE("getTrainingHint — Chains category", "[training_hints]") {
    SECTION("Elimination chain") {
        auto elims = std::vector<Elimination>{{Position{.row = 7, .col = 0}, 8}};
        auto step = makeEliminationStep(SolvingTechnique::XYChain, elims);

        auto hint1 = getTrainingHint(SolvingTechnique::XYChain, 1, step);
        CHECK(hint1.text.find("chain") != std::string::npos);
        CHECK(hint1.highlights[0].role == CellRole::ChainA);

        auto hint2 = getTrainingHint(SolvingTechnique::XYChain, 2, step);
        CHECK(hint2.text.find("path") != std::string::npos);

        auto hint3 = getTrainingHint(SolvingTechnique::XYChain, 3, step);
        CHECK(hint3.text.find("Eliminate") != std::string::npos);
    }

    SECTION("Placement chain") {
        auto step = makePlacementStep(SolvingTechnique::ForcingChain, Position{.row = 2, .col = 5}, 4);
        auto hint3 = getTrainingHint(SolvingTechnique::ForcingChain, 3, step);
        CHECK(hint3.text.find("4") != std::string::npos);
        CHECK(hint3.text.find("R3C6") != std::string::npos);
    }

    SECTION("Level 1 without positions") {
        auto step = makeEliminationStep(SolvingTechnique::XYChain, {});
        step.explanation_data.positions.clear();
        auto hint = getTrainingHint(SolvingTechnique::XYChain, 1, step);
        CHECK(hint.text.find("chain") != std::string::npos);
    }
}

TEST_CASE("getTrainingHint — SetLogic category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 1, .col = 4}, 2}};
    auto step = makeEliminationStep(SolvingTechnique::ALSxZ, elims);

    SECTION("Level 1") {
        auto hint = getTrainingHint(SolvingTechnique::ALSxZ, 1, step);
        CHECK(hint.text.find("Almost Locked Set") != std::string::npos);
    }

    SECTION("Level 2") {
        auto hint = getTrainingHint(SolvingTechnique::ALSxZ, 2, step);
        CHECK(hint.text.find("ALS") != std::string::npos);
    }

    SECTION("Level 3") {
        auto hint = getTrainingHint(SolvingTechnique::ALSxZ, 3, step);
        CHECK(hint.highlights[0].role == CellRole::Fin);
    }
}

TEST_CASE("getTrainingHint — Special (BUG) category", "[training_hints]") {
    auto elims = std::vector<Elimination>{{Position{.row = 4, .col = 4}, 5}};
    auto step = makeEliminationStep(SolvingTechnique::BUG, elims);

    SECTION("Level 1") {
        auto hint = getTrainingHint(SolvingTechnique::BUG, 1, step);
        CHECK(hint.text.find("three candidates") != std::string::npos);
    }

    SECTION("Level 2 with positions") {
        auto hint = getTrainingHint(SolvingTechnique::BUG, 2, step);
        CHECK(hint.text.find("key cell") != std::string::npos);
    }

    SECTION("Level 2 without positions") {
        step.explanation_data.positions.clear();
        auto hint = getTrainingHint(SolvingTechnique::BUG, 2, step);
        CHECK(hint.text == step.explanation);
    }

    SECTION("Level 3") {
        auto hint = getTrainingHint(SolvingTechnique::BUG, 3, step);
        CHECK(hint.highlights[0].role == CellRole::Fin);
    }
}
