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

using EdgeCaseTestFixture = sudoku::test::GameViewModelFixture;

TEST_CASE("GameViewModel - Hint Edge Cases", "[game_view_model][hint]") {
    EdgeCaseTestFixture fixture;

    SECTION("Get hint without selection sets error") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->getHint(std::nullopt);

        // No cell selected — getHint requires selection
        REQUIRE_FALSE(fixture.view_model->errorMessage.get().empty());
    }

    SECTION("Get hint with empty cell selected succeeds") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        auto empty = test::findEmptyCell(state);
        REQUIRE(empty.has_value());

        int hints_before = fixture.view_model->getHintCount();
        fixture.view_model->getHint(empty.value());

        REQUIRE(fixture.view_model->getHintCount() == hints_before - 1);
        REQUIRE(fixture.view_model->errorMessage.get().empty());
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

        // Request hint on a filled cell — triggers "cell already has a value" error
        fixture.view_model->getHint(Position{.row = 0, .col = 0});

        CHECK(!fixture.view_model->errorMessage.get().empty());
    }
}

TEST_CASE("GameViewModel - Enter Number Edge Cases", "[game_view_model][enter]") {
    EdgeCaseTestFixture fixture;

    SECTION("Enter number on given cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find a given cell
        const auto& state = fixture.view_model->gameState.get();
        auto given_pos = test::findCell(state, [](const auto& c) { return c.is_given; });
        REQUIRE(given_pos.has_value());
        Position pos = given_pos.value();
        int original_value = state.getCell(pos).value;

        fixture.view_model->enterNumber(pos, 9);
        fixture.view_model->enterNumber(pos, 9);  // Double-press

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).value == original_value);
    }

    SECTION("Enter invalid number (0)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->enterNumber(pos, 0);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).value == 0);
    }

    SECTION("Enter invalid number (10)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->enterNumber(pos, 10);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).value == 0);
    }
}

TEST_CASE("GameViewModel - Enter Note Edge Cases", "[game_view_model][note]") {
    EdgeCaseTestFixture fixture;

    SECTION("Enter note on given cell") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find a given cell
        const auto& state = fixture.view_model->gameState.get();
        auto given_pos = test::findCell(state, [](const auto& c) { return c.is_given; });
        REQUIRE(given_pos.has_value());
        Position pos = given_pos.value();

        fixture.view_model->enterNote(pos, 5);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).notes.empty());
    }

    SECTION("Enter note on cell with value") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell and place value
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->enterNumber(pos, 5);
        fixture.view_model->enterNumber(pos, 5);  // Double-press

        // Try to add note - should not work
        fixture.view_model->enterNote(pos, 7);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).notes.empty());
    }

    SECTION("Enter invalid note (0)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->enterNote(pos, 0);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).notes.empty());
    }

    SECTION("Enter invalid note (10)") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        // Find empty cell
        const auto& state = fixture.view_model->gameState.get();
        auto empty_pos = test::findEmptyCell(state);
        REQUIRE(empty_pos.has_value());
        Position pos = empty_pos.value();

        fixture.view_model->enterNote(pos, 10);

        const auto& after = fixture.view_model->gameState.get();
        REQUIRE(after.getCell(pos).notes.empty());
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

    // BL-14a: recordMove must clamp last_valid_state_index_ when the redo tail is truncated;
    // otherwise the tracked index dangles past the new array and undoToLastValid silently
    // no-ops while telling the user the board was reverted.
    SECTION("Undo to last valid after redo-branch truncation actually reverts the wrong move") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.hasSolution());
        const auto& solution = state.getSolutionBoard();

        // Three correct placements in distinct empty cells → last_valid_state_index_ = 2.
        auto empties_opt = test::findEmptyCells(state, 4);
        REQUIRE(empties_opt.has_value());
        const auto& empties = *empties_opt;
        const auto& p0 = empties[0];
        const auto& p1 = empties[1];
        const auto& p2 = empties[2];
        const auto& p_wrong = empties[3];

        fixture.view_model->enterNumber(p0, solution[p0.row][p0.col]);
        fixture.view_model->enterNumber(p1, solution[p1.row][p1.col]);
        fixture.view_model->enterNumber(p2, solution[p2.row][p2.col]);

        // Roll back two moves, then place a value at a fresh cell that does NOT match the
        // solution. Pick "solution + 1 (mod 9, +1)" to guarantee an error vs the solution.
        fixture.view_model->undo();
        fixture.view_model->undo();

        const int correct = solution[p_wrong.row][p_wrong.col];
        const int wrong = (correct % MAX_VALUE) + MIN_VALUE;  // always 1..9 and != correct
        REQUIRE(wrong != correct);
        fixture.view_model->enterNumber(p_wrong, wrong);

        // Sanity: wrong value is on the board before we ask for revert.
        REQUIRE(fixture.view_model->gameState.get().getValue(p_wrong) == wrong);

        fixture.view_model->undoToLastValid();

        // BL-14a regression: without the clamp, last_valid_state_index_ still points into
        // the truncated tail (was 2, now beyond size-1). The loop condition becomes false
        // and the wrong move is left on the board.
        CHECK(fixture.view_model->gameState.get().getValue(p_wrong) == 0);
        // The original correct placement at p0 must still be on the board.
        CHECK(fixture.view_model->gameState.get().getValue(p0) == solution[p0.row][p0.col]);
    }

    // BL-14b: undoToLastValid must land EXACTLY at last_valid_state_index_ without help from
    // undo()'s hint-skipping. This test exercises the wrong-move-after-undo path (same setup
    // as the BL-14a test) and additionally verifies the redo state — if undoToLastValid
    // overshot, redo would not be available because move_history_index_ would be below the
    // truncation point.
    SECTION("Undo to last valid leaves the surviving correct prefix redoable") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.hasSolution());
        const auto& solution = state.getSolutionBoard();

        auto empties_opt = test::findEmptyCells(state, 3);
        REQUIRE(empties_opt.has_value());
        const auto& empties = *empties_opt;
        const auto& p0 = empties[0];
        const auto& p1 = empties[1];
        const auto& p_wrong = empties[2];

        fixture.view_model->enterNumber(p0, solution[p0.row][p0.col]);
        fixture.view_model->enterNumber(p1, solution[p1.row][p1.col]);
        fixture.view_model->undo();  // move_history_index_ now points at p0

        const int correct = solution[p_wrong.row][p_wrong.col];
        const int wrong = (correct % MAX_VALUE) + MIN_VALUE;
        REQUIRE(wrong != correct);
        fixture.view_model->enterNumber(p_wrong, wrong);

        REQUIRE(fixture.view_model->canRedo() == false);  // sanity: tail was truncated

        fixture.view_model->undoToLastValid();

        // After reverting the wrong move, we should be sitting at p0 (the surviving valid
        // state), with p_wrong cleared and redo of the wrong move available.
        CHECK(fixture.view_model->gameState.get().getValue(p_wrong) == 0);
        CHECK(fixture.view_model->gameState.get().getValue(p0) == solution[p0.row][p0.col]);
        CHECK(fixture.view_model->canRedo());  // wrong move still in history at index+1
    }
}

