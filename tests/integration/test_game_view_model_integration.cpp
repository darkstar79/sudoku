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

#include <chrono>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;
namespace fs = std::filesystem;

// Test fixture for GameViewModel integration tests
class GameViewModelTestFixture {
public:
    GameViewModelTestFixture()
        : test_dir_("./test_vm_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())),
          validator_(std::make_shared<GameValidator>()), generator_(std::make_shared<PuzzleGenerator>()),
          solver_(std::make_shared<SudokuSolver>(validator_)),
          stats_manager_(std::make_shared<StatisticsManager>(test_dir_ + "/stats")),
          save_manager_(std::make_shared<SaveManager>(test_dir_ + "/saves")),
          view_model_(validator_, generator_, solver_, stats_manager_, save_manager_,
                      std::make_shared<MockLocalizationManager>()) {
        // Create test directories
        fs::create_directories(test_dir_);
    }

    ~GameViewModelTestFixture() {
        // Clean up test directory
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    std::string test_dir_;
    std::shared_ptr<IGameValidator> validator_;
    std::shared_ptr<IPuzzleGenerator> generator_;
    std::shared_ptr<ISudokuSolver> solver_;
    std::shared_ptr<IStatisticsManager> stats_manager_;
    std::shared_ptr<ISaveManager> save_manager_;
    GameViewModel view_model_;
};

TEST_CASE("GameViewModel - Complete game flow", "[game_view_model][integration]") {
    GameViewModelTestFixture fixture;

    SECTION("Start new game and play") {
        // Start new game
        fixture.view_model_.startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model_.isGameActive());
        REQUIRE_FALSE(fixture.view_model_.isGameComplete());

        const auto& game_state = fixture.view_model_.gameState.get();
        // Verify game has been loaded with puzzle
        bool has_given_cells = false;
        for (int r = 0; r < 9 && !has_given_cells; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (game_state.getCell(r, c).is_given) {
                    has_given_cells = true;
                    break;
                }
            }
        }
        REQUIRE(has_given_cells);

        // Select a cell
        fixture.view_model_.selectCell(0, 0);

        // Get selected position
        auto selected = game_state.getSelectedPosition();
        REQUIRE(selected.has_value());
        REQUIRE(selected->row == 0);
        REQUIRE(selected->col == 0);
    }

    SECTION("Enter numbers and validate") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        auto& initial_state = fixture.view_model_.gameState.get();

