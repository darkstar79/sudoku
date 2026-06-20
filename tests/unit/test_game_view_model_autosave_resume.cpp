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

// Story 6.5: an in-progress auto-save must reopen on restart. The corruption guards in
// restoreGameState used "empty move_history" as a corruption signal, but auto-saves deliberately
// omit history (SaveManager::autoSave sets include_history = false), so every in-progress auto-save
// was misclassified as corrupt and discarded. These tests pin the resume behavior and prove the fix
// does not disable corruption protection for *manual* saves (which do carry history).

#include "../helpers/game_view_model_fixture.h"
#include "../helpers/test_utils.h"

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

namespace {

// A valid, near-complete puzzle (unique solution). (0,0) is empty here; its correct value is 5.
// A freshly generated puzzle will not equal this exact board, so extractGivenNumbers() == kPuzzle
// is a reliable "resumed THIS puzzle (not a new game)" discriminator. Returned by value (not a
// namespace-scope object) to avoid a throwing static initializer (cert-err58-cpp).
BoardData makePuzzle() {
    return {{0, 3, 0, 6, 7, 8, 9, 1, 2}, {6, 0, 2, 1, 9, 5, 3, 4, 8}, {0, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 0, 1, 4, 2, 3}, {4, 2, 6, 0, 0, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 0, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 0}, {2, 8, 7, 4, 1, 9, 6, 0, 5}, {3, 4, 5, 2, 8, 6, 0, 7, 0}};
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Auto-save resume on restart",
          "[game_view_model][autosave][regression][bug-autosave-resume]") {
    sudoku::test::GameViewModelFixture fixture;
    const BoardData kPuzzle = makePuzzle();

    SECTION("AC#1: auto-save with a placed digit resumes (not discarded as a new game)") {
        SavedGame saved;
        saved.original_puzzle = kPuzzle;
        saved.current_state = kPuzzle;
        saved.current_state[0][0] = 5;  // user-placed digit → current_state != original_puzzle
        saved.difficulty = Difficulty::Easy;
        saved.is_auto_save = true;
        // move_history intentionally empty — this is how real auto-saves are written.

        fixture.view_model->restoreGameState(saved);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.extractGivenNumbers() == kPuzzle);  // resumed our puzzle, not a generated one
        REQUIRE(state.getValue(0, 0) == 5);               // the user's digit survived
        REQUIRE(!state.isGiven(0, 0));                    // it's a user value, not a clue
        REQUIRE(fixture.view_model->isGameActive());      // timer running, game playable
    }

    SECTION("AC#2: notes-only auto-save resumes (pencil marks preserved)") {
        SavedGame saved;
        saved.original_puzzle = kPuzzle;
        saved.current_state = kPuzzle;  // no digit placed → original_puzzle == current_state
        saved.notes[0][0].add(5);       // progress lives only in the notes
        saved.notes[0][0].add(7);
        saved.difficulty = Difficulty::Easy;
        saved.is_auto_save = true;

        fixture.view_model->restoreGameState(saved);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.extractGivenNumbers() == kPuzzle);  // resumed, not a new game
        REQUIRE(state.getNotes(0, 0).contains(5));
        REQUIRE(state.getNotes(0, 0).contains(7));
    }

    SECTION("AC#3a: a *manual* save with original==current + notes is still rejected as corrupt") {
        SavedGame saved;
        saved.original_puzzle = kPuzzle;
        saved.current_state = kPuzzle;  // no digit progress but...
        saved.notes[0][0].add(5);       // ...notes present → genuine "all-given" corruption signature
        saved.difficulty = Difficulty::Easy;
        saved.is_auto_save = false;  // manual save: empty/contradictory history IS a corruption tell

        fixture.view_model->restoreGameState(saved);

        // Guard fires → fresh game → the corrupt board is NOT resumed.
        REQUIRE(fixture.view_model->gameState.get().extractGivenNumbers() != kPuzzle);
    }

    SECTION("AC#3b: a *manual* save with user values but empty history is still rejected as corrupt") {
        SavedGame saved;
        saved.original_puzzle = kPuzzle;
        saved.current_state = kPuzzle;
        saved.current_state[0][0] = 5;  // user value present...
        saved.difficulty = Difficulty::Easy;
        saved.is_auto_save = false;  // ...but a manual save with NO move_history is corrupt.
        // move_history intentionally empty.

        fixture.view_model->restoreGameState(saved);

        REQUIRE(fixture.view_model->gameState.get().extractGivenNumbers() != kPuzzle);
    }

    SECTION("AC#7: round-trip through the real SaveManager auto-save path resumes") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        const auto pre_givens = fixture.view_model->gameState.get().extractGivenNumbers();

        auto empty = sudoku::test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        // has_value() block so clang-tidy's std::optional dataflow sees the access as guarded; the
        // REQUIRE above still fails loudly if a generated board somehow had no empty cell. (loaded
        // below is std::expected, which that check does not track, so it needs no such guard.)
        if (empty.has_value()) {
            const core::Position cell = empty.value();
            fixture.view_model->enterNote(cell, 5);  // progress recorded; digits unchanged
            fixture.view_model->autoSave();          // writes autosave.yaml (include_history = false)

            // Reload exactly as the app does on launch.
            auto loaded = fixture.save_manager->loadAutoSave();
            REQUIRE(loaded.has_value());
            REQUIRE(loaded.value().is_auto_save);

            fixture.view_model->restoreGameState(loaded.value());

            const auto& state = fixture.view_model->gameState.get();
            REQUIRE(state.extractGivenNumbers() == pre_givens);  // same puzzle reopened
            REQUIRE(state.getNotes(cell).contains(5));           // pencil mark survived round-trip
        }
    }
}