// Story 6.2 / GH #77 (premise closed 2026-06-14): "undo to last valid" stays digit-only — a
// notes-level contradiction is NOT a board error and must never drive a (destructive) rewind.
// This guard locks the decision: no notes state, contradictory or not, may change undoToLastValid's
// behaviour. Once 6.1 (#76) stops corrupting notes, the original complaint cannot even arise.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("undoToLastValid stays digit-only regardless of notes (#77 guard)",
          "[game_view_model][undo][regression][bug-77-undo][guard]") {
    EdgeCaseTestFixture fixture;

    SECTION("An impossible notes state does not trigger a rewind on a digit-valid board") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.hasSolution());
        const auto& solution = state.getSolutionBoard();

        // Two correct placements → a digit-valid board with a recorded last-valid state.
        auto empties_opt = test::findEmptyCells(state, 3);
        REQUIRE(empties_opt.has_value());
        const auto empties = empties_opt.value_or(std::vector<Position>{});
        const Position p0 = empties[0];
        const Position p1 = empties[1];
        const Position noted = empties[2];

        fixture.view_model->enterNumber(p0, solution[p0.row][p0.col]);
        fixture.view_model->enterNumber(p1, solution[p1.row][p1.col]);

        // Plant a deliberately impossible notes state: an empty non-given cell with NO candidates.
        fixture.view_model->gameState.update([&](model::GameState& s) { s.setNotes(noted, core::CellNotes{}); });

        // A notes-level contradiction must NOT register as a board error — that is the half of the
        // invariant that keeps undoToLastValid digit-only (asserted before AND after the call).
        CHECK(!fixture.view_model->hasBoardErrors());

        fixture.view_model->undoToLastValid();

        // The board is digit-valid, so nothing is rewound — both correct placements survive.
        CHECK(fixture.view_model->gameState.get().getValue(p0) == solution[p0.row][p0.col]);
        CHECK(fixture.view_model->gameState.get().getValue(p1) == solution[p1.row][p1.col]);
        CHECK(!fixture.view_model->hasBoardErrors());
    }
}

// BL-13: updateUIState() built a fresh UIState every call, carrying forward only input_mode
// and notes_filled. show_conflicts, show_hints, and status_message were silently reset to
// their defaults on every keystroke / undo / completion check.
TEST_CASE("GameViewModel - UIState sticky-field preservation (BL-13)", "[game_view_model][uistate]") {
    EdgeCaseTestFixture fixture;

    SECTION("setShowConflicts(false) survives the next move") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->setShowConflicts(false);
        REQUIRE(fixture.view_model->uiState.get().show_conflicts == false);

        const auto& state = fixture.view_model->gameState.get();
        auto empty = test::findEmptyCell(state);
        REQUIRE(empty.has_value());
        fixture.view_model->enterNumber(*empty, 5);

        // Regression: pre-fix this resets to the default `true`.
        CHECK(fixture.view_model->uiState.get().show_conflicts == false);
    }

    SECTION("setShowHints(false) survives the next move") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        fixture.view_model->setShowHints(false);
        REQUIRE(fixture.view_model->uiState.get().show_hints == false);

        const auto& state = fixture.view_model->gameState.get();
        auto empty = test::findEmptyCell(state);
        REQUIRE(empty.has_value());
        fixture.view_model->enterNumber(*empty, 5);

        CHECK(fixture.view_model->uiState.get().show_hints == false);
    }

    SECTION("status_message survives a subsequent updateUIState trigger") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        const std::string msg = "stick-around-please";
        fixture.view_model->uiState.update([&msg](auto& ui) { ui.status_message = msg; });
        REQUIRE(fixture.view_model->uiState.get().status_message == msg);

        // Trigger updateUIState() via a public API (enterNumber calls it at the end).
        const auto& state = fixture.view_model->gameState.get();
        auto empty = test::findEmptyCell(state);
        REQUIRE(empty.has_value());
        fixture.view_model->enterNumber(*empty, 5);

        CHECK(fixture.view_model->uiState.get().status_message == msg);
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

        // autoSave() early-returns when no game is active; verify no error was set
        CHECK(fixture.view_model->errorMessage.get().empty());
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

        const auto& stats = fixture.view_model->statistics.get();
        CHECK(stats.games_played >= 0);
    }
}
