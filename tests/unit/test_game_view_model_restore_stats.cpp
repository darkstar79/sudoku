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

// Story 8.1 (findings SAVE-2 / SAVE-3): restoring a save left the game half-wired.
//   * SAVE-2 — restoreGameState never applied saved_game.elapsed_time (GameState had no setter at
//     all), so a resumed game showed 00:00:00 and the *next* auto-save wrote that zero over the
//     stored value: irreversible loss. The moves_made/hints_used/mistakes save fields round-tripped
//     through the serializer but no producer ever wrote them and no consumer ever read them.
//   * SAVE-3 — restoreGameState never called startGameSession(), so current_game_session_ stayed 0:
//     getHintCount() returned 0 (hint button dead), recordMove skipped statistics entirely, and the
//     completion's endGameSession(true) was a no-op — a finished resumed game never reached stats.
// This is the DEFAULT launch path (main.cpp resumes any auto-save on start). These tests pin the
// whole restore contract: elapsed time, hint budget, counters, and a countable statistics session.

#include "../../src/core/settings_manager.h"
#include "../helpers/game_view_model_fixture.h"
#include "../helpers/test_utils.h"

#include <chrono>
#include <memory>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

namespace {

/// Default hint budget when no ISettingsManager is injected (the fixture injects none).
/// Mirrors kDefaultMaxHints in the ViewModel.
constexpr int kMaxHints = 10;

// A valid, near-complete puzzle with a unique solution. Empty cells: (0,0), (1,1), (2,0), (4,3),
// (4,4), (5,5), (6,8), (7,7), (8,6), (8,8). A freshly generated board will not equal this one, so
// `extractGivenNumbers() == kPuzzle` is a reliable "resumed THIS puzzle" discriminator.
BoardData makePuzzle() {
    return {{0, 3, 0, 6, 7, 8, 9, 1, 2}, {6, 0, 2, 1, 9, 5, 3, 4, 8}, {0, 9, 8, 3, 4, 2, 5, 6, 7},
            {8, 5, 9, 7, 0, 1, 4, 2, 3}, {4, 2, 6, 0, 0, 3, 7, 9, 1}, {7, 1, 3, 9, 2, 0, 8, 5, 6},
            {9, 6, 1, 5, 3, 7, 2, 8, 0}, {2, 8, 7, 4, 1, 9, 6, 0, 5}, {3, 4, 5, 2, 8, 6, 0, 7, 0}};
}

/// A valid complete solution with a single empty cell at (0,0) — placing 1 there completes it.
BoardData makeSolvedExceptOneCell() {
    return {{0, 2, 3, 4, 5, 6, 7, 8, 9}, {4, 5, 6, 7, 8, 9, 1, 2, 3}, {7, 8, 9, 1, 2, 3, 4, 5, 6},
            {2, 3, 1, 5, 6, 4, 8, 9, 7}, {5, 6, 4, 8, 9, 7, 2, 3, 1}, {8, 9, 7, 2, 3, 1, 5, 6, 4},
            {3, 1, 2, 6, 4, 5, 9, 7, 8}, {6, 4, 5, 9, 7, 8, 3, 1, 2}, {9, 7, 8, 3, 1, 2, 6, 4, 5}};
}

/// Auto-save shaped like the ones SaveManager::autoSave actually writes: no move history.
SavedGame makeAutoSave(const BoardData& puzzle) {
    SavedGame saved;
    saved.original_puzzle = puzzle;
    saved.current_state = puzzle;
    saved.difficulty = Difficulty::Easy;
    saved.is_auto_save = true;
    return saved;
}

}  // namespace

