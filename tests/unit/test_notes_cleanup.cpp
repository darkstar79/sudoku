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

#include <algorithm>
#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;
using namespace sudoku::model;

TEST_CASE("Notes Cleanup - Comprehensive Testing", "[notes-cleanup]") {
    SECTION("GameViewModel basic initialization") {
        auto validator = std::make_shared<GameValidator>();
        auto generator = std::make_shared<PuzzleGenerator>();
        auto solver = std::make_shared<SudokuSolver>(validator);
        auto stats_manager = std::make_shared<StatisticsManager>();
        auto save_manager = std::make_shared<SaveManager>();

        auto view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                          std::make_shared<MockLocalizationManager>());

        REQUIRE(view_model != nullptr);
        view_model->startNewGame(Difficulty::Easy);
        auto& state = view_model->gameState.get();
        REQUIRE_FALSE(state.isComplete());
    }

    SECTION("Conflicting notes cleanup in same row, column, and block") {
        auto validator = std::make_shared<GameValidator>();
        auto generator = std::make_shared<PuzzleGenerator>();
        auto solver = std::make_shared<SudokuSolver>(validator);
        auto stats_manager = std::make_shared<StatisticsManager>();
        auto save_manager = std::make_shared<SaveManager>();

        auto view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                          std::make_shared<MockLocalizationManager>());

        // Create a custom game state with known empty cells for testing
        view_model->startNewGame(Difficulty::Easy);
        auto& initial_state = view_model->gameState.get();

        // Find three empty cells: one to place number, others in same row/col/block
        std::optional<Position> target_cell;
        std::optional<Position> same_row_cell;
        std::optional<Position> same_col_cell;

        // Look for suitable test positions
        for (size_t row = 0; row < 9 && !target_cell.has_value(); ++row) {
            for (size_t col = 0; col < 9 && !target_cell.has_value(); ++col) {
                if (initial_state.getCell(row, col).value == 0) {
                    target_cell = Position{.row = row, .col = col};

                    // Find another empty cell in same row
                    for (size_t c = 0; c < 9; ++c) {
                        if (c != col && initial_state.getCell(row, c).value == 0) {
                            same_row_cell = Position{.row = row, .col = c};
                            break;
                        }
                    }

                    // Find another empty cell in same column
                    for (size_t r = 0; r < 9; ++r) {
                        if (r != row && initial_state.getCell(r, col).value == 0) {
                            same_col_cell = Position{.row = r, .col = col};
                            break;
                        }
                    }
                }
            }
        }

        if (target_cell.has_value() && same_row_cell.has_value() && same_col_cell.has_value()) {
            // Test the cleanup functionality
            const int test_number = 7;

            // Add notes to cells that should be cleaned up
            view_model->selectCell(same_row_cell.value());
            view_model->enterNote(test_number);

            view_model->selectCell(same_col_cell.value());
            view_model->enterNote(test_number);

            // Verify notes were added
            auto& state_with_notes = view_model->gameState.get();
            const auto& sr_pos = same_row_cell.value();
            const auto& sc_pos = same_col_cell.value();
            auto row_notes = state_with_notes.getCell(sr_pos.row, sr_pos.col).notes;
            auto col_notes = state_with_notes.getCell(sc_pos.row, sc_pos.col).notes;
            REQUIRE(std::find(row_notes.begin(), row_notes.end(), test_number) != row_notes.end());
            REQUIRE(std::find(col_notes.begin(), col_notes.end(), test_number) != col_notes.end());

            // Place the number in target cell - this should trigger cleanup
            view_model->selectCell(target_cell.value());
            view_model->enterNumber(test_number);

            // Verify the cleanup worked
            auto& final_state = view_model->gameState.get();
            const auto& tc_pos = target_cell.value();
            REQUIRE(final_state.getCell(tc_pos.row, tc_pos.col).value == test_number);

            // The conflicting notes should have been removed
            auto final_row_notes = final_state.getCell(sr_pos.row, sr_pos.col).notes;
            auto final_col_notes = final_state.getCell(sc_pos.row, sc_pos.col).notes;
            REQUIRE(std::find(final_row_notes.begin(), final_row_notes.end(), test_number) == final_row_notes.end());
            REQUIRE(std::find(final_col_notes.begin(), final_col_notes.end(), test_number) == final_col_notes.end());
        } else {
            // If we can't find suitable test cells, just verify basic functionality
            REQUIRE_FALSE(initial_state.isComplete());
        }
    }

    SECTION("Notes cleanup preserves other numbers") {
        auto validator = std::make_shared<GameValidator>();
        auto generator = std::make_shared<PuzzleGenerator>();
        auto solver = std::make_shared<SudokuSolver>(validator);
        auto stats_manager = std::make_shared<StatisticsManager>();
        auto save_manager = std::make_shared<SaveManager>();

        auto view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                          std::make_shared<MockLocalizationManager>());

        view_model->startNewGame(Difficulty::Easy);
        auto& initial_state = view_model->gameState.get();

        // Find empty cells for testing
        std::optional<Position> target_cell;
        std::optional<Position> test_cell;

        for (size_t row = 0; row < 9 && !target_cell.has_value(); ++row) {
            for (size_t col = 0; col < 9 && !target_cell.has_value(); ++col) {
                if (initial_state.getCell(row, col).value == 0) {
                    target_cell = Position{.row = row, .col = col};

                    // Find another empty cell in same row
                    for (size_t c = 0; c < 9; ++c) {
                        if (c != col && initial_state.getCell(row, c).value == 0) {
                            test_cell = Position{.row = row, .col = c};
                            break;
                        }
                    }
                }
            }
        }

        if (target_cell.has_value() && test_cell.has_value()) {
            const int number_to_place = 5;
            const int other_note = 3;

            // Add multiple notes to test cell
            view_model->selectCell(test_cell.value());
            view_model->enterNote(number_to_place);  // This should be cleaned up
            view_model->enterNote(other_note);       // This should remain

            // Verify both notes are present
            auto& state_with_notes = view_model->gameState.get();
            const auto& tc_pos = test_cell.value();
            auto test_notes = state_with_notes.getCell(tc_pos.row, tc_pos.col).notes;
            REQUIRE(std::find(test_notes.begin(), test_notes.end(), number_to_place) != test_notes.end());
            REQUIRE(std::find(test_notes.begin(), test_notes.end(), other_note) != test_notes.end());

            // Place number - should clean up only the matching note
            view_model->selectCell(target_cell.value());
            view_model->enterNumber(number_to_place);

            // Verify selective cleanup
            auto& final_state = view_model->gameState.get();
            const auto& tgt_pos = target_cell.value();
            REQUIRE(final_state.getCell(tgt_pos.row, tgt_pos.col).value == number_to_place);
            auto final_test_notes = final_state.getCell(tc_pos.row, tc_pos.col).notes;
            REQUIRE(std::find(final_test_notes.begin(), final_test_notes.end(), number_to_place) ==
                    final_test_notes.end());
            REQUIRE(std::find(final_test_notes.begin(), final_test_notes.end(), other_note) !=
                    final_test_notes.end());  // Should still be there
        } else {
            // Basic verification if suitable cells not found
            REQUIRE_FALSE(initial_state.isComplete());
        }
    }
}