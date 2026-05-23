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

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;
using namespace sudoku::model;

namespace {

using HintTestFixture = test::GameViewModelFixture;

/// Find a cell that became hint-revealed between two game state snapshots.
[[nodiscard]] auto findNewHintCell(const GameState& before, const GameState& after) -> std::optional<Position> {
    for (size_t row = 0; row < BOARD_SIZE; ++row) {
        for (size_t col = 0; col < BOARD_SIZE; ++col) {
            Position pos{row, col};
            if (!before.getCell(pos).is_hint_revealed && after.getCell(pos).is_hint_revealed) {
                return pos;
            }
        }
    }
    return std::nullopt;
}

}  // anonymous namespace

TEST_CASE("Hint Revelation - Basic Functionality", "[hint][revelation]") {
    HintTestFixture fixture;

    SECTION("Hint reveals correct solution value") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        const auto& solution = state.getSolutionBoard();

        // Find any empty cell to pass to getHint()
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());

        const auto before = fixture.view_model->gameState.get();
        fixture.view_model->getHint(empty_cell.value());
        const auto& after = fixture.view_model->gameState.get();

        // Find where the hint was actually placed (solver chooses position)
        auto hint_pos = findNewHintCell(before, after);
        REQUIRE(hint_pos.has_value());

        const auto& cell = after.getCell(hint_pos.value());
        int expected_value = solution[hint_pos->row][hint_pos->col];

        REQUIRE(cell.value == expected_value);
        REQUIRE(cell.is_hint_revealed == true);
        REQUIRE(cell.notes.empty());
    }

    SECTION("Hint count decrements after hint usage") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        int initial_hints = fixture.view_model->getHintCount();
        REQUIRE(initial_hints == 10);

        // Find and select empty cell using helper
        const auto& state = fixture.view_model->gameState.get();
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());

        fixture.view_model->getHint(empty_cell.value());

        int remaining_hints = fixture.view_model->getHintCount();
        REQUIRE(remaining_hints == 9);
    }

    SECTION("Multiple hints decrement count correctly") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Re-find an empty cell each time since hints may fill previously-found cells
        for (int i = 0; i < 3; ++i) {
            const auto& current = fixture.view_model->gameState.get();
            auto empty = test::findEmptyCell(current);
            REQUIRE(empty.has_value());
            fixture.view_model->getHint(empty.value());
        }

        REQUIRE(fixture.view_model->getHintCount() == 7);
    }

    SECTION("Hint clears existing notes on target cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();

        // Add notes to several empty cells so the solver's target likely has notes
        auto empty_cells_opt = test::findEmptyCells(state, 5);
        REQUIRE(empty_cells_opt.has_value());
        for (const auto& pos : empty_cells_opt.value()) {
            fixture.view_model->enterNote(pos, 1);
            fixture.view_model->enterNote(pos, 2);
        }

        // Get hint on an empty cell
        const auto before = fixture.view_model->gameState.get();
        fixture.view_model->getHint(empty_cells_opt.value().front());
        const auto& after = fixture.view_model->gameState.get();

        // The hint-revealed cell should have its notes cleared
        auto hint_pos = findNewHintCell(before, after);
        REQUIRE(hint_pos.has_value());
        REQUIRE(after.getCell(hint_pos.value()).notes.empty());
        REQUIRE(after.getCell(hint_pos.value()).value != 0);
    }

    SECTION("Solution board is available after game start") {
        fixture.view_model->startNewGame(Difficulty::Medium);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.hasSolution());

        const auto& solution = state.getSolutionBoard();
        REQUIRE(solution.size() == BOARD_SIZE);

        // Verify solution is valid (all cells filled)
        for (const auto& row : solution) {
            for (int val : row) {
                REQUIRE(val >= MIN_VALUE);
                REQUIRE(val <= MAX_VALUE);
            }
        }
    }
}

