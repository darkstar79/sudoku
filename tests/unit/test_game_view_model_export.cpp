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
#include "../helpers/in_memory_clipboard_provider.h"
#include "../helpers/test_utils.h"

#include <memory>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

constexpr std::string_view kEasyImport = "034678912"
                                         "672195348"
                                         "198342567"
                                         "859761423"
                                         "426853791"
                                         "713924856"
                                         "961537284"
                                         "287419635"
                                         "345286179";

struct ExportFixture {
    sudoku::test::TempTestDir temp_dir;
    std::shared_ptr<IGameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<SolutionCounter> counter;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IPuzzleAnalyzer> analyzer;
    std::shared_ptr<sudoku::test::InMemoryClipboardProvider> clipboard;
    std::unique_ptr<GameViewModel> view_model;

    ExportFixture() {
        validator = std::make_shared<GameValidator>();
        generator = std::make_shared<PuzzleGenerator>();
        solver = std::make_shared<SudokuSolver>(validator);
        counter = std::make_shared<SolutionCounter>();
        stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
        save_manager = std::make_shared<SaveManager>(temp_dir.path());
        analyzer = std::make_shared<PuzzleAnalyzer>(validator, solver, counter);
        clipboard = std::make_shared<sudoku::test::InMemoryClipboardProvider>();
        view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                     /*settings_manager*/ nullptr, analyzer, clipboard);
    }
};

}  // namespace

TEST_CASE("exportPuzzleAsString writes the givens-only 81-char string to the clipboard", "[game_view_model][export]") {
    ExportFixture fixture;
    fixture.view_model->importPuzzleFromString(kEasyImport);
    REQUIRE(fixture.view_model->isGameActive());

    fixture.view_model->exportPuzzleAsString();

    const auto clipboard_text = fixture.clipboard->getText();
    REQUIRE(clipboard_text.size() == 81);
    // The serializer emits '.' for empty cells; the import had a leading '0' (empty),
    // so the round-trip via parseString+serializeToString swaps '0' → '.'.
    std::string expected{kEasyImport};
    expected[0] = '.';
    REQUIRE(clipboard_text == expected);
}

TEST_CASE("exportPuzzleAsString round-trips through parseString", "[game_view_model][export]") {
    ExportFixture fixture;
    fixture.view_model->importPuzzleFromString(kEasyImport);
    fixture.view_model->exportPuzzleAsString();

    auto re_parsed = fixture.analyzer->parseString(fixture.clipboard->getText());
    REQUIRE(re_parsed.has_value());
    // The givens match the original import.
    const auto& state = fixture.view_model->gameState.get();
    REQUIRE(*re_parsed == state.extractGivenNumbers());
}

TEST_CASE("exportPuzzleAsString is a no-op without a clipboard provider", "[game_view_model][export]") {
    sudoku::test::TempTestDir temp_dir;
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto counter = std::make_shared<SolutionCounter>();
    auto stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
    auto save_manager = std::make_shared<SaveManager>(temp_dir.path());
    auto analyzer = std::make_shared<PuzzleAnalyzer>(validator, solver, counter);
    GameViewModel vm(validator, generator, solver, stats_manager, save_manager,
                     /*settings_manager*/ nullptr, analyzer);  // no clipboard
    vm.importPuzzleFromString(kEasyImport);

    REQUIRE_NOTHROW(vm.exportPuzzleAsString());
}
