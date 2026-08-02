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

// Story 8.16: a game resumed from a save was a second-class citizen on two paths, both of which
// assumed data that only a *generated* game carries.
//   * D1 — resetGame() read GameState::getSolutionBoard() with no hasSolution() gate. A restored
//     game never carries a solution board (restoreGameState rebuilds from original_puzzle + overlay
//     and the save format has no solution field), so Reset threw std::bad_optional_access out of a
//     Qt slot on the DEFAULT launch path: play → quit → relaunch (auto-save resumes) → Reset.
//   * D2 — saveCurrentGame() copied the in-memory move_history_, which is empty for a game resumed
//     from an auto-save (SaveManager::autoSave writes include_history = false by design, story 6.5).
//     The resulting MANUAL save carried user values with no move log, which is exactly the
//     phantom-value signature isCorruptedManualSave() rejects — so reloading it silently replaced
//     the player's board with a freshly generated puzzle.
// These cases pin the parity contract: Reset and Save behave on a resumed game the way they behave
// on a generated one.

#include "../../src/core/puzzle_analyzer.h"
#include "../../src/core/solution_counter.h"
#include "../helpers/game_view_model_fixture.h"
#include "../helpers/test_utils.h"
#include "core/board_utils.h"

#include <chrono>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

namespace {

/// Default hint budget when no ISettingsManager is injected (the fixture injects none).
/// Mirrors kDefaultMaxHints in the ViewModel.
constexpr int kMaxHints = 10;

/// A valid, near-complete puzzle with a unique solution. Empty cells: (0,0), (1,1), (2,0), (4,3),
/// (4,4), (5,5), (6,8), (7,7), (8,6), (8,8). A freshly generated board will not equal this one, so
/// `extractGivenNumbers() == kPuzzle` is a reliable "resumed THIS puzzle" discriminator.
/// Same board as test_game_view_model_restore_stats.cpp / test_game_view_model_autosave_resume.cpp.
BoardData makePuzzle() {
    return {{0, 3, 0, 6, 7, 8, 9, 1, 2}, {6, 0, 2, 1, 9, 5, 3, 4, 8}, {0, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 0, 1, 4, 2, 3}, {4, 2, 6, 0, 0, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 0, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 0}, {2, 8, 7, 4, 1, 9, 6, 0, 5}, {3, 4, 5, 2, 8, 6, 0, 7, 0}};
}

/// Auto-save shaped like the ones SaveManager::autoSave actually writes: no move history.
/// This is the shape the app resumes from on every launch.
SavedGame makeAutoSave(const BoardData& puzzle) {
    SavedGame saved;
    saved.original_puzzle = puzzle;
    saved.current_state = puzzle;
    saved.difficulty = Difficulty::Easy;
    saved.is_auto_save = true;
    return saved;
}

/// An auto-save carrying real progress: a placed digit, pencil marks, play time and spent counters.
SavedGame makeAutoSaveWithProgress(const BoardData& puzzle) {
    SavedGame saved = makeAutoSave(puzzle);
    saved.current_state[0][0] = 5;  // user-placed digit on an empty cell → not a given
    saved.notes[1][1].add(4);       // pencil marks on another empty cell
    saved.elapsed_time = std::chrono::seconds(90);
    saved.hints_used = 3;
    saved.mistakes = 2;
    return saved;
}

}  // namespace

