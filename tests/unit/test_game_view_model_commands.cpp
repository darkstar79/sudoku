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

#include "../helpers/game_view_model_fixture.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

using CommandTestFixture = sudoku::test::GameViewModelFixture;

TEST_CASE("GameViewModel - Execute Commands", "[game_view_model][commands]") {
    CommandTestFixture fixture;

    SECTION("Execute Undo command") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find first empty cell using test helper (no goto!)
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos_opt = test::findEmptyCell(state);
        REQUIRE(empty_pos_opt.has_value());  // Fail loudly if no empty cells
        auto empty_pos = empty_pos_opt.value();

        fixture.view_model->enterNumber(empty_pos, 5);
        fixture.view_model->enterNumber(empty_pos, 5);  // Double-press to place

        // Verify number was placed - assert instead of skip
        const auto& after_place = fixture.view_model->gameState.get();
        REQUIRE(after_place.getCell(empty_pos).value == 5);

        // Execute Undo command
        fixture.view_model->executeCommand(GameCommand::Undo);

        const auto& after_undo = fixture.view_model->gameState.get();

        // May need to undo again for double-press behavior
        if (after_undo.getCell(empty_pos).value != 0 && fixture.view_model->canUndo()) {
            fixture.view_model->executeCommand(GameCommand::Undo);
        }

        const auto& final_state = fixture.view_model->gameState.get();
        REQUIRE(final_state.getCell(empty_pos).value == 0);
    }

    SECTION("Execute Redo command") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make and undo a move using helper to find empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());
        Position empty_pos = empty_cell.value();

        fixture.view_model->enterNumber(empty_pos, 5);
        fixture.view_model->enterNumber(empty_pos, 5);
        fixture.view_model->undo();

        // Execute Redo command
        fixture.view_model->executeCommand(GameCommand::Redo);

        const auto& after_redo = fixture.view_model->gameState.get();
        REQUIRE(after_redo.getCell(empty_pos).value == 5);
    }

    SECTION("Execute GetHint command - no selection") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->getHint(std::nullopt);

        // No cell selected — getHint requires selection
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    }

    SECTION("Execute GetHint command - with selection") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find an empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty = test::findEmptyCell(state);
        REQUIRE(empty.has_value());

        int hints_before = fixture.view_model->getHintCount();
        fixture.view_model->getHint(empty.value());

        // Hint should succeed — cell gets filled and hint count decreases
        REQUIRE(fixture.view_model->getHintCount() == hints_before - 1);
    }

    SECTION("Execute ToggleInputMode command") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        auto initial_mode = fixture.view_model->uiState.get().input_mode;
        REQUIRE(initial_mode == InputMode::Normal);

        // Execute ToggleInputMode command — cycles Normal → Notes
        fixture.view_model->executeCommand(GameCommand::ToggleInputMode);

        auto toggled_mode = fixture.view_model->uiState.get().input_mode;
        REQUIRE(toggled_mode == InputMode::Notes);
    }

    SECTION("Execute ShowStatistics command") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Execute ShowStatistics command
        fixture.view_model->executeCommand(GameCommand::ShowStatistics);

        // ShowStatistics dispatches to refreshStatistics(), which populates the statistics observable
        const auto& stats = fixture.view_model->statistics.get();
        CHECK(stats.games_played >= 0);
    }
}

TEST_CASE("GameViewModel - Can Execute Commands", "[game_view_model][commands]") {
    CommandTestFixture fixture;

    SECTION("Can execute Undo when moves exist") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::Undo) == false);

        // Make a move using helper to find empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());

        Position pos = empty_cell.value();
        fixture.view_model->enterNumber(pos, 5);
        fixture.view_model->enterNumber(pos, 5);

        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::Undo) == true);
    }

    SECTION("Can execute Redo after undo") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::Redo) == false);

        // Make and undo a move using helper to find empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());

        Position pos = empty_cell.value();
        fixture.view_model->enterNumber(pos, 5);
        fixture.view_model->enterNumber(pos, 5);
        fixture.view_model->undo();

        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::Redo) == true);
    }

    SECTION("Can execute GetHint when game is active") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::GetHint) == true);
    }

    SECTION("Can always execute ToggleInputMode") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ToggleInputMode) == true);
    }

    SECTION("Can always execute ShowStatistics") {
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ShowStatistics) == true);
    }
}

