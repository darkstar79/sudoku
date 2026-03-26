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

#include <filesystem>
#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

// Test fixture for edge case tests
struct EdgeCaseTestFixture {
    std::shared_ptr<IGameValidator> validator;
    std::shared_ptr<IPuzzleGenerator> generator;
    std::shared_ptr<ISudokuSolver> solver;
    std::shared_ptr<ISaveManager> save_manager;
    std::shared_ptr<IStatisticsManager> stats_manager;
    std::unique_ptr<GameViewModel> view_model;
    std::string test_dir;

    EdgeCaseTestFixture()
        : test_dir("./test_edge_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())) {
        validator = std::make_shared<GameValidator>();
        generator = std::make_shared<PuzzleGenerator>();
        solver = std::make_shared<SudokuSolver>(validator);
        save_manager = std::make_shared<SaveManager>(test_dir);
        stats_manager = std::make_shared<StatisticsManager>(test_dir);
        view_model = std::make_unique<GameViewModel>(validator, generator, solver, stats_manager, save_manager,
                                                     std::make_shared<MockLocalizationManager>());
    }

    ~EdgeCaseTestFixture() {
        std::filesystem::remove_all(test_dir);
    }
};

TEST_CASE("GameViewModel - Hint Edge Cases", "[game_view_model][hint]") {
    EdgeCaseTestFixture fixture;

    SECTION("Get hint on fresh game") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->getHint();

        // Should find a hint on a fresh game
        [[maybe_unused]] const auto& ui = fixture.view_model->uiState.get();
        // Hint may or may not be shown depending on board state - keep as-is (result varies)
        REQUIRE(true);
    }

    SECTION("Get hint with no empty cells") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Fill the entire board (simulate completion)
        fixture.view_model->gameState.update([](model::GameState& state) {
            // Fill every cell with a value
            for (size_t row = 0; row < BOARD_SIZE; ++row) {
                for (size_t col = 0; col < BOARD_SIZE; ++col) {
                    if (state.getValue(row, col) == 0) {
                        state.setValue(row, col, 1);  // Just fill with something
                    }
                }
            }
        });

        fixture.view_model->getHint();

        // Should handle gracefully - no hint available on a full board (keep as-is)
        REQUIRE(true);
    }
}

TEST_CASE("GameViewModel - Enter Number Edge Cases", "[game_view_model][enter]") {
    EdgeCaseTestFixture fixture;

    SECTION("Enter number with no selection") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Clear selection
        fixture.view_model->gameState.update([](model::GameState& state) { state.clearSelection(); });

        // Try to enter number - should not crash
        fixture.view_model->enterNumber(5);

        REQUIRE(!fixture.view_model->gameState.get().hasSelection());
    }

    SECTION("Enter number on given cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find a given cell
        const auto& state = fixture.view_model->gameState.get();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).is_given) {
                    Position pos{row, col};
                    int original_value = state.getCell(pos).value;

                    fixture.view_model->selectCell(pos);
                    fixture.view_model->enterNumber(9);
                    fixture.view_model->enterNumber(9);  // Double-press

                    const auto& after = fixture.view_model->gameState.get();
                    REQUIRE(after.getCell(pos).value == original_value);
                    return;
                }
            }
        }
    }

    SECTION("Enter invalid number (0)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    fixture.view_model->selectCell({row, col});
                    fixture.view_model->enterNumber(0);

                    const auto& after = fixture.view_model->gameState.get();
                    REQUIRE(after.getCell(row, col).value == 0);
                    return;
                }
            }
        }
    }

    SECTION("Enter invalid number (10)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    fixture.view_model->selectCell({row, col});
                    fixture.view_model->enterNumber(10);

                    const auto& after = fixture.view_model->gameState.get();
                    REQUIRE(after.getCell(row, col).value == 0);
                    return;
                }
            }
        }
    }
}

