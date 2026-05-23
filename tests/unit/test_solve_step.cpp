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

#include "../../src/core/solve_step.h"
#include "../../src/core/solving_technique.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("Elimination - Construction", "[solve_step]") {
    SECTION("Creates elimination with position and value") {
        Elimination elim{Position{.row = 3, .col = 5}, 7};

        REQUIRE(elim.position.row == 3);
        REQUIRE(elim.position.col == 5);
        REQUIRE(elim.value == 7);
    }

    SECTION("Position and value are accessible") {
        Elimination elim{Position{.row = 0, .col = 8}, 9};

        REQUIRE(elim.position.row == 0);
        REQUIRE(elim.position.col == 8);
        REQUIRE(elim.value == 9);
    }
}

TEST_CASE("Elimination - Equality Comparison", "[solve_step]") {
    SECTION("Equal eliminations are equal") {
        Elimination elim1{Position{.row = 2, .col = 4}, 5};
        Elimination elim2{Position{.row = 2, .col = 4}, 5};

        REQUIRE(elim1 == elim2);
    }

    SECTION("Different positions are not equal") {
        Elimination elim1{Position{.row = 2, .col = 4}, 5};
        Elimination elim2{Position{.row = 3, .col = 4}, 5};

        REQUIRE_FALSE(elim1 == elim2);
    }

    SECTION("Different values are not equal") {
        Elimination elim1{Position{.row = 2, .col = 4}, 5};
        Elimination elim2{Position{.row = 2, .col = 4}, 6};

        REQUIRE_FALSE(elim1 == elim2);
    }
}

TEST_CASE("SolveStepType - Enum Values", "[solve_step]") {
    SECTION("Both step types are defined") {
        auto placement = SolveStepType::Placement;
        auto elimination = SolveStepType::Elimination;

        REQUIRE(placement != elimination);
    }
}

TEST_CASE("SolveStep - Placement Step Construction", "[solve_step]") {
    SECTION("Creates placement step with all required fields") {
        SolveStep step{.type = SolveStepType::Placement,
                       .technique = SolvingTechnique::NakedSingle,
                       .position = Position{.row = 3, .col = 5},
                       .value = 7,
                       .eliminations = {},
                       .explanation = "Naked Single at R4C6: only value 7 is possible",
                       .rating = 2.3,
                       .explanation_data = {}};

        REQUIRE(step.type == SolveStepType::Placement);
        REQUIRE(step.technique == SolvingTechnique::NakedSingle);
        REQUIRE(step.position.row == 3);
        REQUIRE(step.position.col == 5);
        REQUIRE(step.value == 7);
        REQUIRE(step.eliminations.empty());
        REQUIRE(step.explanation == "Naked Single at R4C6: only value 7 is possible");
        REQUIRE(step.rating == 2.3);
    }

    SECTION("Placement step points match technique points") {
        SolveStep step{.type = SolveStepType::Placement,
                       .technique = SolvingTechnique::HiddenSingle,
                       .position = Position{.row = 0, .col = 0},
                       .value = 1,
                       .eliminations = {},
                       .explanation = "Test explanation",
                       .rating = getTechniqueRating(SolvingTechnique::HiddenSingle),
                       .explanation_data = {}};

        REQUIRE(step.rating == 1.5);
        REQUIRE(step.rating == getTechniqueRating(step.technique));
    }
}

TEST_CASE("SolveStep - Elimination Step Construction", "[solve_step]") {
    SECTION("Creates elimination step with multiple eliminations") {
        std::vector<Elimination> eliminations = {
            {Position{.row = 3, .col = 2}, 4}, {Position{.row = 3, .col = 7}, 4}, {Position{.row = 3, .col = 8}, 7}};

        SolveStep step{.type = SolveStepType::Elimination,
                       .technique = SolvingTechnique::NakedPair,
                       .position = Position{.row = 0, .col = 0},  // Not used for eliminations
                       .value = 0,                                // Not used for eliminations
                       .eliminations = eliminations,
                       .explanation = "Naked Pair [4, 7] at R4C3, R4C8 eliminates candidates",
                       .rating = 3.0,
                       .explanation_data = {}};

        REQUIRE(step.type == SolveStepType::Elimination);
        REQUIRE(step.technique == SolvingTechnique::NakedPair);
        REQUIRE(step.eliminations.size() == 3);
        REQUIRE(step.eliminations[0].position.row == 3);
        REQUIRE(step.eliminations[0].value == 4);
        REQUIRE(step.explanation.find("Naked Pair") != std::string::npos);
        REQUIRE(step.rating == 3.0);
    }

    SECTION("Elimination step can have empty eliminations list") {
        // Edge case: technique found but no candidates to eliminate
        SolveStep step{.type = SolveStepType::Elimination,
                       .technique = SolvingTechnique::HiddenPair,
                       .position = Position{.row = 0, .col = 0},
                       .value = 0,
                       .eliminations = {},
                       .explanation = "Hidden Pair found but no eliminations needed",
                       .rating = 3.4,
                       .explanation_data = {}};

        REQUIRE(step.type == SolveStepType::Elimination);
        REQUIRE(step.eliminations.empty());
    }
}