// ============================================================================
// D1 — Reset on a restored game (AC1, AC2, AC3, AC4)
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Reset on a restored game", "[game_view_model][reset][regression][bug-reset-no-solution]") {
    sudoku::test::GameViewModelFixture fixture;
    const BoardData kPuzzle = makePuzzle();
    REQUIRE(fixture.validator->validateBoard(kPuzzle));  // fixture precondition

    SECTION("AC1: resetGame() on a game resumed from an auto-save does not throw") {
        fixture.view_model->restoreGameState(makeAutoSave(kPuzzle));
        REQUIRE(fixture.view_model->gameState.get().extractGivenNumbers() == kPuzzle);  // really resumed
        REQUIRE(!fixture.view_model->gameState.get().hasSolution());                    // the precondition this pins
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ResetGame));         // the button IS enabled

        REQUIRE_NOTHROW(fixture.view_model->resetGame());
    }

    SECTION("AC2: reset returns a restored game to the saved puzzle with a clean slate") {
        fixture.view_model->restoreGameState(makeAutoSaveWithProgress(kPuzzle));
        REQUIRE(fixture.view_model->gameState.get().getValue(0, 0) == 5);  // progress is really there
        REQUIRE(fixture.view_model->getMistakeCount() == 2);
        REQUIRE(fixture.view_model->getHintCount() == kMaxHints - 3);
        // Place a digit so there IS an undo entry to clear. A restored game starts with an empty
        // log, so asserting !canUndo() after the reset would otherwise hold before it too and
        // pin nothing.
        fixture.view_model->enterNumber({.row = 2, .col = 0}, 1);
        REQUIRE(fixture.view_model->canUndo());

        fixture.view_model->resetGame();

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.extractNumbers() == kPuzzle);                // user digit gone, clues intact
        REQUIRE(state.extractGivenNumbers() == kPuzzle);           // still THIS puzzle, not a new one
        REQUIRE(state.getNotes(1, 1).empty());                     // pencil marks cleared
        REQUIRE(fixture.view_model->getMistakeCount() == 0);       // counter reset
        REQUIRE(fixture.view_model->getHintCount() == kMaxHints);  // fresh, unseeded stats session
        REQUIRE(!fixture.view_model->canUndo());                   // move history cleared
        REQUIRE(!fixture.view_model->canRedo());
        REQUIRE(fixture.view_model->isGameActive());  // and the game is playable again
        // Elapsed time is compared against the SEEDED 90 s rather than an absolute "< 1 s" bound:
        // the VM builds its GameState on the real SystemTimeProvider (no injection point at this
        // layer), so an absolute bound would be a wall-clock assertion a loaded runner could cross.
        // "Did not inherit the restored clock" is what the reset actually promises here.
        REQUIRE(state.getElapsedTime() < std::chrono::seconds(90));
    }

    SECTION("AC3: reset does not invent a solution board for a restored game") {
        fixture.view_model->restoreGameState(makeAutoSaveWithProgress(kPuzzle));
        REQUIRE(!fixture.view_model->gameState.get().hasSolution());

        fixture.view_model->resetGame();

        // Solution-less games are a supported shape. Reset must not run the solver to manufacture
        // one — that would change hasSolution() semantics that story 6.8's hasPuzzle() work
        // deliberately separated, and put a solver run on the reset path.
        REQUIRE(!fixture.view_model->gameState.get().hasSolution());
    }

    SECTION("AC4: mistake detection still falls back to the conflict check after the reset") {
        fixture.view_model->restoreGameState(makeAutoSaveWithProgress(kPuzzle));
        fixture.view_model->resetGame();
        REQUIRE(fixture.view_model->getMistakeCount() == 0);

        // (0,0) is empty and row 0 already holds a 3 at (0,1) — a duplicate the conflict-only
        // fallback in enterNumber must still catch with no solution board to compare against.
        fixture.view_model->enterNumber({.row = 0, .col = 0}, 3);

        REQUIRE(fixture.view_model->gameState.get().getValue(0, 0) == 3);  // the move was applied
        REQUIRE(fixture.view_model->getMistakeCount() == 1);               // ...and counted as a mistake
    }
}

// Edit-mode (custom-puzzle) games hit the same nullopt: commitEditedPuzzle builds its GameState via
// loadPuzzle and never calls setSolutionBoard. Needs its own fixture because the shared
// GameViewModelFixture injects no IPuzzleAnalyzer, and commit is a no-op without one.
namespace {

struct EditModeResetFixture {
    sudoku::test::TempTestDir temp_dir;
    std::shared_ptr<IGameValidator> validator = std::make_shared<GameValidator>();
    std::shared_ptr<IPuzzleGenerator> generator = std::make_shared<PuzzleGenerator>();
    std::shared_ptr<ISudokuSolver> solver = std::make_shared<SudokuSolver>(validator);
    std::shared_ptr<SolutionCounter> counter = std::make_shared<SolutionCounter>();
    std::shared_ptr<IStatisticsManager> stats_manager = std::make_shared<StatisticsManager>(temp_dir.path());
    std::shared_ptr<ISaveManager> save_manager = std::make_shared<SaveManager>(temp_dir.path());
    std::shared_ptr<IPuzzleAnalyzer> analyzer = std::make_shared<PuzzleAnalyzer>(validator, solver, counter);
    std::unique_ptr<GameViewModel> view_model = std::make_unique<GameViewModel>(
        validator, generator, solver, stats_manager, save_manager, /*settings_manager*/ nullptr, analyzer);
};

}  // namespace