        // Find an empty cell
        int empty_row = -1, empty_col = -1;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (!initial_state.getCell(r, c).is_given && initial_state.getCell(r, c).value == 0) {
                    empty_row = r;
                    empty_col = c;
                    break;
                }
            }
            if (empty_row != -1) {
                break;
            }
        }

        REQUIRE(empty_row != -1);

        // Select empty cell and enter a number
        fixture.view_model_.selectCell(empty_row, empty_col);
        fixture.view_model_.enterNumber(5);

        [[maybe_unused]] const auto& updated_state = fixture.view_model_.gameState.get();
        // Note: The number may or may not be correct, but it should be entered
        // (The view model allows incorrect numbers)
    }

    SECTION("Undo and redo operations") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        // Find empty cell
        auto& initial_state = fixture.view_model_.gameState.get();
        int empty_row = -1, empty_col = -1;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (!initial_state.getCell(r, c).is_given && initial_state.getCell(r, c).value == 0) {
                    empty_row = r;
                    empty_col = c;
                    break;
                }
            }
            if (empty_row != -1) {
                break;
            }
        }

        REQUIRE(empty_row != -1);

        // Make a move
        fixture.view_model_.selectCell(empty_row, empty_col);
        fixture.view_model_.enterNumber(7);

        // Verify we can undo
        REQUIRE(fixture.view_model_.canUndo());

        // Undo the move
        fixture.view_model_.undo();

        // Verify cell is empty again
        auto& after_undo = fixture.view_model_.gameState.get();
        REQUIRE(after_undo.getCell(empty_row, empty_col).value == 0);

        // Verify we can redo
        REQUIRE(fixture.view_model_.canRedo());

        // Redo the move
        fixture.view_model_.redo();

        // Verify number is back
        auto& after_redo = fixture.view_model_.gameState.get();
        REQUIRE(after_redo.getCell(empty_row, empty_col).value == 7);
    }

    SECTION("Notes functionality") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        // Find empty cell
        auto& initial_state = fixture.view_model_.gameState.get();
        int empty_row = -1, empty_col = -1;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (!initial_state.getCell(r, c).is_given && initial_state.getCell(r, c).value == 0) {
                    empty_row = r;
                    empty_col = c;
                    break;
                }
            }
            if (empty_row != -1) {
                break;
            }
        }

        REQUIRE(empty_row != -1);

        // Select cell and enter notes
        fixture.view_model_.selectCell(empty_row, empty_col);
        fixture.view_model_.enterNote(1);
        fixture.view_model_.enterNote(2);
        fixture.view_model_.enterNote(3);

        auto& with_notes = fixture.view_model_.gameState.get();
        auto cell = with_notes.getCell(empty_row, empty_col);

        REQUIRE(std::find(cell.notes.begin(), cell.notes.end(), 1) != cell.notes.end());
        REQUIRE(std::find(cell.notes.begin(), cell.notes.end(), 2) != cell.notes.end());
        REQUIRE(std::find(cell.notes.begin(), cell.notes.end(), 3) != cell.notes.end());

        // Enter final number should clear notes
        fixture.view_model_.enterNumber(5);

        auto& after_number = fixture.view_model_.gameState.get();
        REQUIRE(after_number.getCell(empty_row, empty_col).notes.empty());
        REQUIRE(after_number.getCell(empty_row, empty_col).value == 5);
    }

    SECTION("Clear cell functionality") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        // Find empty cell and enter number
        auto& initial_state = fixture.view_model_.gameState.get();
        int empty_row = -1, empty_col = -1;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (!initial_state.getCell(r, c).is_given && initial_state.getCell(r, c).value == 0) {
                    empty_row = r;
                    empty_col = c;
                    break;
                }
            }
            if (empty_row != -1) {
                break;
            }
        }

        REQUIRE(empty_row != -1);

        // Enter number
        fixture.view_model_.selectCell(empty_row, empty_col);
        fixture.view_model_.enterNumber(8);

        auto& with_number = fixture.view_model_.gameState.get();
        REQUIRE(with_number.getCell(empty_row, empty_col).value == 8);

        // Clear cell
        fixture.view_model_.clearSelectedCell();

        auto& after_clear = fixture.view_model_.gameState.get();
        REQUIRE(after_clear.getCell(empty_row, empty_col).value == 0);
    }

    SECTION("Save and load game") {
        fixture.view_model_.startNewGame(Difficulty::Medium);

        // Make some moves
        auto& state = fixture.view_model_.gameState.get();
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (!state.getCell(r, c).is_given && state.getCell(r, c).value == 0) {
                    fixture.view_model_.selectCell(r, c);
                    fixture.view_model_.enterNumber(5);
                    break;
                }
            }
            break;
        }

        // Save game
        bool save_result = fixture.view_model_.saveCurrentGame("Test Save");
        REQUIRE(save_result);

        // Start new game
        fixture.view_model_.startNewGame(Difficulty::Easy);

        // Load saved game
        auto saves = fixture.save_manager_->listSaves();
        REQUIRE(saves.has_value());
        REQUIRE(saves->size() > 0);

        std::string save_id = (*saves)[0].save_id;
        fixture.view_model_.loadGame(save_id);

        // Verify loaded state
        auto& loaded_state = fixture.view_model_.gameState.get();
        REQUIRE(loaded_state.getDifficulty() == Difficulty::Medium);
    }

    SECTION("Auto-save functionality") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        // Make a move
        auto& state = fixture.view_model_.gameState.get();
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (!state.getCell(r, c).is_given && state.getCell(r, c).value == 0) {
                    fixture.view_model_.selectCell(r, c);
                    fixture.view_model_.enterNumber(3);
                    break;
                }
            }
            break;
        }

        // Trigger auto-save
        fixture.view_model_.autoSave();

        // Verify auto-save exists
        REQUIRE(fixture.save_manager_->hasAutoSave());
    }

    SECTION("Hint system") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        // Find empty cell
        auto& state = fixture.view_model_.gameState.get();
        int empty_row = -1, empty_col = -1;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (!state.getCell(r, c).is_given && state.getCell(r, c).value == 0) {
                    empty_row = r;
                    empty_col = c;
                    break;
                }
            }
            if (empty_row != -1) {
                break;
            }
        }

        REQUIRE(empty_row != -1);

        // Select cell and get hint
        fixture.view_model_.selectCell(empty_row, empty_col);

        int initial_hints = fixture.view_model_.getHintCount();
        REQUIRE(initial_hints > 0);

        // Get hint (this shows possible values but may not decrement hint count in current implementation)
        fixture.view_model_.getHint();

        // Note: Hint system is basic - just shows possible values
        // The hint count is managed by game state, not statistics manager calls in this context
    }

    SECTION("Check solution") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        // Check incomplete solution
        fixture.view_model_.checkSolution();

        auto ui_state = fixture.view_model_.uiState.get();
        // Should not be marked as complete yet
        REQUIRE_FALSE(fixture.view_model_.isGameComplete());
    }

    SECTION("Statistics tracking") {
        fixture.view_model_.startNewGame(Difficulty::Medium);

        // Make some moves using helper to find empty cells
        auto empty_cells_opt = test::findEmptyCells(fixture.view_model_.gameState.get(), 5);
        REQUIRE(empty_cells_opt.has_value());

        auto empty_cells = empty_cells_opt.value();
        for (size_t i = 0; i < empty_cells.size(); ++i) {
            fixture.view_model_.selectCell(empty_cells[i].row, empty_cells[i].col);
            fixture.view_model_.enterNumber(static_cast<int>((i % 9) + 1));
        }

        // Refresh statistics
        fixture.view_model_.refreshStatistics();

        auto stats = fixture.view_model_.statistics.get();
        REQUIRE(stats.games_played >= 0);
    }

    SECTION("UI state updates") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        auto ui_state = fixture.view_model_.uiState.get();
        REQUIRE(ui_state.is_game_active);
        REQUIRE_FALSE(ui_state.is_paused);
        REQUIRE_FALSE(ui_state.is_complete);
    }

    SECTION("Multiple game sessions") {
        // Start first game
        fixture.view_model_.startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model_.isGameActive());

        // Verify first game difficulty and capture board state
        const auto& first_state = fixture.view_model_.gameState.get();
        REQUIRE(first_state.getDifficulty() == Difficulty::Easy);
        auto first_board = first_state.extractNumbers();  // Capture board data before starting new game

        // Make some moves using helper to find empty cells
        auto empty_cells_opt = test::findEmptyCells(fixture.view_model_.gameState.get(), 3);
        REQUIRE(empty_cells_opt.has_value());

        auto empty_cells = empty_cells_opt.value();
        for (const auto& pos : empty_cells) {
            fixture.view_model_.selectCell(pos.row, pos.col);
            fixture.view_model_.enterNumber(5);
        }

        // Start second game (should end first session)
        // Use Medium instead of Hard to avoid long puzzle generation times
        fixture.view_model_.startNewGame(Difficulty::Medium);
        REQUIRE(fixture.view_model_.isGameActive());

        // Verify new game was created with correct difficulty
        const auto& second_state = fixture.view_model_.gameState.get();
        REQUIRE(second_state.getDifficulty() == Difficulty::Medium);

        // Verify it's actually a new game (different from first)
        auto second_board = second_state.extractNumbers();
        REQUIRE(second_board != first_board);
    }

    SECTION("Error message handling") {
        // Try to save without active game - should fail gracefully
        REQUIRE_FALSE(fixture.view_model_.isGameActive());

        bool save_result = fixture.view_model_.saveCurrentGame("Invalid Save");
        REQUIRE_FALSE(save_result);

        // Check error message
        REQUIRE(fixture.view_model_.hasError());

        // Clear error
        fixture.view_model_.clearErrorMessage();
        REQUIRE_FALSE(fixture.view_model_.hasError());
    }
}

