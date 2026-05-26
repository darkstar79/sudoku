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

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <limits>

#include <catch2/catch_test_macros.hpp>

using namespace sudoku;
using namespace sudoku::viewmodel;
using namespace sudoku::core;

using StateCoverageFixture = sudoku::test::GameViewModelFixture;

// Out-of-band value used to exercise the `default:` arm in the GameCommand switches.
// GameCommand's underlying type is uint8_t — its max never collides with a real
// enumerator since the live enum has < 20 entries. If the enum ever grows past
// ~250 entries, bump this and re-verify.
constexpr GameCommand kBogusCommand = static_cast<GameCommand>(std::numeric_limits<std::uint8_t>::max());

TEST_CASE("GameViewModel::executeCommand routes coaching/reset commands and warns on unknown",
          "[game_view_model][state][coverage]") {
    StateCoverageFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    SECTION("GetHint command dispatches through executeCommand") {
        // Route through executeCommand to cover the `case GameCommand::GetHint:` arm.
        // With no selection, getHint() publishes an error message — observing it
        // proves the dispatch actually reached the handler.
        fixture.view_model->clearErrorMessage();
        fixture.view_model->executeCommand(GameCommand::GetHint);
        REQUIRE(fixture.view_model->hasError());
    }

    SECTION("Coaching commands route through to the coaching subsystem") {
        // GetCoachingHint on a fresh game either activates a coaching session
        // (active=true, level>=1) or publishes an error if no logical step is
        // found / no hints remain — either outcome proves the dispatch reached
        // requestCoachingHint().
        fixture.view_model->clearErrorMessage();
        REQUIRE_FALSE(fixture.view_model->coachingState.get().active);
        fixture.view_model->executeCommand(GameCommand::GetCoachingHint);
        const bool dispatched = fixture.view_model->hasError() || fixture.view_model->coachingState.get().active;
        REQUIRE(dispatched);

        // CheckCoachingAnswer / ApplyCoachingStep outside TryIt are no-ops — but the
        // switch arms still need to be hit. Just verify they don't throw and leave
        // the observable consistent.
        REQUIRE_NOTHROW(fixture.view_model->executeCommand(GameCommand::CheckCoachingAnswer));
        REQUIRE_NOTHROW(fixture.view_model->executeCommand(GameCommand::ApplyCoachingStep));
    }

    SECTION("ResetGame command restores the puzzle to its initial state") {
        auto empty = test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        fixture.view_model->enterNumber(empty.value(), 5);
        REQUIRE(fixture.view_model->gameState.get().getValue(empty.value()) == 5);

        fixture.view_model->executeCommand(GameCommand::ResetGame);
        REQUIRE(fixture.view_model->gameState.get().getValue(empty.value()) == 0);
    }

    SECTION("Unknown command falls into default warn branch without side effects") {
        fixture.view_model->clearErrorMessage();
        const auto board_before = fixture.view_model->gameState.get().extractNumbers();
        fixture.view_model->executeCommand(kBogusCommand);
        REQUIRE(fixture.view_model->gameState.get().extractNumbers() == board_before);
    }
}

TEST_CASE("GameViewModel::canExecuteCommand covers coaching, reset and default branches",
          "[game_view_model][state][coverage]") {
    StateCoverageFixture fixture;

    SECTION("ResetGame requires an active game") {
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ResetGame));
        fixture.view_model->startNewGame(Difficulty::Easy);
        REQUIRE(fixture.view_model->canExecuteCommand(GameCommand::ResetGame));
    }

    SECTION("Coaching commands gated by CoachingPhase::TryIt") {
        fixture.view_model->startNewGame(Difficulty::Easy);
        // No coaching session active → not in TryIt.
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::CheckCoachingAnswer));
        REQUIRE_FALSE(fixture.view_model->canExecuteCommand(GameCommand::ApplyCoachingStep));
    }

    SECTION("Unknown command falls into default-true branch") {
        REQUIRE(fixture.view_model->canExecuteCommand(kBogusCommand));
    }
}

TEST_CASE("GameViewModel - state-query and UI-flag setters", "[game_view_model][state][coverage]") {
    StateCoverageFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    SECTION("isGameStateDirty is callable in both states") {
        // Exercise the predicate before and after a write so the underlying
        // GameState::isDirty path runs in both branches. We don't assert the
        // before/after values — that's covered by GameState's own tests.
        (void)fixture.view_model->isGameStateDirty();
        auto empty = test::findEmptyCell(fixture.view_model->gameState.get());
        REQUIRE(empty.has_value());
        fixture.view_model->enterNumber(empty.value(), 5);
        fixture.view_model->enterNumber(empty.value(), 5);
        (void)fixture.view_model->isGameStateDirty();
    }

    SECTION("setShowConflicts / setShowHints toggle UIState flags") {
        fixture.view_model->setShowConflicts(true);
        REQUIRE(fixture.view_model->uiState.get().show_conflicts);
        fixture.view_model->setShowConflicts(false);
        REQUIRE_FALSE(fixture.view_model->uiState.get().show_conflicts);

        fixture.view_model->setShowHints(true);
        REQUIRE(fixture.view_model->uiState.get().show_hints);
        fixture.view_model->setShowHints(false);
        REQUIRE_FALSE(fixture.view_model->uiState.get().show_hints);
    }

    SECTION("clearErrorMessage / hasError round-trip") {
        // Provoke an error: getHint with no selection publishes errorMessage.
        fixture.view_model->getHint(std::nullopt);
        REQUIRE(fixture.view_model->hasError());

        fixture.view_model->clearErrorMessage();
        REQUIRE_FALSE(fixture.view_model->hasError());
    }
}

