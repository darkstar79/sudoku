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

#include "../../src/core/di_container.h"
#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_analyzer.h"
#include "../../src/core/solution_counter.h"
#include "../../src/core/sudoku_solver.h"

#include <chrono>
#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku::core;
using namespace std::chrono_literals;

namespace {

void wireAnalyzerContainer(DIContainer& container) {
    container.clear();
    container.registerSingleton<IGameValidator>([]() { return std::make_unique<GameValidator>(); });
    container.registerSingleton<ISudokuSolver>([&container]() {
        auto validator = container.resolve<IGameValidator>();
        return std::make_unique<SudokuSolver>(validator);
    });
    container.registerSingleton<SolutionCounter>([]() { return std::make_unique<SolutionCounter>(); });
    container.registerSingleton<IPuzzleAnalyzer>([&container]() {
        auto validator = container.resolve<IGameValidator>();
        auto solver = container.resolve<ISudokuSolver>();
        auto counter = container.resolve<SolutionCounter>();
        return std::make_unique<PuzzleAnalyzer>(validator, solver, counter);
    });
}

}  // namespace

TEST_CASE("IPuzzleAnalyzer resolves from DI and supports a full round-trip", "[integration][puzzle_analyzer]") {
    DIContainer container;
    wireAnalyzerContainer(container);

    auto analyzer = container.resolve<IPuzzleAnalyzer>();
    REQUIRE(analyzer != nullptr);

    // A known-easy 81-char puzzle: world-famous "easy" pattern (a single naked-single).
    constexpr std::string_view kInput = "034678912"
                                        "672195348"
                                        "198342567"
                                        "859761423"
                                        "426853791"
                                        "713924856"
                                        "961537284"
                                        "287419635"
                                        "345286179";

    // Parse.
    auto parsed = analyzer->parseString(kInput);
    REQUIRE(parsed.has_value());

    // Validate.
    REQUIRE(analyzer->validate(*parsed).has_value());

    // Uniqueness.
    REQUIRE(analyzer->checkUniqueness(*parsed) == UniquenessResult::Unique);

    // Score.
    auto score = analyzer->scoreDifficulty(*parsed, 1000ms);
    REQUIRE(score.has_value());
    REQUIRE(score->max_rating >= 1.5);
    REQUIRE(score->max_rating <= 3.0);
    REQUIRE_FALSE(score->requires_backtracking);

    // Serialize round-trip.
    auto serialized = analyzer->serializeToString(*parsed);
    auto reparsed = analyzer->parseString(serialized);
    REQUIRE(reparsed.has_value());
    REQUIRE(*reparsed == *parsed);
}
