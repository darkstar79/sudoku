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

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::core;
using namespace sudoku::viewmodel;

namespace {

struct EditModeFixture {
    sudoku::test::TempTestDir temp_dir;
    std::shared_ptr<IGameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<SolutionCounter> counter;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IPuzzleAnalyzer> analyzer;
    std::unique_ptr<GameViewModel> view_model;

    EditModeFixture() {
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

    // Lay down the kEasyImport board's givens via the edit-mode API.
    void layDownEasyPuzzle() {
        constexpr int easy[81] = {0, 3, 4, 6, 7, 8, 9, 1, 2, 6, 7, 2, 1, 9, 5, 3, 4, 8, 1, 9, 8, 3, 4, 2, 5, 6, 7,
                                  8, 5, 9, 7, 6, 1, 4, 2, 3, 4, 2, 6, 8, 5, 3, 7, 9, 1, 7, 1, 3, 9, 2, 4, 8, 5, 6,
                                  9, 6, 1, 5, 3, 7, 2, 8, 4, 2, 8, 7, 4, 1, 9, 6, 3, 5, 3, 4, 5, 2, 8, 6, 1, 7, 9};
        for (size_t i = 0; i < 81; ++i) {
            const size_t r = i / 9;
            const size_t c = i % 9;
            if (easy[i] != 0) {
                view_model->setEditModeGiven({.row = r, .col = c}, easy[i]);
            }
        }
    }
};

}  // namespace

TEST_CASE("enterEditMode clears the board and switches InputMode to EditGivens", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();

    REQUIRE(fixture.view_model->getInputMode() == InputMode::EditGivens);
    // Board is empty.
    const auto& state = fixture.view_model->gameState.get();
    bool any_value = false;
    for (size_t r = 0; r < 9 && !any_value; ++r) {
        for (size_t c = 0; c < 9 && !any_value; ++c) {
            if (state.getValue({.row = r, .col = c}) != 0) {
                any_value = true;
            }
        }
    }
    REQUIRE_FALSE(any_value);
}

TEST_CASE("setEditModeGiven places and clears given cells while in edit mode", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();

    fixture.view_model->setEditModeGiven({.row = 3, .col = 5}, 7);
    REQUIRE(fixture.view_model->gameState.get().getValue({.row = 3, .col = 5}) == 7);
    REQUIRE(fixture.view_model->gameState.get().isGiven({.row = 3, .col = 5}));

    fixture.view_model->setEditModeGiven({.row = 3, .col = 5}, 0);  // clear
    REQUIRE(fixture.view_model->gameState.get().getValue({.row = 3, .col = 5}) == 0);
    REQUIRE_FALSE(fixture.view_model->gameState.get().isGiven({.row = 3, .col = 5}));
}

TEST_CASE("commitEditedPuzzle on a valid unique puzzle switches back to Normal mode", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();
    fixture.layDownEasyPuzzle();

    fixture.view_model->commitEditedPuzzle();

    REQUIRE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->getInputMode() == InputMode::Normal);
    REQUIRE(fixture.view_model->getCurrentPuzzleOrigin() == PuzzleOrigin::ImportedEditMode);
    REQUIRE(fixture.view_model->isGameActive());
}

TEST_CASE("commitEditedPuzzle rejects a board with conflicts and stays in edit mode", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();
    // Two 5s in row 0 → invalid.
    fixture.view_model->setEditModeGiven({.row = 0, .col = 0}, 5);
    fixture.view_model->setEditModeGiven({.row = 0, .col = 4}, 5);

    fixture.view_model->commitEditedPuzzle();

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->getInputMode() == InputMode::EditGivens);
}

TEST_CASE("commitEditedPuzzle rejects a multi-solution board and stays in edit mode", "[game_view_model][edit_mode]") {
    EditModeFixture fixture;
    fixture.view_model->enterEditMode();
    // Only a few clues — multiple solutions.
    fixture.view_model->setEditModeGiven({.row = 0, .col = 0}, 1);
    fixture.view_model->setEditModeGiven({.row = 1, .col = 1}, 2);

    fixture.view_model->commitEditedPuzzle();

    REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    REQUIRE(fixture.view_model->getInputMode() == InputMode::EditGivens);
}