// ============================================================================
// AC2 / AC3 — elapsed time survives restore, and re-saving no longer destroys it
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Restore applies the saved elapsed time",
          "[game_view_model][restore][regression][bug-restore-session]") {
    sudoku::test::GameViewModelFixture fixture;
    const BoardData kPuzzle = makePuzzle();
    REQUIRE(fixture.validator->validateBoard(kPuzzle));  // fixture precondition

    SECTION("AC2: a save with 90 s elapsed resumes at 90 s, not 00:00:00") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.elapsed_time = std::chrono::seconds(90);

        fixture.view_model->restoreGameState(saved);

        // The VM builds its GameState on the default SystemTimeProvider (no injection point at this
        // layer), so assert a monotone lower bound — never an exact wall-clock value.
        REQUIRE(fixture.view_model->gameState.get().getElapsedTime() >= std::chrono::seconds(90));
        REQUIRE(fixture.view_model->getFormattedTime() != "00:00:00");
    }

    SECTION("AC3: autoSave after a restore preserves the stored time instead of overwriting it") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.elapsed_time = std::chrono::seconds(90);
        fixture.view_model->restoreGameState(saved);

        fixture.view_model->autoSave();

        auto reloaded = fixture.save_manager->loadAutoSave();
        REQUIRE(reloaded.has_value());
        REQUIRE(reloaded->elapsed_time >= std::chrono::seconds(90));
    }

    SECTION("AC3: saveCurrentGame after a restore preserves the stored time") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.elapsed_time = std::chrono::seconds(90);
        fixture.view_model->restoreGameState(saved);

        REQUIRE(fixture.view_model->saveCurrentGame("resumed"));

        auto listed = fixture.save_manager->listSaves();
        REQUIRE(listed.has_value());
        REQUIRE(listed->size() == 1);
        REQUIRE(listed->front().elapsed_time >= std::chrono::seconds(90));
    }
}

// ============================================================================
// AC4 / AC8 — the hint budget survives a restart
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Restore starts a statistics session so hints work",
          "[game_view_model][restore][regression][bug-restore-session]") {
    sudoku::test::GameViewModelFixture fixture;
    const BoardData kPuzzle = makePuzzle();
    REQUIRE(fixture.validator->validateBoard(kPuzzle));

    SECTION("AC4: a restored game has a full hint budget, not 0") {
        fixture.view_model->restoreGameState(makeAutoSave(kPuzzle));

        REQUIRE(fixture.view_model->getHintCount() == kMaxHints);
    }

    SECTION("AC4: getHint on a legal empty cell consumes a hint") {
        fixture.view_model->restoreGameState(makeAutoSave(kPuzzle));
        const int before = fixture.view_model->getHintCount();

        fixture.view_model->getHint(core::Position{.row = 0, .col = 0});  // empty, not a given

        REQUIRE(fixture.view_model->getHintCount() == before - 1);
    }

    SECTION("AC8: restored hints_used is charged against the budget (no hint farming by relaunch)") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.hints_used = 3;

        fixture.view_model->restoreGameState(saved);

        REQUIRE(fixture.view_model->getHintCount() == kMaxHints - 3);
    }

    SECTION("AC8: a hostile hints_used floors the budget at 0, never negative") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.hints_used = 999;  // corrupt/hostile value — story 7-1 does not validate this field

        fixture.view_model->restoreGameState(saved);

        REQUIRE(fixture.view_model->getHintCount() == 0);
        // `== 0` alone is NOT a failing test: it is also what the unfixed code returned (no session
        // at all → getHintCount() short-circuits to 0). Pin the mechanism as well — the session must
        // exist and carry the value — so reverting the feature fails here.
        fixture.view_model->autoSave();
        auto reloaded = fixture.save_manager->loadAutoSave();
        REQUIRE(reloaded.has_value());
        REQUIRE(reloaded->hints_used == 999);  // seeded verbatim, not clamped down to the budget
    }

    SECTION("AC8: hints_used above the budget survives a settings round-trip (no free hints)") {
        // Regression guard for the clamp-then-write-back trap: if the restore path clamped
        // hints_used to the *current* max_hints, lowering the budget and raising it again would
        // hand the player back hints they had already spent.
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.hints_used = 8;  // 8 of the default 10 spent

        fixture.view_model->restoreGameState(saved);
        REQUIRE(fixture.view_model->getHintCount() == kMaxHints - 8);

        fixture.view_model->autoSave();
        auto reloaded = fixture.save_manager->loadAutoSave();
        REQUIRE(reloaded.has_value());
        REQUIRE(reloaded->hints_used == 8);  // the true count, not a budget-clamped one
    }

    SECTION("AC8: a hostile mistake count is clamped before it reaches the game-info dialog") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.mistakes = 2'000'000'000;

        fixture.view_model->restoreGameState(saved);

        REQUIRE(fixture.view_model->getMistakeCount() == core::MAX_PROGRESS_COUNTER);
    }

    SECTION("AC8: a negative hints_used cannot inflate the budget") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.hints_used = -5;

        fixture.view_model->restoreGameState(saved);

        REQUIRE(fixture.view_model->getHintCount() == kMaxHints);
    }

    SECTION("AC8: the restored mistake count reaches GameState (game-info dialog is truthful)") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.mistakes = 2;

        fixture.view_model->restoreGameState(saved);

        REQUIRE(fixture.view_model->getMistakeCount() == 2);
    }
}