TEST_CASE("Hint Revelation - Error Handling", "[hint][error]") {
    HintTestFixture fixture;

    SECTION("Error when no cell selected") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->getHint(std::nullopt);

        const auto& error = fixture.view_model->errorMessage.get();
        REQUIRE(error == "Please select a cell first");
    }

    SECTION("Error when trying to hint a given cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();

        // Find a given cell
        auto given_pos_opt = test::findCell(state, [](const auto& c) { return c.is_given; });
        REQUIRE(given_pos_opt.has_value());
        Position given_pos = given_pos_opt.value();

        fixture.view_model->getHint(given_pos);

        const auto& error = fixture.view_model->errorMessage.get();
        REQUIRE(error == "Cannot reveal hint for given cells");
    }

    SECTION("Error when trying to hint an already filled cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();

        // Find empty cell using helper, fill it, then try to hint
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());
        Position pos = empty_cell.value();

        fixture.view_model->enterNumber(pos, 5);  // Any number
        fixture.view_model->getHint(pos);

        const auto& error = fixture.view_model->errorMessage.get();
        REQUIRE(error == "Cell already has a value");
    }

    SECTION("Hint respects maximum limit of 10") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Use all 10 hints, re-finding empty cells each time
        for (int i = 0; i < 10; ++i) {
            const auto& current = fixture.view_model->gameState.get();
            auto empty = test::findEmptyCell(current);
            REQUIRE(empty.has_value());
            fixture.view_model->getHint(empty.value());
        }

        REQUIRE(fixture.view_model->getHintCount() == 0);

        // Try to use 11th hint - should fail
        const auto& state = fixture.view_model->gameState.get();
        auto empty = test::findEmptyCell(state);
        if (empty.has_value()) {
            fixture.view_model->getHint(empty.value());
            REQUIRE(fixture.view_model->getHintCount() == 0);
        }
    }
}

TEST_CASE("Hint Revelation - Integration with Statistics", "[hint][statistics]") {
    HintTestFixture fixture;

    SECTION("Hint usage is tracked in statistics") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();

        // Use a hint on first empty cell
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());

        fixture.view_model->getHint(empty_cell.value());

        // Check that hint count reflects statistics
        REQUIRE(fixture.view_model->getHintCount() == 9);
    }

    SECTION("Hint count persists across game sessions") {
        // Start first game and use hints
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Use 2 hints in first game
        for (int i = 0; i < 2; ++i) {
            const auto& current = fixture.view_model->gameState.get();
            auto empty = test::findEmptyCell(current);
            REQUIRE(empty.has_value());
            fixture.view_model->getHint(empty.value());
        }

        REQUIRE(fixture.view_model->getHintCount() == 8);

        // Start new game - hints should reset to 10
        fixture.view_model->startNewGame(Difficulty::Medium);
        REQUIRE(fixture.view_model->getHintCount() == 10);
    }
}

TEST_CASE("Hint Revelation - Cell State Tracking", "[hint][cell_state]") {
    HintTestFixture fixture;

    SECTION("Hint-revealed cells are marked correctly") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());

        const auto before = fixture.view_model->gameState.get();
        fixture.view_model->getHint(empty_cell.value());
        const auto& after = fixture.view_model->gameState.get();

        auto hint_pos = findNewHintCell(before, after);
        REQUIRE(hint_pos.has_value());

        const auto& cell = after.getCell(hint_pos.value());
        REQUIRE(cell.is_hint_revealed == true);
        REQUIRE(cell.is_given == false);
    }

    SECTION("User-entered cells are not marked as hint-revealed") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();

        // Find empty cell and enter number manually
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->enterNumber(pos, 5);

        const auto& updated_state = fixture.view_model->gameState.get();
        const auto& cell = updated_state.getCell(pos);

        REQUIRE(cell.is_hint_revealed == false);
    }

    SECTION("Given cells are never marked as hint-revealed") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();

        // Check all given cells
        for (size_t row = 0; row < BOARD_SIZE; ++row) {
            for (size_t col = 0; col < BOARD_SIZE; ++col) {
                const auto& cell = state.getCell({row, col});
                if (cell.is_given) {
                    REQUIRE(cell.is_hint_revealed == false);
                }
            }
        }
    }
}

TEST_CASE("Hint Revelation - Move Counting", "[hint][moves]") {
    HintTestFixture fixture;

    SECTION("Hint counts as a move") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        int initial_moves = state.getMoveCount();

        // Use hint on first empty cell
        auto empty_cell = test::findEmptyCell(state);
        REQUIRE(empty_cell.has_value());

        fixture.view_model->getHint(empty_cell.value());

        const auto& updated_state = fixture.view_model->gameState.get();
        REQUIRE(updated_state.getMoveCount() == initial_moves + 1);
    }
}

