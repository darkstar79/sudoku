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
#include "core/board_utils.h"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

// Test fixture for fill-notes tests, extends shared GameViewModelFixture
struct FillNotesFixture {
    test::GameViewModelFixture base;

    void startGame(Difficulty difficulty = Difficulty::Easy) {
        base.view_model->startNewGame(difficulty);
    }

    [[nodiscard]] size_t countTotalNotes() const {
        size_t count = 0;
        const auto& state = base.view_model->gameState.get();
        core::forEachCell(
            [&](size_t row, size_t col) { count += static_cast<size_t>(state.getCell(row, col).notes.count()); });
        return count;
    }

    [[nodiscard]] bool allNotesMatchPossibleValues() const {
        const auto& state = base.view_model->gameState.get();
        auto board = state.extractNumbers();
        bool all_match = true;
        core::forEachCell([&](size_t row, size_t col) {
            const auto& cell = state.getCell(row, col);
            auto expected = base.validator->getPossibleValues(board, {.row = row, .col = col});
            core::CellNotes expected_notes;
            for (int v : expected) {
                expected_notes.add(v);
            }
            if (cell.notes != expected_notes) {
                all_match = false;
            }
        });
        return all_match;
    }
};

TEST_CASE("GameViewModel - fillNotes fills candidates for all empty cells", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();

    REQUIRE(f.countTotalNotes() == 0);

    f.base.view_model->fillNotes();

    REQUIRE(f.countTotalNotes() > 0);
    REQUIRE(f.allNotesMatchPossibleValues());
}

TEST_CASE("GameViewModel - fillNotes does not set notes on given or filled cells", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();
    f.base.view_model->fillNotes();

    const auto& state = f.base.view_model->gameState.get();
    bool any_given_has_notes = false;
    core::forEachCell([&](size_t row, size_t col) {
        const auto& cell = state.getCell(row, col);
        if ((cell.is_given || cell.value > 0) && !cell.notes.empty()) {
            any_given_has_notes = true;
        }
    });
    REQUIRE_FALSE(any_given_has_notes);
}

TEST_CASE("GameViewModel - fillNotes updates after placing a number", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();
    f.base.view_model->fillNotes();

    size_t notes_before = f.countTotalNotes();

    // Place a correct number
    const auto& state = f.base.view_model->gameState.get();
    auto empty_opt = test::findEmptyCell(state);
    REQUIRE(empty_opt.has_value());
    auto pos = empty_opt.value();
    auto solution = state.getSolutionBoard();
    int correct_value = solution[pos.row][pos.col];

    f.base.view_model->enterNumber(pos, correct_value);

    // Fill again — should reflect the new board state
    f.base.view_model->fillNotes();

    REQUIRE(f.allNotesMatchPossibleValues());
    REQUIRE(f.countTotalNotes() < notes_before);
}

TEST_CASE("GameViewModel - manual notes work independently of fillNotes", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();

    const auto& state = f.base.view_model->gameState.get();
    auto empty_opt = test::findEmptyCell(state);
    REQUIRE(empty_opt.has_value());
    auto pos = empty_opt.value();

    // Manual note entry works
    f.base.view_model->enterNote(pos, 5);

    auto notes = f.base.view_model->gameState.get().getCell(pos).notes;
    REQUIRE(std::find(notes.begin(), notes.end(), 5) != notes.end());

    // fillNotes overwrites manual notes with computed values
    f.base.view_model->fillNotes();
    REQUIRE(f.allNotesMatchPossibleValues());
}

TEST_CASE("GameViewModel - fillNotes toggles between fill and clear", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();

    // First press fills notes
    f.base.view_model->fillNotes();
    REQUIRE(f.countTotalNotes() > 0);
    REQUIRE(f.base.view_model->uiState.get().notes_filled);

    // Second press clears all notes
    f.base.view_model->fillNotes();
    REQUIRE(f.countTotalNotes() == 0);
    REQUIRE_FALSE(f.base.view_model->uiState.get().notes_filled);

    // Third press fills again
    f.base.view_model->fillNotes();
    REQUIRE(f.countTotalNotes() > 0);
    REQUIRE(f.base.view_model->uiState.get().notes_filled);
}

TEST_CASE("GameViewModel - placing number resets notes_filled toggle", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();
    f.base.view_model->fillNotes();
    REQUIRE(f.base.view_model->uiState.get().notes_filled);

    // Place a number — toggle resets so next press re-fills
    const auto& state = f.base.view_model->gameState.get();
    auto empty_opt = test::findEmptyCell(state);
    REQUIRE(empty_opt.has_value());
    auto pos = empty_opt.value();
    auto solution = state.getSolutionBoard();
    f.base.view_model->enterNumber(pos, solution[pos.row][pos.col]);

    REQUIRE_FALSE(f.base.view_model->uiState.get().notes_filled);

    // Next fillNotes re-fills (not clears)
    f.base.view_model->fillNotes();
    REQUIRE(f.countTotalNotes() > 0);
    REQUIRE(f.allNotesMatchPossibleValues());
}

TEST_CASE("GameViewModel - fillNotes is one-shot, not persistent", "[game_view_model][fill_notes]") {
    FillNotesFixture f;
    f.startGame();
    f.base.view_model->fillNotes();

    REQUIRE(f.countTotalNotes() > 0);

    // Place a number — notes are NOT auto-recomputed (only cleanup runs)
    const auto& state = f.base.view_model->gameState.get();
    auto empty_opt = test::findEmptyCell(state);
    REQUIRE(empty_opt.has_value());
    auto pos = empty_opt.value();
    auto solution = state.getSolutionBoard();

    f.base.view_model->enterNumber(pos, solution[pos.row][pos.col]);

    // Notes were cleaned up (conflicting removed) but not fully recomputed
    // Some cells may have stale notes — this is expected (one-shot behavior)
    // The placed cell should have no notes
    REQUIRE(f.base.view_model->gameState.get().getCell(pos).notes.empty());
}
