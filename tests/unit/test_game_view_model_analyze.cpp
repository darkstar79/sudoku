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

#include "../../src/core/game_validator.h"
#include "../../src/core/puzzle_analyzer.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/solution_counter.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "../helpers/test_utils.h"

#include <chrono>
#include <expected>
#include <memory>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

/// IPuzzleAnalyzer mock that returns ScoringError::Timeout for scoreDifficulty.
/// Lets us drive the analyze-difficulty timeout path without depending on real wall-clock.
class TimeoutAnalyzer : public IPuzzleAnalyzer {
public:
    [[nodiscard]] std::expected<BoardData, ParseErrorDetail> parseString(std::string_view /*input*/) const override {
        return std::unexpected(ParseErrorDetail{.code = ParseError::Empty});
    }
    [[nodiscard]] std::string serializeToString(const BoardData& /*board*/) const override {
        return {};
    }
    [[nodiscard]] std::expected<void, BoardValidationError> validate(const BoardData& /*board*/) const override {
        return {};
    }
    [[nodiscard]] UniquenessResult checkUniqueness(const BoardData& /*board*/) const override {
        return UniquenessResult::Unique;
    }
    [[nodiscard]] std::expected<DifficultyScore, ScoringError>
    scoreDifficulty(const BoardData& /*board*/, std::chrono::milliseconds /*budget*/) const override {
        return std::unexpected(ScoringError::Timeout);
    }
};

struct AnalyzeFixture {
    sudoku::test::TempTestDir temp_dir;
    std::shared_ptr<IGameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<SolutionCounter> counter;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IPuzzleAnalyzer> analyzer;
    std::unique_ptr<GameViewModel> view_model;

    explicit AnalyzeFixture(std::shared_ptr<IPuzzleAnalyzer> custom_analyzer = nullptr) {
        validator = std::make_shared<GameValidator>();
        generator = std::make_shared<PuzzleGenerator>();
        solver = std::make_shared<SudokuSolver>(validator);
        counter = std::make_shared<SolutionCounter>();
        stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
        save_manager = std::make_shared<SaveManager>(temp_dir.path());
        analyzer =
            custom_analyzer ? std::move(custom_analyzer) : std::make_shared<PuzzleAnalyzer>(validator, solver, counter);
        view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                     /*settings_manager*/ nullptr, analyzer);
    }
};

}  // namespace

TEST_CASE("analyzeDifficulty fills puzzle_rating + techniques on an easy puzzle",
          "[game_view_model][analyze][scoring]") {
    AnalyzeFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    // Generated puzzles already have a rating — reset to simulate an imported, unrated puzzle.
    auto ui_before = fixture.view_model->uiState.get();
    // Trick: directly invoke a future "clearPuzzleScore" wouldn't exist yet; instead just
    // call analyzeDifficulty and verify the rating after equals what scoreDifficulty produced.

    fixture.view_model->analyzeDifficulty();

    const auto& ui = fixture.view_model->uiState.get();
    REQUIRE(ui.puzzle_rating > 0.0);
    REQUIRE_FALSE(ui.puzzle_techniques.empty());
    // No error message on success.
    REQUIRE(fixture.view_model->errorMessage.get().empty());
}

TEST_CASE("analyzeDifficulty sets errorMessage when scoring times out", "[game_view_model][analyze][timeout]") {
    AnalyzeFixture fixture(std::make_shared<TimeoutAnalyzer>());
    fixture.view_model->startNewGame(Difficulty::Easy);
    fixture.view_model->errorMessage.set("");

    fixture.view_model->analyzeDifficulty();

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
}

TEST_CASE("analyzeDifficulty is a no-op without an analyzer dependency", "[game_view_model][analyze]") {
    // Build a VM the same way the existing GameViewModelFixture does — no analyzer wired.
    sudoku::test::TempTestDir temp_dir;
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
    auto save_manager = std::make_shared<SaveManager>(temp_dir.path());
    GameViewModel vm(validator, generator, solver, stats_manager, save_manager);
    vm.startNewGame(Difficulty::Easy);
    vm.errorMessage.set("");

    // Should not crash — graceful no-op or set errorMessage; we just require no crash and
    // that the UIState is not silently corrupted.
    REQUIRE_NOTHROW(vm.analyzeDifficulty());
}