TEST_CASE("GameViewModel - data accessors and session-history helpers", "[game_view_model][state][coverage]") {
    StateCoverageFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    SECTION("getSaveList returns whatever the save manager has (possibly empty)") {
        // Fresh temp dir → no saves on disk yet.
        auto saves = fixture.view_model->getSaveList();
        REQUIRE(saves.empty());
    }

    SECTION("getAggregateStats and getRecentGames return optional/vector wrappers") {
        auto stats = fixture.view_model->getAggregateStats();
        // Empty stats store still returns an aggregate (zeroed); we only care
        // that the call path executes.
        (void)stats;

        auto recent = fixture.view_model->getRecentGames(5);
        REQUIRE(recent.size() <= 5);
    }

    SECTION("refreshRecentSaves publishes to the observable") {
        fixture.view_model->refreshRecentSaves();
        // Empty store → empty list, but the call must have run without error.
        REQUIRE(fixture.view_model->recentSaves.get().empty());
    }

    SECTION("deleteSessionHistory and flushStatsSessions are callable") {
        REQUIRE_NOTHROW(fixture.view_model->deleteSessionHistory());
        REQUIRE_NOTHROW(fixture.view_model->flushStatsSessions());
    }

    SECTION("formatDuration formats as HH:MM:SS") {
        using namespace std::chrono_literals;
        REQUIRE(GameViewModel::formatDuration(0ms) == "00:00:00");
        REQUIRE(GameViewModel::formatDuration(3661s) == "01:01:01");
    }
}

TEST_CASE("GameViewModel CSV/JSON export entry points", "[game_view_model][state][coverage]") {
    StateCoverageFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    // The default-directory export functions (exportAggregateStatsCsv / exportGameSessionsCsv)
    // resolve their output path via AppDirectoryManager::getDefaultDirectory, which honors
    // XDG_DATA_HOME on Linux. Redirect it to the fixture's temp dir so the test stays
    // hermetic and doesn't pollute the user's real ~/.local/share/sudoku/.
    struct XdgOverride {
        std::string previous;
        bool had_previous{false};
        explicit XdgOverride(const std::string& path) {
            if (const char* prev = std::getenv("XDG_DATA_HOME")) {
                previous = prev;
                had_previous = true;
            }
            ::setenv("XDG_DATA_HOME", path.c_str(), 1);
        }
        XdgOverride(const XdgOverride&) = delete;
        XdgOverride& operator=(const XdgOverride&) = delete;
        XdgOverride(XdgOverride&&) = delete;
        XdgOverride& operator=(XdgOverride&&) = delete;
        ~XdgOverride() {
            if (had_previous) {
                ::setenv("XDG_DATA_HOME", previous.c_str(), 1);
            } else {
                ::unsetenv("XDG_DATA_HOME");
            }
        }
    };
    XdgOverride xdg(fixture.temp_dir.path().string());

    // The serializer does not auto-create parent directories — pre-create the
    // path that AppDirectoryManager will resolve to under the redirected XDG_DATA_HOME.
    std::filesystem::create_directories(fixture.temp_dir.path() / "sudoku" / "stats");

    SECTION("exportAggregateStatsCsv writes a CSV under the redirected XDG path") {
        auto result = fixture.view_model->exportAggregateStatsCsv();
        REQUIRE(result.has_value());
    }

    SECTION("exportGameSessionsCsv writes a CSV under the redirected XDG path") {
        auto result = fixture.view_model->exportGameSessionsCsv();
        REQUIRE(result.has_value());
    }

    SECTION("exportStatistics writes a JSON file at the explicit path") {
        auto target = fixture.temp_dir.path() / "stats_export.json";
        fixture.view_model->exportStatistics(target.string());
        REQUIRE(std::filesystem::exists(target));
    }
}

TEST_CASE("GameViewModel checkGameCompletion fires when last cell is filled with the solution",
          "[game_view_model][state][coverage]") {
    StateCoverageFixture fixture;
    fixture.view_model->startNewGame(Difficulty::Easy);

    // Drive completion through the real path: enterNumber → checkGameCompletion.
    // Fill every empty cell with its solution value; the call after the last
    // placement should flip isGameComplete() to true.
    const auto& solution = fixture.view_model->gameState.get().getSolutionBoard();
    const auto initial = fixture.view_model->gameState.get().extractNumbers();
    for (size_t row = 0; row < 9; ++row) {
        for (size_t col = 0; col < 9; ++col) {
            if (initial[row][col] == 0) {
                fixture.view_model->enterNumber({.row = row, .col = col}, solution[row][col]);
            }
        }
    }

    REQUIRE(fixture.view_model->isGameComplete());
}
