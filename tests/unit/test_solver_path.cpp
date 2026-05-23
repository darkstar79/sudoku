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

#include "../../src/core/cpu_features.h"
#include "../../src/core/puzzle_generator.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;

TEST_CASE("SolverPath - isSolverPathAvailable", "[solver_path]") {
    SECTION("Auto and Scalar are always available") {
        REQUIRE(isSolverPathAvailable(SolverPath::Auto));
        REQUIRE(isSolverPathAvailable(SolverPath::Scalar));
    }

    SECTION("AVX2 availability matches CPU detection") {
        REQUIRE(isSolverPathAvailable(SolverPath::AVX2) == hasAvx2());
    }

    SECTION("AVX-512 availability matches CPU detection") {
        REQUIRE(isSolverPathAvailable(SolverPath::AVX512) == hasAvx512());
    }
}

TEST_CASE("SolverPath - solverPathName", "[solver_path]") {
    REQUIRE(std::string(solverPathName(SolverPath::Auto)) == "Auto");
    REQUIRE(std::string(solverPathName(SolverPath::Scalar)) == "Scalar");
    REQUIRE(std::string(solverPathName(SolverPath::AVX2)) == "AVX2");
    REQUIRE(std::string(solverPathName(SolverPath::AVX512)) == "AVX-512");
}

TEST_CASE("SolverPath - GenerationSettings solver_path field", "[solver_path]") {
    SECTION("Default is nullopt (Auto)") {
        GenerationSettings settings;
        REQUIRE_FALSE(settings.solver_path.has_value());
    }

    SECTION("Can set specific path") {
        GenerationSettings settings;
        settings.solver_path = SolverPath::Scalar;
        REQUIRE(settings.solver_path.has_value());
        REQUIRE(*settings.solver_path == SolverPath::Scalar);
    }
}

TEST_CASE("SolverPath - PuzzleGenerator setSolverPath", "[solver_path]") {
    PuzzleGenerator generator;

    SECTION("Default solver path is Auto") {
        REQUIRE(generator.solverPath() == SolverPath::Auto);
    }

    SECTION("Can set and get solver path") {
        generator.setSolverPath(SolverPath::Scalar);
        REQUIRE(generator.solverPath() == SolverPath::Scalar);

        generator.setSolverPath(SolverPath::AVX2);
        REQUIRE(generator.solverPath() == SolverPath::AVX2);
    }
}

TEST_CASE("SolverPath - Scalar and AVX2 produce identical puzzles", "[solver_path]") {
    // Skip if AVX2 is not available
    if (!hasAvx2()) {
        SKIP("AVX2 not available on this CPU");
    }

    PuzzleGenerator generator;

    // Use a fixed seed for deterministic comparison
    constexpr uint32_t test_seed = 42;

    // Generate with Scalar path
    GenerationSettings scalar_settings;
    scalar_settings.difficulty = Difficulty::Medium;
    scalar_settings.seed = test_seed;
    scalar_settings.ensure_unique = false;
    scalar_settings.solver_path = SolverPath::Scalar;

    auto scalar_result = generator.generatePuzzle(scalar_settings);
    REQUIRE(scalar_result.has_value());

    // Generate with AVX2 path
    GenerationSettings avx2_settings;
    avx2_settings.difficulty = Difficulty::Medium;
    avx2_settings.seed = test_seed;
    avx2_settings.ensure_unique = false;
    avx2_settings.solver_path = SolverPath::AVX2;

    auto avx2_result = generator.generatePuzzle(avx2_settings);
    REQUIRE(avx2_result.has_value());

    // Both paths should produce the same puzzle
    REQUIRE(scalar_result->board == avx2_result->board);
    REQUIRE(scalar_result->solution == avx2_result->solution);
}

TEST_CASE("SolverPath - Settings solver_path is respected", "[solver_path]") {
    PuzzleGenerator generator;

    GenerationSettings settings;
    settings.difficulty = Difficulty::Easy;
    settings.seed = 99;
    settings.ensure_unique = false;
    settings.solver_path = SolverPath::Scalar;

    auto result = generator.generatePuzzle(settings);
    REQUIRE(result.has_value());

    // After generation, the generator's active path should reflect the settings
    REQUIRE(generator.solverPath() == SolverPath::Scalar);
}

TEST_CASE("SolverPath - AVX-512 produces identical puzzles when available", "[solver_path]") {
    if (!hasAvx512()) {
        SKIP("AVX-512 not available on this CPU");
    }

    PuzzleGenerator generator;
    constexpr uint32_t test_seed = 42;

    // Generate with AVX2
    GenerationSettings avx2_settings;
    avx2_settings.difficulty = Difficulty::Medium;
    avx2_settings.seed = test_seed;
    avx2_settings.ensure_unique = false;
    avx2_settings.solver_path = SolverPath::AVX2;

    auto avx2_result = generator.generatePuzzle(avx2_settings);
    REQUIRE(avx2_result.has_value());

    // Generate with AVX-512
    GenerationSettings avx512_settings;
    avx512_settings.difficulty = Difficulty::Medium;
    avx512_settings.seed = test_seed;
    avx512_settings.ensure_unique = false;
    avx512_settings.solver_path = SolverPath::AVX512;

    auto avx512_result = generator.generatePuzzle(avx512_settings);
    REQUIRE(avx512_result.has_value());

    // Both paths should produce the same puzzle
    REQUIRE(avx2_result->board == avx512_result->board);
    REQUIRE(avx2_result->solution == avx512_result->solution);
}
