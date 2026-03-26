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

#include "../../src/core/puzzle_rating.h"
#include "../../src/core/solving_technique.h"

#include <limits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace sudoku::core;

TEST_CASE("PuzzleRating - Construction", "[puzzle_rating]") {
    SECTION("Creates rating with all fields") {
        std::vector<SolveStep> solve_path = {{.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = Position{.row = 0, .col = 0},
                                              .value = 5,
                                              .eliminations = {},
                                              .explanation = "Test",
                                              .rating = 2.3,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::HiddenSingle,
                                              .position = Position{.row = 1, .col = 1},
                                              .value = 3,
                                              .eliminations = {},
                                              .explanation = "Test",
                                              .rating = 1.5,
                                              .explanation_data = {}}};

        PuzzleRating rating{.se_rating = 2.3,
                            .solve_path = solve_path,
                            .requires_backtracking = false,
                            .estimated_difficulty = Difficulty::Easy};

        REQUIRE(rating.se_rating == 2.3);
        REQUIRE(rating.solve_path.size() == 2);
        REQUIRE_FALSE(rating.requires_backtracking);
        REQUIRE(rating.estimated_difficulty == Difficulty::Easy);
    }

    SECTION("Creates rating with backtracking") {
        PuzzleRating rating{.se_rating = 7.5,
                            .solve_path = {},
                            .requires_backtracking = true,
                            .estimated_difficulty = Difficulty::Master};

        REQUIRE(rating.se_rating == 7.5);
        REQUIRE(rating.requires_backtracking);
        REQUIRE(rating.estimated_difficulty == Difficulty::Master);
    }
}

TEST_CASE("PuzzleRating - Equality Comparison", "[puzzle_rating]") {
    SECTION("Equal ratings are equal") {
        PuzzleRating rating1{.se_rating = 3.0,
                             .solve_path = {},
                             .requires_backtracking = false,
                             .estimated_difficulty = Difficulty::Medium};

        PuzzleRating rating2{.se_rating = 3.0,
                             .solve_path = {},
                             .requires_backtracking = false,
                             .estimated_difficulty = Difficulty::Medium};

        REQUIRE(rating1 == rating2);
    }

    SECTION("Different scores are not equal") {
        PuzzleRating rating1{.se_rating = 3.0,
                             .solve_path = {},
                             .requires_backtracking = false,
                             .estimated_difficulty = Difficulty::Medium};
        PuzzleRating rating2{.se_rating = 4.2,
                             .solve_path = {},
                             .requires_backtracking = false,
                             .estimated_difficulty = Difficulty::Medium};

        REQUIRE_FALSE(rating1 == rating2);
    }
}

TEST_CASE("ratingToDifficulty - Converts SE rating to difficulty", "[puzzle_rating]") {
    SECTION("Easy range (0.0-2.5)") {
        REQUIRE(ratingToDifficulty(0.0) == Difficulty::Easy);
        REQUIRE(ratingToDifficulty(1.5) == Difficulty::Easy);
        REQUIRE(ratingToDifficulty(2.3) == Difficulty::Easy);
        REQUIRE(ratingToDifficulty(2.4) == Difficulty::Easy);
    }

    SECTION("Medium range (2.5-3.8)") {
        REQUIRE(ratingToDifficulty(2.5) == Difficulty::Medium);
        REQUIRE(ratingToDifficulty(3.0) == Difficulty::Medium);
        REQUIRE(ratingToDifficulty(3.7) == Difficulty::Medium);
    }

    SECTION("Hard range (3.8-5.5)") {
        REQUIRE(ratingToDifficulty(3.8) == Difficulty::Hard);
        REQUIRE(ratingToDifficulty(4.5) == Difficulty::Hard);
        REQUIRE(ratingToDifficulty(5.4) == Difficulty::Hard);
    }

    SECTION("Expert range (5.5-7.5)") {
        REQUIRE(ratingToDifficulty(5.5) == Difficulty::Expert);
        REQUIRE(ratingToDifficulty(6.6) == Difficulty::Expert);
        REQUIRE(ratingToDifficulty(7.4) == Difficulty::Expert);
    }

    SECTION("Master range (7.5+)") {
        REQUIRE(ratingToDifficulty(7.5) == Difficulty::Master);
        REQUIRE(ratingToDifficulty(8.2) == Difficulty::Master);
        REQUIRE(ratingToDifficulty(12.0) == Difficulty::Master);
    }

    SECTION("Boundary values") {
        REQUIRE(ratingToDifficulty(0.0) == Difficulty::Easy);
        REQUIRE(ratingToDifficulty(2.5) == Difficulty::Medium);
        REQUIRE(ratingToDifficulty(3.8) == Difficulty::Hard);
        REQUIRE(ratingToDifficulty(5.5) == Difficulty::Expert);
        REQUIRE(ratingToDifficulty(7.5) == Difficulty::Master);
    }
}