// ============================================================================
// AC5 / AC6 / AC8 — a resumed game is countable
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Completing a restored game reaches statistics",
          "[game_view_model][restore][regression][bug-restore-session]") {
    sudoku::test::GameViewModelFixture fixture;
    fixture.stats_manager->setCollectDetailedStats(true);  // so the session record is inspectable

    SavedGame saved = makeAutoSave(makeSolvedExceptOneCell());
    saved.elapsed_time = std::chrono::seconds(90);
    saved.moves_made = 5;
    saved.mistakes = 1;
    saved.difficulty = Difficulty::Hard;  // distinct from the fixture default, so AC4 is provable
    saved.puzzle_rating = 4.5;

    SECTION("AC5/AC6/AC8: the finished session is recorded with seeded counters and prior time") {
        fixture.view_model->restoreGameState(saved);

        fixture.view_model->enterNumber(core::Position{.row = 0, .col = 0}, 1);  // final digit
        REQUIRE(fixture.view_model->isGameComplete());

        auto aggregate = fixture.stats_manager->getAggregateStats();
        REQUIRE(aggregate.has_value());
        REQUIRE(aggregate->total_completed == 1);  // AC5: it counted at all

        auto sessions = fixture.stats_manager->getAllSessions();
        REQUIRE(sessions.has_value());
        REQUIRE(sessions->size() == 1);
        const auto& recorded = sessions->front();
        REQUIRE(recorded.completed);
        REQUIRE(recorded.moves_made == 6);                          // AC6: 5 seeded + 1 played
        REQUIRE(recorded.mistakes == 1);                            // seeded, not re-zeroed
        REQUIRE(recorded.time_played >= std::chrono::seconds(90));  // AC8: prior time included

        // AC4's ordering requirement, which nothing else pins: startRestoredSession() must run
        // AFTER the rating/difficulty block in restoreGameState. Hoisting it above that block —
        // the exact mistake the AC and the Landmines section forbid — records rating 0.0 and the
        // default difficulty, and every other assertion in this file still passes.
        REQUIRE(recorded.puzzle_rating == 4.5);
        REQUIRE(recorded.difficulty == Difficulty::Hard);
    }
}

// ============================================================================
// AC7 — the save producers populate the progress counters
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Saves carry the progress counters",
          "[game_view_model][restore][regression][bug-restore-session]") {
    sudoku::test::GameViewModelFixture fixture;

    SECTION("AC7: autoSave writes moves_made/hints_used from the active session") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        auto empty = sudoku::test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        if (empty.has_value()) {
            fixture.view_model->getHint(empty);  // consumes one hint
            REQUIRE(fixture.view_model->getHintCount() == kMaxHints - 1);

            auto next_empty = sudoku::test::findEmptyCell(fixture.view_model->gameState.get());
            REQUIRE(next_empty.has_value());
            if (next_empty.has_value()) {
                fixture.view_model->enterNumber(next_empty.value(), 1);
            }

            fixture.view_model->autoSave();

            auto reloaded = fixture.save_manager->loadAutoSave();
            REQUIRE(reloaded.has_value());
            REQUIRE(reloaded->hints_used == 1);
            REQUIRE(reloaded->moves_made >= 1);
        }
    }

    SECTION("AC7: saveCurrentGame writes the same counters") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        auto empty = sudoku::test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        if (empty.has_value()) {
            fixture.view_model->getHint(empty);
        }

        REQUIRE(fixture.view_model->saveCurrentGame("counters"));

        auto listed = fixture.save_manager->listSaves();
        REQUIRE(listed.has_value());
        REQUIRE(listed->size() == 1);
        REQUIRE(listed->front().hints_used == 1);
    }

    SECTION("AC7/AC9d: a legacy save with zero counters starts from zero and then tracks live play") {
        // Asserting "0 round-trips as 0" would be tautological — the fields are 0 before and after,
        // so deleting applyProgressCounters entirely would keep it green. Play one hint after the
        // restore so the assertion distinguishes "counters wired to the live session" from
        // "counters left at their defaults".
        SavedGame saved = makeAutoSave(makePuzzle());  // legacy shape: all counters default 0
        fixture.view_model->restoreGameState(saved);
        REQUIRE(fixture.view_model->getHintCount() == kMaxHints);  // nothing seeded

        fixture.view_model->getHint(core::Position{.row = 0, .col = 0});
        fixture.view_model->autoSave();

        auto reloaded = fixture.save_manager->loadAutoSave();
        REQUIRE(reloaded.has_value());
        REQUIRE(reloaded->hints_used == 1);  // the hint spent after the restore was recorded
        REQUIRE(reloaded->mistakes == 0);
    }
}