TEST_CASE("SolveStep - Equality Comparison", "[solve_step]") {
    SECTION("Equal placement steps are equal") {
        SolveStep step1{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        SolveStep step2{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        REQUIRE(step1 == step2);
    }

    SECTION("Different types are not equal") {
        SolveStep step1{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        SolveStep step2{.type = SolveStepType::Elimination,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        REQUIRE_FALSE(step1 == step2);
    }

    SECTION("Different techniques are not equal") {
        SolveStep step1{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        SolveStep step2{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::HiddenSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 15,
                        .explanation_data = {}};

        REQUIRE_FALSE(step1 == step2);
    }

    SECTION("Different positions are not equal") {
        SolveStep step1{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        SolveStep step2{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 4, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        REQUIRE_FALSE(step1 == step2);
    }

    SECTION("Different values are not equal") {
        SolveStep step1{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 7,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        SolveStep step2{.type = SolveStepType::Placement,
                        .technique = SolvingTechnique::NakedSingle,
                        .position = Position{.row = 3, .col = 5},
                        .value = 8,
                        .eliminations = {},
                        .explanation = "Test",
                        .rating = 2.3,
                        .explanation_data = {}};

        REQUIRE_FALSE(step1 == step2);
    }

    SECTION("Different eliminations are not equal") {
        SolveStep step1{.type = SolveStepType::Elimination,
                        .technique = SolvingTechnique::NakedPair,
                        .position = Position{.row = 0, .col = 0},
                        .value = 0,
                        .eliminations = {{Position{.row = 1, .col = 2}, 3}},
                        .explanation = "Test",
                        .rating = 3.0,
                        .explanation_data = {}};

        SolveStep step2{.type = SolveStepType::Elimination,
                        .technique = SolvingTechnique::NakedPair,
                        .position = Position{.row = 0, .col = 0},
                        .value = 0,
                        .eliminations = {{Position{.row = 1, .col = 3}, 3}},
                        .explanation = "Test",
                        .rating = 3.0,
                        .explanation_data = {}};

        REQUIRE_FALSE(step1 == step2);
    }
}

TEST_CASE("SolveStep - Explanation Content", "[solve_step]") {
    SECTION("Explanation describes technique") {
        SolveStep step{.type = SolveStepType::Placement,
                       .technique = SolvingTechnique::NakedSingle,
                       .position = Position{.row = 3, .col = 5},
                       .value = 7,
                       .eliminations = {},
                       .explanation = "Naked Single at R4C6: only value 7 is possible",
                       .rating = 2.3,
                       .explanation_data = {}};

        REQUIRE_FALSE(step.explanation.empty());
        REQUIRE(step.explanation.find("Naked Single") != std::string::npos);
    }

    SECTION("Explanation can contain position information") {
        SolveStep step{.type = SolveStepType::Placement,
                       .technique = SolvingTechnique::HiddenSingle,
                       .position = Position{.row = 0, .col = 8},
                       .value = 9,
                       .eliminations = {},
                       .explanation = "Hidden Single at R1C9: value 9 can only appear here in Row 1",
                       .rating = 15,
                       .explanation_data = {}};

        REQUIRE(step.explanation.find("R1C9") != std::string::npos);
    }
}

TEST_CASE("SolveStep - Points Consistency", "[solve_step]") {
    SECTION("Points should match technique points for logical techniques") {
        auto techniques = {SolvingTechnique::NakedSingle, SolvingTechnique::HiddenSingle,
                           SolvingTechnique::NakedPair,   SolvingTechnique::NakedTriple,
                           SolvingTechnique::HiddenPair,  SolvingTechnique::HiddenTriple};

        for (auto technique : techniques) {
            SolveStep step{.type = SolveStepType::Placement,
                           .technique = technique,
                           .position = Position{.row = 0, .col = 0},
                           .value = 1,
                           .eliminations = {},
                           .explanation = "Test",
                           .rating = getTechniqueRating(technique),
                           .explanation_data = {}};

            REQUIRE(step.rating == getTechniqueRating(technique));
            REQUIRE(step.rating > 0);
        }
    }

    SECTION("Backtracking step has zero points") {
        SolveStep step{.type = SolveStepType::Placement,
                       .technique = SolvingTechnique::Backtracking,
                       .position = Position{.row = 0, .col = 0},
                       .value = 1,
                       .eliminations = {},
                       .explanation = "Backtracking guess",
                       .rating = getTechniqueRating(SolvingTechnique::Backtracking),
                       .explanation_data = {}};

        REQUIRE(step.rating == 12.0);
    }
}