TEST_CASE("GameViewModel - Reset on an edit-mode game", "[game_view_model][reset][regression][bug-reset-no-solution]") {
    EditModeResetFixture fixture;
    const BoardData kPuzzle = makePuzzle();

    fixture.view_model->enterEditMode();
    core::forEachCell([&](size_t row, size_t col) {
        if (kPuzzle[row][col] != 0) {
            fixture.view_model->setEditModeGiven({.row = row, .col = col}, kPuzzle[row][col]);
        }
    });
    fixture.view_model->commitEditedPuzzle();

    REQUIRE(fixture.view_model->getInputMode() == InputMode::Normal);  // commit accepted
    REQUIRE(!fixture.view_model->gameState.get().hasSolution());       // the precondition this pins

    REQUIRE_NOTHROW(fixture.view_model->resetGame());

    REQUIRE(fixture.view_model->gameState.get().extractGivenNumbers() == kPuzzle);
    REQUIRE(!fixture.view_model->gameState.get().hasSolution());
}

// ============================================================================
// The corruption guard rejects a save whose original_puzzle has no empty cells. That shape must
// therefore be unreachable, or the rejection is itself a data-loss path — a completely filled grid
// passes both validation and uniqueness (a full board has exactly one solution), so before these
// guards it could be imported or committed, saved, and then silently replaced on reload.
// ============================================================================

namespace {

/// A valid, completely filled grid — a legal Sudoku solution, but not a puzzle.
BoardData makeCompleteGrid() {
    return {{1, 2, 3, 4, 5, 6, 7, 8, 9}, {4, 5, 6, 7, 8, 9, 1, 2, 3}, {7, 8, 9, 1, 2, 3, 4, 5, 6},
            {2, 3, 1, 5, 6, 4, 8, 9, 7}, {5, 6, 4, 8, 9, 7, 2, 3, 1}, {8, 9, 7, 2, 3, 1, 5, 6, 4},
            {3, 1, 2, 6, 4, 5, 9, 7, 8}, {6, 4, 5, 9, 7, 8, 3, 1, 2}, {9, 7, 8, 3, 1, 2, 6, 4, 5}};
}

std::string toPuzzleString(const BoardData& board) {
    std::string out;
    core::forEachCell([&](size_t row, size_t col) { out += static_cast<char>('0' + board[row][col]); });
    return out;
}

}  // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - A puzzle with no empty cells is refused at the source",
          "[game_view_model][edit_mode][import][regression][bug-reset-no-solution]") {
    const BoardData kComplete = makeCompleteGrid();

    SECTION("edit-mode commit refuses a completely filled board") {
        EditModeResetFixture fixture;
        REQUIRE(fixture.validator->validateBoard(kComplete));  // it IS a legal grid — that is the trap

        fixture.view_model->enterEditMode();
        core::forEachCell([&](size_t row, size_t col) {
            fixture.view_model->setEditModeGiven({.row = row, .col = col}, kComplete[row][col]);
        });
        fixture.view_model->commitEditedPuzzle();

        // Rejected, with a message, and still in edit mode so the user can remove a digit.
        REQUIRE(fixture.view_model->getInputMode() == InputMode::EditGivens);
        REQUIRE(!fixture.view_model->errorMessage.get().empty());
    }

    SECTION("import refuses a completely filled board") {
        EditModeResetFixture fixture;

        fixture.view_model->importPuzzleFromString(toPuzzleString(kComplete));

        REQUIRE(!fixture.view_model->errorMessage.get().empty());
        REQUIRE(fixture.view_model->gameState.get().extractGivenNumbers() != kComplete);  // not adopted
    }

    SECTION("a puzzle with a single empty cell is still accepted") {
        // The boundary: the guard must reject only "nothing to solve", not "almost nothing".
        EditModeResetFixture fixture;
        BoardData nearly = kComplete;
        nearly[0][0] = 0;

        fixture.view_model->enterEditMode();
        core::forEachCell([&](size_t row, size_t col) {
            if (nearly[row][col] != core::EMPTY_CELL) {
                fixture.view_model->setEditModeGiven({.row = row, .col = col}, nearly[row][col]);
            }
        });
        fixture.view_model->commitEditedPuzzle();

        REQUIRE(fixture.view_model->getInputMode() == InputMode::Normal);  // accepted
        REQUIRE(fixture.view_model->gameState.get().extractGivenNumbers() == nearly);
    }
}

