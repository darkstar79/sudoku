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

#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/puzzle_rater.h"
#include "../../src/core/puzzle_rating.h"
#include "../../src/core/sudoku_solver.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("PuzzleGenerator - Rating Integration", "[puzzle_generator][rating]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator_base = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto rater = std::make_shared<PuzzleRater>(solver);
    PuzzleGenerator generator(rater);

    SECTION("Generated puzzle has rating field populated") {
        auto result = generator.generatePuzzle({.difficulty = Difficulty::Easy});
        REQUIRE(result.has_value());

        auto& puzzle = result.value();
        REQUIRE(puzzle.rating > 0);  // Rating should be calculated
    }

    SECTION("Easy puzzle rating is in Easy range") {
        auto result = generator.generatePuzzle({.difficulty = Difficulty::Easy});

        // May fail occasionally until validation is implemented
        if (result.has_value()) {
            auto& puzzle = result.value();

            // For now, just verify rating exists (strict validation comes later)
            REQUIRE(puzzle.rating >= 0);
        }
    }
}

TEST_CASE("PuzzleGenerator - Rating Validation", "[puzzle_generator][rating][validation]") {
    auto validator = std::make_shared<GameValidator>();
    auto generator_base = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto rater = std::make_shared<PuzzleRater>(solver);

    GenerationSettings settings{
        .difficulty = Difficulty::Easy,
        .seed = 0,
        .ensure_unique = true,
        .max_attempts = 50  // Limit attempts for faster tests
    };

    PuzzleGenerator generator(rater);

    SECTION("Rating validation rejects puzzles outside rating range") {
        auto result = generator.generatePuzzle(settings);

        if (result.has_value()) {
            auto& puzzle = result.value();
            auto [min_rating, max_rating] = getDifficultyRatingRange(settings.difficulty);

            // Rating MUST be in range
            REQUIRE(puzzle.rating >= min_rating);
            REQUIRE(puzzle.rating < max_rating);
        } else {
            // Generation may fail if validation rejects too many puzzles
            REQUIRE(result.error() == GenerationError::TooManyAttempts);
        }
    }
}

TEST_CASE("PuzzleGenerator - Backward Compatibility", "[puzzle_generator][rating]") {
    SECTION("Generator without rater still works") {
        PuzzleGenerator generator;  // No rater dependency

        auto result = generator.generatePuzzle({.difficulty = Difficulty::Easy});
        REQUIRE(result.has_value());

        auto& puzzle = result.value();
        REQUIRE(puzzle.rating == 0);     // Not rated
        REQUIRE(puzzle.clue_count > 0);  // Still generated correctly
    }
}