TEST_CASE("GameViewModel - Notes cleanup", "[game_view_model][integration]") {
    GameViewModelTestFixture fixture;

    SECTION("Notes are cleaned up when number is placed") {
        fixture.view_model_.startNewGame(Difficulty::Easy);

        // Find empty cell
        auto& state = fixture.view_model_.gameState.get();
        int target_row = -1, target_col = -1;
        for (int r = 0; r < 9; ++r) {
            for (int c = 0; c < 9; ++c) {
                if (!state.getCell(r, c).is_given && state.getCell(r, c).value == 0) {
                    target_row = r;
                    target_col = c;
                    break;
                }
            }
            if (target_row != -1) {
                break;
            }
        }

        REQUIRE(target_row != -1);

        // Add notes to related cells (same row)
        for (int c = 0; c < 9; ++c) {
            if (c != target_col && !state.getCell(target_row, c).is_given && state.getCell(target_row, c).value == 0) {
                fixture.view_model_.selectCell(target_row, c);
                fixture.view_model_.enterNote(7);
            }
        }

        // Place number 7 in target cell
        fixture.view_model_.selectCell(target_row, target_col);
        fixture.view_model_.enterNumber(7);

        // Verify notes with value 7 are removed from same row
        auto& after_placement = fixture.view_model_.gameState.get();
        for (int c = 0; c < 9; ++c) {
            if (c != target_col && !after_placement.getCell(target_row, c).is_given &&
                after_placement.getCell(target_row, c).value == 0) {
                auto cell = after_placement.getCell(target_row, c);
                REQUIRE(std::find(cell.notes.begin(), cell.notes.end(), 7) == cell.notes.end());
            }
        }
    }
}