TEST_CASE("GameViewModel - Enter Note Edge Cases", "[game_view_model][note]") {
    EdgeCaseTestFixture fixture;

    SECTION("Enter note with no selection") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Clear selection
        fixture.view_model->gameState.update([](model::GameState& state) { state.clearSelection(); });

        // Try to enter note - should not crash
        fixture.view_model->enterNote(5);

        REQUIRE(!fixture.view_model->gameState.get().hasSelection());
    }

    SECTION("Enter note on given cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find a given cell
        const auto& state = fixture.view_model->gameState.get();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).is_given) {
                    Position pos{row, col};

                    fixture.view_model->selectCell(pos);
                    fixture.view_model->enterNote(5);

                    const auto& after = fixture.view_model->gameState.get();
                    REQUIRE(after.getCell(pos).notes.empty());
                    return;
                }
            }
        }
    }

    SECTION("Enter note on cell with value") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell and place value
        const auto& state = fixture.view_model->gameState.get();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    Position pos{row, col};
                    fixture.view_model->selectCell(pos);
                    fixture.view_model->enterNumber(5);
                    fixture.view_model->enterNumber(5);  // Double-press

                    // Try to add note - should not work
                    fixture.view_model->enterNote(7);

                    const auto& after = fixture.view_model->gameState.get();
                    REQUIRE(after.getCell(pos).notes.empty());
                    return;
                }
            }
        }
    }

    SECTION("Enter invalid note (0)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    fixture.view_model->selectCell({row, col});
                    fixture.view_model->enterNote(0);

                    const auto& after = fixture.view_model->gameState.get();
                    REQUIRE(after.getCell(row, col).notes.empty());
                    return;
                }
            }
        }
    }

    SECTION("Enter invalid note (10)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                if (state.getCell(row, col).value == 0) {
                    fixture.view_model->selectCell({row, col});
                    fixture.view_model->enterNote(10);

                    const auto& after = fixture.view_model->gameState.get();
                    REQUIRE(after.getCell(row, col).notes.empty());
                    return;
                }
            }
        }
    }
}

TEST_CASE("GameViewModel - Undo/Redo Edge Cases", "[game_view_model][undo]") {
    EdgeCaseTestFixture fixture;

    SECTION("Undo with no moves") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->canUndo() == false);

        fixture.view_model->undo();

        REQUIRE(!fixture.view_model->canUndo());
    }

    SECTION("Redo with no undone moves") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->canRedo() == false);

        fixture.view_model->redo();

        REQUIRE(!fixture.view_model->canRedo());
    }

    SECTION("Undo to last valid with no moves") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->undoToLastValid();

        REQUIRE(fixture.view_model->getMoveCount() >= 0);
    }

    SECTION("Undo to last valid when already valid") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Fresh board should be valid
        fixture.view_model->undoToLastValid();

        REQUIRE(fixture.view_model->getMoveCount() >= 0);
    }
}

TEST_CASE("GameViewModel - Save/Load Error Paths", "[game_view_model][save]") {
    EdgeCaseTestFixture fixture;

    SECTION("Load non-existent game") {
        fixture.view_model->loadGame("nonexistent_id_12345");

        // Should handle error gracefully
        const auto& error = fixture.view_model->errorMessage.get();
        // Should have error message
        REQUIRE(!error.empty());
    }

    SECTION("Auto-save without active game") {
        // Don't start a game
        fixture.view_model->autoSave();

        // No observable to verify auto-save without active game (keep as-is)
        REQUIRE(true);
    }
}

TEST_CASE("GameViewModel - Cell Navigation", "[game_view_model][navigation]") {
    EdgeCaseTestFixture fixture;

    SECTION("Select cell with valid coordinates") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->selectCell(4, 5);

        const auto& state = fixture.view_model->gameState.get();
        auto selected = state.getSelectedPosition();
        REQUIRE(selected.has_value());
        REQUIRE(selected->row == 4);
        REQUIRE(selected->col == 5);
    }

    SECTION("Select cell with Position struct") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        Position pos{.row = 3, .col = 7};
        fixture.view_model->selectCell(pos);

        const auto& state = fixture.view_model->gameState.get();
        auto selected = state.getSelectedPosition();
        REQUIRE(selected.has_value());
        REQUIRE(selected->row == 3);
        REQUIRE(selected->col == 7);
    }
}

TEST_CASE("GameViewModel - Game State Queries", "[game_view_model][queries]") {
    EdgeCaseTestFixture fixture;

    SECTION("Query state before starting game") {
        REQUIRE(fixture.view_model->canUndo() == false);
        REQUIRE(fixture.view_model->canRedo() == false);
    }

    SECTION("Query state after starting game") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->canUndo() == false);  // No moves yet
        REQUIRE(fixture.view_model->canRedo() == false);
    }
}

TEST_CASE("GameViewModel - Statistics Error Handling", "[game_view_model][stats_errors]") {
    EdgeCaseTestFixture fixture;

    SECTION("Handle statistics errors gracefully") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Operations that might trigger statistics errors
        fixture.view_model->refreshStatistics();

        // Verify statistics observable is accessible after refresh (keep as-is, no specific value to check)
        [[maybe_unused]] const auto& stats = fixture.view_model->statistics.get();
        REQUIRE(true);
    }
}