// ============================================================================
// D2 — A manual save made from a resumed game must reload (AC9)
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Manual save of a resumed game round-trips",
          "[game_view_model][save][regression][bug-resumed-manual-save]") {
    sudoku::test::GameViewModelFixture fixture;
    const BoardData kPuzzle = makePuzzle();
    REQUIRE(fixture.validator->validateBoard(kPuzzle));  // fixture precondition

    SECTION("AC9: the real quit → relaunch → Save Game → Load Game path returns the same game") {
        // End-to-end through the actual SaveManager, hand-setting nothing: this is the reproduction
        // as a player hits it. Every other case in this file builds a SavedGame in memory, which
        // cannot prove that the flag the fix relies on is written and read on the real path.
        fixture.view_model->startNewGame(Difficulty::Easy);
        const auto pre_givens = fixture.view_model->gameState.get().extractGivenNumbers();
        auto empty = sudoku::test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        if (empty.has_value()) {
            const core::Position cell = empty.value();
            fixture.view_model->enterNumber(cell, 5);  // progress before quitting
            fixture.view_model->autoSave();            // closeEvent's exit auto-save

            auto resumed = fixture.save_manager->loadAutoSave();  // relaunch resumes it (main.cpp)
            REQUIRE(resumed.has_value());
            REQUIRE(resumed->move_history.empty());  // auto-saves carry no log, by design (6.5)
            REQUIRE(!resumed->history_complete);     // ...and now say so
            fixture.view_model->restoreGameState(*resumed);
            REQUIRE(fixture.view_model->gameState.get().getValue(cell) == 5);

            REQUIRE(fixture.view_model->saveCurrentGame("saved-after-resume"));

            auto listed = fixture.save_manager->listSaves();
            REQUIRE(listed.has_value());
            REQUIRE(listed->size() == 1);
            fixture.view_model->loadGame(listed->front().save_id);

            const auto& state = fixture.view_model->gameState.get();
            REQUIRE(state.extractGivenNumbers() == pre_givens);  // NOT a freshly generated puzzle
            REQUIRE(state.getValue(cell) == 5);                  // the player's digit survived
        }
    }

    SECTION("AC11: the history_complete key records who can be judged by the corruption heuristic") {
        // Pins the two production values the D2 fix keys off, so no other case has to hand-set the
        // flag on faith. An auto-save drops the log (include_history = false) and is therefore never
        // a complete account; a manual save of a game played from its own start is.
        fixture.view_model->startNewGame(Difficulty::Easy);
        auto empty = sudoku::test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        if (empty.has_value()) {
            fixture.view_model->enterNumber(empty.value(), 5);
        }

        fixture.view_model->autoSave();
        auto auto_saved = fixture.save_manager->loadAutoSave();
        REQUIRE(auto_saved.has_value());
        REQUIRE(!auto_saved->history_complete);

        REQUIRE(fixture.view_model->saveCurrentGame("played-from-the-start"));
        auto listed = fixture.save_manager->listSaves();
        REQUIRE(listed.has_value());
        REQUIRE(listed->size() == 1);
        auto manual = fixture.save_manager->loadGame(listed->front().save_id);
        REQUIRE(manual.has_value());
        REQUIRE(manual->history_complete);
    }

    SECTION("AC9: save → load of a game resumed from an auto-save returns the same game") {
        // history_complete = false is what the serializer writes for every auto-save (pinned by the
        // AC11 section above); this in-memory fixture states it directly so the case can also carry
        // the seeded counters an end-to-end setup cannot produce as precisely.
        SavedGame resumed = makeAutoSaveWithProgress(kPuzzle);
        resumed.history_complete = false;
        fixture.view_model->restoreGameState(resumed);

        REQUIRE(fixture.view_model->saveCurrentGame("resumed-manual"));

        auto listed = fixture.save_manager->listSaves();
        REQUIRE(listed.has_value());
        REQUIRE(listed->size() == 1);
        const std::string save_id = listed->front().save_id;
        // The file itself has always been written correctly — only the reload discarded it.
        REQUIRE(listed->front().hints_used == 3);
        REQUIRE(listed->front().elapsed_time >= std::chrono::seconds(90));

        fixture.view_model->loadGame(save_id);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.extractGivenNumbers() == kPuzzle);               // NOT a freshly generated puzzle
        REQUIRE(state.getValue(0, 0) == 5);                            // the user's digit survived
        REQUIRE(!state.isGiven(0, 0));                                 // ...as a user value, not a clue
        REQUIRE(state.getNotes(1, 1).contains(4));                     // pencil marks survived
        REQUIRE(fixture.view_model->getHintCount() == kMaxHints - 3);  // spent budget survived
        REQUIRE(state.getElapsedTime() >= std::chrono::seconds(90));   // play time survived
    }

    SECTION("AC9: a PRE-8.16 auto-save — no history_complete key at all — resumes and re-saves") {
        // The upgrade path, and the one that would hit every existing player exactly once. A save
        // written by an older build carries no history_complete key, so it deserializes to the
        // default TRUE. Inheriting that flag verbatim on restore would mark the resumed session as
        // fully logged, and its first manual save would be rejected on reload — D2, reopened for
        // the whole of that session (and permanently, for anyone with auto-save disabled).
        // restoreGameState therefore derives completeness from the board, not from the flag.
        SavedGame legacy = makeAutoSaveWithProgress(kPuzzle);
        REQUIRE(legacy.history_complete);  // the pre-8.16 shape: key absent → default true

        fixture.view_model->restoreGameState(legacy);
        REQUIRE(fixture.view_model->gameState.get().getValue(0, 0) == 5);
        REQUIRE(fixture.view_model->saveCurrentGame("saved-after-legacy-resume"));

        auto listed = fixture.save_manager->listSaves();
        REQUIRE(listed.has_value());
        REQUIRE(listed->size() == 1);
        fixture.view_model->loadGame(listed->front().save_id);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.extractGivenNumbers() == kPuzzle);  // NOT a freshly generated puzzle
        REQUIRE(state.getValue(0, 0) == 5);
    }

    SECTION("AC10: resuming a progress-free auto-save does NOT exempt the session from the guard") {
        // The other direction of the same mistake. Every auto-save is written history_complete =
        // false, so inheriting the flag would mean that quitting with no moves made and relaunching
        // left the whole next session exempt from the phantom-value check — even though the log it
        // builds from that untouched board is a complete account of it. Combined with the legacy
        // case above, that would have collapsed the guard's real coverage to "sessions that began
        // with New Game".
        SavedGame untouched = makeAutoSave(kPuzzle);  // original == current: no progress at all
        untouched.history_complete = false;           // ...exactly as SaveManager::autoSave writes it

        fixture.view_model->restoreGameState(untouched);
        REQUIRE(fixture.view_model->saveCurrentGame("played-from-a-clean-resume"));

        auto listed = fixture.save_manager->listSaves();
        REQUIRE(listed.has_value());
        REQUIRE(listed->size() == 1);
        auto reloaded = fixture.save_manager->loadGame(listed->front().save_id);
        REQUIRE(reloaded.has_value());
        REQUIRE(reloaded->history_complete);  // judged by the heuristic, like any normally-played game
    }

    SECTION("AC9: a manual save of a resumed game is still loadable after further play") {
        // Guard rail rather than a discriminating regression case: a non-empty move log means
        // neither corruption check could fire, before or after this story. It is here so that the
        // ordinary resumed-then-played path cannot silently break while the empty-log paths above
        // are being reworked.
        SavedGame resumed = makeAutoSaveWithProgress(kPuzzle);
        resumed.history_complete = false;
        fixture.view_model->restoreGameState(resumed);
        fixture.view_model->enterNumber({.row = 2, .col = 0}, 1);

        REQUIRE(fixture.view_model->saveCurrentGame("resumed-then-played"));

        auto listed = fixture.save_manager->listSaves();
        REQUIRE(listed.has_value());
        REQUIRE(listed->size() == 1);

        fixture.view_model->loadGame(listed->front().save_id);

        const auto& state = fixture.view_model->gameState.get();
        REQUIRE(state.extractGivenNumbers() == kPuzzle);
        REQUIRE(state.getValue(0, 0) == 5);  // the pre-resume digit
        REQUIRE(state.getValue(2, 0) == 1);  // the post-resume digit
    }
}