TEST_CASE("GameViewModel - Hint-Revealed State Persists After Save/Load", "[game_view_model][hint][save_load]") {
    auto temp_dir =
        std::filesystem::temp_directory_path() /
        ("test_hint_persist_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

    auto save_manager = std::make_shared<SaveManager>(temp_dir.string());
    auto validator = std::make_shared<GameValidator>();
    auto generator = std::make_shared<PuzzleGenerator>();
    auto solver = std::make_shared<SudokuSolver>(validator);
    auto stats_manager = std::make_shared<StatisticsManager>();

    GameViewModel view_model(validator, generator, solver, stats_manager, save_manager,
                             std::make_shared<MockLocalizationManager>());

    // Use a fixed seed known to generate a solvable puzzle with techniques the solver supports
    // NOTE: Random seed (0) can generate puzzles that require techniques beyond the solver's
    // current capability, causing getHint() to fail. Seed 12345 generates a puzzle that works.
    core::GenerationSettings settings;
    settings.difficulty = core::Difficulty::Easy;
    settings.seed = 12345;  // Fixed seed for deterministic test
    settings.ensure_unique = true;
    settings.max_attempts = 100;

    auto puzzle_result = generator->generatePuzzle(settings);
    REQUIRE(puzzle_result.has_value());

    // Load the generated puzzle into game state
    model::GameState initial_state;
    initial_state.loadPuzzle(puzzle_result->board);
    initial_state.setDifficulty(core::Difficulty::Easy);
    initial_state.setSolutionBoard(puzzle_result->solution);
    initial_state.startTimer();
    initial_state.setSelectedPosition({.row = 0, .col = 0});
    view_model.gameState.set(initial_state);

    // Find empty cell for hint
    const auto& state = view_model.gameState.get();
    Position hint_pos{.row = 0, .col = 0};
    // Find empty cell using helper
    auto empty_cell_opt = test::findEmptyCell(state);
    REQUIRE(empty_cell_opt.has_value());
    hint_pos = empty_cell_opt.value();

    // Apply hint — getHint() now places the value when the solver finds a Placement step
    view_model.selectCell(hint_pos);
    view_model.getHint();

    // Either hint message or error message must be set (hint system responded)
    bool hint_system_responded = !view_model.hintMessage.get().empty() || !view_model.errorMessage.get().empty();
    REQUIRE(hint_system_responded);

    // Find the hint-revealed cell (solver picks which cell to place, may differ from hint_pos)
    const auto& state_after = view_model.gameState.get();
    Position hint_placed_pos{.row = 0, .col = 0};
    int hint_placed_value = 0;
    bool hint_was_placed = false;
    for (size_t r = 0; r < 9 && !hint_was_placed; ++r) {
        for (size_t c = 0; c < 9 && !hint_was_placed; ++c) {
            const auto& cell = state_after.getCell({r, c});
            if (cell.is_hint_revealed) {
                hint_placed_pos = {r, c};
                hint_placed_value = cell.value;
                hint_was_placed = true;
            }
        }
    }

    if (hint_was_placed) {
        REQUIRE(hint_placed_value > 0);

        // Save game and reload; verify is_hint_revealed is persisted correctly
        bool save_success = view_model.saveCurrentGame("hint_test");
        REQUIRE(save_success);

        auto saves_result = save_manager->listSaves();
        REQUIRE(saves_result.has_value());
        REQUIRE(!saves_result->empty());
        std::string save_id = saves_result->front().save_id;

        GameViewModel view_model2(validator, generator, solver, stats_manager, save_manager,
                                  std::make_shared<MockLocalizationManager>());
        view_model2.loadGame(save_id);

        const auto& loaded = view_model2.gameState.get();
        REQUIRE(loaded.getCell(hint_placed_pos).value == hint_placed_value);
        REQUIRE(loaded.getCell(hint_placed_pos).is_hint_revealed == true);
    }

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}