// ============================================================================
// AC9 — no behavior regressions
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Restore session handling does not regress existing behavior",
          "[game_view_model][restore][regression][bug-restore-session]") {
    sudoku::test::GameViewModelFixture fixture;
    fixture.stats_manager->setCollectDetailedStats(true);
    const BoardData kPuzzle = makePuzzle();

    SECTION("AC9a: a rejected manual save falls through to a new game and starts exactly one session") {
        SavedGame corrupt;
        corrupt.original_puzzle = kPuzzle;
        corrupt.current_state = kPuzzle;
        corrupt.notes[0][0].add(5);  // progress signal with original == current → corruption tell
        corrupt.difficulty = Difficulty::Easy;
        corrupt.is_auto_save = false;
        corrupt.hints_used = 4;  // must NOT leak into the fresh game

        fixture.view_model->restoreGameState(corrupt);

        REQUIRE(fixture.view_model->gameState.get().extractGivenNumbers() != kPuzzle);  // fresh game
        REQUIRE(fixture.view_model->getHintCount() == kMaxHints);                       // fresh budget, not seeded
        // A second startGameSession() on the reject path would have ended the fresh game's session
        // as abandoned and pushed a record; nothing has ended, so the aggregate is still empty.
        auto aggregate = fixture.stats_manager->getAggregateStats();
        REQUIRE(aggregate.has_value());
        REQUIRE(aggregate->total_games == 0);
    }

    // The elapsed-time assertions below deliberately compare against the SEEDED value (90 s) rather
    // than a small absolute bound like "< 1 s". The VM builds its GameState on the real
    // SystemTimeProvider (no injection point at this layer), so an absolute bound is a wall-clock
    // assertion that a loaded CI runner, a sanitizer build, or a filesystem stall could cross. What
    // these cases actually mean is "the fresh game did not inherit the restored game's clock", and
    // a 90 s margin states exactly that with no timing sensitivity.
    SECTION("AC9b: startNewGame still begins at 00:00:00 with a full budget and zero counters") {
        SavedGame seeded = makeAutoSave(kPuzzle);
        seeded.elapsed_time = std::chrono::seconds(90);
        seeded.hints_used = 3;
        seeded.mistakes = 2;
        fixture.view_model->restoreGameState(seeded);

        fixture.view_model->startNewGame(Difficulty::Easy);

        REQUIRE(fixture.view_model->gameState.get().getElapsedTime() < std::chrono::seconds(90));
        REQUIRE(fixture.view_model->getFormattedTime() != "00:01:30");  // not the restored 90 s
        REQUIRE(fixture.view_model->getHintCount() == kMaxHints);
        REQUIRE(fixture.view_model->getMistakeCount() == 0);
    }

    // Scoped to a generated game on purpose: resetGame() reads getSolutionBoard() unguarded
    // (game_view_model.cpp:148) and therefore throws bad_optional_access on a *restored* game,
    // which never carries a solution board. That is a pre-existing defect on the restore path,
    // unrelated to SAVE-2/SAVE-3 and deliberately not absorbed by this story.
    SECTION("AC9b: resetGame still returns a generated game to 00:00:00 with a full budget") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        auto empty = sudoku::test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        if (empty.has_value()) {
            fixture.view_model->getHint(empty);  // spend a hint before resetting
            REQUIRE(fixture.view_model->getHintCount() == kMaxHints - 1);
        }

        fixture.view_model->resetGame();

        REQUIRE(fixture.view_model->getHintCount() == kMaxHints);
        REQUIRE(fixture.view_model->getMistakeCount() == 0);
        // No elapsed-time assertion here on purpose. This section starts a *generated* game, so
        // there is no seeded value to compare against, and the only alternative — an absolute
        // "< 1 s" bound — is a real wall-clock assertion: the window spans a full solver run
        // (getHint) plus startGameSession/endGameSession file I/O, which a loaded CI runner or a
        // sanitizer build can stretch past a second. resetTimer()'s exact semantics are already
        // pinned deterministically at the GameState layer under MockTimeProvider
        // (test_game_state.cpp, "Reset timer").
    }

    SECTION("AC9c: restoring over a live game ends the prior session as abandoned — exactly once") {
        fixture.view_model->startNewGame(Difficulty::Easy);

        fixture.view_model->restoreGameState(makeAutoSave(kPuzzle));

        auto sessions = fixture.stats_manager->getAllSessions();
        REQUIRE(sessions.has_value());
        REQUIRE(sessions->size() == 1);  // the abandoned one, not two
        REQUIRE(!sessions->front().completed);
        REQUIRE(fixture.view_model->getHintCount() == kMaxHints);  // the restored game is live
    }
}

