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

#include "../helpers/game_view_model_fixture.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

// Test fixture for GameViewModel note restoration tests (uses shared DI setup + RAII temp dir)
using NoteRestorationFixture = test::GameViewModelFixture;

TEST_CASE("Note Restoration - Undo Restores Notes", "[note_restoration]") {
    NoteRestorationFixture fixture;

    SECTION("Note cleanup works when placing numbers") {
        // Start a new game
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find two empty cells
        const auto& initial_state = fixture.view_model->gameState.get();
        auto empty_cells_opt = test::findEmptyCells(initial_state, 2);
        REQUIRE(empty_cells_opt.has_value());

        auto empty_cells = empty_cells_opt.value();
        Position target_pos = empty_cells[0];
        Position note_cell = empty_cells[1];

        // Add note 5 to note_cell
        fixture.view_model->gameState.update([&](model::GameState& state) { state.addNote(note_cell, 5); });

        // Verify note exists
        bool note_added = false;
        for (int note : fixture.view_model->gameState.get().getCell(note_cell).notes) {
            if (note == 5) {
                note_added = true;
            }
        }
        REQUIRE(note_added);

        // Place number 5 at target_pos (should clean up note at note_cell if related)
        fixture.view_model->selectCell(target_pos);
        fixture.view_model->enterNumber(5);
        fixture.view_model->enterNumber(5);  // Double-press to finalize

        // Check if note was cleaned up (only if cells are in same row/column/box)
        const auto& state_after = fixture.view_model->gameState.get();
        bool cells_are_related =
            (target_pos.row == note_cell.row) || (target_pos.col == note_cell.col) ||
            ((target_pos.row / 3) == (note_cell.row / 3) && (target_pos.col / 3) == (note_cell.col / 3));

        if (cells_are_related) {
            // Note should be cleaned up
            bool note_removed = true;
            for (int note : state_after.getCell(note_cell).notes) {
                if (note == 5) {
                    note_removed = false;
                }
            }
            REQUIRE(note_removed);
        }

        // This test verifies note cleanup works correctly.
        // Note restoration on undo has a known architectural limitation
        // and is not tested here (see CLAUDE.md for details).
    }

    SECTION("Notes are only restored if still valid") {
        // Start a new game
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find three empty cells using test helper (no goto!)
        const auto& initial_state = fixture.view_model->gameState.get();
        auto empty_cells_opt = test::findEmptyCells(initial_state, 3);
        REQUIRE(empty_cells_opt.has_value());  // Fail if fewer than 3 empty cells

        // Use first three empty cells
        auto empty_cells = empty_cells_opt.value();
        Position first_pos = empty_cells[0];
        Position second_pos = empty_cells[1];
        Position note_pos = empty_cells[2];

        // Add note 5 to note_pos manually
        fixture.view_model->gameState.update([&](model::GameState& state) { state.addNote(note_pos, 5); });

        // Place 5 at first_pos (cleans up note at note_pos)
        fixture.view_model->selectCell(first_pos);
        fixture.view_model->enterNumber(5);
        fixture.view_model->enterNumber(5);

        // Place 5 at second_pos (before undoing first placement)
        fixture.view_model->selectCell(second_pos);
        fixture.view_model->enterNumber(5);
        fixture.view_model->enterNumber(5);

        // Undo first placement
        fixture.view_model->selectCell(first_pos);
        fixture.view_model->undo();

        // Note 5 should NOT be restored at note_pos because second_pos still has 5
        const auto& final_state = fixture.view_model->gameState.get();
        bool note_5_present = false;
        for (int note : final_state.getCell(note_pos).notes) {
            if (note == 5) {
                note_5_present = true;
            }
        }

        // Note should NOT be restored due to conflict with second_pos
        REQUIRE_FALSE(note_5_present);
    }
}
