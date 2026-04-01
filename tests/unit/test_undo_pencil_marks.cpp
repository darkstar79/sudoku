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
#include "../../src/core/puzzle_generator.h"
#include "../../src/core/save_manager.h"
#include "../../src/core/statistics_manager.h"
#include "../../src/core/sudoku_solver.h"
#include "../../src/view_model/game_view_model.h"
#include "../helpers/mock_localization_manager.h"
#include "../helpers/test_utils.h"

#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

TEST_CASE("Undo restores pencil marks after placing number", "[undo][pencil_marks]") {
    // Setup
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto stats_mgr = std::make_shared<StatisticsManager>("./test_stats_undo_pencil");
    auto save_mgr = std::make_shared<SaveManager>("./test_saves_undo_pencil");

    GameViewModel view_model(validator, generator, solver, stats_mgr, save_mgr,
                             std::make_shared<MockLocalizationManager>());
    view_model.startNewGame(Difficulty::Easy);

    SECTION("Placing number clears pencil marks, undo restores them") {
        // Find first empty cell
        const auto& game_state = view_model.gameState.get();
        auto empty_cell_opt = test::findEmptyCell(game_state);
        REQUIRE(empty_cell_opt.has_value());
        Position empty_cell = empty_cell_opt.value();

        // Add some pencil marks (notes)
        view_model.enterNote(empty_cell, 1);
        view_model.enterNote(empty_cell, 2);
        view_model.enterNote(empty_cell, 3);

        // Verify notes were added
        const auto& cell_with_notes = view_model.gameState.get().getCell(empty_cell);
        REQUIRE(cell_with_notes.notes.count() == 3);
        REQUIRE(cell_with_notes.notes.contains(1));
        REQUIRE(cell_with_notes.notes.contains(2));
        REQUIRE(cell_with_notes.notes.contains(3));
        REQUIRE(cell_with_notes.value == 0);

        // Place a number (this should clear notes)
        view_model.enterNumber(empty_cell, 5);

        // Verify number was placed and notes were cleared
        const auto& cell_after_place = view_model.gameState.get().getCell(empty_cell);
        REQUIRE(cell_after_place.value == 5);
        REQUIRE(cell_after_place.notes.empty());

        // Undo the number placement
        view_model.undo();

        // Verify notes were restored
        const auto& cell_after_undo = view_model.gameState.get().getCell(empty_cell);
        REQUIRE(cell_after_undo.value == 0);
        REQUIRE(cell_after_undo.notes.count() == 3);
        REQUIRE(cell_after_undo.notes.contains(1));
        REQUIRE(cell_after_undo.notes.contains(2));
        REQUIRE(cell_after_undo.notes.contains(3));
    }

    SECTION("Clearing cell preserves notes on undo") {
        // Find a cell with a user-entered number
        const auto& game_state = view_model.gameState.get();
        auto target_cell_opt = test::findEmptyCell(game_state);
        REQUIRE(target_cell_opt.has_value());
        Position target_cell = target_cell_opt.value();

        // Place a number
        view_model.enterNumber(target_cell, 7);

        // Verify number was placed
        REQUIRE(view_model.gameState.get().getCell(target_cell).value == 7);

        // Clear the cell
        view_model.clearCell(target_cell);

        // Verify cell was cleared
        REQUIRE(view_model.gameState.get().getCell(target_cell).value == 0);

        // Undo the clear operation
        view_model.undo();

        // Verify number was restored
        REQUIRE(view_model.gameState.get().getCell(target_cell).value == 7);

        // Undo the number placement
        view_model.undo();

        // Verify we're back to empty cell
        REQUIRE(view_model.gameState.get().getCell(target_cell).value == 0);
    }

    SECTION("Multiple undo/redo operations preserve notes") {
        // Find first empty cell
        const auto& game_state = view_model.gameState.get();
        auto empty_cell_opt = test::findEmptyCell(game_state);
        REQUIRE(empty_cell_opt.has_value());
        Position empty_cell = empty_cell_opt.value();

        // Add notes
        view_model.enterNote(empty_cell, 4);
        view_model.enterNote(empty_cell, 5);

        // Verify notes
        const auto& cell = view_model.gameState.get().getCell(empty_cell);
        REQUIRE(cell.notes.count() == 2);

        // Place number
        view_model.enterNumber(empty_cell, 9);
        REQUIRE(view_model.gameState.get().getCell(empty_cell).value == 9);
        REQUIRE(view_model.gameState.get().getCell(empty_cell).notes.empty());

        // Undo
        view_model.undo();
        const auto& after_first_undo = view_model.gameState.get().getCell(empty_cell);
        REQUIRE(after_first_undo.value == 0);
        REQUIRE(after_first_undo.notes.count() == 2);

        // Redo
        view_model.redo();
        const auto& after_redo = view_model.gameState.get().getCell(empty_cell);
        REQUIRE(after_redo.value == 9);
        REQUIRE(after_redo.notes.empty());

        // Undo again
        view_model.undo();
        const auto& after_second_undo = view_model.gameState.get().getCell(empty_cell);
        REQUIRE(after_second_undo.value == 0);
        REQUIRE(after_second_undo.notes.count() == 2);
        REQUIRE(after_second_undo.notes.contains(4));
        REQUIRE(after_second_undo.notes.contains(5));
    }
}