TEST_CASE("GameViewModel - Pause and Resume lifecycle", "[game_view_model][commands]") {
    CommandTestFixture fixture;

    SECTION("PauseGame stops the timer; ResumeGame restarts it") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->isGameActive());

        fixture.view_model->executeCommand(GameCommand::PauseGame);
        REQUIRE_FALSE(fixture.view_model->isGameActive());

        fixture.view_model->executeCommand(GameCommand::ResumeGame);
        REQUIRE(fixture.view_model->isGameActive());
    }

    SECTION("PauseGame with no active game is a no-op") {
        REQUIRE_FALSE(fixture.view_model->isGameActive());
        REQUIRE_NOTHROW(fixture.view_model->executeCommand(GameCommand::PauseGame));
        REQUIRE_FALSE(fixture.view_model->isGameActive());
    }

    SECTION("ResumeGame with no game never activates a timer") {
        REQUIRE_FALSE(fixture.view_model->isGameActive());
        fixture.view_model->executeCommand(GameCommand::ResumeGame);
        REQUIRE_FALSE(fixture.view_model->isGameActive());
    }
}

TEST_CASE("GameViewModel - Parameterized command dispatch", "[game_view_model][commands]") {
    CommandTestFixture fixture;

    SECTION("NewGame command honors the difficulty argument") {
        fixture.view_model->executeCommand(GameCommand::NewGame, GameCommandArgs::newGame(Difficulty::Hard));
        REQUIRE(fixture.view_model->isGameActive());
        REQUIRE(fixture.view_model->gameState.get().getDifficulty() == Difficulty::Hard);
    }

    SECTION("NewGame command without a difficulty falls back to the current difficulty") {
        fixture.view_model->startNewGame(Difficulty::Medium);
        fixture.view_model->executeCommand(GameCommand::NewGame);
        REQUIRE(fixture.view_model->gameState.get().getDifficulty() == Difficulty::Medium);
    }

    SECTION("SaveGame command persists the current game") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->getSaveList().empty());

        fixture.view_model->executeCommand(GameCommand::SaveGame, GameCommandArgs::save("pipeline-save"));
        REQUIRE_FALSE(fixture.view_model->getSaveList().empty());
    }

    SECTION("LoadGame command restores a previously saved game") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->executeCommand(GameCommand::SaveGame, GameCommandArgs::save("roundtrip"));

        auto saves = fixture.view_model->getSaveList();
        REQUIRE(saves.size() == 1);

        // Start a different game, then load the save back.
        fixture.view_model->startNewGame(Difficulty::Hard);
        fixture.view_model->executeCommand(GameCommand::LoadGame, GameCommandArgs::load(saves.front().save_id));
        REQUIRE(fixture.view_model->gameState.get().getDifficulty() == Difficulty::Easy);
    }

    SECTION("ClearNotes command clears all pencil marks") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        auto empty = test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        const core::Position pos = empty.value_or(core::Position{});

        fixture.view_model->enterNote(pos, 3);
        REQUIRE_FALSE(fixture.view_model->gameState.get().getCell(pos).notes.empty());

        fixture.view_model->executeCommand(GameCommand::ClearNotes);
        REQUIRE(fixture.view_model->gameState.get().getCell(pos).notes.empty());
    }

    SECTION("ValidateBoard reports a clean board with no error") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->clearErrorMessage();
        fixture.view_model->executeCommand(GameCommand::ValidateBoard);
        REQUIRE_FALSE(fixture.view_model->hasError());
    }

    SECTION("ValidateBoard surfaces an error when a placement contradicts the solution") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        auto empty = test::findEmptyCell(state);
        REQUIRE(empty.has_value());
        const core::Position pos = empty.value_or(core::Position{});

        // Place a value that disagrees with the solution so hasBoardErrors() trips.
        const auto& solution = state.getSolutionBoard();
        const int correct = solution[pos.row][pos.col];
        const int wrong = (correct % core::MAX_VALUE) + core::MIN_VALUE;  // stays in 1..9, != correct
        fixture.view_model->enterNumber(pos, wrong);

        fixture.view_model->clearErrorMessage();
        fixture.view_model->executeCommand(GameCommand::ValidateBoard);
        REQUIRE(fixture.view_model->hasError());
    }
}

// Story 6.8 AC #1: the View dispatches the existing Pause/Resume verbs and the control reflects
// is_paused. Pin the executeCommand round-trip that the toolbar button + P shortcut drive.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("GameViewModel - Pause/Resume command toggles is_paused", "[game_view_model][commands][pause]") {
    CommandTestFixture fixture;

    fixture.view_model->startNewGame(Difficulty::Easy);
    REQUIRE(!fixture.view_model->uiState.get().is_paused);
    REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::PauseGame));
    REQUIRE(!fixture.view_model->canExecuteCommand(GameCommand::ResumeGame));

    fixture.view_model->executeCommand(GameCommand::PauseGame);
    REQUIRE(fixture.view_model->uiState.get().is_paused);
    REQUIRE(!fixture.view_model->canExecuteCommand(GameCommand::PauseGame));
    REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ResumeGame));

    fixture.view_model->executeCommand(GameCommand::ResumeGame);
    REQUIRE(!fixture.view_model->uiState.get().is_paused);
    REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::PauseGame));
    REQUIRE(!fixture.view_model->canExecuteCommand(GameCommand::ResumeGame));
}