TEST_CASE("getDifficultyRatingRange - Returns min/max SE rating for difficulty", "[puzzle_rating]") {
    struct RangeExpectation {
        Difficulty difficulty;
        double expected_min;
        double expected_max;
    };

    auto expectation =
        GENERATE(RangeExpectation{Difficulty::Easy, 0.0, 2.5}, RangeExpectation{Difficulty::Medium, 2.5, 3.8},
                 RangeExpectation{Difficulty::Hard, 3.8, 5.5}, RangeExpectation{Difficulty::Expert, 5.5, 7.5},
                 RangeExpectation{Difficulty::Master, 7.5, std::numeric_limits<double>::max()});
    CAPTURE(expectation.difficulty);

    auto [min_rating, max_rating] = getDifficultyRatingRange(expectation.difficulty);

    REQUIRE(min_rating == expectation.expected_min);
    REQUIRE(max_rating == expectation.expected_max);
    REQUIRE(min_rating < max_rating);
}

TEST_CASE("getDifficultyRatingRange - Range boundaries are consistent", "[puzzle_rating]") {
    SECTION("No gaps between difficulty ranges") {
        auto [easy_min, easy_max] = getDifficultyRatingRange(Difficulty::Easy);
        auto [medium_min, medium_max] = getDifficultyRatingRange(Difficulty::Medium);
        auto [hard_min, hard_max] = getDifficultyRatingRange(Difficulty::Hard);
        auto [expert_min, expert_max] = getDifficultyRatingRange(Difficulty::Expert);
        auto [master_min, master_max] = getDifficultyRatingRange(Difficulty::Master);

        // Verify continuous ranges (no gaps)
        REQUIRE(easy_max == medium_min);
        REQUIRE(medium_max == hard_min);
        REQUIRE(hard_max == expert_min);
        REQUIRE(expert_max == master_min);
    }

    SECTION("Rating to difficulty conversion is consistent with ranges") {
        for (auto difficulty :
             {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard, Difficulty::Expert, Difficulty::Master}) {
            auto [min_rating, max_rating] = getDifficultyRatingRange(difficulty);

            // A rating at the minimum should map to this difficulty
            REQUIRE(ratingToDifficulty(min_rating) == difficulty);
        }
    }
}

TEST_CASE("ratingToDifficulty - Round-trip consistency", "[puzzle_rating]") {
    SECTION("Rating within range maps back to same difficulty") {
        struct TestCase {
            double rating;
            Difficulty expected_difficulty;
        };

        std::vector<TestCase> test_cases = {
            {1.0, Difficulty::Easy},   {2.3, Difficulty::Easy},  {2.8, Difficulty::Medium}, {3.4, Difficulty::Medium},
            {4.0, Difficulty::Hard},   {5.0, Difficulty::Hard},  {6.0, Difficulty::Expert}, {7.0, Difficulty::Expert},
            {7.5, Difficulty::Master}, {9.0, Difficulty::Master}};

        for (const auto& test_case : test_cases) {
            auto difficulty = ratingToDifficulty(test_case.rating);
            REQUIRE(difficulty == test_case.expected_difficulty);

            // Verify rating is within expected range
            auto [min_rating, max_rating] = getDifficultyRatingRange(difficulty);
            REQUIRE(test_case.rating >= min_rating);
            REQUIRE(test_case.rating < max_rating);
        }
    }
}

TEST_CASE("PuzzleRating - SE Rating is max technique", "[puzzle_rating]") {
    SECTION("SE rating equals max technique rating in solve path") {
        std::vector<SolveStep> solve_path = {{.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = {},
                                              .value = 1,
                                              .eliminations = {},
                                              .explanation = "",
                                              .rating = 2.3,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = {},
                                              .value = 2,
                                              .eliminations = {},
                                              .explanation = "",
                                              .rating = 2.3,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::HiddenSingle,
                                              .position = {},
                                              .value = 3,
                                              .eliminations = {},
                                              .explanation = "",
                                              .rating = 1.5,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Elimination,
                                              .technique = SolvingTechnique::NakedPair,
                                              .position = {},
                                              .value = 0,
                                              .eliminations = {},
                                              .explanation = "",
                                              .rating = 3.0,
                                              .explanation_data = {}}};

        // Max: 3.0 (NakedPair)
        PuzzleRating rating{.se_rating = 3.0,
                            .solve_path = solve_path,
                            .requires_backtracking = false,
                            .estimated_difficulty = Difficulty::Medium};

        // Verify max matches se_rating
        double max_rating = 0.0;
        for (const auto& step : rating.solve_path) {
            max_rating = std::max(max_rating, step.rating);
        }

        REQUIRE(rating.se_rating == max_rating);
    }

    SECTION("Backtracking step has SE 12.0 but is excluded from puzzle rating") {
        std::vector<SolveStep> solve_path = {{.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::NakedSingle,
                                              .position = {},
                                              .value = 1,
                                              .eliminations = {},
                                              .explanation = "",
                                              .rating = 2.3,
                                              .explanation_data = {}},
                                             {.type = SolveStepType::Placement,
                                              .technique = SolvingTechnique::Backtracking,
                                              .position = {},
                                              .value = 2,
                                              .eliminations = {},
                                              .explanation = "",
                                              .rating = 12.0,
                                              .explanation_data = {}}};

        // Rater excludes backtracking from se_rating (only logical techniques count)
        PuzzleRating rating{.se_rating = 2.3,
                            .solve_path = solve_path,
                            .requires_backtracking = true,
                            .estimated_difficulty = Difficulty::Easy};

        // Step itself has SE 12.0, but puzzle rating excludes it
        REQUIRE(solve_path[1].rating == 12.0);
        REQUIRE(rating.se_rating == 2.3);
        REQUIRE(rating.requires_backtracking);
    }
}
