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

using ResetTestFixture = sudoku::test::GameViewModelFixture;

TEST_CASE("GameViewModel - Reset Game", "[game_view_model][reset]") {
    ResetTestFixture fixture;

    SECTION("Reset clears user-placed numbers and preserves given cells") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Capture original puzzle (given numbers only)
        const auto& state_before = fixture.view_model->gameState.get();
        auto original_puzzle = state_before.extractGivenNumbers();

        // Find an empty cell and place a number
        auto empty_pos_opt = test::findEmptyCell(state_before);
        REQUIRE(empty_pos_opt.has_value());
        auto empty_pos = empty_pos_opt.value();

        fixture.view_model->enterNumber(empty_pos, 5);

        // Verify number was placed
        const auto& state_after_place = fixture.view_model->gameState.get();
        REQUIRE(state_after_place.getCell(empty_pos).value == 5);

        // Reset the game
        fixture.view_model->resetGame();

        // Verify user-placed number is gone
        const auto& state_after_reset = fixture.view_model->gameState.get();
        REQUIRE(state_after_reset.getCell(empty_pos).value == 0);

        // Verify given numbers are preserved
        auto reset_given = state_after_reset.extractGivenNumbers();
        REQUIRE(test::boardsEqual(original_puzzle, reset_given));
    }

    SECTION("Reset clears all notes") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell and add notes
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos_opt = test::findEmptyCell(state);
        REQUIRE(empty_pos_opt.has_value());
        auto empty_pos = empty_pos_opt.value();

        fixture.view_model->enterNote(empty_pos, 1);
        fixture.view_model->enterNote(empty_pos, 3);
        fixture.view_model->enterNote(empty_pos, 7);

        // Verify notes were added
        const auto& state_with_notes = fixture.view_model->gameState.get();
        REQUIRE_FALSE(state_with_notes.getCell(empty_pos).notes.empty());

        // Reset the game
        fixture.view_model->resetGame();

        // Verify notes are cleared
        const auto& state_after_reset = fixture.view_model->gameState.get();
        REQUIRE(state_after_reset.getCell(empty_pos).notes.empty());
    }

    SECTION("Reset resets timer to zero") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make a move to advance game state (timer is running)
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos_opt = test::findEmptyCell(state);
        REQUIRE(empty_pos_opt.has_value());
        fixture.view_model->enterNumber(empty_pos_opt.value(), 5);

        // Reset the game
        fixture.view_model->resetGame();

        // Timer should be running (game is active) but elapsed time should be near zero
        REQUIRE(fixture.view_model->isGameActive());
        const auto& state_after_reset = fixture.view_model->gameState.get();
        // Elapsed time should be very small (just started)
        auto elapsed = state_after_reset.getElapsedTime();
        REQUIRE(elapsed.count() < 1000);  // Less than 1 second
    }

    SECTION("Reset clears move history - undo and redo not possible") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make some moves
        const auto& state = fixture.view_model->gameState.get();
        auto empty_cells_opt = test::findEmptyCells(state, 2);
        REQUIRE(empty_cells_opt.has_value());
        auto empty_cells = empty_cells_opt.value();

        fixture.view_model->enterNumber(empty_cells[0], 5);
        fixture.view_model->enterNumber(empty_cells[1], 3);

        // Verify undo is possible
        REQUIRE(fixture.view_model->canUndo());

        // Reset the game
        fixture.view_model->resetGame();

        // Verify undo/redo are not possible
        REQUIRE_FALSE(fixture.view_model->canUndo());
        REQUIRE_FALSE(fixture.view_model->canRedo());
    }

    SECTION("Reset preserves difficulty level") {
        fixture.view_model->startNewGame(Difficulty::Hard);

        const auto& state_before = fixture.view_model->gameState.get();
        REQUIRE(state_before.getDifficulty() == Difficulty::Hard);

        // Reset the game
        fixture.view_model->resetGame();

        const auto& state_after = fixture.view_model->gameState.get();
        REQUIRE(state_after.getDifficulty() == Difficulty::Hard);
    }

    SECTION("Reset preserves solution board for hints") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state_before = fixture.view_model->gameState.get();
        REQUIRE(state_before.hasSolution());
        auto solution_before = state_before.getSolutionBoard();

        // Reset the game
        fixture.view_model->resetGame();

        const auto& state_after = fixture.view_model->gameState.get();
        REQUIRE(state_after.hasSolution());
        REQUIRE(test::boardsEqual(solution_before, state_after.getSolutionBoard()));
    }

    SECTION("Reset on inactive game is a no-op") {
        // Don't start a game — view model has no active game
        REQUIRE_FALSE(fixture.view_model->isGameActive());

        // Reset should not crash
        fixture.view_model->resetGame();

        // Still no active game
        REQUIRE_FALSE(fixture.view_model->isGameActive());
    }

    SECTION("Game is playable after reset") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make a move, then reset
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos_opt = test::findEmptyCell(state);
        REQUIRE(empty_pos_opt.has_value());
        fixture.view_model->enterNumber(empty_pos_opt.value(), 5);

        fixture.view_model->resetGame();

        // Game should still be active
        REQUIRE(fixture.view_model->isGameActive());

        // Should be able to place numbers after reset
        const auto& reset_state = fixture.view_model->gameState.get();
        auto new_empty_opt = test::findEmptyCell(reset_state);
        REQUIRE(new_empty_opt.has_value());
        fixture.view_model->enterNumber(new_empty_opt.value(), 7);

        const auto& after_play = fixture.view_model->gameState.get();
        REQUIRE(after_play.getCell(new_empty_opt.value()).value == 7);
    }

    SECTION("Reset ends previous stats session as abandoned") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make a move so the session has data
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos_opt = test::findEmptyCell(state);
        REQUIRE(empty_pos_opt.has_value());
        fixture.view_model->enterNumber(empty_pos_opt.value(), 5);

        // Reset — this should end the session as not completed
        fixture.view_model->resetGame();

        // Refresh statistics and verify the abandoned game is counted
        fixture.view_model->refreshStatistics();
        const auto& stats = fixture.view_model->statistics.get();
        // At least one game should be played (the abandoned one)
        REQUIRE(stats.games_played >= 1);
        // The abandoned game should NOT count as completed
        REQUIRE(stats.games_completed == 0);
    }

    SECTION("Reset gives fresh hint count") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Use a hint before reset
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos_opt = test::findEmptyCell(state);
        REQUIRE(empty_pos_opt.has_value());
        fixture.view_model->getHint(empty_pos_opt.value());

        int hints_before_reset = fixture.view_model->getHintCount();

        // Reset the game
        fixture.view_model->resetGame();

        // Hint count should be fully replenished (new session)
        int hints_after_reset = fixture.view_model->getHintCount();
        REQUIRE(hints_after_reset > hints_before_reset);
        REQUIRE(hints_after_reset == 10);
    }

    SECTION("Reset via executeCommand works") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Make a move
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos_opt = test::findEmptyCell(state);
        REQUIRE(empty_pos_opt.has_value());
        auto empty_pos = empty_pos_opt.value();
        fixture.view_model->enterNumber(empty_pos, 5);

        // Reset via command
        fixture.view_model->executeCommand(GameCommand::ResetGame);

        // Verify number was cleared
        const auto& after_reset = fixture.view_model->gameState.get();
        REQUIRE(after_reset.getCell(empty_pos).value == 0);
    }

    SECTION("canExecuteCommand for ResetGame requires active game") {
        // No active game
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ResetGame));

        // Start a game
        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ResetGame));
    }

    SECTION("Reset resets same puzzle - same given cells remain") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Capture the given cells pattern
        const auto& state_before = fixture.view_model->gameState.get();
        auto given_before = state_before.extractGivenNumbers();
        size_t given_count_before = 0;
        for (const auto& row : given_before) {
            for (int val : row) {
                if (val != 0) {
                    given_count_before++;
                }
            }
        }

        // Reset
        fixture.view_model->resetGame();

        // Same given cells
        const auto& state_after = fixture.view_model->gameState.get();
        auto given_after = state_after.extractGivenNumbers();
        REQUIRE(test::boardsEqual(given_before, given_after));

        // Same count of given cells
        size_t given_count_after = 0;
        for (const auto& row : given_after) {
            for (int val : row) {
                if (val != 0) {
                    given_count_after++;
                }
            }
        }
        REQUIRE(given_count_before == given_count_after);
    }
}