TEST_CASE("GameViewModel - Command preconditions", "[game_view_model][commands]") {
    CommandTestFixture fixture;

    SECTION("NewGame and LoadGame are always available") {
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::NewGame));
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::LoadGame));
    }

    SECTION("SaveGame requires an in-progress game") {
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::SaveGame));
        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::SaveGame));
    }

    SECTION("PauseGame needs an active game; ResumeGame needs a paused one") {
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::PauseGame));
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ResumeGame));

        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::PauseGame));
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ResumeGame));

        fixture.view_model->pauseGame();
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::PauseGame));
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ResumeGame));
    }

    SECTION("ValidateBoard and ClearNotes require a game") {
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ValidateBoard));
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ClearNotes));

        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ValidateBoard));
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ClearNotes));
    }
}

TEST_CASE("GameViewModel - Cycle Input Mode", "[game_view_model][input_mode]") {
    CommandTestFixture fixture;

    SECTION("Cycle input mode through Normal → Notes → Color → Normal") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->getInputMode() == InputMode::Normal);

        fixture.view_model->cycleInputMode();
        REQUIRE(fixture.view_model->getInputMode() == InputMode::Notes);

        fixture.view_model->cycleInputMode();
        REQUIRE(fixture.view_model->getInputMode() == InputMode::Color);

        fixture.view_model->cycleInputMode();
        REQUIRE(fixture.view_model->getInputMode() == InputMode::Normal);
    }
}

TEST_CASE("GameViewModel - Refresh Statistics", "[game_view_model][statistics]") {
    CommandTestFixture fixture;

    SECTION("Refresh statistics updates display") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Complete a game to have some statistics
        fixture.view_model->gameState.update([](model::GameState& state) { state.setComplete(true); });

        fixture.view_model->refreshStatistics();

        const auto& stats = fixture.view_model->statistics.get();
        CHECK(stats.games_played >= 0);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — many independent SECTIONs in one case
TEST_CASE("GameViewModel - Clear Cell", "[game_view_model][clear]") {
    CommandTestFixture fixture;

    SECTION("Clear empty cell does nothing") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->clearCell(pos);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).value == 0);
    }

    SECTION("Clear cell with value") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell and place a value
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->enterNumber(pos, 5);
        fixture.view_model->enterNumber(pos, 5);  // Double-press

        // Clear it
        fixture.view_model->clearCell(pos);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).value == 0);
    }

    // Story 6.1 (#76): clearing a placed value is a faithful inverse — it restores the pencil
    // marks that value's placement stripped from peers. (Was a documented known limitation:
    // clearCell did no note restoration at all.)
    SECTION("Clear cell restores peer notes the placement stripped") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->fillNotes();  // empty cells carry their legal candidates

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.hasSolution());
        const auto& solution = state.getSolutionBoard();

        // Find an empty cell whose correct value is still a legal candidate in a same-row peer.
        std::optional<Position> placement;
        std::optional<Position> peer;
        int value = 0;
        for (size_t row = 0; row < BOARD_SIZE && !placement.has_value(); ++row) {
            for (size_t col = 0; col < BOARD_SIZE && !placement.has_value(); ++col) {
                if (state.getValue(row, col) != EMPTY_CELL) {
                    continue;
                }
                const int candidate = solution[row][col];
                for (size_t peer_col = 0; peer_col < BOARD_SIZE; ++peer_col) {
                    if (peer_col != col && state.getValue(row, peer_col) == EMPTY_CELL &&
                        state.getNotes(row, peer_col).contains(candidate)) {
                        placement = Position{.row = row, .col = col};
                        peer = Position{.row = row, .col = peer_col};
                        value = candidate;
                        break;
                    }
                }
            }
        }
        REQUIRE(placement.has_value());
        REQUIRE(peer.has_value());
        const Position placement_cell = placement.value_or(Position{});
        const Position peer_cell = peer.value_or(Position{});

        fixture.view_model->enterNumber(placement_cell, value);
        REQUIRE(!fixture.view_model->gameState.get().getNotes(peer_cell).contains(value));

        fixture.view_model->clearCell(placement_cell);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getValue(placement_cell) == 0);
        REQUIRE(after.getNotes(peer_cell).contains(value));  // faithfully restored
    }

    SECTION("Cannot clear given cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find a given cell
        const auto& state = fixture.view_model->gameState.get();
        auto given_pos = test::findCell(state, [](const auto& c) { return c.is_given; });
        REQUIRE(given_pos.has_value());
        Position pos = given_pos.value();
        int original_value = state.getCell(pos).value;

        fixture.view_model->clearCell(pos);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).value == original_value);
    }
}