// ============================================================================
// AC4/AC8 with a real ISettingsManager injected — the configuration that ships.
// GameViewModelFixture passes nullptr for settings_manager, so every other case in this file
// exercises only the kDefaultMaxHints fallback. Production always injects one (main.cpp), and the
// budget arithmetic differs whenever max_hints != 10, so the shipped branch needs its own cover.
// ============================================================================

// NOLINTNEXTLINE(readability-function-cognitive-complexity) — Catch2 SECTIONs expand to nested conditionals
TEST_CASE("GameViewModel - Restored hint budget honors a configured max_hints",
          "[game_view_model][restore][regression][bug-restore-session]") {
    sudoku::test::TempTestDir temp_dir;
    auto validator = std::make_shared<core::GameValidator>();
    auto generator = std::make_shared<core::PuzzleGenerator>();
    auto solver = std::make_shared<core::SudokuSolver>(validator);
    auto stats_manager = std::make_shared<core::StatisticsManager>(temp_dir.path());
    auto save_manager = std::make_shared<core::SaveManager>(temp_dir.path());
    auto settings_manager = std::make_shared<core::SettingsManager>(temp_dir.path() / "settings.yaml");

    constexpr int kConfiguredMaxHints = 4;
    settings_manager->setMaxHints(kConfiguredMaxHints);

    viewmodel::GameViewModel view_model(validator, generator, solver, stats_manager, save_manager, settings_manager);
    const BoardData kPuzzle = makePuzzle();

    SECTION("A fresh restore gets the configured budget, not the built-in default") {
        view_model.restoreGameState(makeAutoSave(kPuzzle));

        REQUIRE(view_model.getHintCount() == kConfiguredMaxHints);
    }

    SECTION("Seeded hints_used is charged against the configured budget") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.hints_used = 3;

        view_model.restoreGameState(saved);

        REQUIRE(view_model.getHintCount() == kConfiguredMaxHints - 3);
    }

    SECTION("A save carrying more spent hints than the current budget floors at 0, not negative") {
        SavedGame saved = makeAutoSave(kPuzzle);
        saved.hints_used = 8;  // budget was larger when the game was saved

        view_model.restoreGameState(saved);

        REQUIRE(view_model.getHintCount() == 0);

        // ...and the true count is preserved, so raising the budget again restores the right
        // remainder rather than gifting hints back.
        view_model.autoSave();
        auto reloaded = save_manager->loadAutoSave();
        REQUIRE(reloaded.has_value());
        REQUIRE(reloaded->hints_used == 8);

        settings_manager->setMaxHints(10);
        view_model.restoreGameState(*reloaded);
        REQUIRE(view_model.getHintCount() == 2);  // 10 - 8, not 10 - 4
    }
}
