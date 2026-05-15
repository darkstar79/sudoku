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
#include "../../src/core/i_save_manager.h"
#include "../../src/core/puzzle_analyzer.h"
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/solution_counter.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "../helpers/test_utils.h"

#include <memory>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

// Known-easy 81-char puzzle — one naked single (R1C1 = 5).
constexpr std::string_view kEasyImport = "034678912"
                                         "672195348"
                                         "198342567"
                                         "859761423"
                                         "426853791"
                                         "713924856"
                                         "961537284"
                                         "287419635"
                                         "345286179";

struct ImportFixture {
    sudoku::test::TempTestDir temp_dir;
    std::shared_ptr<IGameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<SolutionCounter> counter;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IPuzzleAnalyzer> analyzer;
    std::unique_ptr<GameViewModel> view_model;

    ImportFixture() {
        validator = std::make_shared<GameValidator>();
        generator = std::make_shared<PuzzleGenerator>();
        solver = std::make_shared<SudokuSolver>(validator);
        counter = std::make_shared<SolutionCounter>();
        stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
        save_manager = std::make_shared<SaveManager>(temp_dir.path());
        analyzer = std::make_shared<PuzzleAnalyzer>(validator, solver, counter);
        view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                     /*settings_manager*/ nullptr, analyzer);
    }
};

}  // namespace

TEST_CASE("importPuzzleFromString loads a valid 81-char puzzle as the new game state", "[game_view_model][import]") {
    ImportFixture fixture;
    fixture.view_model->importPuzzleFromString(kEasyImport);

    REQUIRE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->isGameActive());

    // The board's givens match the import string (R1C1 empty, R1C2=3, etc.)
    const auto& state = fixture.view_model->gameState.get();
    REQUIRE(state.getValue({.row = 0, .col = 0}) == 0);
    REQUIRE(state.getValue({.row = 0, .col = 1}) == 3);
    REQUIRE(state.getValue({.row = 8, .col = 8}) == 9);

    // The empty cell (0,0) is NOT a given (it's a solvable cell).
    REQUIRE_FALSE(state.isGiven({.row = 0, .col = 0}));
    // The filled cells from the import ARE givens.
    REQUIRE(state.isGiven({.row = 0, .col = 1}));

    // Origin is tracked as ImportedString (verified indirectly via save round-trip below).
    REQUIRE(fixture.view_model->getCurrentPuzzleOrigin() == PuzzleOrigin::ImportedString);
}

TEST_CASE("importPuzzleFromString rejects malformed input via errorMessage", "[game_view_model][import]") {
    ImportFixture fixture;
    const bool was_active_before = fixture.view_model->isGameActive();

    fixture.view_model->importPuzzleFromString("only 5 chars");

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->isGameActive() == was_active_before);
}

TEST_CASE("importPuzzleFromString rejects a board with rule conflicts", "[game_view_model][import]") {
    ImportFixture fixture;
    // Same easy import but with a duplicate 3 in row 0.
    std::string bad{kEasyImport};
    bad[0] = '3';  // R1C1 also = 3 → row 0 has two 3s

    fixture.view_model->importPuzzleFromString(bad);

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
}

TEST_CASE("importPuzzleFromString rejects a multi-solution puzzle", "[game_view_model][import]") {
    ImportFixture fixture;
    // Near-empty board with a couple clues — many solutions.
    std::string sparse(81, '.');
    sparse[0] = '1';
    sparse[10] = '2';

    fixture.view_model->importPuzzleFromString(sparse);

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
}

TEST_CASE("importPuzzleFromString persists origin through save/load round-trip", "[game_view_model][import][origin]") {
    ImportFixture fixture;
    fixture.view_model->importPuzzleFromString(kEasyImport);
    REQUIRE(fixture.view_model->isGameActive());

    // Save with inspectable settings.
    const auto& current = fixture.view_model->gameState.get();
    SavedGame to_save;
    to_save.save_id = "import_origin_test";
    to_save.display_name = "Import origin test";
    to_save.original_puzzle = current.extractGivenNumbers();
    to_save.current_state = current.extractNumbers();
    to_save.origin = fixture.view_model->getCurrentPuzzleOrigin();

    SaveSettings settings;
    settings.compress = false;
    settings.encrypt = false;
    auto save_result = fixture.save_manager->saveGame(to_save, settings);
    REQUIRE(save_result.has_value());

    auto loaded = fixture.save_manager->loadGame(*save_result);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->origin == PuzzleOrigin::ImportedString);
}