TEST_CASE("Hint Revelation - Undo Behavior", "[hint][undo]") {
    HintTestFixture fixture;

    SECTION("Undo skips over hints and undoes previous user move") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        auto empty_cells_opt = test::findEmptyCells(state, 2);
        REQUIRE(empty_cells_opt.has_value());

        auto empty_cells = empty_cells_opt.value();
        Position user_pos = empty_cells[0];

        // User enters number 5
        fixture.view_model->enterNumber(user_pos, 5);

        const auto& after_user = fixture.view_model->gameState.get();
        REQUIRE(after_user.getCell(user_pos).value == 5);
        REQUIRE(after_user.getCell(user_pos).is_hint_revealed == false);

        // Use hint on another empty cell
        int hints_before = fixture.view_model->getHintCount();
        const auto before_hint = fixture.view_model->gameState.get();
        fixture.view_model->getHint(empty_cells[1]);
        int hints_after = fixture.view_model->getHintCount();

        REQUIRE(hints_after == hints_before - 1);  // Hint consumed

        // Find where the hint was actually placed
        const auto& after_hint = fixture.view_model->gameState.get();
        auto actual_hint_pos = findNewHintCell(before_hint, after_hint);
        REQUIRE(actual_hint_pos.has_value());

        int hint_value = after_hint.getCell(actual_hint_pos.value()).value;
        REQUIRE(hint_value != 0);
        REQUIRE(after_hint.getCell(actual_hint_pos.value()).is_hint_revealed == true);

        // Undo - should skip hint and undo user's move
        fixture.view_model->undo();

        const auto& after_undo = fixture.view_model->gameState.get();

        // Hint should still be there (not undone)
        REQUIRE(after_undo.getCell(actual_hint_pos.value()).value == hint_value);
        REQUIRE(after_undo.getCell(actual_hint_pos.value()).is_hint_revealed == true);

        // User's move should be undone
        REQUIRE(after_undo.getCell(user_pos).value == 0);

        // Hint count unchanged (hint still consumed)
        REQUIRE(fixture.view_model->getHintCount() == hints_after);
    }

    SECTION("Hints not consumed on validation errors - no selection") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        int hints_initial = fixture.view_model->getHintCount();

        // Try hint with no selection
        fixture.view_model->getHint(std::nullopt);

        // Hint should NOT be consumed
        REQUIRE(fixture.view_model->getHintCount() == hints_initial);

        const auto& error = fixture.view_model->errorMessage.get();
        REQUIRE(error == "Please select a cell first");
    }

    SECTION("Hints not consumed on validation errors - given cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        int hints_initial = fixture.view_model->getHintCount();

        // Try hint on given cell
        const auto& state = fixture.view_model->gameState.get();
        auto given_pos = test::findCell(state, [](const auto& c) { return c.is_given; });
        REQUIRE(given_pos.has_value());

        fixture.view_model->getHint(given_pos.value());

        // Hint should NOT be consumed
        REQUIRE(fixture.view_model->getHintCount() == hints_initial);

        const auto& error = fixture.view_model->errorMessage.get();
        REQUIRE(error == "Cannot reveal hint for given cells");
    }

    SECTION("Hints not consumed on validation errors - filled cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        int hints_initial = fixture.view_model->getHintCount();

        // Find empty cell and fill it
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->enterNumber(pos, 5);

        // Now try to get hint on filled cell
        fixture.view_model->getHint(pos);

        // Hint should NOT be consumed
        REQUIRE(fixture.view_model->getHintCount() == hints_initial);

        const auto& error = fixture.view_model->errorMessage.get();
        REQUIRE(error == "Cell already has a value");
    }

    SECTION("Multiple undo skips all hints") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // User move 1
        auto user_cell = test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(user_cell.has_value());
        Position user_pos = user_cell.value();
        fixture.view_model->enterNumber(user_pos, 1);

        // Hint 1 — re-find empty cell, track actual placement
        auto empty1 = test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty1.has_value());
        const auto before_hint1 = fixture.view_model->gameState.get();
        fixture.view_model->getHint(empty1.value());
        const auto& after_hint1 = fixture.view_model->gameState.get();
        auto hint1_pos = findNewHintCell(before_hint1, after_hint1);
        REQUIRE(hint1_pos.has_value());
        int hint1_value = after_hint1.getCell(hint1_pos.value()).value;

        // Hint 2 — re-find empty cell, track actual placement
        auto empty2 = test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty2.has_value());
        const auto before_hint2 = fixture.view_model->gameState.get();
        fixture.view_model->getHint(empty2.value());
        const auto& after_hint2 = fixture.view_model->gameState.get();
        auto hint2_pos = findNewHintCell(before_hint2, after_hint2);
        REQUIRE(hint2_pos.has_value());
        int hint2_value = after_hint2.getCell(hint2_pos.value()).value;

        // Undo - should skip both hints and undo user move 1
        fixture.view_model->undo();

        const auto& after_undo = fixture.view_model->gameState.get();

        // Both hints should remain
        REQUIRE(after_undo.getCell(hint1_pos.value()).value == hint1_value);
        REQUIRE(after_undo.getCell(hint1_pos.value()).is_hint_revealed == true);
        REQUIRE(after_undo.getCell(hint2_pos.value()).value == hint2_value);
        REQUIRE(after_undo.getCell(hint2_pos.value()).is_hint_revealed == true);

        // User move should be undone
        REQUIRE(after_undo.getCell(user_pos).value == 0);
    }
}
