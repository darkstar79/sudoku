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

#include <optional>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

using OverrideFixture = sudoku::test::GameViewModelFixture;

namespace {
// Find a given (locked) cell on the freshly generated board.
std::optional<Position> findGivenCell(const model::GameState& state) {
    for (size_t r = 0; r < BOARD_SIZE; ++r) {
        for (size_t c = 0; c < BOARD_SIZE; ++c) {
            if (state.getCell(r, c).is_given) {
                return Position{.row = r, .col = c};
            }
        }
    }
    return std::nullopt;
}
}  // namespace

// handleNumberInput with an explicit layer override is the net-new ViewModel entry point for
// story 0a.2: Ctrl→value, Shift→pencil, Alt→color, regardless of the active InputMode.
TEST_CASE("GameViewModel - Modifier override routing", "[game_view_model][input_mode][overrides]") {
    OverrideFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);
    auto& vm = *fixture.view_model;

    auto empty_opt = test::findEmptyCell(vm.gameState.get());
    REQUIRE(empty_opt.has_value());
    const auto pos = empty_opt.value();

    SECTION("Ctrl override forces a final value while in Notes mode (AC-2)") {
        vm.setInputMode(InputMode::Notes);

        vm.handleNumberInput(pos, 5, InputMode::Normal);

        REQUIRE(vm.gameState.get().getCell(pos).value == 5);
    }

    SECTION("Ctrl override re-press on the same value clears it (toggle-off, AC-2)") {
        vm.setInputMode(InputMode::Color);

        vm.handleNumberInput(pos, 4, InputMode::Normal);
        REQUIRE(vm.gameState.get().getCell(pos).value == 4);

        vm.handleNumberInput(pos, 4, InputMode::Normal);
        REQUIRE(vm.gameState.get().getCell(pos).value == 0);
    }

    SECTION("Shift override toggles a pencil mark while in Normal mode (AC-3)") {
        vm.setInputMode(InputMode::Normal);

        vm.handleNumberInput(pos, 3, InputMode::Notes);
        REQUIRE(vm.gameState.get().getCell(pos).notes.contains(3));

        vm.handleNumberInput(pos, 3, InputMode::Notes);
        REQUIRE_FALSE(vm.gameState.get().getCell(pos).notes.contains(3));
    }

    SECTION("Shift override on a filled cell is a no-op (empty-cell guard, AC-3)") {
        vm.handleNumberInput(pos, 5, InputMode::Normal);  // place a value first
        REQUIRE(vm.gameState.get().getCell(pos).value == 5);

        vm.handleNumberInput(pos, 2, InputMode::Notes);  // pencil override on a filled cell

        REQUIRE_FALSE(vm.gameState.get().getCell(pos).notes.contains(2));
    }

    SECTION("Alt override applies, replaces, and toggles off a color (AC-4)") {
        vm.setInputMode(InputMode::Normal);

        vm.handleNumberInput(pos, 3, InputMode::Color);
        REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 3);

        // A different digit replaces the color.
        vm.handleNumberInput(pos, 4, InputMode::Color);
        REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 4);

        // The same digit again removes it.
        vm.handleNumberInput(pos, 4, InputMode::Color);
        REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 0);
    }

    SECTION("Alt override for digits 7-9 is a silent no-op (palette is 1-6, AC-4)") {
        vm.handleNumberInput(pos, 7, InputMode::Color);
        REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 0);
        vm.handleNumberInput(pos, 9, InputMode::Color);
        REQUIRE(vm.gameState.get().getCellColor(pos.row, pos.col) == 0);
    }

    SECTION("No override defaults to the active mode (AC-1 regression-safe)") {
        vm.setInputMode(InputMode::Notes);
        vm.handleNumberInput(pos, 6, std::nullopt);
        REQUIRE(vm.gameState.get().getCell(pos).notes.contains(6));
        REQUIRE(vm.gameState.get().getCell(pos).value == 0);
    }

    SECTION("Overrides are no-ops on a locked given cell (AC-9)") {
        auto given_opt = findGivenCell(vm.gameState.get());
        REQUIRE(given_opt.has_value());
        const auto given_pos = given_opt.value();
        const int given_value = vm.gameState.get().getCell(given_pos).value;

        vm.handleNumberInput(given_pos, (given_value % 9) + 1, InputMode::Normal);

        REQUIRE(vm.gameState.get().getCell(given_pos).value == given_value);
    }
}

TEST_CASE("GameViewModel - clearCellNotes clears only pencil marks", "[game_view_model][overrides]") {
    OverrideFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);
    auto& vm = *fixture.view_model;

    auto empty_opt = test::findEmptyCell(vm.gameState.get());
    REQUIRE(empty_opt.has_value());
    const auto pos = empty_opt.value();

    SECTION("clearCellNotes removes every pencil mark in the cell (Shift+Delete, AC-5)") {
        vm.enterNote(pos, 2);
        vm.enterNote(pos, 5);
        vm.enterNote(pos, 8);
        REQUIRE(vm.gameState.get().getCell(pos).notes.contains(2));

        vm.clearCellNotes(pos);

        const auto notes = vm.gameState.get().getCell(pos).notes;
        REQUIRE_FALSE(notes.contains(2));
        REQUIRE_FALSE(notes.contains(5));
        REQUIRE_FALSE(notes.contains(8));
    }
}

// In EditGivens the modifier is meaningless, not the keystroke: an override is stripped and the key
// acts plain (so Ctrl+8/Alt+8 set given 8, matching the already-plain Ctrl+Delete given-clear).
// This is the UX ruling (2026-06-04) replacing the earlier "override is a hard no-op" behaviour.
TEST_CASE("GameViewModel - EditGivens strips modifier overrides and acts plain",
          "[game_view_model][edit_mode][overrides]") {
    OverrideFixture fixture;
    fixture.view_model->enterEditMode();
    auto& vm = *fixture.view_model;
    const Position pos{.row = 3, .col = 5};

    SECTION("A value override (Ctrl) in EditGivens sets the given, ignoring the modifier") {
        vm.handleNumberInput(pos, 7, InputMode::Normal);  // Ctrl+7

        REQUIRE(vm.gameState.get().getValue(pos) == 7);
        REQUIRE(vm.gameState.get().isGiven(pos));
    }

    SECTION("A color override (Alt) in EditGivens also sets the given, then toggles it off") {
        vm.handleNumberInput(pos, 7, InputMode::Color);  // Alt+7 → set given 7
        REQUIRE(vm.gameState.get().getValue(pos) == 7);
        REQUIRE(vm.gameState.get().isGiven(pos));

        vm.handleNumberInput(pos, 7, InputMode::Color);  // same digit again clears the given
        REQUIRE(vm.gameState.get().getValue(pos) == 0);
        REQUIRE_FALSE(vm.gameState.get().isGiven(pos));
    }
}
